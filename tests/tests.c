#include <stdio.h>
#include <math.h>
#include <string.h>
#include "perspector.h"

/* Forward declarations of private functions being tested. */
bool projectable(rect *result, pixelset *anchors);
bool make_transform_matrix(double transform_matrix[9], pixelset *anchors, coord width, coord height);

static void test_transform(gsl_matrix *m, coord x, coord y) {
	double a[3] = { x, y, 1 };
	gsl_vector_view vec = gsl_vector_view_array(a, 3);
	double vbuf[3];
	gsl_vector_view check = gsl_vector_view_array(vbuf, 3);
	gsl_blas_dgemv(CblasNoTrans, 1.0, m, &vec.vector, 0.0, &check.vector);

	/* 'round' is required since a cast floors the value. */
	coord x2 = round(vbuf[0] / vbuf[2]);
	coord y2 = round(vbuf[1] / vbuf[2]);

	printf("Transform (%i, %i) -> (%i, %i)\n", x, y, x2, y2);
}

static char *comp_vertex(rect r, pixel p) {
	if (r.bl.x == p.x && r.bl.y == p.y) {
		return "bl";
	} else if (r.br.x == p.x && r.br.y == p.y) {
		return "br";
	} else if (r.tr.x == p.x && r.tr.y == p.y) {
		return "tr";
	} else {
		return "tl";
	}
}

static void test_project(coord blx, coord bly, coord brx, coord bry, coord trx, coord try, coord tlx, coord tly, bool expect) {
	pixel bl, br, tr, tl;
	bl.x = blx; bl.y = bly;
	br.x = brx; br.y = bry;
	tr.x = trx; tr.y = try;
	tl.x = tlx; tl.y = tly;
	pixelset anchors = { .pixels = { bl, br, tr, tl }, .count = 4 };
	rect order;
	bool result = projectable(&order, &anchors);
	char *msg = "OK";
	char *blmsg;
	char *brmsg;
	char *trmsg;
	char *tlmsg;

	if (expect != result) {
		msg = "FAIL";
	}

	blmsg = comp_vertex(order, bl);
	brmsg = comp_vertex(order, br);
	trmsg = comp_vertex(order, tr);
	tlmsg = comp_vertex(order, tl);
	printf("%s [expect %i, got %i] bl(%i, %i)->%s, br(%i, %i)->%s, tr(%i, %i)->%s, tl(%i, %i)->%s\n",
		msg, expect, result,
		bl.x, bl.y, blmsg,
		br.x, br.y, brmsg,
		tr.x, tr.y, trmsg,
		tl.x, tl.y, tlmsg);
}

int main(void) {
	/* Init */
	pixelset ps = {
		.pixels = {
			{ 32, 64 },
			{ 80, 48 },
			{ 48, 96 },
			{ 16, 384 }
		},
		.count = 4
	};

	double transform[9];
	make_transform_matrix(transform, &ps, 1024, 768);

	/* Tests */
	gsl_matrix_view transform_mv = gsl_matrix_view_array(transform, 3, 3);
	test_transform(&transform_mv.matrix, 32, 64);
	test_transform(&transform_mv.matrix, 80, 48);
	test_transform(&transform_mv.matrix, 48, 96);
	test_transform(&transform_mv.matrix, 16, 384);

	test_project(0, 0, 1, 2, 1, 3, 0, 1, true); /* 2 pairs aligned on X. */
	test_project(0, 0, 1, 0, 3, 1, 2, 1, true); /* 2 pairs aligned on Y. */
	test_project(0, 0, 3, 2, 2, 2, 0, 1, false); /* 2 pairs aligned, one on X, one on Y. */
	test_project(0, 1, 0, -1, 1, 0, 2, 0, false); /* Middle pair aligned on X. */
	test_project(-1, 0, 1, 0, 0, 1, 0, 2, false); /* Middle pair aligned on Y. */
	test_project(-1, 0, 0, 1, 1, 0, 0, -1, false); /* Losange. */
	test_project(0, 0, 1, 0, 1, 1, 0, 1, true); /* Square. */
	test_project(0, 0, 2, 0, 2, 2, 1, 1, true); /* 3 aligned. */
	test_project(0, 0, 3, 2, 4, 3, 1, 1, true); /* Order dependent. */
	test_project(-2, 1, -1, 2, 2, -1, 1, -2, false); /* Ambiguous input. */
	test_project(0, 0, 0, 0, 0, 1, 1, 1, false); /* Two points share coordinates. */
	test_project(0, 0, 1, 1, 2, 2, 3, 3, false); /* 4 aligned on a diagonal */

	return 0;
}
