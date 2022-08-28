#ifndef ALPORT_PRIMITIVE2_H
#define ALPORT_PRIMITIVE2_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

void ellipse(BITMAP *bmp, int x, int y, int rx, int ry, int color);
void ellipsefill(BITMAP *bmp, int ix, int iy, int rx0, int ry0, int color);
void arc(BITMAP *bmp, int x, int y, fixed ang1, fixed ang2, int r, int color);
void floodfill(BITMAP *bmp, int x, int y, int color);
void spline(BITMAP *bmp, const int points[8], int color);

#ifdef __cplusplus
}
#endif

#endif          /* ifndef ALPORT_PRIMITIVE2_H */
