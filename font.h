#ifndef ALPORT_FONT_H
#define ALPORT_FONT_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FONT_GLYPH           /* a single monochrome font character */
{
   unsigned char *data;
   int width;
   int height;
} FONT_GLYPH;


typedef struct FONT
{
   FONT_GLYPH *glyph;               /* glyphs array */
   int height;
   int first;                       /* first char and */
   int last;                        /* one-past-the-end char */
   int total_glyphs;
} FONT;



void *load_dat_font(PACKFILE *f, long size);
void destroy_font(FONT *f);
int char_length(const FONT *f, int ch);
int text_length(const FONT *f, const char *str);
int text_height(const FONT *f);
void textout_ex(BITMAP *bmp, const FONT *f, const char *str, int x, int y,
                int fg, int bg);
void textout_centre_ex(BITMAP *bmp, const FONT *f, const char *str, int x,
                       int y, int fg, int bg);
void textout_right_ex(BITMAP *bmp, const FONT *f, const char *str, int x, int y,
                      int fg, int bg);                       
void textprintf_ex(BITMAP *bmp, const FONT *f, int x, int y, int fg, int bg,
                   const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif          /* ifndef ALPORT_FONT_H */
