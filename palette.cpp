#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "alport.h"


RGB_MAP *rgb_map = NULL;               /* RGB -> palette entry conversion */
COLOR_MAP *color_map = NULL;           /* translucency/lighting table */

/* default palette structures */
PALETTE black_palette =
{
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
   {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}
};

/* 1.5k lookup table for color matching */
static unsigned int col_diff[3 * 128];


/* bestfit_init:
 *  Color matching is done with weighted squares, which are much faster
 *  if we pregenerate a little lookup table...
 */
static void bestfit_init(void)
{
   int i;

   for (i = 1; i < 64; i++)
   {
      int k = i * i;
      col_diff[      i] = col_diff[      128 - i] = k * (59 * 59);
      col_diff[128 + i] = col_diff[128 + 128 - i] = k * (30 * 30);
      col_diff[256 + i] = col_diff[256 + 128 - i] = k * (11 * 11);
   }
}


/* bestfit_color:
 *  Searches a palette for the color closest to the requested R, G, B value.
 */
int bestfit_color(const PALETTE pal, int r, int g, int b)
{
   int i, coldiff, lowest, bestfit;

   if (col_diff[1] == 0)
      bestfit_init();

   bestfit = 0;
   lowest = INT_MAX;

   /* only the transparent (pink) color can be mapped to index 0 */
   if ((r == 63) && (g == 0) && (b == 63))
      i = 0;
   else
      i = 1;

   while (i < PAL_SIZE)
   {
      const RGB *rgb = &pal[i];
      coldiff = (col_diff + 0) [(rgb->g - g) & 0x7F ];
      if (coldiff < lowest)
      {
         coldiff += (col_diff + 128) [(rgb->r - r) & 0x7F ];
         if (coldiff < lowest)
         {
            coldiff += (col_diff + 256) [(rgb->b - b) & 0x7F ];
            if (coldiff < lowest)
            {
               bestfit = rgb - pal;    /* faster than `bestfit = i;' */
               if (coldiff == 0)
                  return bestfit;
               lowest = coldiff;
            }
         }
      }
      i++;
   }

   return bestfit;
}


/* create_rgb_table:
 *  Fills an RGB_MAP lookup table with conversion data for the specified
 *  palette. This is the faster version by Jan Hubicka.
 *
 *  Uses alg. similar to floodfill - it adds one seed per every color in
 *  palette to its best position. Then areas around seed are filled by
 *  same color because it is best approximation for them, and then areas
 *  about them etc...
 *
 *  It does just about 80000 tests for distances and this is about 100
 *  times better than normal 256*32000 tests so the calculation time
 *  is now less than one second at all computers I tested.
 */
void create_rgb_table(RGB_MAP *table, const PALETTE pal)
{
#define UNUSED 65535
#define LAST 65532

   /* macro add adds to single linked list */
#define add(i)                                           \
   (next[(i)] == UNUSED ?                                \
   (next[(i)] = LAST,                                    \
   (first != LAST ? (next[last] = (i)) : (first = (i))), \
   (last = (i))) : 0)

   /* same but w/o checking for first element */
#define add1(i)                                          \
   (next[(i)] == UNUSED ?                                \
   (next[(i)] = LAST, next[last] = (i), (last = (i))) :  \
   0)

   /* calculates distance between two colors */
#define dist(a1, a2, a3, b1, b2, b3)         \
   (col_diff[((a2) - (b2)) & 0x7F] +         \
   (col_diff + 128)[((a1) - (b1)) & 0x7F] +  \
   (col_diff + 256)[((a3) - (b3)) & 0x7F])

   /* converts r,g,b to position in array and back */
#define pos(r, g, b)                                  \
   (((r) / 2) * 32 * 32 + ((g) / 2) * 32 + ((b) / 2))

#define depos(pal, r, g, b)                  \
   ((b) = (( pal) &  31) *  2,               \
    (g) = (((pal) >>  5) & 31) * 2,          \
    (r) = (((pal) >> 10) & 31) * 2)

   /* is current color better than pal1? */
#define better(r1, g1, b1, pal1)                            \
   (((int)dist((r1), (g1), (b1),                            \
               (pal1).r, (pal1).g, (pal1).b)) > (int)dist2)

   /* checking of position */
#define dopos(rp, gp, bp, ts)                                              \
   if ((rp > -1 || r > 0) && (rp < 1 || r < 61) &&                         \
         (gp > -1 || g > 0) && (gp < 1 || g < 61) &&                       \
         (bp > -1 || b > 0) && (bp < 1 || b < 61))                         \
   {                                                                       \
      i = first + rp * 32 * 32 + gp * 32 + bp;                             \
      if (!data[i])                                                        \
      {                                                                    \
         data[i] = val;                                                    \
         add1(i);                                                          \
      }                                                                    \
      else if ((ts) && (data[i] != val))                                   \
      {                                                                    \
         dist2 = (rp ? (col_diff+128)[(r+2*rp-pal[val].r) & 0x7F] : r2) +  \
                 (gp ? (col_diff    )[(g+2*gp-pal[val].g) & 0x7F] : g2) +  \
                 (bp ? (col_diff+256)[(b+2*bp-pal[val].b) & 0x7F] : b2);   \
         if (better((r+2*rp), (g+2*gp), (b+2*bp), pal[data[i]]))           \
         {                                                                 \
            data[i] = val;                                                 \
            add1(i);                                                       \
         }                                                                 \
      }                                                                    \
   }

   int i, curr, r, g, b, val, dist2;
   unsigned int r2, g2, b2;
   unsigned short next[32 * 32 * 32];
   unsigned char *data;
   int first = LAST;
   int last = LAST;

#define AVERAGE_COUNT   18000

   if (col_diff[1] == 0)
      bestfit_init();

   memset(next, 255, sizeof(next));
   memset(table->data, 0, sizeof(char) * 32 * 32 * 32);

   data = (unsigned char *)table->data;

   /* add starting seeds for floodfill */
   for (i = 1; i < PAL_SIZE; i++)
   {
      curr = pos(pal[i].r, pal[i].g, pal[i].b);
      if (next[curr] == UNUSED)
      {
         data[curr] = i;
         add(curr);
      }
   }

   /* main floodfill: two versions of loop for faster growing in blue axis */
   while (first != LAST)
   {
      depos(first, r, g, b);

      /* calculate distance of current color */
      val = data[first];
      r2 = (col_diff + 128)[((pal[val].r) - (r)) & 0x7F];
      g2 = (col_diff)[((pal[val].g) - (g)) & 0x7F];
      b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7F];

      /* try to grow to all directions */
      dopos(0, 0, 1, 1);
      dopos(0, 0, -1, 1);
      dopos(1, 0, 0, 1);
      dopos(-1, 0, 0, 1);
      dopos(0, 1, 0, 1);
      dopos(0, -1, 0, 1);

      /* faster growing of blue direction */
      if ((b > 0) && (data[first - 1] == val))
      {
         b -= 2;
         first--;
         b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7F];

         dopos(-1, 0, 0, 0);
         dopos(1, 0, 0, 0);
         dopos(0, -1, 0, 0);
         dopos(0, 1, 0, 0);

         first++;
      }

      /* get next from list */
      i = first;
      first = next[first];
      next[i] = UNUSED;

      /* second version of loop */
      if (first != LAST)
      {
         depos(first, r, g, b);

         val = data[first];
         r2 = (col_diff + 128)[((pal[val].r) - (r)) & 0x7F];
         g2 = (col_diff)[((pal[val].g) - (g)) & 0x7F];
         b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7F];

         dopos(0, 0, 1, 1);
         dopos(0, 0, -1, 1);
         dopos(1, 0, 0, 1);
         dopos(-1, 0, 0, 1);
         dopos(0, 1, 0, 1);
         dopos(0, -1, 0, 1);

         if ((b < 61) && (data[first + 1] == val))
         {
            b += 2;
            first++;
            b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7f];

            dopos(-1, 0, 0, 0);
            dopos(1, 0, 0, 0);
            dopos(0, -1, 0, 0);
            dopos(0, 1, 0, 0);

            first--;
         }

         i = first;
         first = next[first];
         next[i] = UNUSED;
      }
   }

   /* only the transparent (pink) color can be mapped to index 0 */
   if ((pal[0].r == 63) && (pal[0].g == 0) && (pal[0].b == 63))
      table->data[31][0][31] = 0;
}


/* fade_interpolate:
 *  Calculates a palette part way between source and dest, returning it
 *  in output. The pos indicates how far between the two extremes it should
 *  be: 0 = return source, 64 = return dest, 32 = return exactly half way.
 *  Only affects colors between from and to (inclusive).
 */
void fade_interpolate(const PALETTE source, const PALETTE dest, PALETTE output,
                      int pos, int from, int to)
{
   int c;

   for (c = from; c <= to; c++)
   {
      output[c].r = ((int)source[c].r * (63 - pos) + (int)dest[c].r * pos) / 64;
      output[c].g = ((int)source[c].g * (63 - pos) + (int)dest[c].g * pos) / 64;
      output[c].b = ((int)source[c].b * (63 - pos) + (int)dest[c].b * pos) / 64;
   }
}


void *load_dat_palette(PACKFILE *f, long size)
{
   RGB *p;
   int c;

   /* Supporting 256 color palettes only
    * VGA 0-63 values format.
    */
   if ((size / 4) != PAL_SIZE)
      return NULL;

   p = (RGB *)malloc(sizeof(PALETTE));
   if (!p)
      return NULL;

   for (c = 0; c < PAL_SIZE; c++)
   {
      p[c].r = pack_getc(f);
      p[c].g = pack_getc(f);
      p[c].b = pack_getc(f);
      /* Read & discard filler byte */
      pack_getc(f);
      /*Set it to zero in the struct */
      p[c].filler = 0;
   }

   return p;
}


/* makecol:
 *  Converts R, G, and B values (ranging 0-255) to an 8 bit paletted color.
 *  If the global rgb_map table is initialised, it uses that, otherwise
 *  it returns 0.
 */
int makecol(int r, int g, int b)
{
   if (rgb_map)
      return rgb_map->data[r >> 3][g >> 3][b >> 3];

   return 0;
}
