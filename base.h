#ifndef ALPORT_BASE_H
#define ALPORT_BASE_H

#include <stddef.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE         -1
#define FALSE         0
#endif

#undef MIN
#undef MAX
#undef ABS
#define MIN(x,y)     (((x) < (y)) ? (x) : (y))
#define MAX(x,y)     (((x) > (y)) ? (x) : (y))
#define ABS(x)       (((x) >= 0) ? (x) : (-(x)))
#define CLAMP(x,y,z) MAX((x), MIN((y), (z)))

#ifdef _WIN32
#define OTHER_PATH_SEPARATOR  '\\'
#define DEVICE_SEPARATOR      ':'
#else
#define OTHER_PATH_SEPARATOR  '/'
#define DEVICE_SEPARATOR      '\0'
#endif

#define EMPTY_STRING          "\0"
#define MASKED_COLOR            0
#define PI 3.14159265358979323846

#define SWAP32(a)    ((a & 0xFF000000) >> 24) | ((a & 0x00FF0000) >> 8) | ((a & 0x0000FF00) << 8) | ((a & 0x000000FF) << 24)
#define SWAP16(a)    ((a & 0xFF00) >> 8) | ((a & 0x00FF) << 8)

typedef int32_t fixed;

#endif          /* ifndef ALPORT_BASE_H */
