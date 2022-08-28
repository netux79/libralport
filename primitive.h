#ifndef ALPORT_PRIMITIVE_H
#define ALPORT_PRIMITIVE_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRAW_MODE_SOLID             0        /* flags for drawing_mode() */
#define DRAW_MODE_TRANS             1

extern void (*putpixel)(BITMAP *bmp, int x, int y, int color);
extern void (*hline)(BITMAP *bmp, int x1, int y, int x2, int color);
extern void (*vline)(BITMAP *bmp, int x, int y1, int y2, int color);

void drawing_mode(int mode);
int getpixel(BITMAP *bmp, int x, int y);
void line(BITMAP *bmp, int x1, int y1, int x2, int y2, int color);
void rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color);
void rectfill(BITMAP *bmp, int x1, int y1, int x2, int y2, int color);
void circle(BITMAP *bmp, int x, int y, int radius, int color);
void circlefill(BITMAP *bmp, int x, int y, int radius, int color);

#ifdef __cplusplus
}
#endif

#endif          /* ifndef ALPORT_PRIMITIVE_H */
