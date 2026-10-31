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

/* offroad_gfx.c -- see offroad_gfx.h */

#include "offroad_gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static unsigned og_be16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static unsigned long og_be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] <<  8) |  (unsigned long)p[3];
}

const char *og_strerror(int err)
{
    switch (err) {
    case OG_OK:      return "ok";
    case OG_E_ARG:   return "bad argument";
    case OG_E_SHORT: return "truncated data";
    case OG_E_MAGIC: return "bad signature";
    case OG_E_SIZE:  return "embedded length does not match buffer";
    case OG_E_RANGE: return "out of range";
    case OG_E_NOMEM: return "out of memory";
    case OG_E_IO:    return "could not write the file";
    default:        return "unknown error";
    }
}

/* Attribute Controller palette as loaded by the game, read from a
 * DOSBox-X CSPAL dump on the copyright screen. */
const unsigned char og_palette_super_offroad[16 * 3] = {
    175, 206, 238,   /*  0  pale blue                                 */
    238, 238, 238,   /*  1  white                                     */
    238, 206, 111,   /*  2  pale gold                                 */
    143, 206, 238,   /*  3  light blue                                */
    238, 175,   0,   /*  4  amber                                     */
    238, 206,  79,   /*  5  gold                                      */
    206, 111,   0,   /*  6  orange                                    */
    206,   0,   0,   /*  7  red                                       */
      0, 111,   0,   /*  8  green                                     */
    175,   0,   0,   /*  9  dark red                                  */
    111,  48,   0,   /* 10  brown                                     */
     79,   0,   0,   /* 11  maroon                                    */
      0, 143, 238,   /* 12  blue                                      */
      0,   0,   0,   /* 13  black                                     */
    238, 238, 238,   /* 14  white                                     */
      0,  79, 111    /* 15  dark teal (screen background)             */
};

const unsigned char og_ega_palette[16 * 3] = {
      0,  0,  0,    0,  0,170,    0,170,  0,    0,170,170,
    170,  0,  0,  170,  0,170,  170, 85,  0,  170,170,170,
     85, 85, 85,   85, 85,255,   85,255, 85,   85,255,255,
    255, 85, 85,  255, 85,255,  255,255, 85,  255,255,255
};

/* ------------------------------------------------------------------ */
/* Images                                                              */
/* ------------------------------------------------------------------ */

int og_image_init(og_image *img, int w, int h)
{
    if (!img || w <= 0 || h <= 0) return OG_E_ARG;
    img->px = (unsigned char *)calloc((size_t)w * (size_t)h, 1);
    if (!img->px) { img->w = img->h = 0; return OG_E_NOMEM; }
    img->w = w;
    img->h = h;
    return OG_OK;
}

void og_image_free(og_image *img)
{
    if (!img) return;
    free(img->px);
    img->px = NULL;
    img->w = img->h = 0;
}

void og_image_blit(og_image *dst, const og_image *src, int x, int y, int key)
{
    int sy, sx;

    if (!dst || !src || !dst->px || !src->px) return;

    for (sy = 0; sy < src->h; ++sy) {
        int dy = y + sy;
        if (dy < 0 || dy >= dst->h) continue;
        for (sx = 0; sx < src->w; ++sx) {
            int dx = x + sx;
            unsigned char v;
            if (dx < 0 || dx >= dst->w) continue;
            v = src->px[(size_t)sy * src->w + sx];
            if (key >= 0 && v == (unsigned char)key) continue;
            dst->px[(size_t)dy * dst->w + dx] = v;
        }
    }
}

void og_image_to_rgb(const og_image *img, const unsigned char *palette,
                     unsigned char *rgb)
{
    size_t i, n;

    if (!img || !img->px || !palette || !rgb) return;

    n = (size_t)img->w * (size_t)img->h;
    for (i = 0; i < n; ++i) {
        const unsigned char *c = palette + (img->px[i] & 15) * 3;
        rgb[i * 3 + 0] = c[0];
        rgb[i * 3 + 1] = c[1];
        rgb[i * 3 + 2] = c[2];
    }
}

/* ------------------------------------------------------------------ */
/* .XX full-screen pictures                                            */
/* ------------------------------------------------------------------ */

/* Faithful port of the decompressor at EGA.EXE 0009:2531.
 *
 * The plane is written column by column, stepping one *screen row*
 * (40 bytes) per output byte -- the stream is column-major.  Only the
 * values 0x00 and 0xFF are run-coded: either is followed by a repeat
 * count giving the number of ADDITIONAL copies, and a count of zero
 * means the byte was a plain literal. */
int og_xx_decode_plane(const unsigned char *data, size_t len,
                       size_t *io_pos, unsigned char *dst)
{
    size_t pos;
    int col;

    if (!data || !io_pos || !dst) return OG_E_ARG;
    pos = *io_pos;

    memset(dst, 0, OG_XX_PLANE_BYTES);

    for (col = 0; col < OG_XX_BYTES_PER_ROW; ++col) {
        int di    = col;
        int count = OG_XX_HEIGHT;

        while (count > 0) {
            unsigned char b;

            if (pos >= len) return OG_E_SHORT;
            b = data[pos++];

            dst[di] = b;
            di += OG_XX_BYTES_PER_ROW;
            --count;
            if (count == 0) break;

            if (b == 0x00 || b == 0xFF) {
                unsigned char n;
                if (pos >= len) return OG_E_SHORT;
                n = data[pos++];
                /* n == 0: the byte really was a literal, keep going. */
                while (n > 0 && count > 0) {
                    dst[di] = b;
                    di += OG_XX_BYTES_PER_ROW;
                    --count;
                    --n;
                }
            }
        }
    }

    *io_pos = pos;
    return OG_OK;
}

int og_xx_decode(const unsigned char *data, size_t len, unsigned flags,
                 og_image *out)
{
    unsigned char *planes = NULL;
    size_t pos = 4;
    int p, y, x, rc;

    if (!data || !out) return OG_E_ARG;
    if (len < 4) return OG_E_SHORT;

    if (flags & OG_F_CHECK_LENGTH) {
        if (og_be32(data) != (unsigned long)len) return OG_E_SIZE;
    }

    planes = (unsigned char *)malloc((size_t)OG_XX_PLANE_BYTES * 4);
    if (!planes) return OG_E_NOMEM;

    for (p = 0; p < 4; ++p) {
        rc = og_xx_decode_plane(data, len, &pos,
                                planes + (size_t)p * OG_XX_PLANE_BYTES);
        if (rc != OG_OK) { free(planes); return rc; }
    }

    /* EGA.EXE 0x256D: NOT every plane.  Equivalent to v -> 15-v once
     * the planes are combined; done below unless OG_F_RAW. */

    rc = og_image_init(out, OG_XX_WIDTH, OG_XX_HEIGHT);
    if (rc != OG_OK) { free(planes); return rc; }

    for (p = 0; p < 4; ++p) {
        const unsigned char *pl = planes + (size_t)p * OG_XX_PLANE_BYTES;
        for (y = 0; y < OG_XX_HEIGHT; ++y) {
            const unsigned char *row = pl + (size_t)y * OG_XX_BYTES_PER_ROW;
            unsigned char *dst = out->px + (size_t)y * OG_XX_WIDTH;
            for (x = 0; x < OG_XX_WIDTH; ++x) {
                unsigned bit = (row[x >> 3] >> (7 - (x & 7))) & 1u;
                dst[x] |= (unsigned char)(bit << p);
            }
        }
    }

    if (!(flags & OG_F_RAW)) {
        size_t i, n = (size_t)out->w * (size_t)out->h;
        for (i = 0; i < n; ++i)
            out->px[i] = (unsigned char)(15 - out->px[i]);
    }

    free(planes);
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* .PCG cell banks                                                     */
/* ------------------------------------------------------------------ */

int og_pcg_open(og_pcg *p, const unsigned char *data, size_t len,
                int cell_bytes, unsigned flags)
{
    if (!p || !data) return OG_E_ARG;
    if (cell_bytes < 1 || cell_bytes > OG_PCG_MAX_CELL_BYTES)
        return OG_E_ARG;

    p->data       = data;
    p->len        = len;
    p->flags      = flags;
    p->cell_bytes = cell_bytes;
    p->cell_w     = cell_bytes * 8;
    p->stride     = cell_bytes * 4;      /* four bitplanes            */
    p->rows       = (int)(len / (size_t)p->stride);

    return p->rows > 0 ? OG_OK : OG_E_SHORT;
}

static int og_pcg_blit_strip(const og_pcg *p, int row, int height,
                             og_image *img, int dst_x)
{
    int y, plane, xb, k;

    if (row < 0 || height <= 0 || row + height > p->rows)
        return OG_E_RANGE;

    for (y = 0; y < height; ++y) {
        const unsigned char *src =
            p->data + (size_t)(row + y) * (size_t)p->stride;
        unsigned char *dst = img->px + (size_t)y * img->w + dst_x;

        for (plane = 0; plane < 4; ++plane) {
            const unsigned char *pl = src + (size_t)plane * p->cell_bytes;
            for (xb = 0; xb < p->cell_bytes; ++xb) {
                unsigned char b = pl[xb];
                for (k = 0; k < 8; ++k) {
                    unsigned bit = (b >> (7 - k)) & 1u;
                    dst[xb * 8 + k] |= (unsigned char)(bit << plane);
                }
            }
        }

        if (!(p->flags & OG_F_RAW)) {
            for (k = 0; k < p->cell_w; ++k)
                dst[k] = (unsigned char)(15 - dst[k]);
        }
    }
    return OG_OK;
}

int og_pcg_strip(const og_pcg *p, int row, int height, og_image *out)
{
    return og_pcg_chunk(p, row, height, 1, out);
}

int og_pcg_chunk(const og_pcg *p, int row, int height, int nstrips,
                 og_image *out)
{
    int i, rc;

    if (!p || !p->data || !out) return OG_E_ARG;
    if (nstrips <= 0 || height <= 0) return OG_E_ARG;
    if (row < 0 || row + (long)height * nstrips > p->rows)
        return OG_E_RANGE;

    rc = og_image_init(out, p->cell_w * nstrips, height);
    if (rc != OG_OK) return rc;

    for (i = 0; i < nstrips; ++i) {
        rc = og_pcg_blit_strip(p, row + i * height, height,
                               out, i * p->cell_w);
        if (rc != OG_OK) { og_image_free(out); return rc; }
    }
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* Fonts                                                               */
/* ------------------------------------------------------------------ */

int og_font_open(og_font *f, const unsigned char *data, size_t len)
{
    size_t table_bytes;

    if (!f || !data) return OG_E_ARG;
    if (len < 16) return OG_E_SHORT;

    if (memcmp(data, "IRON", 4) != 0 && memcmp(data, "ROAD", 4) != 0)
        return OG_E_MAGIC;

    memcpy(f->id, data, 4);
    f->id[4] = '\0';

    f->data   = data;
    f->len    = len;
    f->first  = (int)og_be16(data +  8);
    f->last   = (int)og_be16(data + 10);
    f->height = (int)og_be16(data + 12);
    f->width  = (int)og_be16(data + 14);

    if (f->last < f->first) return OG_E_SIZE;

    /* One dummy slot precedes the first real character. */
    f->nslots = f->last - f->first + 2;

    table_bytes = (size_t)f->nslots * 2u;
    if (16 + table_bytes > len) return OG_E_SHORT;

    return OG_OK;
}

int og_font_glyph(const og_font *f, int ch, og_glyph *g)
{
    int slot;
    size_t off;

    if (!f || !f->data || !g) return OG_E_ARG;
    if (ch < f->first || ch > f->last) return OG_E_RANGE;

    slot = ch - f->first + 1;
    off  = og_be16(f->data + 16 + (size_t)slot * 2u);
    if (off == 0) return OG_E_RANGE;          /* empty slot */
    if (off + OG_GLYPH_HDR_BYTES > f->len) return OG_E_SHORT;

    g->advance = (int)og_be16(f->data + off + 4);
    g->height  = (int)og_be16(f->data + off + 6);
    g->flags   = (int)og_be16(f->data + off + 8);
    g->rows    = g->height + 1;               /* outline needs one more */

    if (off + OG_GLYPH_HDR_BYTES +
        (size_t)g->rows * OG_GLYPH_ROW_BYTES > f->len)
        return OG_E_SHORT;

    g->bits = f->data + off + OG_GLYPH_HDR_BYTES;
    return OG_OK;
}

/* Expand one glyph row into 16 attribute values.  A row is four words,
 * and a pixel's VALUE is a bit from each -- the first word bit 0, the
 * fourth bit 3 -- which the driver maps through the style's 16-entry row
 * (EGA.EXE 9:3092, xlat); value 0 is left alone.  Every style maps 1 to
 * its fill and 2..15 all to its outline colour, so that is the rule
 * here.  The '^' glyph is a solid block of 15: drawn in the OUTLINE
 * colour, it is the eraser the shop's money page (0x9574) and the
 * registration fields lay down before their text. */
static void og_glyph_row(const unsigned char *row, int fill_attr,
                         int outline_attr, unsigned char *out16)
{
    unsigned w0 = og_be16(row + 0), w1 = og_be16(row + 2);
    unsigned w2 = og_be16(row + 4), w3 = og_be16(row + 6);
    int k;

    for (k = 0; k < OG_GLYPH_MAX_WIDTH; ++k) {
        unsigned m = 1u << (15 - k);
        int v = ((w0 & m) ? 1 : 0) | ((w1 & m) ? 2 : 0) |
                ((w2 & m) ? 4 : 0) | ((w3 & m) ? 8 : 0);
        if (v == 1)      out16[k] = (unsigned char)(fill_attr & 15);
        else if (v)      out16[k] = (outline_attr < 0)
                                  ? 0xFF       /* no border: leave it */
                                  : (unsigned char)(outline_attr & 15);
        else             out16[k] = 0xFF;      /* transparent */
    }
}

int og_font_glyph_image(const og_font *f, int ch,
                        int fill_attr, int outline_attr, og_image *out)
{
    og_glyph g;
    unsigned char row[OG_GLYPH_MAX_WIDTH];
    int rc, y, k;

    if (!out) return OG_E_ARG;
    rc = og_font_glyph(f, ch, &g);
    if (rc != OG_OK) return rc;

    rc = og_image_init(out, OG_GLYPH_MAX_WIDTH, g.rows);
    if (rc != OG_OK) return rc;

    for (y = 0; y < g.rows; ++y) {
        og_glyph_row(g.bits + (size_t)y * OG_GLYPH_ROW_BYTES,
                     fill_attr, outline_attr, row);
        for (k = 0; k < OG_GLYPH_MAX_WIDTH; ++k)
            if (row[k] != 0xFF)
                out->px[(size_t)y * out->w + k] = row[k];
    }
    return OG_OK;
}

int og_font_text_width(const og_font *f, const char *s, int spacing)
{
    int w = 0, n = 0;

    if (!f || !s) return 0;
    for (; *s; ++s) {
        og_glyph g;
        if (og_font_glyph(f, (unsigned char)*s, &g) != OG_OK) continue;
        if (n++) w += spacing;
        w += g.advance;
    }
    return w;
}

int og_font_draw(const og_font *f, og_image *dst, int x, int y,
                 const char *s, int spacing,
                 int fill_attr, int outline_attr)
{
    int n = 0;

    if (!f || !dst || !dst->px || !s) return x;

    for (; *s; ++s) {
        og_glyph g;
        unsigned char row[OG_GLYPH_MAX_WIDTH];
        int gy;

        /* ROADFONT is an ALL-CAPS font: its lowercase slots are
         * degenerate -- advance 0, height 0, a top offset past the line
         * -- so a lowercase letter either fails to look up or would
         * stamp a stray bar below the text.  Fall back to the capital,
         * which is what the data itself uses. */
        {
            int ch = (unsigned char)*s;
            int ok = (og_font_glyph(f, ch, &g) == OG_OK) &&
                     g.flags <= f->height && !(g.advance == 0 && g.height == 0);
            if (!ok && ch >= 'a' && ch <= 'z') {
                ch = ch - 'a' + 'A';
                ok = (og_font_glyph(f, ch, &g) == OG_OK) && g.flags <= f->height;
            }
            if (!ok) continue;
        }
        if (n++) x += spacing;

        /* The record's third field is a TOP OFFSET, not a flag: capitals
         * and digits carry 1, lowercase 4, the tall brackets 0, and the
         * comma, full stop and underscore 7, 8 and 9.  With it, 'A'
         * (offset 1, height 9) and 'a' (offset 4, height 6) share a
         * baseline while 'g' (offset 4, height 8) hangs below it.
         * Ignoring it leaves every lowercase letter sitting high. */
        for (gy = 0; gy < g.rows; ++gy) {
            int dy = y + g.flags + gy, k;
            if (dy < 0 || dy >= dst->h) continue;
            og_glyph_row(g.bits + (size_t)gy * OG_GLYPH_ROW_BYTES,
                         fill_attr, outline_attr, row);
            for (k = 0; k < OG_GLYPH_MAX_WIDTH; ++k) {
                int dx = x + k;
                if (row[k] == 0xFF) continue;          /* transparent */
                if (dx < 0 || dx >= dst->w) continue;
                dst->px[(size_t)dy * dst->w + dx] = row[k];
            }
        }
        x += g.advance;
    }
    return x;
}

/* ------------------------------------------------------------------ */
/* Convenience                                                         */
/* ------------------------------------------------------------------ */

int og_read_file(const char *path, unsigned char **out, size_t *out_len)
{
    FILE *fp;
    long n;
    unsigned char *buf;

    if (!path || !out || !out_len) return OG_E_ARG;
    *out = NULL; *out_len = 0;

    fp = fopen(path, "rb");
    if (!fp) return OG_E_ARG;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return OG_E_ARG; }
    n = ftell(fp);
    if (n < 0) { fclose(fp); return OG_E_ARG; }
    rewind(fp);

    buf = (unsigned char *)malloc((size_t)n ? (size_t)n : 1u);
    if (!buf) { fclose(fp); return OG_E_NOMEM; }

    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        free(buf); fclose(fp); return OG_E_SHORT;
    }
    fclose(fp);

    *out = buf;
    *out_len = (size_t)n;
    return OG_OK;
}

int og_write_ppm(const char *path, const og_image *img,
                 const unsigned char *palette)
{
    FILE *fp;
    size_t i, n;

    if (!path || !img || !img->px) return OG_E_ARG;
    if (!palette) palette = og_palette_super_offroad;

    fp = fopen(path, "wb");
    if (!fp) return OG_E_ARG;

    fprintf(fp, "P6\n%d %d\n255\n", img->w, img->h);
    n = (size_t)img->w * (size_t)img->h;
    for (i = 0; i < n; ++i)
        fwrite(palette + (img->px[i] & 15) * 3, 1, 3, fp);

    fclose(fp);
    return OG_OK;
}
