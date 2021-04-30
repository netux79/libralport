#ifndef ALPORT_H
#define ALPORT_H

#include "packfile.h"
#include "lzss.h"
#include "datafile.h"
#include "bitmap.h"
#include "font.h"
#include "primitive.h"
#include "palette.h"
#include "sound.h"
#include "midi.h"

#ifndef TRUE
#define TRUE         -1
#define FALSE        0
#endif

#undef MIN
#undef MAX
#define MIN(x,y)     (((x) < (y)) ? (x) : (y))
#define MAX(x,y)     (((x) > (y)) ? (x) : (y))
#define CLAMP(x,y,z) MAX((x), MIN((y), (z)))

#define int32_t signed int

#define OTHER_PATH_SEPARATOR  '\\'
#define EMPTY_STRING          "\0"

#endif          /* ifndef ALPORT_H */

