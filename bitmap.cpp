#include <stdlib.h>
#include <string.h>
#include "alport.h"

#define MASKED_COLOR    0

/* Only supporting 8-bit bitmaps */
BITMAP *create_bitmap(int width, int height)
{
   BITMAP *bitmap;
   int i;

   bitmap = (BITMAP *)malloc(sizeof(BITMAP));
   if (!bitmap)
      return NULL;

   bitmap->dat = malloc(width * height);
   if (!bitmap->dat)
   {
      free(bitmap);
      return NULL;
   }

   bitmap->w = bitmap->cr = width;
   bitmap->h = bitmap->cb = height;
   bitmap->clip = TRUE;
   bitmap->cl = bitmap->ct = 0;

   bitmap->line = (unsigned char * *)malloc(sizeof(unsigned char *) * height);
   if (!bitmap->line)
   {
      free(bitmap->dat);
      free(bitmap);
      return NULL;
   }

   bitmap->line[0] = (unsigned char *)bitmap->dat;
   for (i = 1; i < height; i++)
      bitmap->line[i] = bitmap->line[i - 1] + width;

   return bitmap;
}


BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int width, int height)
{
   BITMAP *bitmap;
   int i;

   if (x + width > parent->w)
      width = parent->w - x;

   if (y + height > parent->h)
      height = parent->h - y;

   bitmap = (BITMAP *)malloc(sizeof(BITMAP));
   if (!bitmap)
      return NULL;

   /* Since it is a sub bitmap it doesn't have its own memory space. */
   bitmap->dat = NULL;

   bitmap->w = bitmap->cr = width;
   bitmap->h = bitmap->cb = height;
   bitmap->clip = TRUE;
   bitmap->cl = bitmap->ct = 0;

   bitmap->line = (unsigned char * *)malloc(sizeof(unsigned char *) * height);
   if (!bitmap->line)
   {
      free(bitmap);
      return NULL;
   }

   /* Each line points to a line in the parent bitmap */
   for (i = 0; i < height; i++)
      bitmap->line[i] = parent->line[y + i] + x;

   return bitmap;
}


void destroy_bitmap(BITMAP *bitmap)
{
   if (bitmap)
   {
      if (bitmap->line)
         free(bitmap->line);

      if (bitmap->dat)
         free(bitmap->dat);

      free(bitmap);
   }
}


void clear_to_color(BITMAP *bitmap, int color)
{
   int i;
   /* if no data allocated for the bitmap it very
    * likely it is a sub-bitmap so we use its lines
    * to access the shared parent area.
    */
   if (!bitmap->dat)
   {
      for (i = 0; i < bitmap->h; i++)
         memset(bitmap->line[i], color, bitmap->w);

      return;
   }

   /* Since it is a 8 bpp bitmap we simply set
    * the memory to the required color.
    */
   memset(bitmap->dat, color, bitmap->w * bitmap->h);
}


void clear_bitmap(BITMAP *bitmap)
{
   clear_to_color(bitmap, 0);
}


void blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w,
          int h)
{
   int y;

   for (y = 0; y < h; y++)
   {
      unsigned char *s = src->line[y + sy] + sx;
      unsigned char *d = dst->line[y + dy] + dx;

      memmove(d, s, w);
   }
}


/* masked_blit:
 *  Masked (skipping transparent pixels) blitting routine.
 */
void masked_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy,
                 int w, int h)
{
   int x, y;

   for (y = 0; y < h; y++)
   {
      unsigned char *s = src->line[y + sy] + sx;
      unsigned char *d = dst->line[y + dy] + dx;

      for (x = 0; x < w; s++, d++, x++)
      {
         unsigned char c = *s;

         if (c != MASKED_COLOR)
            *d = c;
      }
   }
}


void draw_sprite(BITMAP *bmp, BITMAP *sprite, int dx, int dy)
{
   masked_blit(sprite, bmp, 0, 0, dx, dy, sprite->w, sprite->h);
}


void draw_sprite_v_flip(BITMAP *bmp, BITMAP *sprite, int dx, int dy)
{
   int x, y;
   int h = sprite->h - 1;

   for (y = 0; y <= h; y++)
   {
      unsigned char *s = sprite->line[h - y];
      unsigned char *d = bmp->line[y + dy] + dx;

      for (x = 0; x < sprite->w; s++, d++, x++)
      {
         unsigned char c = *s;

         if (c != MASKED_COLOR)
            *d = c;
      }
   }
}


/* set_clip_rect:
 *  Sets the two opposite corners of the clipping rectangle to be used when
 *  drawing to the bitmap. Nothing will be drawn to positions outside of this
 *  rectangle. When a new bitmap is created the clipping rectangle will be
 *  set to the full area of the bitmap.
 */
void set_clip_rect(BITMAP *bitmap, int x1, int y1, int x2, int y2)
{
   /* internal clipping is inclusive-exclusive */
   x2++;
   y2++;

   bitmap->cl = CLAMP(0, x1, bitmap->w - 1);
   bitmap->ct = CLAMP(0, y1, bitmap->h - 1);
   bitmap->cr = CLAMP(0, x2, bitmap->w);
   bitmap->cb = CLAMP(0, y2, bitmap->h);
}


/* Information for stretching line */
static struct
{
   int xcstart; /* x counter start */
   int sxinc; /* amount to increment src x every time */
   int xcdec; /* amount to decrement counter by, increase sptr when this reaches 0 */
   int xcinc; /* amount to increment counter by when it reaches 0 */
   int linesize; /* size of a whole row of pixels */
} _al_stretch;


static void stretch_line(unsigned char *dptr, unsigned char *sptr)
{
   int xc = _al_stretch.xcstart;
   unsigned char *dend = dptr + _al_stretch.linesize;
   for (; dptr < dend; dptr++, sptr += _al_stretch.sxinc)
   {
      *dptr = *sptr;
      if (xc <= 0)
      {
         sptr++;
         xc += _al_stretch.xcinc;
      }
      else
         xc -= _al_stretch.xcdec;
   }
}

void stretch_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int sw, int sh,
                  int dx, int dy, int dw, int dh)
{
   int y; /* current dst y */
   int yc; /* y counter */
   int sxofs, dxofs; /* start offsets */
   int syinc; /* amount to increment src y each time */
   int ycdec; /* amount to deccrement counter by, increase sy when this reaches 0 */
   int ycinc; /* amount to increment counter by when it reaches 0 */
   int size = 0; /* pixel size */
   int dxbeg, dxend; /* clipping information */
   int dybeg, dyend;
   int i;

   if ((sw <= 0) || (sh <= 0) || (dw <= 0) || (dh <= 0))
      return;

   if (dst->clip)
   {
      dybeg = ((dy > dst->ct) ? dy : dst->ct);
      dyend = (((dy + dh) < dst->cb) ? (dy + dh) : dst->cb);
      if (dybeg >= dyend)
         return;

      dxbeg = ((dx > dst->cl) ? dx : dst->cl);
      dxend = (((dx + dw) < dst->cr) ? (dx + dw) : dst->cr);
      if (dxbeg >= dxend)
         return;
   }
   else
   {
      dxbeg = dx;
      dxend = dx + dw;
      dybeg = dy;
      dyend = dy + dh;
   }

   syinc = sh / dh;
   ycdec = sh - (syinc * dh);
   ycinc = dh - ycdec;
   yc = ycinc;
   sxofs = sx * size;
   dxofs = dx * size;

   _al_stretch.sxinc = sw / dw * size;
   _al_stretch.xcdec = sw - ((sw / dw) * dw);
   _al_stretch.xcinc = dw - _al_stretch.xcdec;
   _al_stretch.linesize = (dxend - dxbeg) * size;

   /* get start state (clip) */
   _al_stretch.xcstart = _al_stretch.xcinc;
   for (i = 0; i < dxbeg - dx; i++, sxofs += _al_stretch.sxinc)
   {
      if (_al_stretch.xcstart <= 0)
      {
         _al_stretch.xcstart += _al_stretch.xcinc;
         sxofs += size;
      }
      else
         _al_stretch.xcstart -= _al_stretch.xcdec;
   }

   dxofs += i * size;

   /* skip clipped lines */
   for (y = dy; y < dybeg; y++, sy += syinc)
   {
      if (yc <= 0)
      {
         sy++;
         yc += ycinc;
      }
      else
         yc -= ycdec;
   }

   /* Stretch it */

   for (; y < dyend; y++, sy += syinc)
   {
      stretch_line(dst->line[y] + dxofs, src->line[sy] + sxofs);
      if (yc <= 0)
      {
         sy++;
         yc += ycinc;
      }
      else
         yc -= ycdec;
   }
}
