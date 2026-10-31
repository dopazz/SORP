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

/* offroad_gfx.h -- readers for the graphics formats used by
 * Ivan Stewart's Super Off Road (DOS, Graftgold/Leland 1989).
 *
 *   .XX  pictures  (TITLES.XX, LOADA.XX, LOADB.XX)
 *        320x200 EGA, 4 planes, column-major RLE.
 *
 *   .PCG cell banks (ETITLE.PCG, EGAME.PCG, ESHOP.PCG, EEND.PCG,
 *                    ETRUCK.PCG, ESHADOW.PCG)
 *        headerless planar EGA cells stored as vertical strips.
 *
 *   .XX  fonts     (IRONFONT.XX, ROADFONT.XX)
 *        "IRON"/"ROAD" magic, glyph offset table, planar glyphs.
 *
 * All multi-byte integers are BIG-endian: the artwork was converted
 * from the 68000-based Leland arcade original.
 *
 *
 * THE COMPLEMENT
 * --------------
 * Pixel data in both .XX pictures and .PCG cells is stored bit-inverted.
 * The game undoes this at load time -- EGA.EXE routine 0x256D does a NOT
 * pass over each decompressed 8000-byte plane, and the .PCG sprite
 * compiler at 0x22C6 does the same with inline `not word ptr [si]`.
 * Complementing all four planes is equivalent to mapping each 4-bit
 * pixel value v to 15-v.
 *
 * So the attribute index the hardware actually sees is 15 minus the
 * value assembled from the file's bits.  This library applies that by
 * default; pass OG_F_RAW to get the literal stored values instead.
 *
 * Feeding un-complemented data to a naive default-EGA palette happens to
 * produce something that looks almost plausible, which is a trap -- the
 * colours are wrong and the artwork is a negative.
 *
 *
 * THE PALETTE
 * -----------
 * The game reprograms the Attribute Controller, so the default EGA
 * colours are wrong.  og_palette_super_offroad[] holds the palette the
 * game loads, transcribed from a DOSBox-X register dump taken on the
 * copyright screen (TITLES.XX).  See the note on its definition about
 * accuracy.
 *
 * Public domain / CC0.  No dependencies beyond the C standard library.
 */
#ifndef OFFROAD_GFX_H
#define OFFROAD_GFX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Errors                                                              */
/* ------------------------------------------------------------------ */

enum {
    OG_OK          =  0,
    OG_E_ARG       = -1,   /* bad argument                            */
    OG_E_SHORT     = -2,   /* buffer ends in the middle of a record   */
    OG_E_MAGIC     = -3,   /* wrong signature                         */
    OG_E_SIZE      = -4,   /* embedded length disagrees with buffer   */
    OG_E_RANGE     = -5,   /* index/geometry out of range             */
    OG_E_NOMEM     = -6,   /* allocation failed                       */
    OG_E_IO        = -7    /* a file could not be written             */
};

const char *og_strerror(int err);

/* Decoder flags */
enum {
    /* Return the literal bits from the file instead of the display
     * attribute indices.  See "THE COMPLEMENT" above. */
    OG_F_RAW          = 1 << 0,

    /* .XX only: verify the big-endian self-length header, the same
     * check GAME.EXE performs against its hardcoded size table. */
    OG_F_CHECK_LENGTH = 1 << 1
};

/* ------------------------------------------------------------------ */
/* Palettes: 16 entries, 3 bytes (R,G,B) each                          */
/* ------------------------------------------------------------------ */

/* The palette Super Off Road loads.  Transcribed from a DOSBox-X
 * CSPAL dump; the screenshot it came from had been rescaled, so the
 * channel values are accurate to a few counts rather than exact.  Every
 * component lands on one of
 *     0, 48, 79, 111, 143, 175, 206, 238
 * which is very likely a clean eight-level ramp that the rescaling has
 * shifted slightly.  Substitute the exact DAC values if you can read
 * them numerically from the emulator. */
extern const unsigned char og_palette_super_offroad[16 * 3];

/* Default EGA colours, for comparison.  Not what the game uses. */
extern const unsigned char og_ega_palette[16 * 3];

/* ------------------------------------------------------------------ */
/* Images: 8bpp, one attribute index (0..15) per byte                  */
/* ------------------------------------------------------------------ */

typedef struct {
    int             w, h;
    unsigned char  *px;      /* w*h bytes, row-major, owned            */
} og_image;

int  og_image_init(og_image *img, int w, int h);   /* zero-filled      */
void og_image_free(og_image *img);

/* Composite src over dst at (x,y).  Pixels equal to `key` in src are
 * treated as transparent; pass key < 0 to copy everything.  Clipped. */
void og_image_blit(og_image *dst, const og_image *src, int x, int y, int key);

/* Expand to 24-bit RGB.  `rgb` must hold w*h*3 bytes. */
void og_image_to_rgb(const og_image *img, const unsigned char *palette,
                     unsigned char *rgb);

/* ------------------------------------------------------------------ */
/* .XX full-screen pictures                                            */
/* ------------------------------------------------------------------ */

#define OG_XX_WIDTH        320
#define OG_XX_HEIGHT       200
#define OG_XX_BYTES_PER_ROW 40
#define OG_XX_PLANE_BYTES  (OG_XX_BYTES_PER_ROW * OG_XX_HEIGHT)  /* 8000 */

/* Decode one RLE plane.  `dst` receives OG_XX_PLANE_BYTES bytes laid
 * out row-major (dst[row*40 + col]), matching EGA video memory.  The
 * bits are as stored, i.e. still complemented.  `*io_pos` is advanced
 * past the plane's stream.  Exposed so callers can drive real EGA
 * hardware a plane at a time the way the game does. */
int og_xx_decode_plane(const unsigned char *data, size_t len,
                       size_t *io_pos, unsigned char *dst);

/* Decode a whole .XX picture into a 320x200 og_image of attribute
 * indices.  `flags` is a combination of OG_F_*. */
int og_xx_decode(const unsigned char *data, size_t len, unsigned flags,
                 og_image *out);

/* ------------------------------------------------------------------ */
/* .PCG cell banks                                                     */
/* ------------------------------------------------------------------ */

/* A .PCG file has no header -- pixel data starts at byte 0.
 *
 * Each scanline is `cell_bytes` bytes per plane for 4 planes, so the
 * row stride is cell_bytes*4 and the cell width is cell_bytes*8.
 * EGA.EXE dispatches on exactly this value (routine 0x22BC indexes a
 * jump table by cell_bytes-1), so only 1..4 (8..32 px) are legal.
 * Every known .PCG uses 4, i.e. 32-pixel-wide cells.
 *
 * Artwork wider than one cell is stored as consecutive vertical
 * strips: strip N sits immediately to the right of strip N-1. */

#define OG_PCG_MAX_CELL_BYTES 4

typedef struct {
    const unsigned char *data;
    size_t               len;
    unsigned             flags;
    int                  cell_bytes;  /* 1..4                         */
    int                  cell_w;      /* cell_bytes * 8               */
    int                  stride;      /* cell_bytes * 4               */
    int                  rows;        /* len / stride                 */
} og_pcg;

int og_pcg_open(og_pcg *p, const unsigned char *data, size_t len,
                int cell_bytes, unsigned flags);

/* Decode one strip: `height` scanlines starting at scanline `row`. */
int og_pcg_strip(const og_pcg *p, int row, int height, og_image *out);

/* Decode `nstrips` consecutive strips of `height` scanlines each,
 * starting at scanline `row`, laid out left to right.
 *
 * The "SUPER OFF ROAD" logo in ETITLE.PCG is
 *     og_pcg_chunk(&pcg, 415, 83, 5, &img)   ->  160x83 */
int og_pcg_chunk(const og_pcg *p, int row, int height, int nstrips,
                 og_image *out);

/* ------------------------------------------------------------------ */
/* Fonts (IRONFONT.XX, ROADFONT.XX)                                    */
/* ------------------------------------------------------------------ */

/* File layout (all big-endian):
 *
 *   +0   char[4]  "IRON" or "ROAD"
 *   +4   u16      reserved (0)
 *   +6   u16      reserved (0)
 *   +8   u16      first character code
 *   +10  u16      last character code
 *   +12  u16      nominal cell height
 *   +14  u16      nominal cell width
 *   +16  u16[n]   glyph offsets from start of file, n = last-first+2
 *
 * Note the +2: slot 0 is a dummy (offset 0) and character c lives in
 * slot (c - first + 1).  The table therefore ends exactly where the
 * first glyph begins.
 *
 * Glyph record at each offset:
 *
 *   +0   u16      reserved (0)
 *   +2   u16      reserved (0)
 *   +4   u16      advance width in pixels
 *   +6   u16      height in pixels
 *   +8   u16      the glyph's TOP OFFSET within the line: 0 for the
 *                 tall brackets, 1 for capitals and digits, 2 for i j t,
 *                 4 for lowercase, and 7, 8, 9 for , . and _ .  This is
 *                 what puts the lowercase on its own x-height line and
 *                 lets g and , descend below the baseline.
 *   +10  rows[height+1], each 4 x u16 = one 16-pixel scanline in
 *                 four EGA bitplanes, MSB leftmost.
 *
 * There is one more stored row than `height`: the glyphs carry a
 * one-pixel outline, and the outline needs the extra scanline.  Space
 * (the first slot) has height 0, so it stores a single blank row and
 * just contributes its advance width.
 *
 * A pixel's VALUE is a bit from each plane (plane 0 bit 0), which the
 * driver maps through the style's row: 0 untouched, 1 the fill, and
 * 2..15 the outline colour in every style.  Value 1 is the fill, 2 the
 * outline, and the '^' glyph a solid block of 15 -- an eraser in the
 * outline colour.  Glyph bits are NOT complemented, so og_font_draw()
 * takes explicit fill and outline attribute indices. */

#define OG_GLYPH_MAX_WIDTH 16
#define OG_GLYPH_HDR_BYTES 10
#define OG_GLYPH_ROW_BYTES 8

typedef struct {
    const unsigned char *data;
    size_t               len;
    char                 id[5];   /* "IRON" / "ROAD", NUL terminated  */
    int                  first, last;
    int                  height, width;   /* nominal cell metrics     */
    int                  nslots;          /* last - first + 2         */
} og_font;

typedef struct {
    int advance;   /* pixels to step after drawing                    */
    int height;    /* fill height; stored rows = height + 1           */
    int rows;      /* height + 1                                      */
    int flags;
    const unsigned char *bits;   /* rows * 8 bytes                    */
} og_glyph;

int og_font_open(og_font *f, const unsigned char *data, size_t len);

/* Look up one character.  OG_E_RANGE if outside [first,last] or the
 * slot is empty. */
int og_font_glyph(const og_font *f, int ch, og_glyph *g);

/* Rasterise one glyph into its own 16-wide image using the given
 * attribute indices.  Pixels outside both masks are left 0. */
int og_font_glyph_image(const og_font *f, int ch,
                        int fill_attr, int outline_attr, og_image *out);

/* Total advance of `s`, adding `spacing` pixels between characters. */
int og_font_text_width(const og_font *f, const char *s, int spacing);

/* Draw `s` into `dst` with its top-left at (x,y).  Clipped.  Returns
 * the x coordinate just past the last glyph.
 *
 * On the copyright screen the game draws IRONFONT with the outline in
 * attribute 13 (black) over the teal background; body text uses a light
 * fill.  Pick indices to taste -- the glyph data only distinguishes
 * "fill" from "outline". */
/* outline_attr < 0 means the glyph has NO border: those pixels are left
 * as they are, so whatever is behind the text shows through.  Passing 0
 * instead paints them attribute 0, which in a composed image is the
 * screen's own background rather than "transparent". */
int og_font_draw(const og_font *f, og_image *dst, int x, int y,
                 const char *s, int spacing,
                 int fill_attr, int outline_attr);

/* ------------------------------------------------------------------ */
/* Convenience                                                         */
/* ------------------------------------------------------------------ */

/* Read a whole file.  Caller frees *out with free(). */
int og_read_file(const char *path, unsigned char **out, size_t *out_len);

/* Write an og_image as a binary PPM through `palette`. */
int og_write_ppm(const char *path, const og_image *img,
                 const unsigned char *palette);

#ifdef __cplusplus
}
#endif
#endif /* OFFROAD_GFX_H */
