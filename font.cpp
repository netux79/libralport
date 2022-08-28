#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "alport.h"


/*  CHAR_404:
 *  What to render missing glyphs as.
 */
#define CHAR_404  '^'


/*  read_font:
 *  Only allegro mono fonts are compatible on this port.
 */
static FONT *read_font(PACKFILE *pack)
{
   FONT *f = NULL;
   int i = 0;
   FONT_GLYPH *gl = NULL;
   unsigned int temp = 0;

   /* fist word needs to be zero */
   temp = pack_mgetw(pack);
   if (temp != 0)
      return NULL;

   /* only 1 range fonts are accepted */
   temp = pack_mgetw(pack);
   if (temp != 1)
      return NULL;

   /* Only mono depth fonts are valid */
   temp = pack_getc(pack);
   if (temp != 1 && temp != 255)
      return NULL;

   f = (FONT *)malloc(sizeof(FONT));
   if (!f)
      return NULL;

   f->first = pack_mgetl(pack);
   f->last = pack_mgetl(pack) + 1;
   f->total_glyphs = f->last - f->first;

   gl = (FONT_GLYPH *)malloc(sizeof(FONT_GLYPH) * f->total_glyphs);
   if (!gl)
   {
      free(f);
      return NULL;
   }

   for (i = 0; i < f->total_glyphs; i++)
   {
      gl[i].width = pack_mgetw(pack);
      gl[i].height = pack_mgetw(pack);
      /* calculate glyph size */
      temp = ((gl[i].width + 7) / 8) * gl[i].height;
      
      gl[i].data = (unsigned char *)malloc(temp);
      if (!gl[i].data)
      {
         while (i >= 0)
         {
            if (gl[i].data)
               free(gl[i].data);
            i--;
         }
         free(gl);
         free(f);
         return NULL;
      }

      /* Read actual glyph data */
      pack_fread(gl[i].data, temp, pack);

      /* Update font height */
      if (gl[i].height > f->height)
         f->height = gl[i].height;
   }

   f->glyph = gl;

   return f;
}


/* destroy_font:
 *  Freeds up all the memory used by the font.
 */
void destroy_font(FONT *f)
{
   int i;
   for (i = 0; i < f->total_glyphs; i++)
      free(f->glyph[i].data);
   free(f->glyph);
   free(f);
}


/* load_dat_font:
 *  Returns a FONT loaded from a DATAFILE.
 */
void *load_dat_font(PACKFILE *f, long size)
{
   (void)size;

   return read_font(f);
}


static FONT_GLYPH *_find_glyph(const FONT *f, int ch)
{
   if (ch >= f->first && ch < f->last)
      return &f->glyph[ch - f->first];

   /* if we don't find the character use the remplacement. */
   if (ch != CHAR_404)
      return _find_glyph(f, CHAR_404);

   return 0;
}


/* char_length:
 *  Returns the length, in pixels, of a character as rendered.
 */
static int char_length(const FONT *f, int ch)
{
   FONT_GLYPH *g = _find_glyph(f, ch);
   return g ? g->width : 0;
}


/* text_length:
 *  Calculates the length of a string in a particular font.
 */
int text_length(const FONT *f, const char *str)
{
   int ch = 0, w = 0;
   const char *p = str;

   while ((ch = *p++))
      w += char_length(f, ch);

   return w;
}


/* text_height:
 *  Returns the height of a character in the specified font.
 */
int text_height(const FONT *f)
{
   return f->height;
}


/* _render_char:
 *  Renders a character onto a bitmap at a given location and in given colors.
 *  Returns the character width, in pixels.
 */
static int _render_char(const FONT *f, int ch, int fg, int bg, BITMAP *bmp,
                        int x, int y)
{
   int cw = 0;
   FONT_GLYPH *g = 0;

   g = _find_glyph(f, ch);
   if (g)
   {
      const unsigned char *data = g->data;
      unsigned char *addr;
      int w = g->width;
      int h = g->height;
      int stride = (w + 7) / 8;
      int lgap = 0;
      int d, i, j;

      /* char width to return */
      cw = g->width;

      if (bmp->clip)
      {
         /* clip the top */
         if (y < bmp->ct)
         {
            d = bmp->ct - y;

            h -= d;
            if (h <= 0)
               return cw;

            data += d * stride;
            y = bmp->ct;
         }

         /* clip the bottom */
         if (y + h >= bmp->cb)
         {
            h = bmp->cb - y;
            if (h <= 0)
               return cw;
         }

         /* clip the left */
         if (x < bmp->cl)
         {
            d = bmp->cl - x;

            w -= d;
            if (w <= 0)
               return cw;

            data += d / 8;
            lgap = d & 7;
            x = bmp->cl;
         }

         /* clip the right */
         if (x + w >= bmp->cr)
         {
            w = bmp->cr - x;
            if (w <= 0)
               return cw;
         }
      }

      stride -= (lgap + w + 7) / 8;

      /* draw it */
      while (h--)
      {
         addr = bmp->line[y++] + x;

         j = 0;
         i = 0x80 >> lgap;
         d = *(data++);

         if (bg >= 0)
         {
            /* opaque mode drawing loop */
            for (;;)
            {
               if (d & i)
                  *addr = fg;
               else
                  *addr = bg;

               j++;
               if (j == w)
                  break;

               i >>= 1;
               if (!i)
               {
                  i = 0x80;
                  d = *(data++);
               }

               addr++;
            }
         }
         else
         {
            /* masked mode drawing loop */
            for (;;)
            {
               if (d & i)
                  *addr = fg;

               j++;
               if (j == w)
                  break;

               i >>= 1;
               if (!i)
               {
                  i = 0x80;
                  d = *(data++);
               }

               addr++;
            }
         }

         data += stride;
      }
   }

   return cw;
}


/* textout_ex:
 *  Writes the null terminated string str onto bmp at position x, y, using
 *  the specified font, foreground color and background color (-1 is trans).
 */
void textout_ex(BITMAP *bmp, const FONT *f, const char *str, int x, int y,
                int fg, int bg)
{
   int ch = 0;
   const unsigned char *p = (const unsigned char *)str;

   while ((ch = *p++))
      x += _render_char(f, ch, fg, bg, bmp, x, y);
}


/* textout_centre_ex:
 *  Like textout_ex(), but uses the x coordinate as the centre rather than
 *  the left of the string.
 */
void textout_centre_ex(BITMAP *bmp, const FONT *f, const char *str, int x,
                       int y, int fg, int bg)
{
   int len;

   len = text_length(f, str);
   textout_ex(bmp, f, str, x - len / 2, y, fg, bg);
}


/* textout_right_ex:
 *  Like textout_ex(), but uses the x coordinate as the right rather than 
 *  the left of the string.
 */
void textout_right_ex(BITMAP *bmp, const FONT *f, const char *str, int x, int y,
                      int fg, int bg)
{
   int len;

   len = text_length(f, str);
   textout_ex(bmp, f, str, x - len, y, fg, bg);
}


/* textprintf_ex:
 *  Formatted text output, using a printf() style format string.
 */
void textprintf_ex(BITMAP *bmp, const FONT *f, int x, int y, int fg, int bg,
                   const char *format, ...)
{
   char buf[512];
   va_list ap;

   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);

   textout_ex(bmp, f, buf, x, y, fg, bg);
}
