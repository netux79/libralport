#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "alport.h"


typedef struct FLOODED_LINE /* store segments which have been flooded */
{
   short flags;      /* status of the segment */
   short lpos, rpos; /* left and right ends of segment */
   short y;          /* y coordinate of the segment */
   int next;         /* linked list if several per line */
} FLOODED_LINE;

#define FLOOD_IN_USE 1
#define FLOOD_TODO_ABOVE 2
#define FLOOD_TODO_BELOW 4

extern int _drawing_mode;
static int flood_count;        /* number of flooded segments */
static void *flood_mem = NULL; /* temp mem to perform floodfilling */

#define FLOOD_LINE(c) (((FLOODED_LINE *)flood_mem) + c)


/* do_ellipse:
 *  Helper function for the ellipse drawing routines. Calculates the points
 *  in an ellipse of radius rx and ry around point x, y, and calls the
 *  specified routine for each one. The output proc will be passed first a
 *  copy of the bmp parameter, then the x, y point, then a copy of the d
 *  parameter (so putpixel() can be used as the callback).
 */
static void do_ellipse(BITMAP *bmp, int ix, int iy, int rx0, int ry0, int d,
                       void (*proc)(BITMAP *, int, int, int))
{
   int rx, ry;
   int x, y;
   float x_change;
   float y_change;
   float ellipse_error;
   float two_a_sq;
   float two_b_sq;
   float stopping_x;
   float stopping_y;
   int midway_x = 0;

   rx = MAX(rx0, 0);
   ry = MAX(ry0, 0);

   two_a_sq = 2 * rx * rx;
   two_b_sq = 2 * ry * ry;

   x = rx;
   y = 0;

   x_change = ry * ry * (1 - 2 * rx);
   y_change = rx * rx;
   ellipse_error = 0.0;

   /* The following two variables decide when to stop.  It's easier than
    * solving for this explicitly.
    */
   stopping_x = two_b_sq * rx;
   stopping_y = 0.0;

   /* First set of points. */
   while (y <= ry)
   {
      proc(bmp, ix + x, iy + y, d);
      if (x != 0)
      {
         proc(bmp, ix - x, iy + y, d);
      }

      if (y != 0)
      {
         proc(bmp, ix + x, iy - y, d);
         if (x != 0)
         {
            proc(bmp, ix - x, iy - y, d);
         }
      }

      y++;
      stopping_y += two_a_sq;
      ellipse_error += y_change;
      y_change += two_a_sq;
      midway_x = x;

      if (stopping_x < stopping_y && x > 1)
      {
         break;
      }

      if ((2.0f * ellipse_error + x_change) > 0.0)
      {
         if (x)
         {
            x--;
            stopping_x -= two_b_sq;
            ellipse_error += x_change;
            x_change += two_b_sq;
         }
      }
   }

   /* To do the other half of the ellipse we reset to the top of it, and
    * iterate in the opposite direction.
    */
   x = 0;
   y = ry;

   x_change = ry * ry;
   y_change = rx * rx * (1 - 2 * ry);
   ellipse_error = 0.0;

   while (x < midway_x)
   {
      proc(bmp, ix + x, iy + y, d);
      if (x != 0)
      {
         proc(bmp, ix - x, iy + y, d);
      }

      if (y != 0)
      {
         proc(bmp, ix + x, iy - y, d);
         if (x != 0)
         {
            proc(bmp, ix - x, iy - y, d);
         }
      }

      x++;
      ellipse_error += x_change;
      x_change += two_b_sq;

      if ((2.0f * ellipse_error + y_change) > 0.0)
      {
         if (y)
         {
            y--;
            ellipse_error += y_change;
            y_change += two_a_sq;
         }
      }
   }
}


/* ellipse:
 *  Draws an ellipse.
 */
void ellipse(BITMAP *bmp, int x, int y, int rx, int ry, int color)
{
   int clip, sx, sy, dx, dy;

   if (bmp->clip)
   {
      sx = x - rx - 1;
      sy = y - ry - 1;
      dx = x + rx + 1;
      dy = y + ry + 1;

      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;

      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;

      clip = TRUE;
   }
   else
      clip = FALSE;

   do_ellipse(bmp, x, y, rx, ry, color, putpixel);

   bmp->clip = clip;
}


/* ellipsefill:
 *  Draws a filled ellipse.
 */
void ellipsefill(BITMAP *bmp, int ix, int iy, int rx0, int ry0, int color)
{
   int rx, ry;
   int x, y;
   float x_change;
   float y_change;
   float ellipse_error;
   float two_a_sq;
   float two_b_sq;
   float stopping_x;
   float stopping_y;
   int clip, sx, sy, dx, dy;
   int last_drawn_y;
   int old_y;
   int midway_x = 0;

   rx = MAX(rx0, 0);
   ry = MAX(ry0, 0);

   if (bmp->clip)
   {
      sx = ix - rx - 1;
      sy = iy - ry - 1;
      dx = ix + rx + 1;
      dy = iy + ry + 1;

      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;

      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;

      clip = TRUE;
   }
   else
      clip = FALSE;

   two_a_sq = 2 * rx * rx;
   two_b_sq = 2 * ry * ry;

   x = rx;
   y = 0;

   x_change = ry * ry * (1 - 2 * rx);
   y_change = rx * rx;
   ellipse_error = 0.0;

   /* The following two variables decide when to stop.  It's easier than
    * solving for this explicitly.
    */
   stopping_x = two_b_sq * rx;
   stopping_y = 0.0;

   /* First set of points */
   while (y <= ry)
   {
      hline(bmp, ix - x, iy + y, ix + x, color);
      if (y)
      {
         hline(bmp, ix - x, iy - y, ix + x, color);
      }

      y++;
      stopping_y += two_a_sq;
      ellipse_error += y_change;
      y_change += two_a_sq;
      midway_x = x;

      if (stopping_x < stopping_y && x > 1)
      {
         break;
      }

      if ((2.0f * ellipse_error + x_change) > 0.0)
      {
         if (x)
         {
            x--;
            stopping_x -= two_b_sq;
            ellipse_error += x_change;
            x_change += two_b_sq;
         }
      }
   }

   last_drawn_y = y - 1;

   /* To do the other half of the ellipse we reset to the top of it, and
    * iterate in the opposite direction until we reach the place we stopped at
    * last time.
    */
   x = 0;
   y = ry;

   x_change = ry * ry;
   y_change = rx * rx * (1 - 2 * ry);
   ellipse_error = 0.0;

   old_y = y;

   while (x < midway_x)
   {
      if (old_y != y)
      {
         hline(bmp, ix - x + 1, iy + old_y, ix + x - 1, color);
         if (old_y)
         {
            hline(bmp, ix - x + 1, iy - old_y, ix + x - 1, color);
         }
      }

      x++;
      ellipse_error += x_change;
      x_change += two_b_sq;
      old_y = y;

      if ((2.0f * ellipse_error + y_change) > 0.0)
      {
         if (y)
         {
            y--;
            ellipse_error += y_change;
            y_change += two_a_sq;
         }
      }
   }

   /* On occasion, a gap appears between the middle and upper halves.
    * This 'afterthought' fills it in.
    */
   if (old_y != last_drawn_y)
   {
      hline(bmp, ix - x + 1, iy + old_y, ix + x - 1, color);
      if (old_y)
      {
         hline(bmp, ix - x + 1, iy - old_y, ix + x - 1, color);
      }
   }

   bmp->clip = clip;
}


/* get_point_on_arc:
 *  Helper function for the do_arc() function, converting from (radius, angle)
 *  to (x, y).
 */
static inline void get_point_on_arc(int r, fixed a, int *out_x, int *out_y, int *out_q)
{
   double s, c;
   double double_a = (a & 0xffffff) * (PI * 2 / (1 << 24));
   s = sin(double_a);
   c = cos(double_a);
   s = -s * r;
   c = c * r;
   *out_x = (int)((c < 0) ? (c - 0.5) : (c + 0.5));
   *out_y = (int)((s < 0) ? (s - 0.5) : (s + 0.5));

   if (c >= 0)
   {
      if (s <= 0)
         *out_q = 0; /* quadrant 0 */
      else
         *out_q = 3; /* quadrant 3 */
   }
   else
   {
      if (s <= 0)
         *out_q = 1; /* quadrant 1 */
      else
         *out_q = 2; /* quadrant 2 */
   }
}


/* do_arc:
 *  Helper function for the arc function. Calculates the points in an arc
 *  of radius r around point x, y, going anticlockwise from fixed point
 *  binary angle ang1 to ang2, and calls the specified routine for each one.
 *  The output proc will be passed first a copy of the bmp parameter, then
 *  the x, y point, then a copy of the d parameter (so putpixel() can be
 *  used as the callback).
 */
static void do_arc(BITMAP *bmp, int x, int y, fixed ang1, fixed ang2, int r, int d,
                   void (*proc)(BITMAP *, int, int, int))
{
   /* start position */
   int sx, sy;
   /* current position */
   int px, py;
   /* end position */
   int ex, ey;
   /* square of radius of circle */
   long rr;
   /* difference between main radius squared and radius squared of three
      potential next points */
   long rr1, rr2, rr3;
   /* square of x and of y */
   unsigned long xx, yy, xx_new, yy_new;
   /* start quadrant, current quadrant and end quadrant */
   int sq, q, qe;
   /* direction of movement */
   int dx, dy;
   /* temporary variable for determining if we have reached end point */
   int det;

   /* Calculate the start point and the end point. */
   /* We have to flip y because bitmaps count y coordinates downwards. */
   get_point_on_arc(r, ang1, &sx, &sy, &q);
   px = sx;
   py = sy;
   get_point_on_arc(r, ang2, &ex, &ey, &qe);

   rr = r * r;
   xx = px * px;
   yy = py * py - rr;

   sq = q;

   if (q > qe)
   {
      /* qe must come after q. */
      qe += 4;
   }
   else if (q == qe)
   {
      /* If q==qe but the beginning comes after the end, make qe be
       * strictly after q.
       */
      if (((ang2 & 0xffffff) < (ang1 & 0xffffff)) ||
          (((ang1 & 0xffffff) < 0x400000) && ((ang2 & 0xffffff) >= 0xc00000)))
         qe += 4;
   }

   /* initial direction of movement */
   if (((q + 1) & 2) == 0)
      dy = -1;
   else
      dy = 1;
   if ((q & 2) == 0)
      dx = -1;
   else
      dx = 1;

   while (TRUE)
   {
      /* Change quadrant when needed.
       * dx and dy determine the possible directions to go in this
       * quadrant, so they must be updated when we change quadrant.
       */
      if ((q & 1) == 0)
      {
         if (px == 0)
         {
            if (qe == q)
               break;
            q++;
            dy = -dy;
         }
      }
      else
      {
         if (py == 0)
         {
            if (qe == q)
               break;
            q++;
            dx = -dx;
         }
      }

      /* Are we in the final quadrant? */
      if (qe == q)
      {
         /* Have we reached (or passed) the end point both in x and y? */
         det = 0;

         if (dy > 0)
         {
            if (py >= ey)
               det++;
         }
         else
         {
            if (py <= ey)
               det++;
         }
         if (dx > 0)
         {
            if (px >= ex)
               det++;
         }
         else
         {
            if (px <= ex)
               det++;
         }

         if (det == 2)
            break;
      }

      proc(bmp, x + px, y + py, d);

      /* From here, we have only 3 possible directions of movement, eg.
       * for the first quadrant:
       *
       *    .........
       *    .........
       *    ......21.
       *    ......3*.
       *
       * These are reached by adding dx to px and/or adding dy to py.
       * We need to find which of these points gives the best
       * approximation of the (square of the) radius.
       */

      xx_new = (px + dx) * (px + dx);
      yy_new = (py + dy) * (py + dy) - rr;
      rr1 = xx_new + yy;
      rr2 = xx_new + yy_new;
      rr3 = xx + yy_new;

      /* Set rr1, rr2, rr3 to be the difference from the main radius of the
       * three points.
       */
      if (rr1 < 0)
         rr1 = -rr1;
      if (rr2 < 0)
         rr2 = -rr2;
      if (rr3 < 0)
         rr3 = -rr3;

      if (rr3 >= MIN(rr1, rr2))
      {
         px += dx;
         xx = xx_new;
      }
      if (rr1 > MIN(rr2, rr3))
      {
         py += dy;
         yy = yy_new;
      }
   }

   /* Only draw last point if it doesn't overlap with first one. */
   if ((px != sx) || (py != sy) || (sq == qe))
      proc(bmp, x + px, y + py, d);
}


/* arc:
 *  Draws an arc.
 */
void arc(BITMAP *bmp, int x, int y, fixed ang1, fixed ang2, int r, int color)
{
   do_arc(bmp, x, y, ang1, ang2, r, color, putpixel);
}


/* flooder:
 *  Fills a horizontal line around the specified position, and adds it
 *  to the list of drawn segments. Returns the first x coordinate after
 *  the part of the line which it has dealt with.
 */
static int flooder(BITMAP *bmp, int x, int y, int src_color, int dest_color)
{
   FLOODED_LINE *p;
   int left = 0, right = 0;
   unsigned char *a;
   int c;

   a = bmp->line[y];

   /* check start pixel */
   if ((*(a + x)) != src_color)
      return x + 1;

   /* work left from starting point */
   for (left = x - 1; left >= bmp->cl; left--)
   {
      if ((*(a + left)) != src_color)
         break;
   }

   /* work right from starting point */
   for (right = x + 1; right < bmp->cr; right++)
   {
      if ((*(a + right)) != src_color)
         break;
   }

   left++;
   right--;

   /* draw the line */
   hline(bmp, left, y, right, dest_color);

   /* store it in the list of flooded segments */
   c = y;
   p = FLOOD_LINE(c);

   if (p->flags)
   {
      while (p->next)
      {
         c = p->next;
         p = FLOOD_LINE(c);
      }

      p->next = c = flood_count++;
      flood_mem = realloc(flood_mem, sizeof(FLOODED_LINE) * flood_count);
      p = FLOOD_LINE(c);
   }

   p->flags = FLOOD_IN_USE;
   p->lpos = left;
   p->rpos = right;
   p->y = y;
   p->next = 0;

   if (y > bmp->ct)
      p->flags |= FLOOD_TODO_ABOVE;

   if (y + 1 < bmp->cb)
      p->flags |= FLOOD_TODO_BELOW;

   return right + 2;
}


/* check_flood_line:
 *  Checks a line segment, using the scratch buffer is to store a list of
 *  segments which have already been drawn in order to minimise the required
 *  number of tests.
 */
static int check_flood_line(BITMAP *bmp, int y, int left, int right, int src_color, int dest_color)
{
   int c;
   FLOODED_LINE *p;
   int ret = FALSE;

   while (left <= right)
   {
      c = y;

      for (;;)
      {
         p = FLOOD_LINE(c);

         if ((left >= p->lpos) && (left <= p->rpos))
         {
            left = p->rpos + 2;
            break;
         }

         c = p->next;

         if (!c)
         {
            left = flooder(bmp, left, y, src_color, dest_color);
            ret = TRUE;
            break;
         }
      }
   }

   return ret;
}


/* floodfill:
 *  Fills an enclosed area (starting at point x, y) with the specified color.
 */
void floodfill(BITMAP *bmp, int x, int y, int color)
{
   int src_color;
   int c, done;
   FLOODED_LINE *p;

   /* make sure we have a valid starting point */
   if ((x < bmp->cl) || (x >= bmp->cr) || (y < bmp->ct) || (y >= bmp->cb))
      return;

   /* what color to replace? */
   src_color = getpixel(bmp, x, y);
   if (src_color == color)
      return;

   /* set up the list of flooded segments */
   flood_mem = malloc(sizeof(FLOODED_LINE) * bmp->cb);
   if (!flood_mem)
      return;

   flood_count = bmp->cb;
   p = (FLOODED_LINE *)flood_mem;

   for (c = 0; c < flood_count; c++)
   {
      p[c].flags = 0;
      p[c].lpos = SHRT_MAX;
      p[c].rpos = SHRT_MIN;
      p[c].y = y;
      p[c].next = 0;
   }

   /* start up the flood algorithm */
   flooder(bmp, x, y, src_color, color);

   /* continue as long as there are some segments still to test */
   do
   {
      done = TRUE;

      /* for each line on the screen */
      for (c = 0; c < flood_count; c++)
      {

         p = FLOOD_LINE(c);

         /* check below the segment? */
         if (p->flags & FLOOD_TODO_BELOW)
         {
            p->flags &= ~FLOOD_TODO_BELOW;
            if (check_flood_line(bmp, p->y + 1, p->lpos, p->rpos, src_color, color))
            {
               done = FALSE;
               p = FLOOD_LINE(c);
            }
         }

         /* check above the segment? */
         if (p->flags & FLOOD_TODO_ABOVE)
         {
            p->flags &= ~FLOOD_TODO_ABOVE;
            if (check_flood_line(bmp, p->y - 1, p->lpos, p->rpos, src_color, color))
            {
               done = FALSE;
               /* special case shortcut for going backwards */
               if ((c < bmp->cb) && (c > 0))
                  c -= 2;
            }
         }
      }

   } while (!done);

   /* free up the temp memory buffer */
   free(flood_mem);
}


/* calc_spline:
 *  Calculates a set of pixels for the bezier spline defined by the four
 *  points specified in the points array. The required resolution
 *  is specified by the npts parameter, which controls how many output
 *  pixels will be stored in the x and y arrays.
 */
static void calc_spline(const int points[8], int npts, int *out_x, int *out_y)
{
   /* Derivatives of x(t) and y(t). */
   double x, dx, ddx, dddx;
   double y, dy, ddy, dddy;
   int i;

   /* Temp variables used in the setup. */
   double dt, dt2, dt3;
   double xdt2_term, xdt3_term;
   double ydt2_term, ydt3_term;

   dt = 1.0 / (npts - 1);
   dt2 = (dt * dt);
   dt3 = (dt2 * dt);

   /* x coordinates. */
   xdt2_term = 3 * (points[4] - 2 * points[2] + points[0]);
   xdt3_term = points[6] + 3 * (-points[4] + points[2]) - points[0];

   xdt2_term = dt2 * xdt2_term;
   xdt3_term = dt3 * xdt3_term;

   dddx = 6 * xdt3_term;
   ddx = -6 * xdt3_term + 2 * xdt2_term;
   dx = xdt3_term - xdt2_term + 3 * dt * (points[2] - points[0]);
   x = points[0];

   out_x[0] = points[0];

   x += .5;
   for (i = 1; i < npts; i++)
   {
      ddx += dddx;
      dx += ddx;
      x += dx;

      out_x[i] = (int)x;
   }

   /* y coordinates. */
   ydt2_term = 3 * (points[5] - 2 * points[3] + points[1]);
   ydt3_term = points[7] + 3 * (-points[5] + points[3]) - points[1];

   ydt2_term = dt2 * ydt2_term;
   ydt3_term = dt3 * ydt3_term;

   dddy = 6 * ydt3_term;
   ddy = -6 * ydt3_term + 2 * ydt2_term;
   dy = ydt3_term - ydt2_term + dt * 3 * (points[3] - points[1]);
   y = points[1];

   out_y[0] = points[1];

   y += .5;

   for (i = 1; i < npts; i++)
   {
      ddy += dddy;
      dy += ddy;
      y += dy;

      out_y[i] = (int)y;
   }
}


#define MAX_POINTS 64
#define DIST(x, y) (sqrt((x) * (x) + (y) * (y)))

/* spline:
 *  Draws a bezier spline onto the specified bitmap in the specified color.
 */
void spline(BITMAP *bmp, const int points[8], int color)
{
   int xpts[MAX_POINTS], ypts[MAX_POINTS];
   int i;
   int num_points;
   int c;

   num_points = (int)(sqrt(DIST(points[2] - points[0], points[3] - points[1]) +
                           DIST(points[4] - points[2], points[5] - points[3]) +
                           DIST(points[6] - points[4], points[7] - points[5])) *
                      1.2);

   if (num_points > MAX_POINTS)
      num_points = MAX_POINTS;

   calc_spline(points, num_points, xpts, ypts);

   if ((_drawing_mode == DRAW_MODE_TRANS))
   {
      /* Must compensate for the end pixel being drawn twice,
         hence the mess. */
      for (i = 1; i < num_points - 1; i++)
      {
         c = getpixel(bmp, xpts[i], ypts[i]);
         line(bmp, xpts[i - 1], ypts[i - 1], xpts[i], ypts[i], color);
         drawing_mode(DRAW_MODE_SOLID);
         putpixel(bmp, xpts[i], ypts[i], c);
         drawing_mode(DRAW_MODE_TRANS);
      }
      line(bmp, xpts[i - 1], ypts[i - 1], xpts[i], ypts[i], color);
   }
   else
   {
      for (i = 1; i < num_points; i++)
         line(bmp, xpts[i - 1], ypts[i - 1], xpts[i], ypts[i], color);
   }
}
