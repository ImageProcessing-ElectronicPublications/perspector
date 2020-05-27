#ifndef PERSPECTOR_H
#define PERSPECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>

/* Pixel coordinates can be negative to express out-of-screen values. A size of
2 billion sounds a reasonable upper value, but we never know what can happen, so
we typedef it. */
typedef int32_t coord;
#define COORD_MAX INT32_MAX

typedef struct
{
    coord x, y;
} pixel;

typedef struct
{
    double x, y;
} point;

typedef struct
{
    pixel pixels[4];
    size_t count;
} pixelset;

/*
b = bottom
t = top
l = left
r = right */
typedef struct
{
    pixel bl, br, tr, tl;
} rect;

/* This color structure represents a cell in the array returned by
cairo_image_surface_get_data() in ARGB mode. WARNING: It is stored backward. */
typedef struct
{
    unsigned char blue, green, red, alpha;
} color;

bool
perspector(color *sink_data, coord sink_width, coord sink_height,
           color *bg_data, coord bg_width, coord bg_height,
           pixelset *anchors);

#endif
