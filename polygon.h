#ifndef ALPORT_POLYGON_H
#define ALPORT_POLYGON_H

#include "base.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* number of fractional bits used by the polygon rasteriser */
#define POLYGON_FIX_SHIFT 18

/* an active polygon edge */
typedef struct POLYGON_EDGE
{
   int top;                   /* top y position */
   int bottom;                /* bottom y position */
   fixed x, dx;               /* fixed point x position and gradient */
   fixed w;                   /* width of line segment */
   POLYGON_SEGMENT dat;       /* texture/gouraud information */
   struct POLYGON_EDGE *prev; /* doubly linked list */
   struct POLYGON_EDGE *next;
} POLYGON_EDGE;


void polygon(BITMAP *bmp, int vertices, const int *points, int color);
void triangle(BITMAP *bmp, int x1, int y1, int x2, int y2, int x3, int y3, int color);

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALPORT_POLYGON_H */
