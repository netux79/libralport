#ifndef ALPORT_3D_H
#define ALPORT_3D_H

#include "base.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define POLYTYPE_FLAT               0
#define POLYTYPE_GCOL               1
#define POLYTYPE_GRGB               2
#define POLYTYPE_ATEX               3
#define POLYTYPE_PTEX               4
#define POLYTYPE_ATEX_MASK          5
#define POLYTYPE_PTEX_MASK          6
#define POLYTYPE_ATEX_LIT           7
#define POLYTYPE_PTEX_LIT           8
#define POLYTYPE_ATEX_MASK_LIT      9
#define POLYTYPE_PTEX_MASK_LIT      10
#define POLYTYPE_ATEX_TRANS         11
#define POLYTYPE_PTEX_TRANS         12
#define POLYTYPE_ATEX_MASK_TRANS    13
#define POLYTYPE_PTEX_MASK_TRANS    14
#define POLYTYPE_MAX                15

   typedef struct V3D_f /* a 3d point (floating point version) */
   {
      float x, y, z; /* position */
      float u, v;    /* texture map coordinates */
      int c;         /* color */
   } V3D_f;

   /* information for polygon scanline fillers */
   typedef struct POLYGON_SEGMENT
   {
      fixed u, v, du, dv;        /* fixed point u/v coordinates */
      fixed c, dc;               /* single color gouraud shade values */
      fixed r, g, b, dr, dg, db; /* RGB gouraud shade values */
      float z, dz;               /* polygon depth (1/z) */
      float fu, fv, dfu, dfv;    /* floating point u/v coordinates */
      unsigned char *texture;    /* the texture map */
      int umask, vmask, vshift;  /* texture map size information */
      uintptr_t read_addr;       /* reading address for transparency modes */
   } POLYGON_SEGMENT;


void quad3d_f(BITMAP *bmp, int type, BITMAP *texture, V3D_f *v1, V3D_f *v2, V3D_f *v3, V3D_f *v4);
void triangle3d_f(BITMAP *bmp, int type, BITMAP *texture, V3D_f *v1, V3D_f *v2, V3D_f *v3);

#ifdef __cplusplus
}
#endif

#endif /* ifndef ALPORT_3D_H */
