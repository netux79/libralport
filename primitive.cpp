#include <string.h>
#include "alport.h"


/* info about the current graphics drawing mode */
int _drawing_mode = DRAW_MODE_SOLID;


/* putpixel_solid:
 *  Draws a solid pixel onto a linear bitmap.
 */
static void putpixel_solid(BITMAP *bmp, int x, int y, int color)
{
   if(bmp->clip) 
   {
      if (y < bmp->ct)
         return;
      if (y >= bmp->cb)
         return;
      if (x < bmp->cl)
         return;
      if (x >= bmp->cr)
         return;
   }

   *(bmp->line[y] + x) = color;
}


/* hline_solid:
 *  Draws a horizontal line onto a linear bitmap.
 */
static void hline_solid(BITMAP *bmp, int x1, int y, int x2, int color)
{
   if (bmp->clip)
   {
     if (x1 < bmp->cl)
         x1 = bmp->cl;

      if (x2 >= bmp->cr)
         x2 = bmp->cr - 1;

      if (y < bmp->ct || y >= bmp->cb)
         return;
   }

   unsigned char *d = bmp->line[y] + x1;
   memset(d, color, x2 - x1 + 1);
}


/* vline_solid:
 *  Draws a vertical line onto a linear bitmap. 
 */
static void vline_solid(BITMAP *bmp, int x, int y1, int y2, int color)
{
   int y;

   if (bmp->clip)
   {
     if (y1 < bmp->ct)
         y1 = bmp->ct;

      if (y2 >= bmp->cb)
         y2 = bmp->cb - 1;

      if (x < bmp->cl || x >= bmp->cr)
         return;
   }

   for (y = y1; y <= y2; y++)
      *(bmp->line[y] + x) = color;
}


/* putpixel_trans:
 *  Draws a transparent pixel onto a linear bitmap using a previously
 * created COLORMAP.
 */
static void putpixel_trans(BITMAP *bmp, int x, int y, int color)
{
   if(bmp->clip) 
   {
      if (y < bmp->ct)
         return;
      if (y >= bmp->cb)
         return;
      if (x < bmp->cl)
         return;
      if (x >= bmp->cr)
         return;
   }

   /* Calculate pixel offset */
   unsigned char *p = bmp->line[y] + x;
   /* Get source color */
   int sc = *p;
   /* Replace the color with the mapped color */
   *p = color_map->data[color][sc];
}


/* hline_trans:
 *  Draws a horizontal line onto a linear bitmapusing a previously
 * created COLORMAP.
 */
static void hline_trans(BITMAP *bmp, int x1, int y, int x2, int color)
{
   if (bmp->clip)
   {
     if (x1 < bmp->cl)
         x1 = bmp->cl;

      if (x2 >= bmp->cr)
         x2 = bmp->cr - 1;

      if (y < bmp->ct || y >= bmp->cb)
         return;
   }

   int sc;
   unsigned char *p = bmp->line[y] + x1;
   unsigned char *pe = bmp->line[y] + x2;
   while (p <= pe)
   {
      sc = *p;
      *p = color_map->data[color][sc];
      p++;
   }
}


/* vline_trans:
 *  Draws a vertical line onto a linear bitmap.
 */
static void vline_trans(BITMAP *bmp, int x, int y1, int y2, int color)
{
   int y, sc;
   unsigned char *p;

   if (bmp->clip)
   {
     if (y1 < bmp->ct)
         y1 = bmp->ct;

      if (y2 >= bmp->cb)
         y2 = bmp->cb - 1;

      if (x < bmp->cl || x >= bmp->cr)
         return;
   }

   for (y = y1; y <= y2; y++)
   {
      p = bmp->line[y] + x;
      sc = *p;
      *p = color_map->data[color][sc];
   }
}


/* putpixel, hline, vline:
 *  Hook functions of the basic drawing to define their
 * current functionality by using drawing_mode().
 */
void (*putpixel)(BITMAP *bmp, int x, int y, int color) = putpixel_solid;
void (*hline)(BITMAP *bmp, int x1, int y, int x2, int color) = hline_solid;
void (*vline)(BITMAP *bmp, int x, int y1, int y2, int color) = vline_solid;


/* drawing_mode:
 *  Sets the drawing mode. This only affects routines like putpixel,
 *  lines, rectangles, triangles, etc, not the blitting or sprite
 *  drawing functions.
 */
void drawing_mode(int mode)
{
   if (mode == DRAW_MODE_TRANS && color_map)
   {
      putpixel = putpixel_trans;
      hline = hline_trans;
      vline = vline_trans;
      _drawing_mode = DRAW_MODE_TRANS;
   }
   else
   {
      putpixel = putpixel_solid;
      hline = hline_solid;
      vline = vline_solid;
      _drawing_mode = DRAW_MODE_SOLID;
   }
}


/* getpixel:
 *  Reads a pixel from a linear bitmap.
 */
int getpixel(BITMAP *bmp, int x, int y)
{
   if ((x < 0) || (x >= bmp->w) || (y < 0) || (y >= bmp->h))
      return -1;

   return *(bmp->line[y] + x);
}


/* do_line:
 *  Calculates all the points along a line between x1, y1 and x2, y2,
 *  calling the supplied function for each one. This will be passed a
 *  copy of the bmp parameter, the x and y position, and a copy of the
 *  d parameter (so do_line() can be used with putpixel()).
 */
static void do_line(BITMAP *bmp, int x1, int y1, int x2, int y2, int d,
                    void (*proc)(BITMAP *, int, int, int))
{
   int dx = x2 - x1;
   int dy = y2 - y1;
   int i1, i2;
   int x, y;
   int dd;

   /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)         \
   {                                                                          \
      if (d##pri_c == 0)                                                      \
      {                                                                       \
         proc(bmp, x1, y1, d);                                                \
         return;                                                              \
      }                                                                       \
                                                                              \
      i1 = 2 * d##sec_c;                                                      \
      dd = i1 - (sec_sign (pri_sign d##pri_c));                               \
      i2 = dd - (sec_sign (pri_sign d##pri_c));                               \
                                                                              \
      x = x1;                                                                 \
      y = y1;                                                                 \
                                                                              \
      while (pri_c pri_cond pri_c##2)                                         \
      {                                                                       \
         proc(bmp, x, y, d);                                                  \
                                                                              \
         if (dd sec_cond 0)                                                   \
         {                                                                    \
            sec_c = sec_c sec_sign 1;                                         \
            dd += i2;                                                         \
         }                                                                    \
         else                                                                 \
            dd += i1;                                                         \
                                                                              \
         pri_c = pri_c pri_sign 1;                                            \
      }                                                                       \
   }

   if (dx >= 0)
   {
      if (dy >= 0)
      {
         if (dx >= dy)
         {
            /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
            DO_LINE(+, x, <=, +, y, >=);
         }
         else
         {
            /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
            DO_LINE(+, y, <=, +, x, >=);
         }
      }
      else
      {
         if (dx >= -dy)
         {
            /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
            DO_LINE(+, x, <=, -, y, <=);
         }
         else
         {
            /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
            DO_LINE(-, y, >=, +, x, >=);
         }
      }
   }
   else
   {
      if (dy >= 0)
      {
         if (-dx >= dy)
         {
            /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
            DO_LINE(-, x, >=, +, y, >=);
         }
         else
         {
            /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
            DO_LINE(+, y, <=, -, x, <=);
         }
      }
      else
      {
         if (-dx >= -dy)
         {
            /* (x1 > x2) && (y1 > y2) && (dx >= dy) */
            DO_LINE(-, x, >=, -, y, <=);
         }
         else
         {
            /* (x1 > x2) && (y1 > y2) && (dx < dy) */
            DO_LINE(-, y, >=, -, x, <=);
         }
      }
   }
#undef DO_LINE
}


/* line:
 *  Draws a line from x1, y1 to x2, y2, using putpixel() to do the work.
 */
void line(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
   int sx, sy, dx, dy, t, clip;

   if (x1 == x2)
   {
      vline(bmp, x1, y1, y2, color);
      return;
   }

   if (y1 == y2)
   {
      hline(bmp, x1, y1, x2, color);
      return;
   }

   clip = bmp->clip;

   /* use a bounding box to check if the line needs clipping */
   if (clip)
   {
      sx = x1;
      sy = y1;
      dx = x2;
      dy = y2;

      if (sx > dx)
      {
         t = sx;
         sx = dx;
         dx = t;
      }

      if (sy > dy)
      {
         t = sy;
         sy = dy;
         dy = t;
      }

      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;

      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;
   }

   do_line(bmp, x1, y1, x2, y2, color, putpixel);

   bmp->clip = clip;
}



/* rect:
 *  Draws an outline rectangle.
 */
void rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
   int t;

   if (x2 < x1) 
   {
      t = x1;
      x1 = x2;
      x2 = t;
   }

   if (y2 < y1) 
   {
      t = y1;
      y1 = y2;
      y2 = t;
   }

   hline(bmp, x1, y1, x2, color);

   if (y2 > y1)
      hline(bmp, x1, y2, x2, color);

   if (y2-1 >= y1+1) 
   {
      vline(bmp, x1, y1+1, y2-1, color);

      if (x2 > x1)
         vline(bmp, x2, y1+1, y2-1, color);
   }
}


/* rectfill:
 *  Draws a solid filled rectangle, using hline() to do the work.
 */
void rectfill(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
   int t, clip;

   if (y1 > y2)
   {
      t = y1;
      y1 = y2;
      y2 = t;
   }

   clip = bmp->clip;

   if (clip)
   {

      if (x1 > x2)
      {
         t = x1;
         x1 = x2;
         x2 = t;
      }

      if (x1 < bmp->cl)
         x1 = bmp->cl;

      if (x2 >= bmp->cr)
         x2 = bmp->cr - 1;

      if (x2 < x1)
         return;

      if (y1 < bmp->ct)
         y1 = bmp->ct;

      if (y2 >= bmp->cb)
         y2 = bmp->cb - 1;

      if (y2 < y1)
         return;

      bmp->clip = FALSE;
   }

   while (y1 <= y2)
   {
      hline(bmp, x1, y1, x2, color);
      y1++;
   };

   bmp->clip = clip;
}


/* do_circle:
 *  Helper function for the circle drawing routines. Calculates the points
 *  in a circle of radius r around point x, y, and calls the specified
 *  routine for each one. The output proc will be passed first a copy of
 *  the bmp parameter, then the x, y point, then a copy of the d parameter
 *  (so putpixel() can be used as the callback).
 */
static void do_circle(BITMAP *bmp, int x, int y, int radius, int d,
                      void (*proc)(BITMAP *, int, int, int))
{
   int cx = 0;
   int cy = radius;
   int df = 1 - radius;
   int d_e = 3;
   int d_se = -2 * radius + 5;

   do
   {
      proc(bmp, x + cx, y + cy, d);

      if (cx)
         proc(bmp, x - cx, y + cy, d);

      if (cy)
         proc(bmp, x + cx, y - cy, d);

      if ((cx) && (cy))
         proc(bmp, x - cx, y - cy, d);

      if (cx != cy)
      {
         proc(bmp, x + cy, y + cx, d);

         if (cx)
            proc(bmp, x + cy, y - cx, d);

         if (cy)
            proc(bmp, x - cy, y + cx, d);

         if (cx && cy)
            proc(bmp, x - cy, y - cx, d);
      }

      if (df < 0)
      {
         df += d_e;
         d_e += 2;
         d_se += 2;
      }
      else
      {
         df += d_se;
         d_e += 2;
         d_se += 4;
         cy--;
      }

      cx++;

   }
   while (cx <= cy);
}


/* circle:
 *  Draws a circle.
 */
void circle(BITMAP *bmp, int x, int y, int radius, int color)
{
   int clip, sx, sy, dx, dy;

   clip = bmp->clip;

   if (clip)
   {
      sx = x - radius - 1;
      sy = y - radius - 1;
      dx = x + radius + 1;
      dy = y + radius + 1;

      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;

      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;
   }

   do_circle(bmp, x, y, radius, color, putpixel);

   bmp->clip = clip;
}


/* circlefill:
 *  Draws a filled circle.
 */
void circlefill(BITMAP *bmp, int x, int y, int radius, int color)
{
   int cx = 0;
   int cy = radius;
   int df = 1 - radius;
   int d_e = 3;
   int d_se = -2 * radius + 5;
   int clip, sx, sy, dx, dy;

   clip = bmp->clip;

   if (clip)
   {
      sx = x - radius - 1;
      sy = y - radius - 1;
      dx = x + radius + 1;
      dy = y + radius + 1;

      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;

      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;
   }

   do
   {
      hline(bmp, x - cy, y - cx, x + cy, color);

      if (cx)
         hline(bmp, x - cy, y + cx, x + cy, color);

      if (df < 0)
      {
         df += d_e;
         d_e += 2;
         d_se += 2;
      }
      else
      {
         if (cx != cy)
         {
            hline(bmp, x - cx, y - cy, x + cx, color);

            if (cy)
               hline(bmp, x - cx, y + cy, x + cx, color);
         }

         df += d_se;
         d_e += 2;
         d_se += 4;
         cy--;
      }

      cx++;

   }
   while (cx <= cy);

   bmp->clip = clip;
}
