/*
Perspective transformation algorithm.

Terms:
* pixel: discrete pairs of coordinates in 2D space.
* point: double precision pairs of coordinates in 2D spaces.
* coord: signed integer big enough to hold the coordinates of a pixel.
* anchor: control point, embodied in a pixel.

We usually work with pixels, but some calculations require intermediate floating
point values, in which case we work with points.

The perspective transformation process takes a background picture `bg` as input
together with 4 anchors, or control points (pixels on the picture). It returns a
transformed picture in which the control points are at the corners.

* First step is to dispatch the anchors over the 4 corners of the resulting
rectangle, it at all possible.

* The core algorithm relies mainly on the computation of a perspective matrix.
Since a 2D perspective transformation is not linear, we work in 3D (i.e. we use
homogeneous coordinates), then we project the result back on the 2D plane. The
perspective matrix is 3x3, however since we ultimately apply a projection, we
only need to determine 8 coefficients.

    The resolution of the matrix is a system of 8 equations. Thus we need 4
points, since each point provides 2 equations, one per dimension. A singular
value decomposition (SVD) is used to solve the system.

* We apply the transformation on every pixel found inside the polygon formed by
the anchors.

* The previous transformation might yield 'holes' in the output picture. To fill
these, we interpolate between the nearest neighbours.

*/

#include <string.h>
#include <math.h>
#include "perspector.h"

/* Globals */
/* The following globals are needed as argument of qsort(). */
/* Reference pixel in angle comparison. */
static pixel ref;
/* Barycentre, used as origin in angle comparison. */
static point bar;

/* Position of one point with respect to another. */
typedef enum {
	LEFT = 1,
	RIGHT = 2,
	EQUAL = 4,
	OPPOSED = 8,
	UNDEF = 16
} position;

typedef enum {
	DIR_X,
	DIR_Y
} direction;

/******************************************************************************/

int comparex(const void *a, const void *b) {
	return ((pixel *)a)->x - ((pixel *)b)->x;
}
int comparey(const void *a, const void *b) {
	return ((pixel *)a)->y - ((pixel *)b)->y;
}

static void psort(pixelset *set, direction dir) {
	if (dir == DIR_X) {
		qsort(set->pixels, set->count, sizeof (pixel), comparex);
	} else {
		qsort(set->pixels, set->count, sizeof (pixel), comparey);
	}
}

/* Return position of point 'p' compared to the vector (refn-origin). */
inline static position pos(point p, point refn) {
	if ((p.x == 0 && p.y == 0)
		|| (refn.x == 0 && refn.y == 0)) {
		return UNDEF;

	} else if (p.y * refn.x - refn.y * p.x == 0) {
		if (p.x * refn.x > 0) {
			/* Aligned and in same direction. */
			return EQUAL;
		} else {
			return OPPOSED;
		}

	} else if (refn.x == 0) {
		/* If refn is on the Y-axis. */
		if ((refn.y > 0 && p.x < 0) || (refn.y < 0 && p.x > 0)) {
			return LEFT;
		} else {
			return RIGHT;
		}

	} else if ((refn.x < 0 && p.y < p.x * refn.y / refn.x) ||
		(refn.x > 0 && p.y > p.x * refn.y / refn.x)) {
		/* If refn is on the left part, p.y must be below the curve from the origin
		* through refn. And vice-versa. */
		return LEFT;

	} else {
		return RIGHT;
	}
}

/* 'a' is less than 'b' if its angle to 'ref' with 'bar' as origin is inferior,
* meaning that 'a' is closer to ref than 'b'. */
static int compare_angle(const void *a, const void *b) {
	point refn = { ref.x - bar.x, ref.y - bar.y };
	point an = { ((pixel *)a)->x - bar.x, ((pixel *)a)->y - bar.y };
	point bn = { ((pixel *)b)->x - bar.x, ((pixel *)b)->y - bar.y };

	position an_refn = pos(an, refn);
	position an_bn = pos(an, bn);
	position bn_refn = pos(bn, refn);

	if (an_bn == EQUAL || an_bn == UNDEF) {
		/* If at least one of the point is equal to the barycentre, comparison does
		not make sense. We return 0 as a fallback, but this should be handled by the
		callee. */
		return 0;
	} else if (an_refn == EQUAL ||
		(bn_refn == LEFT && an_refn == LEFT && an_bn == RIGHT)
		|| (bn_refn == RIGHT && (an_bn == RIGHT || an_refn == LEFT))
		|| (bn_refn == OPPOSED && an_refn == LEFT)) {
		/* 'an' can be colinear to refn. */
		return -1;
	} else {
		/* Opposite of first case. */
		return 1;
	}
}

/* Check if the order in which the points are passed as arguments match
* 'order'. */
static bool check_order(pixelset order, pixel bl, pixel br, pixel tr, pixel tl) {

	#define CMP_PIXEL(offset, pixel) (order.pixels[(i + offset) % 4].x == pixel.x && order.pixels[(i + offset) % 4].y == pixel.y)

	size_t i;
	for (i = 0; i < order.count; i++) {
		if (CMP_PIXEL(0, bl)) {
			break;
		}
	}
	if (CMP_PIXEL(1, br)
		&& CMP_PIXEL(2, tr)
		&& CMP_PIXEL(3, tl)) {
		return true;
	}

	#undef CMP_PIXEL

	return false;
}


/*
We check if pixels can be projected to the vertices of a rectangle without
ambiguity. We split the plane horizontally and vertically to dispatch on the
X-axis and the Y-axis. There are various possible configurations:

- 1 pixel in each of the 4 partitions: no ambiguity.
- 2 pixels are on a split segment (e.g. losange): impossible.
- 2 pixels in the top-left partition, 2 pixels in the bottom-right partition.
- 2 pixels in the top-right partition, 2 pixels in the bottom-left partition.

The last 2 cases are symmetric. In that case, we need to make sure that

- if 2 pixels are X-aligned in one partition, the other 2 pixels are not
Y-aligned, and vice-versa;

- the relative order against the barycentre is preserved;

- there is no ambiguity. Indeed, an X-split followed by an Y-split does not
yield the same result as the other way around in these last 2 cases. We raise
the ambiguity if only one way preserves the relative order.
*/
/* Not static for test purposes. */
bool projectable(rect *result, pixelset *anchors) {
	pixelset xsorted = *anchors;
	pixelset ysorted = *anchors;
	psort(&xsorted, DIR_X);
	psort(&ysorted, DIR_Y);

	if (xsorted.pixels[1].x != xsorted.pixels[2].x
		&& (xsorted.pixels[0].y < xsorted.pixels[2].y
		|| xsorted.pixels[0].y < xsorted.pixels[3].y
		|| xsorted.pixels[1].y < xsorted.pixels[2].y
		|| xsorted.pixels[1].y < xsorted.pixels[3].y)
		&& (xsorted.pixels[0].y > xsorted.pixels[2].y
		|| xsorted.pixels[0].y > xsorted.pixels[3].y
		|| xsorted.pixels[1].y > xsorted.pixels[2].y
		|| xsorted.pixels[1].y > xsorted.pixels[3].y)
		&& (xsorted.pixels[2].y != xsorted.pixels[3].y)) {

		/* 1 pixel in each partition. */
		if (xsorted.pixels[0].y < xsorted.pixels[1].y) {
			result->bl = xsorted.pixels[0];
			result->tl = xsorted.pixels[1];
		} else {
			result->bl = xsorted.pixels[1];
			result->tl = xsorted.pixels[0];
		}
		if (xsorted.pixels[2].y < xsorted.pixels[3].y) {
			result->br = xsorted.pixels[2];
			result->tr = xsorted.pixels[3];
		} else {
			result->br = xsorted.pixels[3];
			result->tr = xsorted.pixels[2];
		}

		return true;

	} else if (xsorted.pixels[1].x == xsorted.pixels[2].x
		|| ysorted.pixels[1].y == ysorted.pixels[2].y) {

		/* 2 pixels on a split axis. */
		return false;

	} else {
		/* 2 pairs in 2 partitions. */

		/* Compute barycentre. We need to set global variables here for `qsort`.
		Variable 'a b c d' are shortcuts. */
		pixel a = anchors->pixels[0];
		pixel b = anchors->pixels[1];
		pixel c = anchors->pixels[2];
		pixel d = anchors->pixels[3];
		bar.x = (double)(a.x + b.x + c.x + d.x) / 4;
		bar.y = (double)(a.y + b.y + c.y + d.y) / 4;

		/* If at least one point is equal to the barycentre, this is not computable
		since this point is not comparable. If at least two points are aligned with
		the barycentre, comparison cannot is not strict and thus
		underterministic. */
		point an = { a.x - bar.x, a.y - bar.y };
		point bn = { b.x - bar.x, b.y - bar.y };
		point cn = { c.x - bar.x, c.y - bar.y };
		point dn = { d.x - bar.x, d.y - bar.y };
		unsigned int UNDESIRED = EQUAL | OPPOSED | UNDEF;
		if ((pos(an, bn) == UNDESIRED)
			|| (pos(an, cn) == UNDESIRED)
			|| (pos(an, dn) == UNDESIRED)
			|| (pos(bn, cn) == UNDESIRED)
			|| (pos(bn, dn) == UNDESIRED)
			|| (pos(cn, dn) == UNDESIRED)) {
			return false;
		}

		/* Order is trigonometric. */
		ref = a;
		pixelset order = { .pixels = { a, b, c, d }, .count = 4 };
		qsort(order.pixels, order.count, sizeof (pixel), compare_angle);

		bool x_ok = false;
		bool y_ok = false;

		rect x_result;
		rect y_result;

		/* X-splitting. */
		if (xsorted.pixels[1].x != xsorted.pixels[2].x
			&& xsorted.pixels[0].y != xsorted.pixels[1].y
			&& xsorted.pixels[2].y != xsorted.pixels[3].y) {

			/* Y-axis. */
			if (xsorted.pixels[0].y < xsorted.pixels[1].y) {
				x_result.bl = xsorted.pixels[0];
				x_result.tl = xsorted.pixels[1];
			} else {
				x_result.bl = xsorted.pixels[1];
				x_result.tl = xsorted.pixels[0];
			}

			if (xsorted.pixels[2].y < xsorted.pixels[3].y) {
				x_result.br = xsorted.pixels[2];
				x_result.tr = xsorted.pixels[3];
			} else {
				x_result.br = xsorted.pixels[3];
				x_result.tr = xsorted.pixels[2];
			}

			if (check_order(order, x_result.bl, x_result.br, x_result.tr, x_result.tl)) {
				x_ok = true;
			}
		}

		/* Y-splitting. */
		if (ysorted.pixels[1].y != ysorted.pixels[2].y
			&& ysorted.pixels[0].x != ysorted.pixels[1].x
			&& ysorted.pixels[2].x != ysorted.pixels[3].x) {

			/* X-axis. */
			if (ysorted.pixels[0].x < ysorted.pixels[1].x) {
				y_result.bl = ysorted.pixels[0];
				y_result.br = ysorted.pixels[1];
			} else {
				y_result.bl = ysorted.pixels[1];
				y_result.br = ysorted.pixels[0];
			}

			if (ysorted.pixels[2].x < ysorted.pixels[3].x) {
				y_result.tl = ysorted.pixels[2];
				y_result.tr = ysorted.pixels[3];
			} else {
				y_result.tl = ysorted.pixels[3];
				y_result.tr = ysorted.pixels[2];
			}

			if (check_order(order, y_result.bl, y_result.br, y_result.tr, y_result.tl)) {
				y_ok = true;
			}
		}

		/* Return OK if only one of the solution is OK. */
		if (y_ok && !x_ok) {
			memcpy(result, &y_result, sizeof (rect));
			return true;
		} else if (!y_ok && x_ok) {
			memcpy(result, &x_result, sizeof (rect));
			return true;
		}
	}
	return false;
}

/* Generate matrix corresponding to the system of equations induced by the
homogeneous transformation of 4 pixels. Note that since we have only 4 points,
we have only 8 equations (4 per dimension). Since we are in homogeneous
coordinates, the result is up to a scale factor; only 8 equations are needed. */
static double *init_system_equation(double system_equation[81], pixel bl, pixel br, pixel tr, pixel tl, coord width, coord height) {

	/* Constants: we only work in 2D, so transformation matrices and systems of
	equations will always be of the same size. There is no need to use variable.
	However, writing raw numbers might make it hard to read, so we use symbols to
	make them more explicit. */

	/* System of equations dimensions. */
	#define ROWS 9
	#define COLS 9

	/* Cells we do not define are 0. */
	memset(system_equation, 0, ROWS * COLS * sizeof (double));

	/* The following macro helps writing the matrix in a mathematical fashion
	* below. */
	#define MATRIX_ROW(row, m0, m1, m2, m3, m4, m5, m6, m7, m8) \
		system_equation[COLS * row + 0] = m0; \
		system_equation[COLS * row + 1] = m1; \
		system_equation[COLS * row + 2] = m2; \
		system_equation[COLS * row + 3] = m3; \
		system_equation[COLS * row + 4] = m4; \
		system_equation[COLS * row + 5] = m5; \
		system_equation[COLS * row + 6] = m6; \
		system_equation[COLS * row + 7] = m7; \
		system_equation[COLS * row + 8] = m8;

	/* We use these to clarify the view of the matrix below. */
	double w = (double)width;
	double h = (double)height;

	/* *INDENT-OFF* */
	MATRIX_ROW(0, bl.x, bl.y, 1,	0,		0,		0,	0,					0,					0);
	MATRIX_ROW(1, 0,		0,		0,	bl.x, bl.y, 1,	0,					0,					0);
	MATRIX_ROW(2, br.x, br.y, 1,	0,		0,		0,	-w * br.x,	-w * br.y,	-w);
	MATRIX_ROW(3, 0,		0,		0,	br.x, br.y, 1,	0,					0,					0);
	MATRIX_ROW(4, tr.x, tr.y, 1,	0,		0,		0,	-w * tr.x,	-w * tr.y,	-w);
	MATRIX_ROW(5, 0,		0,		0,	tr.x, tr.y, 1,	-h * tr.x,	-h * tr.y,	-h);
	MATRIX_ROW(6, tl.x, tl.y, 1,	0,		0,		0,	0,					0,					0);
	MATRIX_ROW(7, 0,		0,		0,	tl.x, tl.y, 1,	-h * tl.x,	-h * tl.y,	-h);
	/* *INDENT-ON* */

	#undef FILL_LINE_WIDTH
	#undef FILL_LINE_HEIGHT
	#undef ROWS
	#undef COLS

	return system_equation;
}

/* Not static so that it can be tested externally. */
bool make_transform_matrix(double transform_matrix[9], pixelset *anchors, coord width, coord height) {

	rect vertices;

	bool status = projectable(&vertices, anchors);
	if (!status) {
		return false;
	}

	/* System of equations dimensions: there are 9 equations of 9 unknowns (the
	number of elements in the transform matrix). */
	#define ROWS 9
	#define COLS 9
	double system_equation[81];
	init_system_equation(system_equation, vertices.bl, vertices.br, vertices.tr, vertices.tl, width, height);

	gsl_matrix_view m = gsl_matrix_view_array(system_equation, ROWS, COLS);

	double s_data[COLS];
	gsl_vector_view s = gsl_vector_view_array(s_data, COLS);

	double work[COLS];
	gsl_vector_view work_vv = gsl_vector_view_array(work, COLS);

	double v[COLS * COLS];
	gsl_matrix_view v_mv = gsl_matrix_view_array(v, COLS, COLS);

	/* SVD result is stored in v. */
	gsl_linalg_SV_decomp(&m.matrix, &v_mv.matrix, &s.vector, &work_vv.vector);

	/* Get final transformation matrix from the last column returned by the SVD.
	We loop over 9 elements, the size of the matrix. The offset '8' is for the
	last column. */
	int i;
	for (i = 0; i < 9; i++) {
		transform_matrix[i] = v[i * COLS + 8];
	}

	#undef ROWS
	#undef COLS

	return true;
}

bool perspector(
	color *sink_data, coord sink_width, coord sink_height,
	color *bg_data, coord bg_width, coord bg_height,
	pixelset *anchors) {

	double transform_matrix[9];
	bool status = make_transform_matrix(transform_matrix, anchors, sink_width, sink_height);
	/* TODO: report status message. */
	if (!status) {
		return false;
	}

	gsl_matrix_view transform_mv = gsl_matrix_view_array(transform_matrix, 3, 3);

	/* We proceed in two steps: first we transform every pixel in 'bg' between the
	* anchors to the 'sink'. Every pixel processed in sink is marked in
	* 'interpol_mask'. Second, we interpolate every pixel from sink that is not
	* marked in 'interpol_mask'. */

	/* TODO: for now, we loop over all pixels and discard those leading values
	outside the sink. It would be much faster to loop over the polygon
	defined by the anchors, but this is quite long to implement: first we
	need to split the polygon in triangles, taking care of not splitting on
	concavities. Second, we loop over every triangle, marking the vertices as
	'processed' when done. This way we can discard a segment if its 2
	vertices are already processed.

	To process a triangle, one need to start from the upper vertex and to
	proceed line by line, starting from the segment where the other vertex is
	highest, ending on the segment of the other vertex. Once reached, and if
	the remaining segment was not reached as well, we start from the last
	segment.

	TODO: Use external library for that?
	*/

	/* Loop over pixels in bg: */
	if (COORD_MAX / sink_width < sink_height) {
		fprintf(stderr, "The picture is too big, memory cannot be allocated.\n");
		return false;
	}

	/* We use the following mask to know which pixel in the sink has been set. */
	bool *transformed_mask = calloc(sink_width * sink_height, sizeof (bool));
	if (!transformed_mask) {
		fprintf(stderr, "Transformed mask allocation error.\n");
		return false;
	}
	coord x, y;
	for (x = 0; x < bg_width; x++) {
		for (y = 0; y < bg_height; y++) {

			double pin[3] = { x, y, 1 };
			gsl_vector_view pin_vv = gsl_vector_view_array(pin, 3);
			double pout[3];
			gsl_vector_view pout_vv = gsl_vector_view_array(pout, 3);
			gsl_blas_dgemv(CblasNoTrans,
				1.0, &transform_mv.matrix, &pin_vv.vector,
				0.0, &pout_vv.vector);

			/* 'round' is required since a cast floors the value. */
			coord x2 = round(pout[0] / pout[2]);
			coord y2 = round(pout[1] / pout[2]);

			if (x2 >= 0 && y2 >= 0 && x2 < sink_width && y2 < sink_height) {
				sink_data[y2 * sink_width + x2] = bg_data[y * bg_width + x];
				transformed_mask[y2 * sink_width + x2] = true;
			}
		}
	}

	/* Interpolate the holes.
	TODO: Improve interpolation.
	-Allow choosing between linear and Lanczlos.
	-Allow choosing between manhattan distance and euclidian distance.
	http://en.wikipedia.org/wiki/Multivariate_interpolation#Irregular_grid_.28scattered_data.29
	*/

	for (x = 0; x < sink_width; x++) {
		for (y = 0; y < sink_height; y++) {
			if (!transformed_mask[y * sink_width + x]) {
				coord radius;
				coord i, j;
				/* We will sum the colors of all the pixels in the radius to compute the
				* mean. We need intermediate variables for colors to prevent
				* overflow. */
				double red_buf, green_buf, blue_buf, alpha_buf;
				red_buf = green_buf = blue_buf = alpha_buf = 0;
				int count = 0;
				for (radius = 1; !count; radius++) {
					/* We use a square instead of a circle for performance reasons. */
					coord x_min = x - radius;
					coord x_max = x + radius;
					coord y_min = y - radius;
					coord y_max = y + radius;
					if (x_min < 0) {
						x_min = 0;
					}
					if (y_min < 0) {
						y_min = 0;
					}
					if (x_max >= sink_width) {
						x_max = sink_width - 1;
					}
					if (y_max >= sink_height) {
						y_max = sink_height - 1;
					}

					for (i = x_min; i <= x_max; i += x_max - x_min) {
						for (j = y_min; j <= y_max; j++) {
							coord index = j * sink_width + i;
							if (transformed_mask[index]) {
								count++;
								red_buf += (double)sink_data[index].red;
								green_buf += (double)sink_data[index].green;
								blue_buf += (double)sink_data[index].blue;
								alpha_buf += (double)sink_data[index].alpha;
							}
						}
					}

					for (j = y_min; j <= y_max; j += y_max - y_min) {
						for (i = x_min; i <= x_max; i++) {
							coord index = j * sink_width + i;
							if (transformed_mask[index]) {
								count++;
								red_buf += (double)sink_data[index].red;
								green_buf += (double)sink_data[index].green;
								blue_buf += (double)sink_data[index].blue;
								alpha_buf += (double)sink_data[index].alpha;
							}
						}
					}
				}

				red_buf /= count;
				green_buf /= count;
				blue_buf /= count;
				alpha_buf /= count;
				coord index = y * sink_width + x;
				sink_data[index].red = (char)red_buf;
				sink_data[index].green = (char)green_buf;
				sink_data[index].blue = (char)blue_buf;
				sink_data[index].alpha = (char)alpha_buf;
			}
		}
	}

	free(transformed_mask);
	return true;
}
