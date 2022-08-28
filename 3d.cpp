#include <stdlib.h>
#include <limits.h>
#include "alport.h"


/* bitfield specifying which polygon attributes need interpolating */
#define INTERP_FLAT           1      /* no interpolation */
#define INTERP_1COL           2      /* gcol or alpha */
#define INTERP_3COL           4      /* grgb */
#define INTERP_FIX_UV         8      /* atex */
#define INTERP_Z              16     /* always in scene3d */
#define INTERP_FLOAT_UV       32     /* ptex */
#define OPT_FLOAT_UV_TO_FIX   64     /* translate ptex to atex */


/* prototype for the scanline filler functions */
typedef void (*SCANLINE_FILLER)(uintptr_t addr, int w, POLYGON_SEGMENT *info);
SCANLINE_FILLER _optim_alternative_drawer;


static void _poly_scanline_dummy(uintptr_t, int, POLYGON_SEGMENT *) {}


/* _poly_scanline_gcol:
 *  Fills a single-color gouraud shaded polygon scanline.
 */
static void _poly_scanline_gcol(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    fixed c, dc;
    unsigned char *d;

    c = info->c;
    dc = info->dc;
    d = (unsigned char *)addr;

    for (x = w - 1; x >= 0; d++, x--)
    {
        *d = (c >> 16);
        c += dc;
    }
}


/* _poly_scanline_grgb:
 *  Fills an gouraud shaded polygon scanline.
 */
static void _poly_scanline_grgb(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    fixed r, g, b;
    fixed dr, dg, db;
    unsigned char *d;

    r = info->r;
    g = info->g;
    b = info->b;
    dr = info->dr;
    dg = info->dg;
    db = info->db;
    d = (unsigned char *)addr;

    for (x = w - 1; x >= 0; d++, x--)
    {
        *d = makecol((r >> 16), (g >> 16), (b >> 16));
        r += dr;
        g += dg;
        b += db;
    }
}


/* _poly_scanline_atex:
 *  Fills an affine texture mapped polygon scanline.
 */
static void _poly_scanline_atex(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    int vmask, vshift, umask;
    fixed u, v, du, dv;
    unsigned char *texture;
    unsigned char *d;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    u = info->u;
    v = info->v;
    du = info->du;
    dv = info->dv;
    texture = info->texture;
    d = (unsigned char *)addr;

    for (x = w - 1; x >= 0; d++, x--)
    {
        unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
        unsigned long color = *s;

        *d = color;
        u += du;
        v += dv;
    }
}


/* _poly_scanline_atex_mask:
 *  Fills a masked affine texture mapped polygon scanline.
 */
static void _poly_scanline_atex_mask(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    int vmask, vshift, umask;
    fixed u, v, du, dv;
    unsigned char *texture;
    unsigned char *d;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    u = info->u;
    v = info->v;
    du = info->du;
    dv = info->dv;
    texture = info->texture;
    d = (unsigned char *)addr;

    for (x = w - 1; x >= 0; d++, x--)
    {
        unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
        unsigned long color = *s;

        if (color != MASKED_COLOR)
        {
            *d = color;
        }
        u += du;
        v += dv;
    }
}


/* _poly_scanline_atex_lit:
 *  Fills a lit affine texture mapped polygon scanline.
 */
static void _poly_scanline_atex_lit(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    int vmask, vshift, umask;
    fixed u, v, c, du, dv, dc;
    unsigned char *texture;
    unsigned char *d;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    u = info->u;
    v = info->v;
    c = info->c;
    du = info->du;
    dv = info->dv;
    dc = info->dc;
    texture = info->texture;
    d = (unsigned char *)addr;

    for (x = w - 1; x >= 0; d++, x--)
    {
        unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
        unsigned long color = *s;
        color = color_map->data[c >> 16][color];

        *d = color;
        u += du;
        v += dv;
        c += dc;
    }
}


/* _poly_scanline_atex_mask_lit:
 *  Fills a masked lit affine texture mapped polygon scanline.
 */
static void _poly_scanline_atex_mask_lit(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    int vmask, vshift, umask;
    fixed u, v, c, du, dv, dc;
    unsigned char *texture;
    unsigned char *d;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    u = info->u;
    v = info->v;
    c = info->c;
    du = info->du;
    dv = info->dv;
    dc = info->dc;
    texture = info->texture;
    d = (unsigned char *)addr;

    for (x = w - 1; x >= 0; d++, x--)
    {
        unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
        unsigned long color = *s;

        if (color != MASKED_COLOR)
        {
            color = color_map->data[c >> 16][color];
            *d = color;
        }
        u += du;
        v += dv;
        c += dc;
    }
}


/* _poly_scanline_ptex:
 *  Fills a perspective correct texture mapped polygon scanline.
 */
static void _poly_scanline_ptex(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x, i, imax = 3;
    int vmask, vshift, umask;
    double fu, fv, fz, dfu, dfv, dfz, z1;
    unsigned char *texture;
    unsigned char *d;
    int64_t u, v;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    fu = info->fu;
    fv = info->fv;
    fz = info->z;
    dfu = info->dfu * 4;
    dfv = info->dfv * 4;
    dfz = info->dz * 4;
    z1 = 1. / fz;
    texture = info->texture;
    d = (unsigned char *)addr;
    u = fu * z1;
    v = fv * z1;

    /* update depth */
    fz += dfz;
    z1 = 1. / fz;

    for (x = w - 1; x >= 0; x -= 4)
    {
        int64_t nextu, nextv, du, dv;
        unsigned char *s;
        unsigned long color;

        fu += dfu;
        fv += dfv;
        fz += dfz;
        nextu = fu * z1;
        nextv = fv * z1;
        z1 = 1. / fz;
        du = (nextu - u) >> 2;
        dv = (nextv - v) >> 2;

        /* scanline subdivision */
        if (x < 3)
            imax = x;
        for (i = imax; i >= 0; i--, d++)
        {
            s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
            color = *s;

            *d = color;
            u += du;
            v += dv;
        }
    }
}


/* _poly_scanline_ptex_mask:
 *  Fills a masked perspective correct texture mapped polygon scanline.
 */
static void _poly_scanline_ptex_mask(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x, i, imax = 3;
    int vmask, vshift, umask;
    double fu, fv, fz, dfu, dfv, dfz, z1;
    unsigned char *texture;
    unsigned char *d;
    int64_t u, v;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    fu = info->fu;
    fv = info->fv;
    fz = info->z;
    dfu = info->dfu * 4;
    dfv = info->dfv * 4;
    dfz = info->dz * 4;
    z1 = 1. / fz;
    texture = info->texture;
    d = (unsigned char *)addr;
    u = fu * z1;
    v = fv * z1;

    /* update depth */
    fz += dfz;
    z1 = 1. / fz;

    for (x = w - 1; x >= 0; x -= 4)
    {
        int64_t nextu, nextv, du, dv;
        unsigned char *s;
        unsigned long color;

        fu += dfu;
        fv += dfv;
        fz += dfz;
        nextu = fu * z1;
        nextv = fv * z1;
        z1 = 1. / fz;
        du = (nextu - u) >> 2;
        dv = (nextv - v) >> 2;

        /* scanline subdivision */
        if (x < 3)
            imax = x;
        for (i = imax; i >= 0; i--, d++)
        {
            s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
            color = *s;

            if (color != MASKED_COLOR)
            {
                *d = color;
            }
            u += du;
            v += dv;
        }
    }
}


/* _poly_scanline_ptex_lit:
 *  Fills a lit perspective correct texture mapped polygon scanline.
 */
static void _poly_scanline_ptex_lit(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x, i, imax = 3;
    int vmask, vshift, umask;
    fixed c, dc;
    double fu, fv, fz, dfu, dfv, dfz, z1;
    unsigned char *texture;
    unsigned char *d;
    int64_t u, v;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    c = info->c;
    dc = info->dc;
    fu = info->fu;
    fv = info->fv;
    fz = info->z;
    dfu = info->dfu * 4;
    dfv = info->dfv * 4;
    dfz = info->dz * 4;
    z1 = 1. / fz;
    texture = info->texture;
    d = (unsigned char *)addr;
    u = fu * z1;
    v = fv * z1;

    /* update depth */
    fz += dfz;
    z1 = 1. / fz;

    for (x = w - 1; x >= 0; x -= 4)
    {
        int64_t nextu, nextv, du, dv;

        fu += dfu;
        fv += dfv;
        fz += dfz;
        nextu = fu * z1;
        nextv = fv * z1;
        z1 = 1. / fz;
        du = (nextu - u) >> 2;
        dv = (nextv - v) >> 2;

        /* scanline subdivision */
        if (x < 3)
            imax = x;
        for (i = imax; i >= 0; i--, d++)
        {
            unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
            unsigned long color = *s;
            color = color_map->data[c >> 16][color];

            *d = color;
            u += du;
            v += dv;
            c += dc;
        }
    }
}


/* _poly_scanline_ptex_mask_lit:
 *  Fills a masked lit perspective correct texture mapped polygon scanline.
 */
static void _poly_scanline_ptex_mask_lit(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x, i, imax = 3;
    int vmask, vshift, umask;
    fixed c, dc;
    double fu, fv, fz, dfu, dfv, dfz, z1;
    unsigned char *texture;
    unsigned char *d;
    int64_t u, v;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    c = info->c;
    dc = info->dc;
    fu = info->fu;
    fv = info->fv;
    fz = info->z;
    dfu = info->dfu * 4;
    dfv = info->dfv * 4;
    dfz = info->dz * 4;
    z1 = 1. / fz;
    texture = info->texture;
    d = (unsigned char *)addr;
    u = fu * z1;
    v = fv * z1;

    /* update depth */
    fz += dfz;
    z1 = 1. / fz;

    for (x = w - 1; x >= 0; x -= 4)
    {
        int64_t nextu, nextv, du, dv;

        fu += dfu;
        fv += dfv;
        fz += dfz;
        nextu = fu * z1;
        nextv = fv * z1;
        z1 = 1. / fz;
        du = (nextu - u) >> 2;
        dv = (nextv - v) >> 2;

        /* scanline subdivision */
        if (x < 3)
            imax = x;
        for (i = imax; i >= 0; i--, d++)
        {
            unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
            unsigned long color = *s;

            if (color != MASKED_COLOR)
            {
                color = color_map->data[c >> 16][color];
                *d = color;
            }
            u += du;
            v += dv;
            c += dc;
        }
    }
}


/* _poly_scanline_atex_trans:
 *  Fills a trans affine texture mapped polygon scanline.
 */
static void _poly_scanline_atex_trans(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    int vmask, vshift, umask;
    fixed u, v, du, dv;
    unsigned char *texture;
    unsigned char *d;
    unsigned char *r;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    u = info->u;
    v = info->v;
    du = info->du;
    dv = info->dv;
    texture = info->texture;
    d = (unsigned char *)addr;
    r = (unsigned char *)info->read_addr;

    for (x = w - 1; x >= 0; d++, r++, x--)
    {
        unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
        unsigned long color = *s;
        color = color_map->data[color][*r];

        *d = color;
        u += du;
        v += dv;
    }
}


/* _poly_scanline_atex_mask_trans:
 *  Fills a trans masked affine texture mapped polygon scanline.
 */
static void _poly_scanline_atex_mask_trans(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x;
    int vmask, vshift, umask;
    fixed u, v, du, dv;
    unsigned char *texture;
    unsigned char *d;
    unsigned char *r;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    u = info->u;
    v = info->v;
    du = info->du;
    dv = info->dv;
    texture = info->texture;
    d = (unsigned char *)addr;
    r = (unsigned char *)info->read_addr;

    for (x = w - 1; x >= 0; d++, r++, x--)
    {
        unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
        unsigned long color = *s;

        if (color != MASKED_COLOR)
        {
            color = color_map->data[color][*r];
            *d = color;
        }
        u += du;
        v += dv;
    }
}


/* _poly_scanline_ptex_trans:
 *  Fills a trans perspective correct texture mapped polygon scanline.
 */
static void _poly_scanline_ptex_trans(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x, i, imax = 3;
    int vmask, vshift, umask;
    double fu, fv, fz, dfu, dfv, dfz, z1;
    unsigned char *texture;
    unsigned char *d;
    unsigned char *r;
    int64_t u, v;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    fu = info->fu;
    fv = info->fv;
    fz = info->z;
    dfu = info->dfu * 4;
    dfv = info->dfv * 4;
    dfz = info->dz * 4;
    z1 = 1. / fz;
    texture = info->texture;
    d = (unsigned char *)addr;
    r = (unsigned char *)info->read_addr;
    u = fu * z1;
    v = fv * z1;

    /* update depth */
    fz += dfz;
    z1 = 1. / fz;

    for (x = w - 1; x >= 0; x -= 4)
    {
        int64_t nextu, nextv, du, dv;

        fu += dfu;
        fv += dfv;
        fz += dfz;
        nextu = fu * z1;
        nextv = fv * z1;
        z1 = 1. / fz;
        du = (nextu - u) >> 2;
        dv = (nextv - v) >> 2;

        /* scanline subdivision */
        if (x < 3)
            imax = x;
        for (i = imax; i >= 0; i--, d++, r++)
        {
            unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
            unsigned long color = *s;

            color = color_map->data[color][*r];
            *d = color;
            u += du;
            v += dv;
        }
    }
}


/* _poly_scanline_ptex_mask_trans:
 *  Fills a trans masked perspective correct texture mapped polygon scanline.
 */
static void _poly_scanline_ptex_mask_trans(uintptr_t addr, int w, POLYGON_SEGMENT *info)
{
    int x, i, imax = 3;
    int vmask, vshift, umask;
    double fu, fv, fz, dfu, dfv, dfz, z1;
    unsigned char *texture;
    unsigned char *d;
    unsigned char *r;
    int64_t u, v;

    vmask = info->vmask << info->vshift;
    vshift = 16 - info->vshift;
    umask = info->umask;
    fu = info->fu;
    fv = info->fv;
    fz = info->z;
    dfu = info->dfu * 4;
    dfv = info->dfv * 4;
    dfz = info->dz * 4;
    z1 = 1. / fz;
    texture = info->texture;
    d = (unsigned char *)addr;
    r = (unsigned char *)info->read_addr;
    u = fu * z1;
    v = fv * z1;

    /* update depth */
    fz += dfz;
    z1 = 1. / fz;

    for (x = w - 1; x >= 0; x -= 4)
    {
        int64_t nextu, nextv, du, dv;

        fu += dfu;
        fv += dfv;
        fz += dfz;
        nextu = fu * z1;
        nextv = fv * z1;
        z1 = 1. / fz;
        du = (nextu - u) >> 2;
        dv = (nextv - v) >> 2;

        /* scanline subdivision */
        if (x < 3)
            imax = x;
        for (i = imax; i >= 0; i--, d++, r++)
        {
            unsigned char *s = (texture + ((v >> vshift) & vmask) + ((u >> 16) & umask));
            unsigned long color = *s;

            if (color != MASKED_COLOR)
            {
                color = color_map->data[color][*r];
                *d = color;
            }
            u += du;
            v += dv;
        }
    }
}


/* _clip_polygon_segment:
 *  Fixed point version of _clip_polygon_segment_f().
 */
static void _clip_polygon_segment(POLYGON_SEGMENT *info, fixed gap, int flags)
{
   if (flags & INTERP_1COL)
      info->c += fixmul(info->dc, gap);

   if (flags & INTERP_3COL)
   {
      info->r += fixmul(info->dr, gap);
      info->g += fixmul(info->dg, gap);
      info->b += fixmul(info->db, gap);
   }

   if (flags & INTERP_FIX_UV)
   {
      info->u += fixmul(info->du, gap);
      info->v += fixmul(info->dv, gap);
   }

   if (flags & INTERP_Z)
   {
      float gap_f = fixtof(gap);

      info->z += info->dz * gap_f;

      if (flags & INTERP_FLOAT_UV)
      {
         info->fu += info->dfu * gap_f;
         info->fv += info->dfv * gap_f;
      }
   }
}


/* _clip_polygon_segment_f:
 *  Updates interpolation state values when skipping several places, eg.
 *  clipping the first part of a scanline.
 */
static void _clip_polygon_segment_f(POLYGON_SEGMENT *info, int gap, int flags)
{
   if (flags & INTERP_1COL)
      info->c += info->dc * gap;

   if (flags & INTERP_3COL)
   {
      info->r += info->dr * gap;
      info->g += info->dg * gap;
      info->b += info->db * gap;
   }

   if (flags & INTERP_FIX_UV)
   {
      info->u += info->du * gap;
      info->v += info->dv * gap;
   }

   if (flags & INTERP_Z)
   {
      info->z += info->dz * gap;

      if (flags & INTERP_FLOAT_UV)
      {
         info->fu += info->dfu * gap;
         info->fv += info->dfv * gap;
      }
   }
}


/* _fill_3d_edge_structure_f:
 *  Polygon helper function: initialises an edge structure for the 3d
 *  rasterising code, using floating point vertex structures. Returns 1 on
 *  success, or 0 if the edge is horizontal or clipped out of existence.
 */
static int _fill_3d_edge_structure_f(POLYGON_EDGE *edge, const V3D_f *v1, const V3D_f *v2, int flags, BITMAP *bmp)
{
   int r1, r2, g1, g2, b1, b2;
   fixed h, step;
   float h1;

   /* swap vertices if they are the wrong way up */
   if (v2->y < v1->y)
   {
      const V3D_f *vt;

      vt = v1;
      v1 = v2;
      v2 = vt;
   }

   /* set up screen rasterising parameters */
   edge->top = fixceil(ftofix(v1->y));
   edge->bottom = fixceil(ftofix(v2->y)) - 1;

   if (edge->bottom < edge->top)
      return 0;

   h1 = 1.0 / (v2->y - v1->y);
   h = ftofix(v2->y - v1->y);
   step = (edge->top << 16) - ftofix(v1->y);

   edge->dx = ftofix((v2->x - v1->x) * h1);
   edge->x = ftofix(v1->x) + fixmul(step, edge->dx);

   edge->prev = NULL;
   edge->next = NULL;
   edge->w = 0;

   if (flags & INTERP_Z)
   {
      float step_f = fixtof(step);

      /* Z (depth) interpolation */
      float z1 = 1. / v1->z;
      float z2 = 1. / v2->z;

      edge->dat.dz = (z2 - z1) * h1;
      edge->dat.z = z1 + edge->dat.dz * step_f;

      if (flags & INTERP_FLOAT_UV)
      {
         /* floating point (perspective correct) texture interpolation */
         float fu1 = v1->u * z1 * 65536.;
         float fv1 = v1->v * z1 * 65536.;
         float fu2 = v2->u * z2 * 65536.;
         float fv2 = v2->v * z2 * 65536.;

         edge->dat.dfu = (fu2 - fu1) * h1;
         edge->dat.dfv = (fv2 - fv1) * h1;
         edge->dat.fu = fu1 + edge->dat.dfu * step_f;
         edge->dat.fv = fv1 + edge->dat.dfv * step_f;
      }
   }

   if (flags & INTERP_FLAT)
   {
      /* if clipping is enabled then clip edge */
      if (bmp->clip)
      {
         if (edge->top < bmp->ct)
         {
            edge->x += (bmp->ct - edge->top) * edge->dx;
            edge->top = bmp->ct;
         }

         if (edge->bottom >= bmp->cb)
            edge->bottom = bmp->cb - 1;
      }

      return (edge->bottom >= edge->top);
   }

   if (flags & INTERP_1COL)
   {
      /* single color shading interpolation */
      edge->dat.dc = fixdiv(itofix(v2->c - v1->c), h);
      edge->dat.c = itofix(v1->c) + fixmul(step, edge->dat.dc);
   }

   if (flags & INTERP_3COL)
   {
      r1 = (v1->c >> 16) & 0xFF;
      r2 = (v2->c >> 16) & 0xFF;
      g1 = (v1->c >> 8) & 0xFF;
      g2 = (v2->c >> 8) & 0xFF;
      b1 = v1->c & 0xFF;
      b2 = v2->c & 0xFF;

      edge->dat.dr = fixdiv(itofix(r2 - r1), h);
      edge->dat.dg = fixdiv(itofix(g2 - g1), h);
      edge->dat.db = fixdiv(itofix(b2 - b1), h);
      edge->dat.r = itofix(r1) + fixmul(step, edge->dat.dr);
      edge->dat.g = itofix(g1) + fixmul(step, edge->dat.dg);
      edge->dat.b = itofix(b1) + fixmul(step, edge->dat.db);
   }

   if (flags & INTERP_FIX_UV)
   {
      /* fixed point (affine) texture interpolation */
      edge->dat.du = ftofix((v2->u - v1->u) * h1);
      edge->dat.dv = ftofix((v2->v - v1->v) * h1);
      edge->dat.u = ftofix(v1->u) + fixmul(step, edge->dat.du);
      edge->dat.v = ftofix(v1->v) + fixmul(step, edge->dat.dv);
   }

   /* if clipping is enabled then clip edge */
   if (bmp->clip)
   {
      if (edge->top < bmp->ct)
      {
         int gap = bmp->ct - edge->top;
         edge->top = bmp->ct;
         edge->x += gap * edge->dx;
         _clip_polygon_segment_f(&(edge->dat), gap, flags);
      }

      if (edge->bottom >= bmp->cb)
         edge->bottom = bmp->cb - 1;
   }

   return (edge->bottom >= edge->top);
}


/* _get_scanline_filler:
 *  Helper function for deciding which rasterisation function and 
 *  interpolation flags we should use for a specific polygon type.
 */
static SCANLINE_FILLER _get_scanline_filler(int type, int *flags, POLYGON_SEGMENT *info, BITMAP *texture)
{
   typedef struct POLYTYPE_INFO
   {
      SCANLINE_FILLER filler;
      SCANLINE_FILLER alternative;
   } POLYTYPE_INFO;

   static int interpinfo[] =
   {
      INTERP_FLAT,
      INTERP_1COL,
      INTERP_3COL,
      INTERP_FIX_UV,
      INTERP_Z | INTERP_FLOAT_UV | OPT_FLOAT_UV_TO_FIX,
      INTERP_FIX_UV,
      INTERP_Z | INTERP_FLOAT_UV | OPT_FLOAT_UV_TO_FIX,
      INTERP_FIX_UV | INTERP_1COL,
      INTERP_Z | INTERP_FLOAT_UV | INTERP_1COL | OPT_FLOAT_UV_TO_FIX,
      INTERP_FIX_UV | INTERP_1COL,
      INTERP_Z | INTERP_FLOAT_UV | INTERP_1COL | OPT_FLOAT_UV_TO_FIX,
      INTERP_FIX_UV,
      INTERP_Z | INTERP_FLOAT_UV | OPT_FLOAT_UV_TO_FIX,
      INTERP_FIX_UV,
      INTERP_Z | INTERP_FLOAT_UV | OPT_FLOAT_UV_TO_FIX
   };

   static POLYTYPE_INFO typeinfo[] =
   {
      {_poly_scanline_dummy, NULL},
      {_poly_scanline_gcol, NULL},
      {_poly_scanline_grgb, NULL},
      {_poly_scanline_atex, NULL},
      {_poly_scanline_ptex, _poly_scanline_atex},
      {_poly_scanline_atex_mask, NULL},
      {_poly_scanline_ptex_mask, _poly_scanline_atex_mask},
      {_poly_scanline_atex_lit, NULL},
      {_poly_scanline_ptex_lit, _poly_scanline_atex_lit},
      {_poly_scanline_atex_mask_lit, NULL},
      {_poly_scanline_ptex_mask_lit, _poly_scanline_atex_mask_lit},
      {_poly_scanline_atex_trans, NULL},
      {_poly_scanline_ptex_trans, _poly_scanline_atex_trans},
      {_poly_scanline_atex_mask_trans, NULL},
      {_poly_scanline_ptex_mask_trans, _poly_scanline_atex_mask_trans}
   };

   type = CLAMP(0, type, POLYTYPE_MAX - 1);
   *flags = interpinfo[type];

   if (texture)
   {
      info->texture = texture->line[0];
      info->umask = texture->w - 1;
      info->vmask = texture->h - 1;
      info->vshift = 0;
      while ((1 << info->vshift) < texture->w)
         info->vshift++;
   }
   else
   {
      info->texture = NULL;
      info->umask = info->vmask = info->vshift = 0;
   }

   _optim_alternative_drawer = typeinfo[type].alternative;

   return typeinfo[type].filler;
}


/* draw_polygon_segment:
 *  Polygon helper function to fill a scanline. Calculates deltas for
 *  whichever values need interpolating, clips the segment, and then calls
 *  the lowlevel scanline filler.
 */
static void draw_polygon_segment(BITMAP *bmp, int ytop, int ybottom, POLYGON_EDGE *e1, POLYGON_EDGE *e2, SCANLINE_FILLER drawer, int flags, int color, POLYGON_SEGMENT *info)
{
   int x, y, w, gap;
   fixed step, width;
   POLYGON_SEGMENT *s1, *s2;
   const SCANLINE_FILLER save_drawer = drawer;

   /* ensure that e1 is the left edge and e2 is the right edge */
   if ((e2->x < e1->x) || ((e1->x == e2->x) && (e2->dx < e1->dx)))
   {
      POLYGON_EDGE *et = e1;
      e1 = e2;
      e2 = et;
   }

   s1 = &(e1->dat);
   s2 = &(e2->dat);

   if (flags & INTERP_FLAT)
      info->c = color;

   /* for each scanline in the polygon... */
   for (y = ytop; y <= ybottom; y++)
   {
      x = fixceil(e1->x);
      w = fixceil(e2->x) - x;
      drawer = save_drawer;

      if (drawer == _poly_scanline_dummy)
      {
         if (w > 0)
            hline(bmp, x, y, x + w - 1, color);
      }
      else
      {
         step = (x << 16) - e1->x;
         width = e2->x - e1->x;
         /*
          *  Nasty trick :
          *  In order to avoid divisions by zero, width is set to -1. This way s1 and s2
          *  are still being updated but the scanline is not drawn since w == 0.
          */
         if (width == 0)
            width = -1 * 65536;  /* -1 << 16 */
         /*
          *  End of nasty trick.
          */
         if (flags & INTERP_1COL)
         {
            info->dc = fixdiv(s2->c - s1->c, width);
            info->c = s1->c + fixmul(step, info->dc);
            s1->c += s1->dc;
            s2->c += s2->dc;
         }

         if (flags & INTERP_3COL)
         {
            info->dr = fixdiv(s2->r - s1->r, width);
            info->dg = fixdiv(s2->g - s1->g, width);
            info->db = fixdiv(s2->b - s1->b, width);
            info->r = s1->r + fixmul(step, info->dr);
            info->g = s1->g + fixmul(step, info->dg);
            info->b = s1->b + fixmul(step, info->db);

            s1->r += s1->dr;
            s2->r += s2->dr;
            s1->g += s1->dg;
            s2->g += s2->dg;
            s1->b += s1->db;
            s2->b += s2->db;
         }

         if (flags & INTERP_FIX_UV)
         {
            info->du = fixdiv(s2->u - s1->u, width);
            info->dv = fixdiv(s2->v - s1->v, width);
            info->u = s1->u + fixmul(step, info->du);
            info->v = s1->v + fixmul(step, info->dv);

            s1->u += s1->du;
            s2->u += s2->du;
            s1->v += s1->dv;
            s2->v += s2->dv;
         }

         if (flags & INTERP_Z)
         {
            float step_f = fixtof(step);
            float w1 = 65536. / width;

            info->dz = (s2->z - s1->z) * w1;
            info->z = s1->z + info->dz * step_f;
            s1->z += s1->dz;
            s2->z += s2->dz;

            if (flags & INTERP_FLOAT_UV)
            {
               info->dfu = (s2->fu - s1->fu) * w1;
               info->dfv = (s2->fv - s1->fv) * w1;
               info->fu = s1->fu + info->dfu * step_f;
               info->fv = s1->fv + info->dfv * step_f;

               s1->fu += s1->dfu;
               s2->fu += s2->dfu;
               s1->fv += s1->dfv;
               s2->fv += s2->dfv;
            }
         }

         /* if clipping is enabled then clip the segment */
         if (bmp->clip)
         {
            if (x < bmp->cl)
            {
               gap = bmp->cl - x;
               x = bmp->cl;
               w -= gap;
               _clip_polygon_segment_f(info, gap, flags);
            }

            if (x + w > bmp->cr)
               w = bmp->cr - x;
         }

         if (w > 0)
         {
            if ((flags & OPT_FLOAT_UV_TO_FIX) && (info->dz == 0))
            {
               float z1 = 1. / info->z;
               info->u = info->fu * z1;
               info->v = info->fv * z1;
               info->du = info->dfu * z1;
               info->dv = info->dfv * z1;
               drawer = _optim_alternative_drawer;
            }

            info->read_addr = (uintptr_t)(bmp->line[y] + x);
            drawer((uintptr_t)(bmp->line[y] + x), w, info);
         }
      }

      e1->x += e1->dx;
      e2->x += e2->dx;
   }
}


/* _triangle_deltas_f:
 *  Floating point version of _triangle_deltas().
 */
static void _triangle_deltas_f(fixed w, POLYGON_SEGMENT *s1, POLYGON_SEGMENT *info, const V3D_f *v, int flags)
{
   if (flags & INTERP_1COL)
      info->dc = fixdiv(s1->c - itofix(v->c), w);

   if (flags & INTERP_3COL)
   {
      int r, g, b;

      r = (v->c >> 16) & 0xFF;
      g = (v->c >> 8) & 0xFF;
      b = v->c & 0xFF;

      info->dr = fixdiv(s1->r - itofix(r), w);
      info->dg = fixdiv(s1->g - itofix(g), w);
      info->db = fixdiv(s1->b - itofix(b), w);
   }

   if (flags & INTERP_FIX_UV)
   {
      info->du = fixdiv(s1->u - ftofix(v->u), w);
      info->dv = fixdiv(s1->v - ftofix(v->v), w);
   }

   if (flags & INTERP_Z)
   {
      float w1 = 65536. / w;

      /* Z (depth) interpolation */
      float z1 = 1. / v->z;

      info->dz = (s1->z - z1) * w1;

      if (flags & INTERP_FLOAT_UV)
      {
         /* floating point (perspective correct) texture interpolation */
         float fu1 = v->u * z1 * 65536.;
         float fv1 = v->v * z1 * 65536.;

         info->dfu = (s1->fu - fu1) * w1;
         info->dfv = (s1->fv - fv1) * w1;
      }
   }
}


/* draw_triangle_part:
 *  Triangle helper function to fill a triangle part. Computes interpolation,
 *  clips the segment, and then calls the lowlevel scanline filler.
 */
static void draw_triangle_part(BITMAP *bmp, int ytop, int ybottom, POLYGON_EDGE *left_edge, POLYGON_EDGE *right_edge, SCANLINE_FILLER drawer, int flags, int color, POLYGON_SEGMENT *info)
{
   int x, y, w;
   int gap;
   fixed step;
   POLYGON_SEGMENT *s1;

   /* ensure that left_edge and right_edge are the right way round */
   if ((right_edge->x < left_edge->x) ||
       ((left_edge->x == right_edge->x) && (right_edge->dx < left_edge->dx)))
   {
      POLYGON_EDGE *other_edge = left_edge;
      left_edge = right_edge;
      right_edge = other_edge;
   }

   s1 = &(left_edge->dat);

   if (flags & INTERP_FLAT)
      info->c = color;

   for (y = ytop; y <= ybottom; y++)
   {
      x = fixceil(left_edge->x);
      w = fixceil(right_edge->x) - x;
      step = (x << 16) - left_edge->x;

      if (drawer == _poly_scanline_dummy)
      {
         if (w > 0)
            hline(bmp, x, y, x + w - 1, color);
      }
      else
      {
         if (flags & INTERP_1COL)
         {
            info->c = s1->c + fixmul(step, info->dc);
            s1->c += s1->dc;
         }

         if (flags & INTERP_3COL)
         {
            info->r = s1->r + fixmul(step, info->dr);
            info->g = s1->g + fixmul(step, info->dg);
            info->b = s1->b + fixmul(step, info->db);

            s1->r += s1->dr;
            s1->g += s1->dg;
            s1->b += s1->db;
         }

         if (flags & INTERP_FIX_UV)
         {
            info->u = s1->u + fixmul(step, info->du);
            info->v = s1->v + fixmul(step, info->dv);

            s1->u += s1->du;
            s1->v += s1->dv;
         }

         if (flags & INTERP_Z)
         {
            float step_f = fixtof(step);

            info->z = s1->z + info->dz * step_f;
            s1->z += s1->dz;

            if (flags & INTERP_FLOAT_UV)
            {
               info->fu = s1->fu + info->dfu * step_f;
               info->fv = s1->fv + info->dfv * step_f;

               s1->fu += s1->dfu;
               s1->fv += s1->dfv;
            }
         }

         /* if clipping is enabled then clip the segment */
         if (bmp->clip)
         {
            if (x < bmp->cl)
            {
               gap = bmp->cl - x;
               x = bmp->cl;
               w -= gap;
               _clip_polygon_segment_f(info, gap, flags);
            }

            if (x + w > bmp->cr)
               w = bmp->cr - x;
         }

         if (w > 0)
         {
            if ((flags & OPT_FLOAT_UV_TO_FIX) && (info->dz == 0))
            {
               float z1 = 1. / info->z;
               info->u = info->fu * z1;
               info->v = info->fv * z1;
               info->du = info->dfu * z1;
               info->dv = info->dfv * z1;
               drawer = _optim_alternative_drawer;
            }

            info->read_addr = (uintptr_t)(bmp->line[y] + x);
            drawer((uintptr_t)(bmp->line[y] + x), w, info);
         }
      }

      left_edge->x += left_edge->dx;
      right_edge->x += right_edge->dx;
   }
}


/* do_polygon3d:
 *  Helper function for rendering 3d polygon, used by both the fixed point
 *  and floating point drawing functions.
 */
static void do_polygon3d(BITMAP *bmp, int top, int bottom, POLYGON_EDGE *left_edge, SCANLINE_FILLER drawer, int flags, int color, POLYGON_SEGMENT *info)
{
   int ytop, ybottom;
   POLYGON_EDGE *right_edge;

   if ((left_edge->prev != left_edge->next) && (left_edge->prev->top == top))
      left_edge = left_edge->prev;

   right_edge = left_edge->next;

   ytop = top;
   for (;;)
   {
      if (right_edge->bottom <= left_edge->bottom)
         ybottom = right_edge->bottom;
      else
         ybottom = left_edge->bottom;

      /* fill the scanline */
      draw_polygon_segment(bmp, ytop, ybottom, left_edge, right_edge, drawer, flags, color, info);

      if (ybottom >= bottom)
         break;

      /* update edges */
      if (ybottom >= left_edge->bottom)
         left_edge = left_edge->prev;
      if (ybottom >= right_edge->bottom)
         right_edge = right_edge->next;

      ytop = ybottom + 1;
   }
}


/* polygon3d_f:
 *  Floating point version of polygon3d().
 */
void polygon3d_f(BITMAP *bmp, int type, BITMAP *texture, int vc, V3D_f *vtx[])
{
   int c;
   int flags;
   int top = INT_MAX;
   int bottom = INT_MIN;
   V3D_f *v1, *v2;
   POLYGON_EDGE *edge, *edge0, *start_edge;
   POLYGON_EDGE *list_edges = NULL;
   POLYGON_SEGMENT info;
   SCANLINE_FILLER drawer;
   void *edge_mem;

   if (vc < 3)
      return;

   /* set up the drawing mode */
   drawer = _get_scanline_filler(type, &flags, &info, texture);
   if (!drawer)
      return;

   /* allocate some space for the active edge table */
   edge_mem = malloc(sizeof(POLYGON_EDGE) * vc);
   if (!edge_mem)
      return;

   start_edge = edge0 = edge = (POLYGON_EDGE *)edge_mem;

   /* fill the double-linked list of edges in clockwise order */
   v2 = vtx[vc - 1];

   for (c = 0; c < vc; c++)
   {
      v1 = v2;
      v2 = vtx[c];

      if (_fill_3d_edge_structure_f(edge, v1, v2, flags, bmp))
      {
         if (edge->top < top)
         {
            top = edge->top;
            start_edge = edge;
         }

         if (edge->bottom > bottom)
            bottom = edge->bottom;

         if (list_edges)
         {
            list_edges->next = edge;
            edge->prev = list_edges;
         }

         list_edges = edge;
         edge++;
      }
   }

   if (list_edges)
   {
      /* close the double-linked list */
      edge0->prev = --edge;
      edge->next = edge0;

      /* render the polygon */
      do_polygon3d(bmp, top, bottom, start_edge, drawer, flags, vtx[0]->c, &info);
   }

   free(edge_mem);
}


/* quad3d_f:
 *  Draws a 3d quad.
 */
void quad3d_f(BITMAP *bmp, int type, BITMAP *texture, V3D_f *v1, V3D_f *v2, V3D_f *v3, V3D_f *v4)
{
   V3D_f *vertex[4];

   vertex[0] = v1;
   vertex[1] = v2;
   vertex[2] = v3;
   vertex[3] = v4;
   polygon3d_f(bmp, type, texture, 4, vertex);
}


/* triangle3d_f:
 *  Draws a 3d triangle.
 */
void triangle3d_f(BITMAP *bmp, int type, BITMAP *texture, V3D_f *v1, V3D_f *v2, V3D_f *v3)
{
   int flags;

   int color = v1->c;
   V3D_f *vt1, *vt2, *vt3;
   POLYGON_EDGE edge1, edge2;
   POLYGON_SEGMENT info;
   SCANLINE_FILLER drawer;

   /* set up the drawing mode */
   drawer = _get_scanline_filler(type, &flags, &info, texture);
   if (!drawer)
      return;

   /* sort the vertices so that vt1->y <= vt2->y <= vt3->y */
   if (v1->y > v2->y)
   {
      vt1 = v2;
      vt2 = v1;
   }
   else
   {
      vt1 = v1;
      vt2 = v2;
   }

   if (vt1->y > v3->y)
   {
      vt3 = vt1;
      vt1 = v3;
   }
   else
      vt3 = v3;

   if (vt2->y > vt3->y)
   {
      V3D_f *vtemp = vt2;
      vt2 = vt3;
      vt3 = vtemp;
   }

   /* do 3D triangle*/
   if (_fill_3d_edge_structure_f(&edge1, vt1, vt3, flags, bmp))
   {
      /* calculate deltas */
      if (drawer != _poly_scanline_dummy)
      {
         fixed w, h;
         POLYGON_SEGMENT s1 = edge1.dat;

         h = ftofix(vt2->y) - (edge1.top << 16);
         _clip_polygon_segment(&s1, h, flags);

         w = edge1.x + fixmul(h, edge1.dx) - ftofix(vt2->x);
         if (w)
            _triangle_deltas_f(w, &s1, &info, vt2, flags);
      }

      /* draws part between y1 and y2 */
      if (_fill_3d_edge_structure_f(&edge2, vt1, vt2, flags, bmp))
         draw_triangle_part(bmp, edge2.top, edge2.bottom, &edge1, &edge2, drawer, flags, color, &info);

      /* draws part between y2 and y3 */
      if (_fill_3d_edge_structure_f(&edge2, vt2, vt3, flags, bmp))
         draw_triangle_part(bmp, edge2.top, edge2.bottom, &edge1, &edge2, drawer, flags, color, &info);
   }
}
