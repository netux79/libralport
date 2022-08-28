#ifndef ALPORT_ROTATE_H
#define ALPORT_ROTATE_H

#include "base.h"

#ifdef __cplusplus
extern "C"
{
#endif

void pivot_scaled_sprite_flip(BITMAP *bmp, BITMAP *sprite,
                              fixed x, fixed y, fixed cx, fixed cy,
                              fixed angle, fixed scale, int v_flip);

inline void rotate_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, fixed angle)
{
   pivot_scaled_sprite_flip(bmp, sprite, (x << 16) + (sprite->w * 0x10000) / 2,
                              (y << 16) + (sprite->h * 0x10000) / 2,
                              sprite->w << 15, sprite->h << 15,
                              angle, 0x10000, FALSE);
}

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALPORT_ROTATE_H */
