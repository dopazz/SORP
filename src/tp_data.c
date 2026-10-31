/* SORP - Super Off Road Port
 * Copyright (C) 2026 dopaz
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.  It is distributed WITHOUT ANY WARRANTY; see
 * the GNU General Public License in LICENSE for details.
 *
 * Developed with the assistance of Claude Opus 5 and Claude Opus 5.5.
 */

/* tp_data.c -- the Track-Pak's tables, turned into GAME.EXE's.
 *
 * Every address below is in the offroadt master program, read the way
 * its Z80 reads it: 0x0000-0x1FFF and 0xA000-0xDFFF fixed, 0x2000-0x9FFF
 * a bank chosen by OUT (F0h) (MAME's offroad_bankswitch).  The base game
 * keeps the same tables a little lower; the table below lists both.
 */
#include "tp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* The master CPU's view                                               */
/* ------------------------------------------------------------------ */

/* Where each set keeps its tables.  The Track-Pak's are traced in its
 * own program; the base game's are the same tables a little lower, found
 * by the same code, except its projection origin, which is
 * not a table at all but constants in the code: ld bc,0F720h at 0x5640
 * for X, and at 0x5668 0x1E40 for track 0, 0x1A20 for track 3 and 0x1E20
 * for the rest. */
typedef struct {
    /* per track, indexed by track & 7 */
    unsigned bank;           /* byte: the bank holding the contour grid  */
    unsigned base;           /* word: the grid's CPU address             */
    unsigned scrx, scry;     /* word: background scroll X and Y, pixels  */
    unsigned gfx;            /* byte: graphics bank, AY port A (0x0BDE)  */
    unsigned ox, oy;         /* word: projection origin, or 0: constants */
    short    ox_k[8], oy_k[8];
    unsigned palettes;       /* word: the palette list                   */
    /* per layout, indexed 0..15 */
    unsigned routes;         /* the route programs                       */
    unsigned gates;          /* the checkpoint lists                     */
    unsigned start_x, start_y, start_h;   /* per-truck starts, 3 - truck */
    unsigned spots;          /* the cash/nitro spots, 3 bytes each       */
    unsigned order;          /* the race order, 0xFF-ended; 0: none      */
    unsigned flag_x, flag_y; /* the flagman, per layout (words)          */
    unsigned poles;          /* the pennant poles, per layout            */
    unsigned banners;        /* the pennant banners, per layout          */
    unsigned board_x, board_y;            /* the scoreboard, per track   */
    unsigned name_x, name_y;              /* the track's name: X word, Y byte */
    unsigned name_a, name_b;              /*   ...and its two sprites     */
} tp_addrs;

static const tp_addrs tp_addr_sets[TP_KINDS] = {
    /* offroadt: setup 0x5140..0x5211, palette 0x51C8, routes 0x43E4,
     * starts 0x537C..0x53BE, spots 0x3E48, order 0x0325 */
    { 0x56C8, 0x5514, 0x566C, 0x568C, 0x56AC, 0x56F0, 0x5700,
      { 0 }, { 0 }, 0x039C,
      0x7DB0, 0xA8F4, 0x554C, 0x55EC, 0x5524, 0x3FDE, 0x5710,
      0x42FE, 0x431E, 0xAFF2, 0xAEE4, 0x4E88, 0x4E98,
      0x4EA8, 0x4EB8, 0x4ED0, 0x4EE0 },
    /* offroad: setup 0x4F8E..0x5018, palette 0x4FDF, routes 0x428A,
     * starts 0x5186, spots 0x3D70; its races follow GAME.EXE's order */
    { 0x54C2, 0x531E, 0x548E, 0x549E, 0x54AE, 0, 0,
      { -0x8E0, -0x8E0, -0x8E0, -0x8E0, -0x8E0, -0x8E0, -0x8E0, -0x8E0 },
      { 0x1E40, 0x1E20, 0x1E20, 0x1A20, 0x1E20, 0x1E20, 0x1E20, 0x1E20 }, 0x0399,
      0x7B02, 0xA8F4, 0x5356, 0x53F6, 0x532E, 0x3F06, 0,
      0x41A4, 0x41C4, 0xB30D, 0xB1DF, 0x4D24, 0x4D34,
      0x4D44, 0x4D54, 0x4D6C, 0x4D7C },
};
#define TP_A        (&tp_addr_sets[tp->kind])
#define TP_T_BANK   (TP_A->bank)
#define TP_T_BASE   (TP_A->base)
#define TP_T_SCRX   (TP_A->scrx)
#define TP_T_SCRY   (TP_A->scry)
#define TP_T_GFX    (TP_A->gfx)
#define TP_PALETTES (TP_A->palettes)
#define TP_ROUTES   (TP_A->routes)
#define TP_GATES    (TP_A->gates)
#define TP_START_X  (TP_A->start_x)
#define TP_START_Y  (TP_A->start_y)
#define TP_START_H  (TP_A->start_h)
#define TP_SPOTS    (TP_A->spots)
#define TP_ORDER    (TP_A->order)
#define TP_ROWTAB   0xA400   /* word per row: row * 44, in both          */

static const unsigned long tp_bank_base[8] = {
    0x02000, 0x02000, 0x10000, 0x18000, 0x20000, 0x28000, 0x30000, 0x38000
};

static unsigned long tp_cpu_to_file(int bank, unsigned addr)
{
    unsigned long base = tp_bank_base[bank & 7];
    if (base == 0x2000 || addr < 0x2000 || addr >= 0xA000) return addr;
    return (base & ~0x7FFFUL) + (addr & 0x7FFF);
}

static unsigned tp_rd8(const tp_set *tp, int bank, unsigned addr)
{
    return tp->region[TP_MASTER][tp_cpu_to_file(bank, addr & 0xffff) & 0x3ffff];
}

static unsigned tp_rd16(const tp_set *tp, int bank, unsigned addr)
{
    return tp_rd8(tp, bank, addr) | (tp_rd8(tp, bank, addr + 1) << 8);
}

static int tp_s16(unsigned v) { return (int)(short)(unsigned short)v; }

/* the arcade projection's origin for a track (OX, OY of sub_5855) */
static void tp_origin_raw(const tp_set *tp, int track, double *OX, double *OY)
{
    if (TP_A->ox) {
        *OX = tp_s16(tp_rd16(tp, 0, TP_A->ox + 2u * (unsigned)track));
        *OY = tp_s16(tp_rd16(tp, 0, TP_A->oy + 2u * (unsigned)track));
    } else {
        *OX = TP_A->ox_k[track & 7];
        *OY = TP_A->oy_k[track & 7];
    }
}

/* half up: the conversions were checked rounding this way */
static int tp_rnd(double v) { return (int)floor(v + 0.5); }

/* ------------------------------------------------------------------ */
/* The contour                                                         */
/* ------------------------------------------------------------------ */

#define TP_W 44
#define TP_H 44
#define TP_K 51.2              /* arcade Z per DOS height unit           */
#define TP_S (256.0 / TP_K)    /* gradient per unit of nx/nz: 5          */
#define TP_WALL_NZ 0x26        /* below this a plane is a wall (0xB207)  */

typedef struct { int nx, ny, nzf, d; } tp_plane;

typedef struct {
    int surf, dir, gx, gy;
} tp_tri;

typedef struct {
    tp_tri t[4];
    int    split;              /* written through QUAD                    */
} tp_cell;

typedef struct {
    int bank;
    unsigned base;
    unsigned row[TP_H];
} tp_grid;

static void tp_grid_at(const tp_set *tp, int track, tp_grid *g)
{
    int r;
    g->bank = (int)tp_rd8(tp, 0, TP_T_BANK + (unsigned)track);
    g->base = tp_rd16(tp, 0, TP_T_BASE + 2u * (unsigned)track);
    for (r = 0; r < TP_H; ++r) g->row[r] = tp_rd16(tp, 0, TP_ROWTAB + 2u * (unsigned)r);
}

static tp_plane tp_plane_at(const tp_set *tp, int bank, unsigned addr)
{
    tp_plane p;
    p.nx  = (signed char)tp_rd8(tp, bank, addr);
    p.ny  = (signed char)tp_rd8(tp, bank, addr + 1);
    p.nzf = (int)tp_rd8(tp, bank, addr + 2);
    p.d   = (signed char)tp_rd8(tp, bank, addr + 3);
    return p;
}

/* the cell's word at arcade (col, row) */
static unsigned tp_word(const tp_set *tp, const tp_grid *g, int c, int r)
{
    return tp_rd16(tp, g->bank, g->base + 2u * (g->row[r] + (unsigned)c));
}

/* the four triangles' planes of an arcade cell, in slot order */
static void tp_corners(const tp_set *tp, const tp_grid *g, int c, int r, tp_plane out[4])
{
    unsigned w = tp_word(tp, g, c, r);
    int i;
    if (w & 1) {
        for (i = 0; i < 4; ++i)
            out[i] = tp_plane_at(tp, g->bank, tp_rd16(tp, g->bank, (w & ~1u) + 2u * (unsigned)i));
    } else {
        out[0] = tp_plane_at(tp, g->bank, w);
        out[1] = out[2] = out[3] = out[0];
    }
}

/* the plane under an arcade point, by the lookup at 0xB207 */
static tp_plane tp_plane_under(const tp_set *tp, const tp_grid *g, long X, long Y)
{
    unsigned w;
    if (X < 0) X = 0;
    if (Y < 0) Y = 0;
    if (X > 0x2bff) X = 0x2bff;
    if (Y > 0x2bff) Y = 0x2bff;
    w = tp_word(tp, g, (int)(X >> 8), (int)(Y >> 8));
    if (w & 1) {
        int fx = (int)(X & 255), fy = (int)(Y & 255);
        unsigned p = w & ~1u;
        if (fx < fy) p |= 4;
        if (fx + fy >= 256) p |= 2;
        w = tp_rd16(tp, g->bank, p);
    }
    return tp_plane_at(tp, g->bank, w);
}

static double tp_plane_z(const tp_plane *p, double fx, double fy)
{
    int nz = p->nzf & 0x7f;
    if (!nz) nz = 1;
    return 32.0 * p->d - (p->nx * fx + p->ny * fy) / nz;
}

static int tp_surf(const tp_plane *p)
{
    if ((p->nzf & 0x7f) < TP_WALL_NZ) return 0x80;
    return (p->nzf & 0x80) ? 2 : 0;
}

static int tp_dir(const tp_plane *p)
{
    if (p->nx == 0 && p->ny == 0) return 128;
    return (tp_rnd(atan2((double)-p->ny, (double)p->nx) * 64.0 / M_PI) * 2) & 0xff;
}

static int tp_s8(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }

static void tp_tri_from(tp_tri *t, const tp_plane *p)
{
    int nz = p->nzf & 0x7f;
    t->surf = tp_surf(p);
    t->dir  = tp_dir(p);
    if (nz == 0) {                 /* a sheer wall (the Track-Pak's) */
        t->gx = t->gy = 0;
    } else {
        t->gx = tp_rnd(-TP_S * p->nx / nz);
        t->gy = tp_rnd(-TP_S * p->ny / nz);
    }
}

/* Set a gradient to `want`: within one of its own slope on the ground,
 * freely on a wall, whose slope no wheel reads (0x1a68). */
static void tp_steer(tp_tri *t, int *g, double want)
{
    int v = tp_rnd(want);
    if (t->surf == 0x80) {
        *g = tp_s8(v < -127 ? -127 : v > 127 ? 127 : v);
    } else {
        int lo = *g - 1, hi = *g + 1;
        *g = tp_s8(v < lo ? lo : v > hi ? hi : v);
    }
}

/* the triangle in a QUAD slot: a direct cell's four are one */
static tp_tri *tp_slot(tp_cell *c, int k) { return &c->t[c->split ? k : 0]; }

int tp_contour_load(const tp_set *tp, int track, og_contour *out, og_surface *s,
                    double *datum)
{
    tp_grid g;
    tp_cell *cell;
    double target[TP_W * TP_H], ground[TP_W * TP_H], base, h, hcol = 0.0, last;
    unsigned char has[TP_W * TP_H], ghas[TP_W * TP_H];
    unsigned char *poly = NULL, *quad = NULL;
    size_t np = 0, nq = 0;
    int i, r, c, k, rc = OG_OK;

    if (!tp || track < 0 || track >= TP_TRACKS || !out || !s) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    memset(s, 0, sizeof *s);
    cell = (tp_cell *)calloc(TP_W * TP_H, sizeof *cell);
    poly = (unsigned char *)malloc(TP_W * TP_H * 4 * 4);
    quad = (unsigned char *)malloc(TP_W * TP_H * 8);
    if (!cell || !poly || !quad) { rc = OG_E_NOMEM; goto done; }
    tp_grid_at(tp, track, &g);

    /* DOS row r is arcade row 43 - r */
    for (r = 0; r < TP_H; ++r)
        for (c = 0; c < TP_W; ++c) {
            tp_plane p[4];
            int ar = TP_H - 1 - r, wall = 1, ng = 0;
            double z = 0.0;
            unsigned w = tp_word(tp, &g, c, ar);
            tp_cell *e = &cell[r * TP_W + c];
            tp_corners(tp, &g, c, ar, p);
            for (k = 0; k < 4; ++k) {
                if (tp_surf(&p[k]) != 0x80) { wall = 0; z += tp_plane_z(&p[k], 128, 128); ++ng; }
            }
            /* a wall is always written split: its four gradients are then
             * free to steer apart */
            e->split = (w & 1) || wall;
            for (k = 0; k < 4; ++k) tp_tri_from(&e->t[k], &p[k]);
            i = r * TP_W + c;
            /* a cell's height is its ground's; a wall leaning like the base
             * game's keeps its own, as DOS's do; a sheer one has none */
            if (ng) { target[i] = z / ng / TP_K; has[i] = 1; }
            else if ((p[0].nzf & 0x7f) >= 8) { target[i] = tp_plane_z(&p[0], 128, 128) / TP_K; has[i] = 1; }
            else has[i] = 0;
        }

    /* a sheer wall aims for the highest ground beside it, failing that
     * for whatever came before it */
    memcpy(ground, target, sizeof ground);
    memcpy(ghas, has, sizeof ghas);
    for (i = 0; i < TP_W * TP_H; ++i) {
        int rr, cc, found = 0;
        double best = 0.0;
        if (ghas[i]) continue;
        r = i / TP_W; c = i % TP_W;
        for (rr = r - 1; rr <= r + 1; ++rr)
            for (cc = c - 1; cc <= c + 1; ++cc)
                if (rr >= 0 && rr < TP_H && cc >= 0 && cc < TP_W && ghas[rr * TP_W + cc]) {
                    double v = ground[rr * TP_W + cc];
                    if (!found || v > best) best = v;
                    found = 1;
                }
        if (found) { target[i] = best; has[i] = 1; }
    }
    last = 0.0;
    for (i = 0; i < TP_W * TP_H; ++i) {
        if (!has[i]) target[i] = last;
        last = target[i];
    }

    /* Steer the gradients the integrator reads (og_contour_heights:
     * slot +2 of a cell and +4 of the next along a row, +0 and +6 down
     * column 0) so it lands on the arcade's height at every cell. */
    base = 128.0 - target[0];
    h = base + target[0];
    for (r = 0; r < TP_H; ++r) {
        if (r) {
            tp_cell *prev = &cell[(r - 1) * TP_W], *cur = &cell[r * TP_W];
            tp_tri *south = tp_slot(prev, 0), *north = tp_slot(cur, 3);
            double want = 2.0 * (hcol - (base + target[r * TP_W]));
            if (north->surf != 0x80 && south->surf == 0x80)
                tp_steer(south, &south->gy, want - north->gy);
            else
                tp_steer(north, &north->gy, want - south->gy);
            hcol -= (south->gy + north->gy) / 2.0;
            h = hcol;
        } else {
            hcol = h;
        }
        for (c = 0; c + 1 < TP_W; ++c) {
            tp_tri *east = tp_slot(&cell[r * TP_W + c], 1);
            tp_tri *west = tp_slot(&cell[r * TP_W + c + 1], 2);
            double goal = 2.0 * (base + target[r * TP_W + c + 1] - h);
            if (west->surf != 0x80 && east->surf == 0x80)
                tp_steer(east, &east->gx, goal - west->gx);    /* leaving a wall */
            else
                tp_steer(west, &west->gx, goal - east->gx);
            h += (east->gx + west->gx) / 2.0;
        }
    }

    /* The records, shared where identical, as POLY's are. */
    for (i = 0; i < TP_W * TP_H; ++i) {
        unsigned offs[4];
        int n = cell[i].split ? 4 : 1;
        for (k = 0; k < n; ++k) {
            const tp_tri *t = &cell[i].t[k];
            unsigned char rec[4];
            size_t j;
            rec[0] = (unsigned char)t->surf;  rec[1] = (unsigned char)t->dir;
            rec[2] = (unsigned char)(t->gx & 0xff); rec[3] = (unsigned char)(t->gy & 0xff);
            for (j = 0; j < np; j += 4)
                if (!memcmp(poly + j, rec, 4)) break;
            if (j == np) { memcpy(poly + np, rec, 4); np += 4; }
            offs[k] = (unsigned)j;
        }
        if (cell[i].split) {
            unsigned char q[8];
            size_t j;
            for (k = 0; k < 4; ++k) { q[2 * k] = (unsigned char)(offs[k] & 0xff); q[2 * k + 1] = (unsigned char)(offs[k] >> 8); }
            for (j = 0; j < nq; j += 8)
                if (!memcmp(quad + j, q, 8)) break;
            if (j == nq) { memcpy(quad + nq, q, 8); nq += 8; }
            out->cell[i] = (unsigned short)(j | 1);
        } else {
            out->cell[i] = (unsigned short)offs[0];
        }
    }
    if (np > 0x3ffc || nq > 0x3ff8) { rc = OG_E_RANGE; goto done; }   /* 14-bit offsets */

    out->loaded = 1;
    s->poly = poly; s->poly_len = np;
    s->quad = quad; s->quad_len = nq ? nq : 0;
    s->loaded = 1;
    poly = quad = NULL;
    if (datum) *datum = target[0];
done:
    free(cell);
    free(poly);
    free(quad);
    return rc;
}

void tp_slopes(og_slopes *out)
{
    int i;
    if (!out) return;
    for (i = 0; i < 8; ++i) { out->bias[i] = 127; out->limit[i] = 255; }
    out->loaded = 1;
}

/* ------------------------------------------------------------------ */
/* The art                                                             */
/* ------------------------------------------------------------------ */

/* MAME's palette_device::BGR_233: R bits 0-2, G 3-5, B 6-7 */
static void tp_rgb(unsigned c, unsigned char *o)
{
    unsigned r = c & 7, g = (c >> 3) & 7, b = c >> 6;
    o[0] = (unsigned char)((r << 5) | (r << 2) | (r >> 1));
    o[1] = (unsigned char)((g << 5) | (g << 2) | (g >> 1));
    o[2] = (unsigned char)(b * 0x55);
}

/* A palette list as the vblank IRQ uploads it (0x0e0e, 0x1008): a word
 * per foreground bank, 0xFFnn fills it, else points at 64 colours; then
 * at +32 the priority patch, which puts chosen background pens over the
 * sprites in banks first..end-1 (0xFFFF: none, as the screens' lists
 * have) */
static void tp_list_rgb(const tp_set *tp, unsigned list, unsigned char rgb[1024 * 3],
                        unsigned char patched[64])
{
    unsigned char cols[1024];
    unsigned f, p, first, end;
    for (f = 0; f < 16; ++f) {
        unsigned w = tp_rd16(tp, 0, list + 2 * f), k;
        for (k = 0; k < 64; ++k)
            cols[f * 64 + k] = (unsigned char)((w >> 8) == 0xff ? (w & 0xff) : tp_rd8(tp, 0, w + k));
    }
    if (patched) memset(patched, 0, 64);
    p = tp_rd16(tp, 0, list + 32);
    if (p != 0xFFFF) {
        first = tp_rd8(tp, 0, p); end = tp_rd8(tp, 0, p + 1);
        for (p += 2; tp_rd8(tp, 0, p) != 0xff && p < 0x10000; p += 2) {
            unsigned pen = tp_rd8(tp, 0, p) & 63, colour = tp_rd8(tp, 0, p + 1);
            for (f = first; f < end && f < 16; ++f) cols[f * 64 + pen] = (unsigned char)colour;
            if (patched) patched[pen] = 1;
        }
    }
    for (f = 0; f < 1024; ++f) tp_rgb(cols[f], rgb + f * 3);
}

int tp_art_pens(const tp_set *tp, int track, unsigned char pens[320 * 240],
                unsigned char rgb[1024 * 3], unsigned char patched[64])
{
    const unsigned char *gfx = tp->region[TP_BG_GFX], *prom = tp->region[TP_BG_PROM];
    const unsigned third = 0x8000, ntiles = 0x8000 / 8;
    unsigned sx, sy, v, gb, char_bank, prom_bank;
    int x, y;

    if (!tp || track < 0 || track >= TP_TRACKS) return OG_E_ARG;
    sx = tp_rd16(tp, 0, TP_T_SCRX + 2u * (unsigned)track);
    sy = tp_rd16(tp, 0, TP_T_SCRY + 2u * (unsigned)track);
    v  = tp_rd8(tp, 0, TP_T_GFX + (unsigned)track);
    gb = ((((v * 2) | (v & ~2u)) & 0xff) * 8) & 0xff;          /* 0x0BDE */
    char_bank = ((gb >> 4) & 3) << 10;
    prom_bank = ((gb >> 3) & 1) << 13;

    /* leland_get_tile_info, over the 40x30 tiles the scroll shows */
    for (y = 0; y < 240; ++y)
        for (x = 0; x < 320; ++x) {
            unsigned col = sx / 8 + (unsigned)x / 8, row = sy / 8 + (unsigned)y / 8;
            unsigned ti = (col & 0xff) | ((row & 0x1f) << 8) | ((row & 0xe0) << 9);
            unsigned pv = prom[(prom_bank | ti) & 0x1ffff];
            unsigned tile = (pv | ((ti >> 7) & 0x300) | char_bank) % ntiles;
            unsigned bit = 7 - ((unsigned)x & 7), o = tile * 8 + ((unsigned)y & 7);
            unsigned px = (((gfx[o] >> bit) & 1) << 2) | (((gfx[third + o] >> bit) & 1) << 1) |
                          ((gfx[2 * third + o] >> bit) & 1);
            pens[y * 320 + x] = (unsigned char)((pv >> 5) * 8 + px);
        }

    /* the track's palette list, as the race's setup picks it */
    tp_list_rgb(tp, tp_rd16(tp, 0, TP_PALETTES + 2u * (unsigned)(track & 7)), rgb, patched);
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* The video CPU's sprites                                             */
/* ------------------------------------------------------------------ */

/* Traced in MAME.  The master names a sprite by an address
 * HHLB: its 16-byte descriptor is at HHL0 in video-CPU bank B (0x11CE
 * puts the low nibble in the bank latch at 0xC000), bank B being slave
 * ROM 0x10000 + 0x8000 * B with window address A at + (A & 0x7FFF).  The
 * descriptor (read at 0x11FB..0x13DD):
 *
 *   +0        the kind, & 0x0E (0x05AB): 0 not drawn; 2 a fill (0x152E,
 *             0x0E9A), every byte of the block the pointer's low byte
 *             -- 0xFF a solid block in pen 15; else the pixels
 *   +1        X offset of the anchor, signed: the left edge is X - it
 *   +2,+3 +4  the pixels for an even left edge: pointer, bytes per row
 *   +5,+6 +7  the same a pixel over, for an odd one
 *   +8        Y offset, signed: the top is Y - it
 *   +9        rows
 *
 * The copier (0x06C0..0x0759) sends each row to port 0x5B, two pixels a
 * byte, the high nibble first, 0 clear; a pixel f is palette bank f. */
static unsigned tp_vrd(const tp_set *tp, unsigned bank, unsigned a)
{
    unsigned long o = 0x10000UL + 0x8000UL * bank + (a & 0x7FFF);
    return o < 0x80000UL ? tp->region[TP_SLAVE][o] : 0;
}

/* A byte of a sprite's block: a fill's own byte, or the pixels' */
static unsigned tp_vbyte(const tp_set *tp, unsigned bank, unsigned kind,
                         unsigned ptr, unsigned i)
{
    return kind == 2 ? ptr & 0xFF : tp_vrd(tp, bank, ptr + i);
}

/* Paint a sprite into a 320x240 picture at (X, Y), in a 1024-colour
 * palette's banks. */
static void tp_paint_sprite(const tp_set *tp, unsigned addr, int X, int Y,
                            const unsigned char *pal, unsigned char *rgb)
{
    const unsigned bank = addr & 15, d = addr & 0xFFF0;
    int left = X - (signed char)tp_vrd(tp, bank, d + 1);
    int top  = Y - (signed char)tp_vrd(tp, bank, d + 8);
    const int rows = (int)tp_vrd(tp, bank, d + 9), odd = left & 1;
    const unsigned ptr = tp_vrd(tp, bank, d + (odd ? 5 : 2)) | (tp_vrd(tp, bank, d + (odd ? 6 : 3)) << 8);
    const int width = (int)tp_vrd(tp, bank, d + (odd ? 7 : 4));
    const unsigned kind = tp_vrd(tp, bank, d) & 0x0E;
    int r, b, k;
    if (!kind) return;
    left -= odd;                     /* the odd block starts a pixel early */
    for (r = 0; r < rows; ++r)
        for (b = 0; b < width; ++b) {
            unsigned v = tp_vbyte(tp, bank, kind, ptr, (unsigned)(r * width + b));
            for (k = 0; k < 2; ++k) {
                unsigned f = k ? v & 15 : v >> 4;
                int x = left + 2 * b + k, y = top + r;
                if (!f || x < 0 || x >= 320 || y < 0 || y >= TP_ROWS_IN) continue;
                memcpy(rgb + (y * 320 + x) * 3, pal + f * 64 * 3, 3);
            }
        }
}

/* THE OVERLAYS.  Some scenery stands in front of some of the course: a
 * ramp over the road beneath it (Cuttoff Pass), pillars (Boulder Hill),
 * the near rails (Rio Trio).  The palette's mask cannot do it -- a
 * pen is over every truck or none -- so every frame the master sends
 * the video CPU a list of sprites for the layout (offroadt 0x415b, the
 * table at 0x418F; offroad 0x4053, 0x4087; by [0xe045]), each with a
 * depth, and the video CPU draws all its sprites in depth order: an
 * overlay covers the trucks of lower depth, further up the course, and
 * the ones in front pass over it.  They are solid shapes in pen 15,
 * whose palette bank leaves the background showing; some are fills
 * (Cuttoff Pass's ramp is two triangles either side of a 40x32 block,
 * Rio Trio's are all fills).  A record:
 *
 *   +0,+1  X (+1 0x80 ends the list)   +2,+3  Y
 *   +4     depth                       +5     command 0x0A
 *   +6,+7  the sprite, HHLB
 *
 * Painted into `dep` (320x240): the greatest depth over each pixel, and
 * into `pen` the sprite pen drawn there -- the hardware shows palette
 * entry pen * 64 + the background's pen, so an overlay is seen in the
 * art too: Cuttoff Pass's start arrow, Sidewinder's barrels.  The lists
 * for a layout and its reverse are the same. */
static void tp_paint_overlays(const tp_set *tp, int track, unsigned char *dep,
                              unsigned char *pen)
{
    unsigned p = tp_rd16(tp, 0, (tp->kind == TP_ARCADE ? 0x4087u : 0x418Fu) + 2u * (unsigned)track);
    int n;
    for (n = 0; n < 32 && tp_rd8(tp, 0, p + 1) != 0x80; ++n, p += 8) {
        const int X = (int)tp_rd16(tp, 0, p), Y = (int)tp_rd16(tp, 0, p + 2);
        const unsigned depth = tp_rd8(tp, 0, p + 4), addr = tp_rd16(tp, 0, p + 6);
        const unsigned bank = addr & 15, d = addr & 0xFFF0;
        int left = X - (signed char)tp_vrd(tp, bank, d + 1);
        const int top = Y - (signed char)tp_vrd(tp, bank, d + 8);
        const int rows = (int)tp_vrd(tp, bank, d + 9), odd = left & 1;
        const unsigned ptr = tp_vrd(tp, bank, d + (odd ? 5 : 2)) | (tp_vrd(tp, bank, d + (odd ? 6 : 3)) << 8);
        const int width = (int)tp_vrd(tp, bank, d + (odd ? 7 : 4));
        const unsigned kind = tp_vrd(tp, bank, d) & 0x0E;
        int r, b, k;
        if (!kind) continue;
        left -= odd;
        for (r = 0; r < rows; ++r)
            for (b = 0; b < width; ++b) {
                unsigned v = tp_vbyte(tp, bank, kind, ptr, (unsigned)(r * width + b));
                for (k = 0; k < 2; ++k) {
                    const int x = left + 2 * b + k, y = top + r;
                    const unsigned f = k ? v & 15 : v >> 4;
                    if (!f) continue;
                    if (x < 0 || x >= 320 || y < 0 || y >= TP_ROWS_IN) continue;
                    if (depth >= dep[y * 320 + x]) {    /* drawn in depth order */
                        dep[y * 320 + x] = (unsigned char)depth;
                        pen[y * 320 + x] = (unsigned char)f;
                    }
                }
            }
    }
}

int tp_art_load(const tp_set *tp, int track, tp_art *out)
{
    unsigned char *pens = (unsigned char *)malloc(320 * 240);
    unsigned char *lit = (unsigned char *)malloc(320 * 240 * 3);
    unsigned char *dark = (unsigned char *)malloc(320 * 240 * 3);
    unsigned char *over = (unsigned char *)malloc(320 * 240);
    unsigned char *dep = (unsigned char *)calloc(320 * 240, 1);
    unsigned char *ovp = (unsigned char *)calloc(320 * 240, 1);
    unsigned char rgb[1024 * 3], patched[64];
    int x, j, i, rc = OG_OK;

    if (!out || !pens || !lit || !dark || !over || !dep || !ovp) { rc = out ? OG_E_NOMEM : OG_E_ARG; goto done; }
    tp_paint_overlays(tp, track, dep, ovp);
    rc = tp_art_pens(tp, track, pens, rgb, patched);
    if (rc != OG_OK) goto done;
    for (i = 0; i < 320 * 240; ++i) {
        /* an overlay's pixel as the arcade shows it (a truck's shadow
         * in front of it still darkens the background's own pen) */
        memcpy(lit + i * 3, rgb + (ovp[i] * 64 + pens[i]) * 3, 3);
        memcpy(dark + i * 3, rgb + (15 * 64 + pens[i]) * 3, 3);
        over[i] = patched[pens[i]];
    }

    /* the track's name, as the race's start draws it (0x4C3A, stages 3
     * and 4: two sprites at one place; the Track-Pak's two are one).
     * The trucks are drawn after it, so it stays under them. */
    {
        const int X = (int)tp_rd16(tp, 0, TP_A->name_x + 2u * (unsigned)track);
        const int Y = (int)tp_rd8(tp, 0, TP_A->name_y + (unsigned)track);
        const unsigned na = tp_rd16(tp, 0, TP_A->name_a + 2u * (unsigned)track);
        const unsigned nb = tp_rd16(tp, 0, TP_A->name_b + 2u * (unsigned)track);
        unsigned char *name = (unsigned char *)malloc(320 * 240 * 3);
        if (name) {
            memcpy(name, lit, 320 * 240 * 3);            /* before */
            tp_paint_sprite(tp, na, X, Y, rgb, lit);
            if (nb != na) tp_paint_sprite(tp, nb, X, Y, rgb, lit);
            for (i = 0; i < 320 * 240; ++i)
                if (memcmp(name + i * 3, lit + i * 3, 3)) {  /* painted */
                    memcpy(dark + i * 3, lit + i * 3, 3);
                    over[i] = 0;
                }
            free(name);
        }
    }

    /* 240 lines to 200 by area: each output row is 1.2 input rows */
    for (j = 0; j < TP_ART_H; ++j) {
        double a = j * 1.2, b = a + 1.2;
        int i0 = (int)floor(a), i1 = (int)ceil(b);
        for (x = 0; x < TP_ART_W; ++x) {
            double acc[6] = { 0 }, fg = 0.0, ov = 0.0;
            int k, deepest = 0;
            for (i = i0; i < i1 && i < TP_ROWS_IN; ++i) {
                double w = (i + 1 < b ? i + 1 : b) - (i > a ? i : a);
                const int p = i * 320 + x;
                for (k = 0; k < 3; ++k) {
                    acc[k]     += w * lit[p * 3 + k];
                    acc[3 + k] += w * dark[p * 3 + k];
                }
                if (over[p]) fg += w;
                if (dep[p]) {
                    ov += w;
                    if (dep[p] > deepest) deepest = dep[p];
                }
            }
            for (k = 0; k < 3; ++k) {
                out->rgb   [(j * TP_ART_W + x) * 3 + k] = (unsigned char)tp_rnd(acc[k] / 1.2);
                out->shadow[(j * TP_ART_W + x) * 3 + k] = (unsigned char)tp_rnd(acc[3 + k] / 1.2);
            }
            out->fg[j * TP_ART_W + x] = (unsigned char)(fg / 1.2 >= 0.5);
            out->over[j * TP_ART_W + x] = (unsigned char)(ov / 1.2 >= 0.5 ? deepest : 0);
        }
    }
done:
    free(pens); free(lit); free(dark); free(over); free(dep); free(ovp);
    return rc;
}

/* ------------------------------------------------------------------ */
/* The projection                                                      */
/* ------------------------------------------------------------------ */

/* The arcade's (sub_5855): with Ys = Y * (1/2 + 1/8 + 1/32 + 1/128),
 *     sx = (X + Ys/2 + OX) / 32,  sy = (OY - Ys - 0.75 Z) / 32
 * in 240 lines.  og_project's, for DOS y = 0x2C00 - Y and a height H
 * of Z/51.2 over the datum:
 *     sx = x/32 + (0x2C00 - y)/96 + ox,  sy = 0x471*y/65536 + 128 - H + oy
 * Squashed by 5/6, the two agree to a third of a pixel when
 *     ox = OX/32,  oy = 5/6 (OY - 0.6640625 * 0x2C00)/32 - datum
 * less the small difference in the y scale at mid-grid and the squash's
 * own twelfth of a line. */
int tp_track_origin(const tp_set *tp, int track, int *ox, int *oy)
{
    og_contour c;
    og_surface s;
    double datum = 0.0, OX, OY, y;
    int rc;
    if (!tp || track < 0 || track >= TP_TRACKS) return OG_E_ARG;
    rc = tp_contour_load(tp, track, &c, &s, &datum);
    if (rc != OG_OK) return rc;
    og_surface_free(&s);
    tp_origin_raw(tp, track, &OX, &OY);
    y = 5.0 / 6.0 * (OY - 0.6640625 * 0x2C00) / 32.0 - datum
        - (0x471 / 65536.0 - 5.0 / 6.0 * 0.6640625 / 32.0) * 0x1600 - 1.0 / 12.0;
    if (ox) *ox = tp_rnd(OX / 32.0);
    if (oy) *oy = tp_rnd(y);
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* Routes                                                              */
/* ------------------------------------------------------------------ */

/* arcade op -> its length, the DOS op, its length, and where each keeps
 * its pointer (0: none).  The arcade's pointers are absolute, GAME.EXE's
 * relative to the displacement word. */
typedef struct { unsigned char op, len, dop, dlen, ptr, dptr; } tp_rop;
static const tp_rop tp_rops[] = {
    { 0x40, 5, 0x80, 5, 3, 3 },    /* checkpoint count in range -> jump  */
    { 0x41, 5, 0x81, 4, 2, 2 },    /* random < n -> jump; 5th byte spare  */
    { 0x42, 5, 0x82, 5, 3, 3 },    /* [ix+0x11] in range -> jump          */
    { 0x80, 3, 0x83, 3, 0, 0 },    /* checkpoint range, scan only         */
    { 0x81, 3, 0x84, 3, 1, 1 },    /* jump                                */
    { 0x82, 3, 0x85, 1, 0, 0 },    /* re-weigh the rubber band            */
    { 0x83, 3, 0x86, 2, 0, 0 },    /* corner radius                       */
    { 0x84, 3, 0x87, 2, 0, 0 },    /* corner width                        */
    { 0x85, 3, 0x88, 3, 0, 0 },
    { 0x86, 3, 0x89, 3, 1, 1 },    /* [ix+13] set -> jump                 */
};

static const tp_rop *tp_rop_for(unsigned op)
{
    size_t i;
    for (i = 0; i < sizeof tp_rops / sizeof tp_rops[0]; ++i)
        if (tp_rops[i].op == op) return &tp_rops[i];
    return NULL;
}

#define TP_ROUTE_TOKENS 512

int tp_route_bytes(const tp_set *tp, int layout, unsigned char *out, int cap)
{
    unsigned short tok[TP_ROUTE_TOKENS], todo[TP_ROUTE_TOKENS], where[TP_ROUTE_TOKENS];
    unsigned char *seen;
    int nt = 0, ntodo = 0, i, j, n = 0;

    if (!tp || layout < 0 || layout >= TP_LAYOUTS || !out) return OG_E_ARG;
    seen = (unsigned char *)calloc(0x10000, 1);
    if (!seen) return OG_E_NOMEM;

    /* every token of the route's graph, following each pointer */
    todo[ntodo++] = (unsigned short)tp_rd16(tp, 0, TP_ROUTES + 2u * (unsigned)layout);
    while (ntodo > 0) {
        unsigned a = todo[--ntodo];
        while (!seen[a]) {
            unsigned b = tp_rd8(tp, 0, a);
            const tp_rop *o;
            seen[a] = 1;
            if (nt >= TP_ROUTE_TOKENS) { free(seen); return OG_E_RANGE; }
            tok[nt++] = (unsigned short)a;
            if ((b & 0xc0) == 0) { a = (a + 2) & 0xffff; continue; }     /* waypoint */
            o = tp_rop_for(b);
            if (!o) { free(seen); return OG_E_MAGIC; }
            if (o->ptr && ntodo < TP_ROUTE_TOKENS)
                todo[ntodo++] = (unsigned short)tp_rd16(tp, 0, a + o->ptr);
            if (b == 0x81) break;                     /* nothing falls through */
            a = (a + o->len) & 0xffff;
        }
    }
    free(seen);

    /* in address order, as the program lies */
    for (i = 1; i < nt; ++i) {
        unsigned short v = tok[i];
        for (j = i; j > 0 && tok[j - 1] > v; --j) tok[j] = tok[j - 1];
        tok[j] = v;
    }
    for (i = 0; i < nt; ++i) {
        unsigned b = tp_rd8(tp, 0, tok[i]);
        where[i] = (unsigned short)n;
        n += (b & 0xc0) == 0 ? 2 : tp_rop_for(b)->dlen;
    }
    if (n + 1 > cap) return OG_E_RANGE;

    n = 0;
    for (i = 0; i < nt; ++i) {
        unsigned a = tok[i], b = tp_rd8(tp, 0, a);
        const tp_rop *o;
        if ((b & 0xc0) == 0) {                         /* waypoint, Y flipped */
            out[n++] = (unsigned char)b;
            out[n++] = (unsigned char)((44 - (int)tp_rd8(tp, 0, a + 1)) & 0xff);
            continue;
        }
        o = tp_rop_for(b);
        out[n] = o->dop;
        for (j = 1; j < o->dlen; ++j) out[n + j] = (unsigned char)tp_rd8(tp, 0, a + (unsigned)j);
        if (o->ptr) {
            unsigned tgt = tp_rd16(tp, 0, a + o->ptr);
            int disp;
            for (j = 0; j < nt && tok[j] != tgt; ++j) {}
            if (j == nt) return OG_E_RANGE;            /* into a token's middle */
            disp = (int)where[j] - (n + o->dptr);
            out[n + o->dptr]     = (unsigned char)(disp & 0xff);
            out[n + o->dptr + 1] = (unsigned char)((disp >> 8) & 0xff);
        }
        n += o->dlen;
    }
    out[n++] = 0x8a;                                   /* the end */
    return n;
}

int tp_route_load(const tp_set *tp, int layout, og_route *out)
{
    unsigned char prog[OG_ROUTE_STREAM];
    int n = tp_route_bytes(tp, layout, prog, (int)sizeof prog);
    if (n < 0) return n;
    return og_route_parse(prog, (long)n, out);
}

/* ------------------------------------------------------------------ */
/* Checkpoints, starts, pickups, the race order                        */
/* ------------------------------------------------------------------ */

int tp_checkpoints_load(const tp_set *tp, int layout, og_checkpoints *out)
{
    unsigned p;
    if (!tp || layout < 0 || layout >= TP_LAYOUTS || !out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    p = tp_rd16(tp, 0, TP_GATES + 2u * (unsigned)layout);
    while (out->n < OG_CKPT_MAX) {
        og_checkpoint *c = &out->cp[out->n++];
        c->x0 = (unsigned char)tp_rd8(tp, 0, p);
        c->x1 = (unsigned char)tp_rd8(tp, 0, p + 1);
        c->y1 = (unsigned char)(44 - (int)tp_rd8(tp, 0, p + 2));     /* Y flipped */
        c->y0 = (unsigned char)(44 - (int)tp_rd8(tp, 0, p + 3));
        c->progress = (unsigned char)tp_rd8(tp, 0, p + 4);
        c->b5       = (unsigned char)tp_rd8(tp, 0, p + 5);
        c->flags    = (unsigned char)tp_rd8(tp, 0, p + 6);
        p += 7;
        if (c->flags & 0x80) break;                    /* finish line */
    }
    return out->n > 0 ? OG_OK : OG_E_SHORT;
}

int tp_race_start_load(const tp_set *tp, int layout, og_race_start *out)
{
    unsigned px, py, ph;
    int t;
    if (!tp || layout < 0 || layout >= TP_LAYOUTS || !out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    px = tp_rd16(tp, 0, TP_START_X + 2u * (unsigned)layout);
    py = tp_rd16(tp, 0, TP_START_Y + 2u * (unsigned)layout);
    ph = tp_rd16(tp, 0, TP_START_H + 2u * (unsigned)layout);
    for (t = 0; t < 4; ++t) {
        unsigned i = 3u - (unsigned)t;                 /* indexed 3 - truck */
        out->dx[t] = (int)tp_rd16(tp, 0, px + 2 * i);
        out->dy[t] = 0x2C00 - (int)tp_rd16(tp, 0, py + 2 * i);
    }
    /* one heading for the grid; Y flipped turns it the other way */
    out->heading = ((256 - (int)tp_rd8(tp, 0, ph + 3)) & 0xff) << 8;
    return OG_OK;
}

static int tp_drivable(const tp_set *tp, const tp_grid *g, long X, long Y)
{
    tp_plane p;
    if (X < 0 || Y < 0 || X > 0x2bff || Y > 0x2bff) return 0;
    p = tp_plane_under(tp, g, X, Y);
    return tp_surf(&p) != 0x80;
}

int tp_spawns_load(const tp_set *tp, int layout, og_spawns *out)
{
    tp_grid g;
    unsigned p;
    double OX, OY;
    int track = layout & 7, i;

    if (!tp || layout < 0 || layout >= TP_LAYOUTS || !out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    tp_grid_at(tp, track, &g);
    tp_origin_raw(tp, track, &OX, &OY);

    /* 3 bytes a spot (0x3E9D): x/2 and y on the screen, then a third the
     * placing does not need; 0xFF ends the list */
    for (p = tp_rd16(tp, 0, TP_SPOTS + 2u * (unsigned)layout);
         tp_rd8(tp, 0, p) != 0xff && out->n < OG_PICKUP_MAX; p += 3) {
        double sx = 2.0 * tp_rd8(tp, 0, p), sy = tp_rd8(tp, 0, p + 1), Z = 0.0, X = 0, Y = 0;
        int it;
        /* back through the projection, on the ground it lands on */
        for (it = 0; it < 30; ++it) {
            tp_plane pl;
            double Ys = OY - 0.75 * Z - 32.0 * sy;
            Y = Ys / 0.6640625;
            X = 32.0 * sx - Ys / 2.0 - OX;
            pl = tp_plane_under(tp, &g, (long)X, (long)Y);
            Z = tp_plane_z(&pl, (double)((long)X & 255), (double)((long)Y & 255));
        }
        /* a spot on a wall moves to the nearest ground */
        if (!tp_drivable(tp, &g, (long)X, (long)Y)) {
            long bx = -1, by = -1, best = 0, dx, dy;
            for (dy = -768; dy <= 768; dy += 64)
                for (dx = -768; dx <= 768; dx += 64) {
                    long d = dx * dx + dy * dy;
                    if ((bx < 0 || d < best) && tp_drivable(tp, &g, (long)X + dx, (long)Y + dy)) {
                        bx = (long)X + dx; by = (long)Y + dy; best = d;
                    }
                }
            if (bx < 0) continue;
            X = (double)bx; Y = (double)by;
        }
        out->x[out->n] = (int)X;
        out->y[out->n] = 0x2C00 - (int)Y;
        ++out->n;
    }
    if (out->n == 0) return OG_E_SHORT;
    for (i = 0; i < 16; ++i) out->sel[i] = (unsigned char)(i % out->n);
    out->loaded = 1;
    return OG_OK;
}

/* The arcade places the flagman, the pennant poles and banners and the
 * scoreboard in SCREEN pixels, from tables per layout (per track for the
 * board), and its video CPU draws them as sprites:
 *
 *   flagman  X and Y words (offroadt 0x42FE / 0x431E, set at 0x4261,
 *            drawn at 0x429D: sprite 0x4407 at rest, the waves from the
 *            lists at 0x434E and 0x4369)
 *   poles    8-byte records (0xAFF2, set at 0xAF36, drawn at 0xAF55):
 *            +0..+3 the box of cells where a truck sets the pennant
 *            swinging, +4 X (word), +6 Y, +7 a depth; bit 7 of +0 ends
 *   banners  the same records, across a track (0xAEE4; only the base
 *            game's Blaster has any)
 *   board    X and Y words (0x4E88 / 0x4E98, set at 0x4C09)
 *
 * GAME.EXE's own flagmen, poles, banners and boards are the base game's
 * tables put through one fixed change each, from the arcade's 240 lines
 * to its 200 and from one set of sprites to the other: the flagman at
 * (X - 8, Y*5/6 - 8) on all sixteen layouts, the poles at (X - 8,
 * Y*5/6 - 22), the banners at (X + 1, Y*5/6 - 7), the board's corner at
 * (X - 21, Y*5/6 - 28).  The same change
 * puts the Track-Pak's where GAME.EXE would have. */
static int tp_y200(unsigned y) { return tp_rnd(y * 5.0 / 6.0); }

int tp_flagman(const tp_set *tp, int layout, int *sx, int *sy)
{
    if (!tp || layout < 0 || layout >= TP_LAYOUTS || !sx || !sy) return OG_E_ARG;
    *sx = (int)tp_rd16(tp, 0, TP_A->flag_x + 2u * (unsigned)layout) - 8;
    *sy = tp_y200(tp_rd16(tp, 0, TP_A->flag_y + 2u * (unsigned)layout)) - 8;
    return OG_OK;
}

static int tp_decos_from(const tp_set *tp, unsigned table, int layout, int type,
                         tp_deco *out, int n, int cap)
{
    unsigned p = tp_rd16(tp, 0, table + 2u * (unsigned)layout);
    while (n < cap && !(tp_rd8(tp, 0, p) & 0x80)) {
        unsigned x0 = tp_rd8(tp, 0, p), x1 = tp_rd8(tp, 0, p + 1);
        unsigned y0 = tp_rd8(tp, 0, p + 2), y1 = tp_rd8(tp, 0, p + 3);
        unsigned X = tp_rd16(tp, 0, p + 4), Y = tp_rd8(tp, 0, p + 6);
        tp_deco *d = &out[n++];
        d->type = type;
        d->sx = (int)X + (type ? 1 : -8);
        d->sy = tp_y200(Y) - (type ? 7 : 22);
        /* the middle of its box of cells, in DOS world units */
        d->wx = (int)((x0 + x1) * 128u);
        d->wy = 0x2C00 - (int)((y0 + y1) * 128u);
        p += 8;
    }
    return n;
}

int tp_scenery(const tp_set *tp, int layout, tp_deco *out, int cap)
{
    int n;
    if (!tp || layout < 0 || layout >= TP_LAYOUTS || !out) return OG_E_ARG;
    n = tp_decos_from(tp, TP_A->poles, layout, 0, out, 0, cap);
    return tp_decos_from(tp, TP_A->banners, layout, 1, out, n, cap);
}

int tp_board_at(const tp_set *tp, int track, int *x, int *y)
{
    int bx, by;
    if (!tp || track < 0 || track >= TP_TRACKS || !x || !y) return OG_E_ARG;
    bx = (int)tp_rd16(tp, 0, TP_A->board_x + 2u * (unsigned)track) - 21;
    by = tp_y200(tp_rd16(tp, 0, TP_A->board_y + 2u * (unsigned)track)) - 28;
    /* on the screen whole, as GAME.EXE's are (its Sidewinder board sits
     * where the arcade's would run off the right edge) */
    if (bx > TP_ART_W - 71) bx = TP_ART_W - 71;
    if (by > TP_ART_H - 31) by = TP_ART_H - 31;
    *x = bx < 0 ? 0 : bx;
    *y = by < 0 ? 0 : by;
    return OG_OK;
}

int tp_schedule(const tp_set *tp, unsigned char *layouts, int cap)
{
    int n = 0;
    unsigned p, v;
    if (!tp || !layouts) return OG_E_ARG;
    p = TP_ORDER;
    if (!p) return 0;
    while (n < cap && (v = tp_rd8(tp, 0, p + (unsigned)n)) != 0xff)
        layouts[n++] = (unsigned char)(v & 15);
    return n;
}

int tp_race_tune(const tp_set *tp, int track)
{
    if (!tp || track < 0 || track >= TP_TRACKS) return -1;
    return (int)tp_rd8(tp, 0, (tp->kind == TP_ARCADE ? 0x54BAu : 0x56C0u) + (unsigned)track);
}

int tp_anthem(const tp_set *tp, int nation)
{
    if (!tp || nation < 0 || nation >= TP_NATIONS) return -1;
    return (int)tp_rd8(tp, 2, (tp->kind == TP_ARCADE ? 0x3AD2u : 0x2682u) + (unsigned)nation);
}

/* ------------------------------------------------------------------ */
/* Every sprite (F3)                                                   */
/* ------------------------------------------------------------------ */

/* A descriptor at window address d of `bank`, if it is one (a rule
 * checked against every sprite MAME drew):
 *   +0  the kind (& 0x0E) not 0; bits 7, 5, 4 and 0 clear -- the drawn
 *       ones' are all 02, 04, 06 or 4C
 *   +9  1 to 0xC0 rows
 *   +4  the even block 1 to 0x20 bytes wide (to 0x40 only when the odd
 *       block is the very same), +7 the odd block the same or one more
 *   both blocks inside the window 0x4000-0xBFFF, or for a fill (kind 2)
 *   one pointer, its high byte 0 (only the low byte is used) */
static int tp_sprite_at(const tp_set *tp, unsigned bank, unsigned d, tp_sprite *o)
{
    const unsigned k0 = tp_vrd(tp, bank, d);
    const unsigned pa = tp_vrd(tp, bank, d + 2) | (tp_vrd(tp, bank, d + 3) << 8);
    const unsigned pb = tp_vrd(tp, bank, d + 5) | (tp_vrd(tp, bank, d + 6) << 8);
    const unsigned wa = tp_vrd(tp, bank, d + 4), wb = tp_vrd(tp, bank, d + 7);
    const unsigned rows = tp_vrd(tp, bank, d + 9);
    int ok;
    if (!(k0 & 0x0E) || (k0 & 0xB1) || !rows || rows > 0xC0 || !wa || wa > 0x40) return 0;
    if (wb != wa && wb != wa + 1) return 0;
    if (wa > 0x20 && !(pb == pa && wb == wa)) return 0;
    if ((k0 & 0x0E) == 2)
        ok = pa == pb && pa < 0x100;
    else
        ok = pa >= 0x4000 && pa + wa * rows <= 0xC000 &&
             pb >= 0x4000 && pb + wb * rows <= 0xC000;
    if (ok && o) {
        o->addr  = (unsigned short)(d | bank);
        o->kind  = (unsigned char)(k0 & 0x0E);
        o->flags = (unsigned char)(k0 & ~0x0Eu);
        o->w     = (unsigned char)(2 * wa);
        o->w_odd = (unsigned char)(2 * wb);
        o->h     = (unsigned char)rows;
        o->ax    = (signed char)tp_vrd(tp, bank, d + 1);
        o->ay    = (signed char)tp_vrd(tp, bank, d + 8);
    }
    return ok;
}

int tp_sprite_list(const tp_set *tp, tp_sprite *out, int cap)
{
    int bank, n = 0;
    unsigned d;
    if (!tp) return 0;
    for (bank = 0; bank < TP_SPRITE_BANKS; ++bank) {
        if (!tp_bank_present(tp, bank)) continue;
        for (d = 0x4000; d < 0xC000; d += 16) {
            tp_sprite s;
            if (!tp_sprite_at(tp, (unsigned)bank, d, &s)) continue;
            if (out && n < cap) out[n] = s;
            ++n;
        }
    }
    return n;
}

/* The palette each sprite is drawn in when it is not a race's.  The
 * master installs a list through 0x038D ([0xe050], 0x10 to [0xe04f]):
 * a race's from the per-track table, the screens' their own -- 0x1A53,
 * 0x1A75 and 0x1BD8, at the same places and the same in both sets.
 * Which sprites go with which came from MAME (logging
 * every sprite drawn, and the list up as it was, over attract mode and
 * a game on each race slot; a sprite only ever drawn under a screen's
 * list is listed, with the other set's evidence where both hold the
 * same sprite), and for a few no run drew, from the code that draws
 * them: Ivan in his truck (0x4084, 0x4094) by the podium's (bank 2
 * 0x37f6), the girls in dresses (0x4005-0x4055) in the swimsuit girls'
 * places, an operator's setting picking which ([0xe91c] bit 7), and the
 * base game's podium pieces (0x4065-0x40f5), named in the podium's
 * table, the same in both sets (bank 2 0x3b55, offroadt 0x26c5).
 * Ranges of consecutive sprites, bank by bank. */
typedef struct { unsigned short first, last, list; } tp_sprpal;
static const tp_sprpal tp_sprpal_a[] = {          /* offroad */
    { 0x4004, 0x4054, 0x1A75 },
    { 0x4084, 0x4094, 0x1A53 },
    { 0x4005, 0x4055, 0x1A75 },
    { 0x4065, 0x40F5, 0x1A53 },
    { 0x4006, 0x4116, 0x1A75 },
    { 0x4196, 0x41B6, 0x1A53 },
    { 0x41C6, 0x41C6, 0x1A75 },
    { 0x4457, 0x44B7, 0x1A75 },
    { 0x4637, 0x4637, 0x1A75 },
    { 0x4657, 0x4657, 0x1A53 },
    { 0x4008, 0x40A8, 0x1BD8 },
    { 0x4009, 0x4109, 0x1A53 },
    { 0x4119, 0x4129, 0x1A75 },
    { 0x4179, 0x4179, 0x1A75 },
};
static const tp_sprpal tp_sprpal_t[] = {          /* offroadt */
    { 0x4004, 0x4054, 0x1A75 },
    { 0x4084, 0x4094, 0x1A53 },
    { 0x4005, 0x4055, 0x1A75 },
    { 0x4065, 0x40F5, 0x1A53 },
    { 0x4105, 0x4115, 0x1A75 },
    { 0x4006, 0x4116, 0x1A75 },
    { 0x4196, 0x41B6, 0x1A53 },
    { 0x41C6, 0x41C6, 0x1A75 },
    { 0x4406, 0x4426, 0x1BD8 },
    { 0x4437, 0x4467, 0x1A75 },
    { 0x4477, 0x4477, 0x1A53 },
    { 0x4487, 0x4497, 0x1A75 },
    { 0x4617, 0x4617, 0x1A75 },
    { 0x4637, 0x4637, 0x1A53 },
    { 0x4008, 0x40A8, 0x1BD8 },
    { 0x4009, 0x4009, 0x1A53 },
    { 0x40A9, 0x4109, 0x1A53 },
    { 0x4119, 0x4119, 0x1A75 },
    { 0x4149, 0x4159, 0x1A75 },
    { 0x4189, 0x4189, 0x1A75 },
};

unsigned tp_sprite_palette(const tp_set *tp, unsigned addr, int track,
                           unsigned char pal[1024 * 3])
{
    const tp_sprpal *t = tp && tp->kind == TP_ARCADE ? tp_sprpal_a : tp_sprpal_t;
    const size_t n = tp && tp->kind == TP_ARCADE
                   ? sizeof tp_sprpal_a / sizeof tp_sprpal_a[0]
                   : sizeof tp_sprpal_t / sizeof tp_sprpal_t[0];
    size_t i;
    if (!tp || !pal) return 0;
    for (i = 0; i < n; ++i)
        if ((addr & 15) == (t[i].first & 15u) &&
            (addr & 0xFFF0) >= (t[i].first & 0xFFF0u) && (addr & 0xFFF0) <= (t[i].last & 0xFFF0u)) {
            tp_list_rgb(tp, t[i].list, pal, NULL);
            return t[i].list;
        }
    tp_list_rgb(tp, tp_rd16(tp, 0, TP_PALETTES + 2u * (unsigned)(track & 7)), pal, NULL);
    return 0;
}

int tp_sprite_draw(const tp_set *tp, unsigned addr, int odd,
                   const unsigned char *pal, int bg,
                   unsigned char *rgb, int w, int h, int x, int y)
{
    const unsigned bank = addr & 15, d = addr & 0xFFF0;
    tp_sprite s;
    unsigned ptr;
    int bw, r, b, k;
    if (!tp || !pal || !rgb || bank >= TP_SPRITE_BANKS || !tp_bank_present(tp, (int)bank) ||
        !tp_sprite_at(tp, bank, d, &s))
        return OG_E_ARG;
    ptr = tp_vrd(tp, bank, d + (odd ? 5 : 2)) | (tp_vrd(tp, bank, d + (odd ? 6 : 3)) << 8);
    bw  = (odd ? s.w_odd : s.w) / 2;
    bg &= 63;
    for (r = 0; r < s.h; ++r)
        for (b = 0; b < bw; ++b) {
            const unsigned v = tp_vbyte(tp, bank, s.kind, ptr, (unsigned)(r * bw + b));
            for (k = 0; k < 2; ++k) {
                const unsigned f = k ? v & 15 : v >> 4;
                const int px = x + 2 * b + k, py = y + r;
                if (!f || px < 0 || px >= w || py < 0 || py >= h) continue;
                memcpy(rgb + ((size_t)py * (size_t)w + (size_t)px) * 3,
                       pal + (f * 64 + (unsigned)bg) * 3, 3);
            }
        }
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* The nations' names and flags                                        */
/* ------------------------------------------------------------------ */

/* Registration lists eight of the sixteen, an operator's choice (the
 * four bytes at [0xe973], a nation a nibble, copied to [0xe98d] at bank 2
 * 0x4b03); each entry is the name, 10 bytes, at 0x47EE + 10n (offroad
 * 0x57F9), and the flag, the sprite at 0x47B9 + 2n (offroad 0x57C4),
 * drawn beside it (0x4738) and on the podium (0x25f1, offroad 0x3a41),
 * where entry 16 is a driver with no nation.  The two sets' flags are the
 * same pictures at different addresses. */
#define TP_NATION_NAMES  (tp->kind == TP_ARCADE ? 0x57F9u : 0x47EEu)
#define TP_NATION_FLAGS  (tp->kind == TP_ARCADE ? 0x57C4u : 0x47B9u)

int tp_nation_name(const tp_set *tp, int nation, char out[TP_NATION_NAME])
{
    int i, a = 0, b;
    char raw[TP_NATION_NAME];
    if (!tp || !out || nation < 0 || nation >= TP_NATIONS) return OG_E_ARG;
    for (i = 0; i < TP_NATION_NAME - 1; ++i) {
        unsigned c = tp_rd8(tp, 2, TP_NATION_NAMES + 10u * (unsigned)nation + (unsigned)i);
        raw[i] = (char)(c >= 0x20 && c < 0x7f ? c : ' ');
    }
    raw[TP_NATION_NAME - 1] = 0;
    /* centred with spaces in a field of nine: trim both ends */
    for (b = TP_NATION_NAME - 2; b >= 0 && raw[b] == ' '; --b) {}
    while (a <= b && raw[a] == ' ') ++a;
    for (i = 0; a <= b; ++a) out[i++] = raw[a];
    out[i] = 0;
    return i ? OG_OK : OG_E_RANGE;
}

int tp_flag_pens(const tp_set *tp, int nation, unsigned char *pens,
                 int *w, int *h, unsigned *addr)
{
    tp_sprite s;
    unsigned a, bank, ptr;
    int r, b, bw;
    if (!tp || !pens || nation < 0 || nation > TP_NATIONS) return OG_E_ARG;
    a = tp_rd16(tp, 2, TP_NATION_FLAGS + 2u * (unsigned)nation);
    bank = a & 15;
    if (bank >= TP_SPRITE_BANKS || !tp_bank_present(tp, (int)bank) ||
        !tp_sprite_at(tp, bank, a & 0xFFF0, &s) || s.kind == 2 ||
        s.w > TP_FLAG_W || s.h > TP_FLAG_H)
        return OG_E_RANGE;
    ptr = tp_vrd(tp, bank, (a & 0xFFF0) + 2) | (tp_vrd(tp, bank, (a & 0xFFF0) + 3) << 8);
    bw = s.w / 2;
    for (r = 0; r < s.h; ++r)
        for (b = 0; b < bw; ++b) {
            const unsigned v = tp_vrd(tp, bank, ptr + (unsigned)(r * bw + b));
            pens[r * s.w + 2 * b]     = (unsigned char)(v >> 4);   /* high nibble first */
            pens[r * s.w + 2 * b + 1] = (unsigned char)(v & 15);
        }
    if (w) *w = s.w;
    if (h) *h = s.h;
    if (addr) *addr = a;
    return OG_OK;
}

/* GAME.EXE's layout n is the base game's layout tp_layout_from_dos[n]:
 * tracks 2 and 3 swapped, and on DOS tracks 1, 2, 4 and 7 the two
 * directions swapped (the gates and routes of both agree on it). */
const unsigned char tp_layout_from_dos[16] = {
    0, 9, 11, 2, 12, 5, 6, 15, 8, 1, 3, 10, 4, 13, 14, 7
};

int tp_course_box(const tp_set *tp, int track, int box[4])
{
    og_race_start st;
    og_contour c;
    og_surface s;
    unsigned char *h = NULL, *seen = NULL;
    short *stack = NULL;
    int ox, oy, sp = 0, i, rc, n = 0;

    if (!tp || track < 0 || track >= TP_TRACKS || !box) return OG_E_ARG;
    rc = tp_race_start_load(tp, track, &st);
    if (rc != OG_OK) return rc;
    rc = tp_contour_load(tp, track, &c, &s, NULL);
    if (rc != OG_OK) return rc;
    h = (unsigned char *)malloc(TP_W * TP_H);
    seen = (unsigned char *)calloc(TP_W * TP_H, 1);
    stack = (short *)malloc(sizeof(short) * TP_W * TP_H * 4);
    if (!h || !seen || !stack || og_contour_heights(&c, &s, h) != OG_OK) { rc = OG_E_NOMEM; goto done; }
    tp_track_origin(tp, track, &ox, &oy);
    box[0] = OG_SCREEN_W; box[1] = OG_SCREEN_H; box[2] = -1; box[3] = -1;

    /* every cell with ground in it that the start reaches */
    i = (st.dy[0] >> 8) * TP_W + (st.dx[0] >> 8);
    if (i >= 0 && i < TP_W * TP_H) stack[sp++] = (short)i;
    while (sp > 0) {
        int cell = stack[--sp], cx = cell % TP_W, cy = cell / TP_W, k, ground = 0, d;
        if (seen[cell]) continue;
        seen[cell] = 1;
        for (k = 0; k < 4; ++k) {
            long po = og_surface_resolve_at(&s, c.cell[cell], k & 1 ? 200 : 50, k & 2 ? 200 : 50);
            if (po >= 0 && !(s.poly[po] & 0x80)) ground = 1;
        }
        if (!ground) continue;
        ++n;
        for (k = 0; k < 4; ++k) {                 /* its corners, projected */
            int px, py;
            og_project((long)(cx + (k & 1)) << 8, (long)(cy + (k >> 1)) << 8,
                       h[cell], ox, oy, &px, &py);
            if (px < box[0]) box[0] = px;
            if (py < box[1]) box[1] = py;
            if (px > box[2]) box[2] = px;
            if (py > box[3]) box[3] = py;
        }
        for (d = 0; d < 4; ++d) {
            int nx = cx + (d == 0) - (d == 1), ny = cy + (d == 2) - (d == 3);
            if (nx >= 0 && nx < TP_W && ny >= 0 && ny < TP_H && !seen[ny * TP_W + nx])
                stack[sp++] = (short)(ny * TP_W + nx);
        }
    }
    /* and the rails round it */
    box[0] -= 6; box[1] -= 6; box[2] += 6; box[3] += 6;
    if (box[0] < 0) box[0] = 0;
    if (box[1] < 0) box[1] = 0;
    if (box[2] > OG_SCREEN_W - 1) box[2] = OG_SCREEN_W - 1;
    if (box[3] > OG_SCREEN_H - 1) box[3] = OG_SCREEN_H - 1;
    rc = n > 0 ? OG_OK : OG_E_SHORT;
done:
    og_surface_free(&s);
    free(h); free(seen); free(stack);
    return rc;
}
