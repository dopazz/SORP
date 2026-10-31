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

/* og_data.c -- see og_data.h */

#include "og_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dirent.h>
#endif

/* ------------------------------------------------------------------ */
/* File location                                                       */
/* ------------------------------------------------------------------ */

static int og_ieq(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
    return *a == *b;
}

static int og_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

int og_resolve(const char *dir, const char *name, char *out, size_t cap)
{
    if (!dir || !name || !out || cap < 2) return OG_E_ARG;

    /* Exact spelling first -- the common case. */
    snprintf(out, cap, "%s/%s", dir, name);
    if (og_exists(out)) return OG_OK;

    /* Then all-lower-case, which covers most repacked installs. */
    {
        char lower[64];
        size_t i;
        for (i = 0; i + 1 < sizeof lower && name[i]; ++i)
            lower[i] = (char)tolower((unsigned char)name[i]);
        lower[i] = '\0';
        snprintf(out, cap, "%s/%s", dir, lower);
        if (og_exists(out)) return OG_OK;
    }

    /* Finally scan the directory and compare case-insensitively. */
#if defined(_WIN32)
    {
        WIN32_FIND_DATAA fd;
        HANDLE h;
        char pat[1024];
        snprintf(pat, sizeof pat, "%s\\*", dir);
        h = FindFirstFileA(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (og_ieq(fd.cFileName, name)) {
                    snprintf(out, cap, "%s/%s", dir, fd.cFileName);
                    FindClose(h);
                    return OG_OK;
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
#else
    {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (og_ieq(e->d_name, name)) {
                    snprintf(out, cap, "%s/%s", dir, e->d_name);
                    closedir(d);
                    return OG_OK;
                }
            }
            closedir(d);
        }
    }
#endif

    return OG_E_ARG;
}

int og_dir_looks_valid(const char *dir)
{
    static const char *want[] = { "TRACK0.XX", "ETRUCK.PCG", "IRONFONT.XX" };
    char path[1024];
    size_t i;

    for (i = 0; i < sizeof want / sizeof want[0]; ++i)
        if (og_resolve(dir, want[i], path, sizeof path) != OG_OK)
            return 0;
    return 1;
}

static int og_slurp(const char *dir, const char *name,
                    unsigned char **buf, size_t *len)
{
    char path[1024];
    int rc = og_resolve(dir, name, path, sizeof path);
    if (rc != OG_OK) return rc;
    return og_read_file(path, buf, len);
}

/* ------------------------------------------------------------------ */
/* Reporting what is loaded (see og_data.h)                            */
/* ------------------------------------------------------------------ */

static og_load_note_fn og_note_fn = NULL;

void og_set_load_note(og_load_note_fn fn) { og_note_fn = fn; }

void og_load_note(const char *file, long offset, long bytes,
                  const char *fmt, ...)
{
    char what[192];
    va_list ap;
    if (!og_note_fn) return;
    va_start(ap, fmt);
    vsnprintf(what, sizeof what, fmt, ap);
    va_end(ap);
    og_note_fn(file, offset, bytes, what);
}

/* The file offset of a GAME.EXE data-segment address: the MZ header,
 * then the load image's DS base. */
#define OG_DS_FILE(hdr, ds)  ((long)(hdr) + (long)OG_GAME_DS_IMG + (long)(ds))

/* ------------------------------------------------------------------ */
/* Verifying the game files                                            */
/* ------------------------------------------------------------------ */

/* SHA-256, FIPS 180-4. */
static const unsigned long og_sha_k[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

#define OG_ROR(x, n) ((((x) >> (n)) | ((x) << (32 - (n)))) & 0xffffffffUL)

static void og_sha_block(unsigned long h[8], const unsigned char *p)
{
    unsigned long w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;
    for (i = 0; i < 16; ++i)
        w[i] = ((unsigned long)p[4 * i] << 24) | ((unsigned long)p[4 * i + 1] << 16) |
               ((unsigned long)p[4 * i + 2] << 8) | (unsigned long)p[4 * i + 3];
    for (i = 16; i < 64; ++i) {
        unsigned long s0 = OG_ROR(w[i - 15], 7) ^ OG_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned long s1 = OG_ROR(w[i - 2], 17) ^ OG_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = (w[i - 16] + s0 + w[i - 7] + s1) & 0xffffffffUL;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; ++i) {
        t1 = (hh + (OG_ROR(e, 6) ^ OG_ROR(e, 11) ^ OG_ROR(e, 25)) +
              ((e & f) ^ (~e & g & 0xffffffffUL)) + og_sha_k[i] + w[i]) & 0xffffffffUL;
        t2 = ((OG_ROR(a, 2) ^ OG_ROR(a, 13) ^ OG_ROR(a, 22)) +
              ((a & b) ^ (a & c) ^ (b & c))) & 0xffffffffUL;
        hh = g; g = f; f = e;
        e = (d + t1) & 0xffffffffUL;
        d = c; c = b; b = a;
        a = (t1 + t2) & 0xffffffffUL;
    }
    h[0] = (h[0] + a) & 0xffffffffUL;  h[1] = (h[1] + b) & 0xffffffffUL;
    h[2] = (h[2] + c) & 0xffffffffUL;  h[3] = (h[3] + d) & 0xffffffffUL;
    h[4] = (h[4] + e) & 0xffffffffUL;  h[5] = (h[5] + f) & 0xffffffffUL;
    h[6] = (h[6] + g) & 0xffffffffUL;  h[7] = (h[7] + hh) & 0xffffffffUL;
}

void og_sha256(const unsigned char *data, size_t len, unsigned char out[32])
{
    unsigned long h[8] = {
        0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
        0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
    };
    unsigned char tail[128];
    size_t full = len / 64 * 64, rest = len - full, n, i;
    unsigned long long bits = (unsigned long long)len * 8u;

    for (i = 0; i < full; i += 64) og_sha_block(h, data + i);
    /* what is left, the 0x80 marker and the length: one block or two */
    memset(tail, 0, sizeof tail);
    if (rest) memcpy(tail, data + full, rest);
    tail[rest] = 0x80;
    n = (rest < 56) ? 64 : 128;
    for (i = 0; i < 8; ++i)
        tail[n - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (i = 0; i < n; i += 64) og_sha_block(h, tail + i);
    for (i = 0; i < 8; ++i) {
        out[4 * i]     = (unsigned char)(h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(h[i] >> 8);
        out[4 * i + 3] = (unsigned char)h[i];
    }
}

/* Every file SORP reads, with the SHA-256 of the DOS release's copy. */
static const struct { const char *name, *sha; } og_gamedata[] = {
    { "ADLIB.SFX",   "4085577ADE69D681F52D0CDA8E48128797EBFD2ED7D98A52654757B2B12831B8" },
    { "CONTOUR0.XX", "B7E16A8639D149D8F93F2F1713BFA7F6A5A6F3679C5AF90CE0FD3815DBFBCBDC" },
    { "CONTOUR1.XX", "3B278B85780C4731D4E284514D19DC5658BEDE063AEEFBE911CF6B9A20648A3D" },
    { "CONTOUR2.XX", "D5BFE59789B7E8B4FFB0A7E69C6CC3ED55F8269EFAA43476400452398F3D3E60" },
    { "CONTOUR3.XX", "19BF1970A4E746FE0403D7A4EF7E2B21D02756769DE8192AF570874E3FA6FAC2" },
    { "CONTOUR4.XX", "F4C91A0620584BB814EA66A69697483546404054D852C78E8079DDFF0F12A52F" },
    { "CONTOUR5.XX", "7E810C8186B69E05555454666507E2757AD506136C42F85E555D24ED3F8A4CA5" },
    { "CONTOUR6.XX", "FDD5BAE3A89049C1609E125AE2BC38D51B44F1AC16ED091EF1424158934389D7" },
    { "CONTOUR7.XX", "68182F945F77A17AE88F1B18259DA972CEF6C4CF8E5C4EAFA29ECDABEC665EF4" },
    { "EEND.PCG",    "867AACB73864EC56A471F55AEAA6052E9E9167FD3A4551BEBE01D6A6CEDC7A23" },
    { "EGA.EXE",     "0000C0595EA4550E7FD56F1F8B4760C89FE0B66F708675C7E3DA6D93C2FA1260" },
    { "EGAME.PCG",   "3DF9751E2343942D7783CE7A4705EE3C452B3DD6FD3B4D95BB426C9FD9F34AD1" },
    { "ESHADOW.PCG", "636ECF497D86440A7E92E6D2CC63BFC99254B52FCE652AF5DCE43FB492F5EB94" },
    { "ESHOP.PCG",   "7FD8CF3B5F3707E32D6DCA00F6895DCA5D094D1E9120250F91575B722E2FD576" },
    { "ETITLE.PCG",  "1BBC50F749A67C0F0822B967B6DEB37A661CF1E6893B6E89AC56F585383C0855" },
    { "ETRUCK.PCG",  "01C14614D5C8DCF99B0C63669A09118291CD5EEFC7BFA356C49D96AC0959A46D" },
    { "GAME.EXE",    "2F6C144A4782DDEA20F3961E56A7F185694E8D409128392144AF8203E0339214" },
    { "IRONFONT.XX", "7B4C71615C2D3702939F0658E53DD78DD69BE5631B6E8DEBDD90C7307AF50691" },
    { "LOADB.XX",    "38A14452597BAB1B7D4FBAFEC96BEEE6F9C2564717E5924F8F702B078A1FDD2C" },
    { "POLY.BIN",    "9A3685C7A4DA06AE64A6E32A7AD084228D9634536C31DC839BD3978ECC558785" },
    { "QUAD.BIN",    "3819787C7B9806DED3B16835A1C95C9D2F533178991FE6D69FA6E63AE4FBCE58" },
    { "ROADFONT.XX", "401E3D26F2FD53FEBA0577540740262647817115C2BA0F3F96DEAA33006059FB" },
    { "TITLES.XX",   "A40091DA6C1322CC76AF42BF73156C7A393EF714AFD7CE506F7CF0BED256CC65" },
    { "TRACK0.XX",   "C9D7620E981664F521D303D7484067BAD53FD5EF7AC91ADB3B6C36C979C0DC60" },
    { "TRACK1.XX",   "660A067F22FF29AB9A117B0B31E794E23E8B5B7FF00533D4FE22D96FC7B9CEBA" },
    { "TRACK2.XX",   "4770B25230A6D57FC1A7A76AE090D5027E0E2AAD0756E603220569116D3C84EE" },
    { "TRACK3.XX",   "CFB98D2636DC2E6611F64E2F460852022152B17E7C7AC617D934CA52E627EEA4" },
    { "TRACK4.XX",   "BF5CE7ACFE6320DBE6FA2A5F5A5F15CD3A62D262DD027F86454C619431E6F15F" },
    { "TRACK5.XX",   "306FA5E150DFD64E82D672AF03125E16A25E5D4FBFA327F8EA5D3496AC211B42" },
    { "TRACK6.XX",   "79043737C8317A4EE4D21534EF8862BEED592EE461FB191BBFC343D5DFA09F03" },
    { "TRACK7.XX",   "17F9BEA5D8928CC9E0E267D13BF10FAD37F94E1865411BD6715D7EDF8C65EFAF" }
};

int og_gamedata_verify(const char *dir, og_file_check_fn check, void *user)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    int failed = 0;

    for (i = 0; i < sizeof og_gamedata / sizeof og_gamedata[0]; ++i) {
        unsigned char *buf = NULL, sum[32];
        size_t len = 0;
        char path[1024], got[65];
        const char *why = NULL;
        long bytes;
        int k;

        if (og_resolve(dir, og_gamedata[i].name, path, sizeof path) != OG_OK)
            why = "missing";
        else if (og_read_file(path, &buf, &len) != OG_OK)
            why = "unreadable";
        else {
            og_sha256(buf, len, sum);
            for (k = 0; k < 32; ++k) {
                got[2 * k]     = hex[sum[k] >> 4];
                got[2 * k + 1] = hex[sum[k] & 15];
            }
            got[64] = '\0';
            if (strcmp(got, og_gamedata[i].sha) != 0)
                why = "SHA-256 does not match";
        }
        bytes = buf ? (long)len : -1L;
        free(buf);
        if (why) ++failed;
        if (check) check(og_gamedata[i].name, bytes, why, user);
    }
    return failed;
}

static unsigned short og_le16(const unsigned char *p)
{
    return (unsigned short)(p[0] | ((unsigned)p[1] << 8));
}

static unsigned long og_be32_(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] <<  8) |  (unsigned long)p[3];
}

/* ------------------------------------------------------------------ */
/* Tracks                                                              */
/* ------------------------------------------------------------------ */

int og_track_load(const char *dir, int index, og_track_art *out)
{
    char name[32];
    unsigned char *buf = NULL;
    size_t len = 0, piclen;
    int rc;

    if (!out || index < 0 || index >= OG_NUM_TRACKS) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    snprintf(name, sizeof name, "TRACK%d.XX", index);
    rc = og_slurp(dir, name, &buf, &len);
    if (rc != OG_OK) return rc;

    if (len < 8) { free(buf); return OG_E_SHORT; }

    rc = og_xx_decode(buf, len, 0, &out->pic);
    if (rc != OG_OK) { free(buf); return rc; }

    /* The picture chunk announces its own size; anything past it is the
     * foreground plane. */
    piclen = (size_t)og_be32_(buf);
    og_load_note(name, 0, (long)(piclen + 4 <= len ? piclen + 4 : len),
                 "Loaded the track picture, %dx%d", out->pic.w, out->pic.h);
    if (piclen + 8 <= len) {
        unsigned char *plane = (unsigned char *)malloc(OG_XX_PLANE_BYTES);
        if (plane) {
            size_t pos = piclen + 4;     /* skip the tail's own length */
            if (og_xx_decode_plane(buf, len, &pos, plane) == OG_OK) {
                out->fg = (unsigned char *)calloc(OG_SCREEN_W * OG_SCREEN_H, 1);
                if (out->fg) {
                    int x, y;
                    for (y = 0; y < OG_SCREEN_H; ++y) {
                        const unsigned char *row =
                            plane + (size_t)y * OG_XX_BYTES_PER_ROW;
                        for (x = 0; x < OG_SCREEN_W; ++x) {
                            unsigned bit = (row[x >> 3] >> (7 - (x & 7))) & 1u;
                            out->fg[(size_t)y * OG_SCREEN_W + x] =
                                (unsigned char)bit;
                        }
                    }
                    out->has_fg = 1;
                    og_load_note(name, (long)(piclen + 4), (long)(pos - (piclen + 4)),
                                 "Loaded the foreground mask, %dx%d, one bitplane",
                                 OG_SCREEN_W, OG_SCREEN_H);
                }
            }
            free(plane);
        }
    }

    free(buf);
    return OG_OK;
}

void og_track_free(og_track_art *t)
{
    if (!t) return;
    og_image_free(&t->pic);
    free(t->fg);
    t->fg = NULL;
    t->has_fg = 0;
}

/* ------------------------------------------------------------------ */
/* Contours                                                            */
/* ------------------------------------------------------------------ */

int og_contour_load(const char *dir, int index, og_contour *out)
{
    char name[32];
    unsigned char *buf = NULL;
    size_t len = 0;
    int rc, i, n = OG_CONTOUR_W * OG_CONTOUR_H;

    if (!out || index < 0 || index >= OG_NUM_TRACKS) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    snprintf(name, sizeof name, "CONTOUR%d.XX", index);
    rc = og_slurp(dir, name, &buf, &len);
    if (rc != OG_OK) return rc;

    if (len < (size_t)n * 2) { free(buf); return OG_E_SHORT; }

    /* Little-endian, matching the x86 word loads in GAME.EXE. */
    for (i = 0; i < n; ++i)
        out->cell[i] = (unsigned short)(buf[i * 2] |
                                       ((unsigned)buf[i * 2 + 1] << 8));
    out->loaded = 1;
    og_load_note(name, 0, (long)n * 2,
                 "Loaded %d contour cells, %dx%d, each a POLY.BIN or QUAD.BIN offset",
                 n, OG_CONTOUR_W, OG_CONTOUR_H);

    free(buf);
    return OG_OK;
}


/* ------------------------------------------------------------------ */
/* Surface records: POLY.BIN and QUAD.BIN                              */
/* ------------------------------------------------------------------ */

int og_surface_load(const char *dir, og_surface *s)
{
    int rc;

    if (!s) return OG_E_ARG;
    memset(s, 0, sizeof *s);

    rc = og_slurp(dir, "POLY.BIN", &s->poly, &s->poly_len);
    if (rc != OG_OK) return rc;

    rc = og_slurp(dir, "QUAD.BIN", &s->quad, &s->quad_len);
    if (rc != OG_OK) { free(s->poly); s->poly = NULL; return rc; }

    s->loaded = 1;
    og_load_note("POLY.BIN", -1, (long)s->poly_len,
                 "Loaded %lu Polygon Gradient Records (%d bytes each) in %lu bytes",
                 (unsigned long)(s->poly_len / OG_POLY_REC), OG_POLY_REC,
                 (unsigned long)s->poly_len);
    og_load_note("QUAD.BIN", -1, (long)s->quad_len,
                 "Loaded %lu Quad Corner Records (%d bytes each) in %lu bytes",
                 (unsigned long)(s->quad_len / OG_QUAD_REC), OG_QUAD_REC,
                 (unsigned long)s->quad_len);
    return OG_OK;
}

void og_surface_free(og_surface *s)
{
    if (!s) return;
    free(s->poly);
    free(s->quad);
    s->poly = s->quad = NULL;
    s->loaded = 0;
}

long og_surface_resolve(const og_surface *s, unsigned short cell)
{
    unsigned long off;

    if (!s || !s->loaded) return -1;

    if (cell & 1u) {
        /* indirect: mask the tag, index QUAD, take the word at +2 */
        off = (unsigned long)(cell & ~1u);
        if (off + 4 > s->quad_len) return -1;
        off = (unsigned long)(s->quad[off + 2] |
                             ((unsigned)s->quad[off + 3] << 8));
    } else {
        off = cell;
    }

    if (off + OG_POLY_REC > s->poly_len) return -1;
    return (long)off;
}

int og_surface_gradient(const og_surface *s, unsigned short cell,
                        int *gx, int *gy)
{
    long off = og_surface_resolve(s, cell);
    if (off < 0) return OG_E_RANGE;
    if (gx) *gx = (int)(signed char)s->poly[off + 2];
    if (gy) *gy = (int)(signed char)s->poly[off + 3];
    return OG_OK;
}

/* Resolve a cell using a specific QUAD corner slot (0,2,4,6) and read
 * one POLY gradient byte (+2 for the X pass, +3 for the Y pass). */
static int og_grad(const og_surface *s, unsigned short cell,
                   int quad_slot, int poly_byte, int *out)
{
    unsigned long off;

    if (!s || !s->loaded) return OG_E_ARG;

    if (cell & 1u) {
        off = (unsigned long)(cell & ~1u) + (unsigned long)quad_slot;
        if (off + 2 > s->quad_len) return OG_E_RANGE;
        off = (unsigned long)(s->quad[off] | ((unsigned)s->quad[off+1] << 8));
    } else {
        off = cell;
    }
    if (off + OG_POLY_REC > s->poly_len) return OG_E_RANGE;
    *out = (int)(signed char)s->poly[off + poly_byte];
    return OG_OK;
}

/* Mirrors the integrator at GAME.EXE image 0x1eab.
 *
 * Per cell the routine stores the accumulator's HIGH BYTE first, then
 * adds two X gradients: the current cell through QUAD slot +2 and the
 * NEXT cell through slot +4, each scaled by 128 (shl 8, sar 1).
 *
 * At the end of a row it does `pop dx`, discarding everything the row
 * accumulated and restoring the value saved before it -- so X gradients
 * never leak between rows.  The row datum then steps by two Y gradients
 * read from POLY byte +3, BOTH subtracted (image 0x1f2a and 0x1f50):
 * the row's first cell via QUAD slot +0 and the next row's first cell
 * via slot +6.  Adding the second instead makes the field drift
 * steadily in Y. */
int og_contour_heights(const og_contour *c, const og_surface *s,
                       unsigned char *out)
{
    int x, y;
    long acc = 0x8000L;

    if (!c || !c->loaded || !s || !s->loaded || !out) return OG_E_ARG;

    for (y = 0; y < OG_CONTOUR_H; ++y) {
        long dx = acc;                       /* the pushed row datum */
        int g;

        for (x = 0; x < OG_CONTOUR_W; ++x) {
            int i = y * OG_CONTOUR_W + x;

            out[i] = (unsigned char)((dx >> 8) & 0xff);   /* store first */

            if (og_grad(s, c->cell[i], 2, 2, &g) == OG_OK)
                dx += (long)g * 128L;
            if (i + 1 < OG_CONTOUR_W * OG_CONTOUR_H &&
                og_grad(s, c->cell[i + 1], 4, 2, &g) == OG_OK)
                dx += (long)g * 128L;

            if (dx < 0)       dx = 0;
            if (dx > 0xffffL) dx = 0xffffL;
        }

        /* pop dx: the row's X accumulation is thrown away */
        dx = acc;
        if (og_grad(s, c->cell[y * OG_CONTOUR_W], 0, 3, &g) == OG_OK)
            dx -= (long)g * 128L;
        if (y + 1 < OG_CONTOUR_H &&
            og_grad(s, c->cell[(y + 1) * OG_CONTOUR_W], 6, 3, &g) == OG_OK)
            dx -= (long)g * 128L;      /* sub dx,ax at image 0x1f50 */

        if (dx < 0)       dx = 0;
        if (dx > 0xffffL) dx = 0xffffL;
        acc = dx;
    }
    return OG_OK;
}





/* ------------------------------------------------------------------ */
/* Vertical physics tables (see og_data.h)                             */
/* ------------------------------------------------------------------ */

int og_physics_load(const char *dir, og_physics *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, b;
    const unsigned char *img;
    int rc, i;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr;
    b = (size_t)OG_GAME_DS_IMG;
    if (b + OG_AIR_DS + 8 > imglen) { free(raw); return OG_E_SHORT; }

    for (i = 0; i < 6; ++i)
        out->shocks[i] = og_le16(img + b + OG_SHOCKS_DS + i * 2) / 65536.0f;
    for (i = 0; i < 18; ++i)
        out->ai[i] = og_le16(img + b + OG_AI_BOUNCE_DS + i * 2) / 65536.0f;
    for (i = 0; i < 8; ++i)
        out->air[i] = (float)img[b + OG_AIR_DS + i];     /* whole units */
    out->loaded = 1;
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_SHOCKS_DS), 6 * 2,
                 "Loaded the shock physics table: bounce by Shocks upgrade, 6 levels");
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_AI_BOUNCE_DS), 18 * 2,
                 "Loaded the computer trucks' shock physics table, 18 levels");
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_AIR_DS), 8,
                 "Loaded the airborne height thresholds, one per track");
    free(raw);
    return OG_OK;
}


/* ------------------------------------------------------------------ */
/* Truck shadows (see og_data.h)                                       */
/* ------------------------------------------------------------------ */

int og_shadows_load(const char *dir, og_shadows *out)
{
    unsigned char *exe = NULL, *pcg = NULL;
    size_t exe_len = 0, pcg_len = 0, pos = 0;
    int rc, i, y, x;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "EGA.EXE", &exe, &exe_len);
    if (rc != OG_OK) return rc;
    rc = og_slurp(dir, "ESHADOW.PCG", &pcg, &pcg_len);
    if (rc != OG_OK) { free(exe); return rc; }

    for (i = 0; i < OG_SHADOWS; ++i) {
        size_t e = OG_SPRTAB_OFFSET + (size_t)(OG_SHADOW_FIRST + i) * 8;
        int rows;
        if (e + 8 > exe_len) { rc = OG_E_SHORT; break; }
        rows = exe[e + 4];
        out->rows[i] = rows;
        out->top[i]  = exe[e + 5];
        out->xoff[i] = exe[e + 7];
        if (pos + (size_t)rows * 3 > pcg_len) { rc = OG_E_SHORT; break; }
        out->mask[i] = (unsigned char *)calloc((size_t)rows * 24, 1);
        if (!out->mask[i]) { rc = OG_E_NOMEM; break; }
        for (y = 0; y < rows; ++y)
            for (x = 0; x < 24; ++x)                 /* MSB first */
                out->mask[i][y * 24 + x] =
                    (unsigned char)((pcg[pos + (size_t)y * 3 + (size_t)(x >> 3)] >> (7 - (x & 7))) & 1);
        pos += (size_t)rows * 3;
    }
    free(exe); free(pcg);
    if (rc != OG_OK) { og_shadows_free(out); return rc; }
    out->loaded = 1;
    og_load_note("EGA.EXE", (long)OG_SPRTAB_OFFSET + (long)OG_SHADOW_FIRST * 8,
                 (long)OG_SHADOWS * 8,
                 "Loaded ESHADOW.PCG's sprite table: %d entries, from sprite %d",
                 OG_SHADOWS, OG_SHADOW_FIRST);
    og_load_note("ESHADOW.PCG", 0, (long)pos,
                 "Loaded %d truck shadows, 24 px wide, one bitplane", OG_SHADOWS);
    return OG_OK;
}


int og_pcg_bank_load(const char *dir, const char *file, int first, int count,
                     int complement, og_bank *out)
{
    /* bit 1 of the flag reverses the plane order (see ETITLE below) */
    const int reverse_planes = (complement & 2) != 0;
    unsigned char *exe = NULL, *pcg = NULL;
    size_t exe_len = 0, pcg_len = 0, pos = 0;
    int rc, i, y, x, p;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "EGA.EXE", &exe, &exe_len);
    if (rc != OG_OK) return rc;
    rc = og_slurp(dir, file, &pcg, &pcg_len);
    if (rc != OG_OK) { free(exe); return rc; }

    out->frames   = (og_image *)calloc((size_t)count, sizeof(og_image));
    out->frame_h  = (int *)calloc((size_t)count, sizeof(int));
    out->frame_ox = (int *)calloc((size_t)count, sizeof(int));
    out->frame_oy = (int *)calloc((size_t)count, sizeof(int));
    if (!out->frames || !out->frame_h || !out->frame_ox || !out->frame_oy) {
        free(exe); free(pcg); og_bank_free(out); return OG_E_NOMEM;
    }
    out->nframes = count;
    out->key = complement ? 15 : 0;    /* the clear index follows */

    for (i = 0; i < count && rc == OG_OK; ++i) {
        size_t e = OG_SPRTAB_OFFSET + (size_t)(first + i) * 8;
        int rows, top, w, bw;
        if (e + 8 > exe_len) { rc = OG_E_SHORT; break; }
        rows = exe[e + 4]; top = exe[e + 5]; w = exe[e + 6]; bw = w / 8;
        if (pos + (size_t)rows * bw * 4 > pcg_len || bw <= 0) { rc = OG_E_SHORT; break; }
        if (og_image_init(&out->frames[i], w, rows) != OG_OK) { rc = OG_E_NOMEM; break; }
        for (y = 0; y < rows; ++y)
            for (p = 0; p < 4; ++p)
                for (x = 0; x < w; ++x) {
                    unsigned b = pcg[pos + ((size_t)y * 4 + (size_t)p) * bw + (size_t)(x >> 3)];
                    int bit = reverse_planes ? (3 - p) : p;
                    out->frames[i].px[y * w + x] |=
                        (unsigned char)(((b >> (7 - (x & 7))) & 1u) << bit);
                }
        /* egame is stored complemented, as etruck is -- without it the cash
         * bag is brown instead of gold.  eshop and eend are stored plain. */
        if (complement & 1)
            for (y = 0; y < rows * w; ++y)
                out->frames[i].px[y] = (unsigned char)(~out->frames[i].px[y] & 15);
        pos += (size_t)rows * bw * 4;
        out->frame_h[i]  = rows;
        out->frame_ox[i] = exe[e + 7];            /* +7: x offset in box */
        out->frame_oy[i] = OG_TRUCK_BOX_ROWS / 2 - top;
        if (w > out->w)    out->w = w;
        if (rows > out->h) out->h = rows;
    }
    free(exe); free(pcg);
    if (rc != OG_OK) { og_bank_free(out); return rc; }
    og_load_note("EGA.EXE", (long)OG_SPRTAB_OFFSET + (long)first * 8, (long)count * 8,
                 "Loaded %s's sprite table: %d entries, from sprite %d", file, count, first);
    og_load_note(file, 0, (long)pos,
                 "Loaded %d sprites, up to %dx%d, 4 bitplanes", count, out->w, out->h);
    return OG_OK;
}

int og_egame_load(const char *dir, og_bank *out)
{
    return og_pcg_bank_load(dir, "EGAME.PCG", OG_EGAME_FIRST, OG_EGAME_COUNT, 1, out);
}

int og_title_bank_load(const char *dir, og_bank *out)
{
    /* ETITLE is COMPLEMENTED, like EGAME and ETRUCK -- not stored plain
     * and not plane-reversed.  Taken plain, the Virgin box comes out green
     * and the Off Road logo tan; complemented it is index 7 red with 14
     * white for the script, and the logo lands on 5 and 14, the yellow and
     * white the raw dump shows. */
    return og_pcg_bank_load(dir, "ETITLE.PCG", OG_ETITLE_FIRST,
                            OG_ETITLE_COUNT, 1, out);
}

int og_shop_load(const char *dir, og_bank *out)
{
    return og_pcg_bank_load(dir, "ESHOP.PCG", OG_ESHOP_FIRST, OG_ESHOP_COUNT, 0, out);
}

int og_end_load(const char *dir, og_bank *out)
{
    /* NOT complemented -- complementing it turns every colour wrong. */
    return og_pcg_bank_load(dir, "EEND.PCG", OG_EEND_FIRST, OG_EEND_COUNT, 0, out);
}

void og_shadows_free(og_shadows *s)
{
    int i;
    if (!s) return;
    for (i = 0; i < OG_SHADOWS; ++i) { free(s->mask[i]); s->mask[i] = NULL; }
    s->loaded = 0;
}



int og_spawns_load(const char *dir, int track, og_spawns *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, D, t;
    const unsigned char *img;
    int rc, i, maxi = 0;

    if (!out || track < 0 || track >= OG_NUM_TRACKS) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr; D = (size_t)OG_GAME_DS_IMG;

    if (D + OG_SPAWNPTR_DS + (size_t)track * 2 + 2 > imglen) { free(raw); return OG_E_SHORT; }
    for (i = 0; i < 16; ++i) {
        out->sel[i] = img[D + OG_SPAWNSEL_DS + (size_t)track * 16 + (size_t)i];
        if (out->sel[i] > maxi) maxi = out->sel[i];
    }
    t = D + og_le16(img + D + OG_SPAWNPTR_DS + (size_t)track * 2);
    out->n = maxi + 1;
    if (out->n > OG_PICKUP_MAX) out->n = OG_PICKUP_MAX;
    if (t + (size_t)out->n * 4 > imglen) { free(raw); return OG_E_SHORT; }
    for (i = 0; i < out->n; ++i) {
        out->x[i] = (int)og_le16(img + t + (size_t)i * 4);
        out->y[i] = (int)og_le16(img + t + (size_t)i * 4 + 2);
    }
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_SPAWNSEL_DS + (size_t)track * 16), 16,
                 "Loaded track %d's pickup selection table", track);
    og_load_note("GAME.EXE", (long)(hdr + t), (long)out->n * 4,
                 "Loaded track %d's pickup spawn points: %d (X, Y) pairs",
                 track, out->n);
    free(raw);
    out->loaded = 1;
    return OG_OK;
}



/* Parse a descriptor: its tag and first sprite from the init pairs, the
 * frame it rests on from the script's first op 32, and its animation from
 * the first op 31 -- which may name its own base sprite. */
static int og_anim_parse(const unsigned char *img, size_t imglen, unsigned desc,
                         og_fx_anim *a, int *idle_frame,
                         unsigned *shape_ptr, unsigned *mask)
{
    size_t D = (size_t)OG_GAME_DS_IMG, o = D + desc, list;
    int guard, base = -1, idle = 0, found = 0;

    memset(a, 0, sizeof *a);
    a->sprite = -1;
    while (o + 4 <= imglen) {                       /* init pairs */
        unsigned v = og_le16(img + o);
        if (v == 0x007a) break;
        if (v & 0x8000) return OG_E_MAGIC;
        if (v & 0x4000) {
            unsigned val = og_le16(img + o + 2);
            if ((v & 0x3fff) == 0x5a) base = (int)val;
            if ((v & 0x3fff) == 0x04) { a->tag[0] = (char)(val >> 8); a->tag[1] = (char)(val & 0xff); }
            o += 4;
        } else o += 3;
    }
    o += 2;
    for (guard = 0; guard < 48 && o + 2 <= imglen; ++guard) {
        unsigned op = og_le16(img + o);
        int n = (int)(op & 7), id = (int)((op & 0x7ff8) >> 3);
        if (id == 32 && n >= 1 && !idle) idle = (int)og_le16(img + o + 2);
        if (id == 25 && n >= 2 && shape_ptr) *shape_ptr = og_le16(img + o + 4);
        if (id == 26 && n >= 1 && mask && !*mask) *mask = og_le16(img + o + 2);
        if (id == 31 && n >= 1) {                   /* the animation */
            int arg1 = (int)(short)og_le16(img + o + 2);
            if (n >= 2 && !(og_le16(img + o + 4) & 0x8000))
                base = (int)og_le16(img + o + 4);   /* its own base */
            list = (size_t)((long)o + arg1);
            for (a->n = 0; a->n < OG_FX_MAXFRAMES && list + 2 <= imglen; ++a->n) {
                unsigned v = og_le16(img + list);
                if (v & 0x8000) break;
                a->frame[a->n] = (unsigned char)(v & 0xff);
                a->dur[a->n]   = (unsigned char)(v >> 8);
                list += 2;
            }
            found = 1;
            break;
        }
        if (id == 43) break;
        o += 2 * (size_t)(n + 1);
    }
    a->sprite = base;
    if (idle_frame) *idle_frame = idle;
    return (base >= 0 && (found ? a->n > 0 : 1)) ? OG_OK : OG_E_MAGIC;
}


int og_truck_shapes_load(const char *dir, og_shape out[16])
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, D;
    const unsigned char *img;
    int rc, i, q;

    if (!out) return OG_E_ARG;
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr; D = (size_t)OG_GAME_DS_IMG;
    if (D + OG_TRUCKBOX_DS + 16 * 8 > imglen) { free(raw); return OG_E_SHORT; }
    for (i = 0; i < 16; ++i) {
        short *v = &out[i].x0;
        for (q = 0; q < 4; ++q)
            v[q] = (short)og_le16(img + D + OG_TRUCKBOX_DS + (size_t)i * 8 + (size_t)q * 2);
    }
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_TRUCKBOX_DS), 16 * 8,
                 "Loaded the truck collision boxes, one per 16 headings");
    free(raw);
    return OG_OK;
}

int og_collide(int ax, int ay, const og_shape *a,
               int bx, int by, const og_shape *b)
{
    int ax0, ax1, ay0, ay1, bx0, bx1, by0, by1;
    if (!a || !b) return 0;
    ax0 = (ax + a->x0) & ~3; ax1 = ax0 + 4 * a->xn;
    ay0 = (ay + a->y0) & ~3; ay1 = ay0 + 4 * a->yn;
    bx0 = (bx + b->x0) & ~3; bx1 = bx0 + 4 * b->xn;
    by0 = (by + b->y0) & ~3; by1 = by0 + 4 * b->yn;
    return (ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1);
}



int og_perf_load(const char *dir, og_perf *out)
{
    static const struct { unsigned ds; size_t off; const char *what; } tab[] = {
        { OG_UPG_SHOCK_DS, offsetof(og_perf, shock), "shocks" },
        { OG_UPG_STEER_DS, offsetof(og_perf, steer), "steering" },
        { OG_UPG_DRAG_DS,  offsetof(og_perf, drag),  "drag" },
        { OG_UPG_SLIP_DS,  offsetof(og_perf, slip),  "slip" },
        { OG_UPG_GRIP_DS,  offsetof(og_perf, grip),  "grip" },
        { OG_UPG_ACCEL_DS, offsetof(og_perf, accel), "acceleration" },
        { OG_UPG_TOP_DS,   offsetof(og_perf, top),   "top speed" }
    };
    static const struct { unsigned ds; size_t off; const char *what; } ctab[] = {
        { OG_CPU_SHOCK_DS, offsetof(og_perf, c_shock), "shocks" },
        { OG_CPU_STEER_DS, offsetof(og_perf, c_steer), "steering" },
        { OG_CPU_DRAG_DS,  offsetof(og_perf, c_drag),  "drag" },
        { OG_CPU_SLIP_DS,  offsetof(og_perf, c_slip),  "slip" },
        { OG_CPU_GRIP_DS,  offsetof(og_perf, c_grip),  "grip" },
        { OG_CPU_ACCEL_DS, offsetof(og_perf, c_accel), "acceleration" },
        { OG_CPU_TOP_DS,   offsetof(og_perf, c_top),   "top speed" }
    };
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, D;
    const unsigned char *img;
    int rc, i, k;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr; D = (size_t)OG_GAME_DS_IMG;
    if (D + OG_UPG_TOP_DS + OG_UPG_LEVELS * 2 > imglen) { free(raw); return OG_E_SHORT; }
    for (i = 0; i < (int)(sizeof tab / sizeof tab[0]); ++i) {
        int *dst = (int *)((char *)out + tab[i].off);
        for (k = 0; k < OG_UPG_LEVELS; ++k)
            dst[k] = (int)og_le16(img + D + tab[i].ds + (size_t)k * 2);
        og_load_note("GAME.EXE", OG_DS_FILE(hdr, tab[i].ds), OG_UPG_LEVELS * 2,
                     "Loaded the player trucks' %s table, %d upgrade levels",
                     tab[i].what, OG_UPG_LEVELS);
    }
    if (D + OG_RUBBER_DS + 16 > imglen) { free(raw); return OG_E_SHORT; }
    for (i = 0; i < (int)(sizeof ctab / sizeof ctab[0]); ++i) {
        int *dst = (int *)((char *)out + ctab[i].off);
        for (k = 0; k < OG_CPU_LEVELS; ++k)
            dst[k] = (int)og_le16(img + D + ctab[i].ds + (size_t)k * 2);
        og_load_note("GAME.EXE", OG_DS_FILE(hdr, ctab[i].ds), OG_CPU_LEVELS * 2,
                     "Loaded the computer trucks' %s table, %d difficulty levels",
                     ctab[i].what, OG_CPU_LEVELS);
    }
    for (k = 0; k < 16; ++k)
        out->rubber[k] = (signed char)img[D + OG_RUBBER_DS + (size_t)k];
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_RUBBER_DS), 16,
                 "Loaded the rubber band table: a computer truck's level by its gap to the leader");
    free(raw);
    out->loaded = 1;
    return OG_OK;
}


int og_rubber_level(const og_perf *p, int base, int laps, int prog,
                    int leader_laps, int leader_prog)
{
    int d;
    if (!p) return base;
    if (laps < leader_laps)      d = 5;          /* a lap down: +5    */
    else if (laps > leader_laps) d = -5;         /* a lap up:   -5    */
    else {
        int q = (prog - leader_prog) >> 4;       /* sar al,1 four times */
        if (q < 0)  q = 0;
        if (q > 15) q = 15;
        d = p->rubber[q];
    }
    d += base;
    if (d < 0) d = 0;
    if (d > 0x11) d = 0x11;                      /* clamped to 0..17  */
    return d;
}

int og_race_start_load(const char *dir, int layout, og_race_start *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, D, slot;
    const unsigned char *img;
    int rc, i;

    if (!out || layout < 0 || layout >= OG_NUM_LAYOUTS) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr; D = (size_t)OG_GAME_DS_IMG;
    if (D + OG_START_SLOT_DS + 32 > imglen) { free(raw); return OG_E_SHORT; }

    out->x       = (int)og_le16(img + D + OG_START_X_DS    + (size_t)layout * 2);
    out->y       = (int)og_le16(img + D + OG_START_Y_DS    + (size_t)layout * 2);
    out->heading = (int)(short)og_le16(img + D + OG_START_HEAD_DS + (size_t)layout * 2);
    /* the second set of slots is used when the start heading is negative */
    slot = D + OG_START_SLOT_DS + (out->heading < 0 ? 0x10u : 0u);
    for (i = 0; i < 4; ++i) {
        out->dx[i] = (int)(short)og_le16(img + slot + (size_t)i * 4);
        out->dy[i] = (int)(short)og_le16(img + slot + (size_t)i * 4 + 2);
    }
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_START_X_DS + (size_t)layout * 2), 2,
                 "Loaded layout %d's start line X", layout);
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_START_Y_DS + (size_t)layout * 2), 2,
                 "Loaded layout %d's start line Y", layout);
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_START_HEAD_DS + (size_t)layout * 2), 2,
                 "Loaded layout %d's start heading", layout);
    og_load_note("GAME.EXE", (long)(hdr + slot), 16,
                 "Loaded the starting grid: 4 slots, the %s set",
                 out->heading < 0 ? "second" : "first");
    free(raw);
    return OG_OK;
}

int og_scenery_load(const char *dir, int layout, og_scenery *out)
{
    static const unsigned kind_ptr[4] = { 0x31c8, 0x31cc, 0x31d0, 0x31d4 };
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, D, t;
    const unsigned char *img;
    int rc, k;

    if (!out || layout < 0 || layout >= OG_NUM_LAYOUTS) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr; D = (size_t)OG_GAME_DS_IMG;

    for (k = 0; k < 4; ++k) {
        int idle = 0; unsigned sh = 0, mk = 0; int q;
        if (og_anim_parse(img, imglen, og_le16(img + D + kind_ptr[k]),
                          &out->hit[k], &idle, &sh, &mk) != OG_OK)
            out->hit[k].n = 0;
        out->mask[k] = (unsigned short)mk;
        if (sh && D + sh + 8 <= imglen) {
            short *v = &out->shape[k].x0;
            for (q = 0; q < 4; ++q) v[q] = (short)og_le16(img + D + sh + (size_t)q * 2);
        } else {                       /* no op 25: bunting leaves the
                                        * field unset, so give it its own
                                        * drawn size, 48 x 8 px */
            out->shape[k].x0 = -2; out->shape[k].xn = 4;
            out->shape[k].y0 = -2; out->shape[k].yn = 2;
        }
        out->base[k] = out->hit[k].sprite < 0 ? 0 : out->hit[k].sprite;
        out->idle[k] = idle;
    }
    {   /* the flagman */
        int idle = 0;
        out->fm_x = (int)og_le16(img + D + OG_FLAGMAN_DS + (size_t)(layout & 7) * 4);
        out->fm_y = (int)og_le16(img + D + OG_FLAGMAN_DS + (size_t)(layout & 7) * 4 + 2);
        if (og_anim_parse(img, imglen, og_le16(img + D + 0x31d8),
                          &out->fm_wave, &idle, NULL, NULL) != OG_OK)
            out->fm_wave.n = 0;
        out->fm_idle = 0x1d6;                 /* EGAME 26, frame 0 */
        /* the checkered flag: the same frames off sprite 0x1be */
        out->fm_chequer = out->fm_wave;
        out->fm_chequer.sprite = 0x1be;
    }

    t = D + og_le16(img + D + OG_SCENERY_DS + (size_t)layout * 2);
    while (out->n < OG_SCENERY_MAX && t + 8 <= imglen) {
        unsigned a = og_le16(img + t);
        int i = out->n;
        if (a == 0) break;
        out->type[i] = (unsigned char)(((a & 0xc000) == 0xc000) ? 3 :
                                       (a & 0x8000) ? 2 :
                                       (a & 0x4000) ? 1 : 0);
        out->sx[i]   = (short)(a & 0x3fff);
        out->sy[i]   = (short)og_le16(img + t + 2);
        out->wx[i]   = (short)(og_le16(img + t + 4) >> 5);
        out->sort[i] = (short)(og_le16(img + t + 6) >> 5);
        ++out->n;
        t += 8;
    }
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, kind_ptr[0]), 5 * 4,
                 "Loaded the scenery scripts: pole, bunting, 2 barrels and the flagman");
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_FLAGMAN_DS + (size_t)(layout & 7) * 4), 4,
                 "Loaded track %d's flagman position", layout & 7);
    og_load_note("GAME.EXE", (long)(hdr + D + og_le16(img + D + OG_SCENERY_DS + (size_t)layout * 2)),
                 (long)out->n * 8 + 2,
                 "Loaded layout %d's scenery: %d pieces (poles, bunting, barrels)",
                 layout, out->n);
    free(raw);
    out->loaded = 1;
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* In-game effects (see og_data.h)                                     */
/* ------------------------------------------------------------------ */

long og_surface_resolve_at(const og_surface *s, unsigned short cell,
                           int frac_x, int frac_y)
{
    unsigned long off;
    if (!s || !s->loaded) return -1;
    if (cell & 1u) {
        unsigned long q = (unsigned long)(cell & ~1u);
        frac_x &= 0xff; frac_y &= 0xff;
        /* add al,dl ; jb skips the +4: it is taken when NO carry */
        if (frac_x + frac_y <= 0xff) q += 4;
        if (frac_x >= frac_y)        q += 2;      /* cmp cl,dl ; jb skips */
        if (q + 2 > s->quad_len) return -1;
        off = (unsigned long)(s->quad[q] | ((unsigned)s->quad[q + 1] << 8));
    } else {
        off = cell;
    }
    if (off + OG_POLY_REC > s->poly_len) return -1;
    return (long)off;
}

static int og_fx_parse(const unsigned char *img, size_t imglen,
                       unsigned desc, og_fx_anim *a)
{
    size_t D = (size_t)OG_GAME_DS_IMG, o = D + desc, list;
    int guard, op, arg1;
    memset(a, 0, sizeof *a);
    a->sprite = -1;
    for (guard = 0; guard < 32; ++guard) {            /* init script, 0x732 */
        unsigned v;
        if (o + 4 > imglen) return OG_E_SHORT;
        v = og_le16(img + o);
        if (v == 0x007a) break;
        if (v & 0x8000) return OG_E_MAGIC;            /* not used here */
        if (v & 0x4000) {
            unsigned val = og_le16(img + o + 2);
            if ((v & 0x3fff) == 0x5a) a->sprite = (int)val;
            if ((v & 0x3fff) == 0x04) {       /* a word constant 'DU' */
                a->tag[0] = (char)(val >> 8); a->tag[1] = (char)(val & 0xff);
            }
            o += 4;
        } else {
            o += 3;
        }
    }
    o += 2;                                           /* the script */
    if (o + 6 > imglen) return OG_E_SHORT;
    op = og_le16(img + o);
    if ((op & 0x7ff8) != 0xf8) return OG_E_MAGIC;     /* expect op 31 */
    arg1 = (int)(short)og_le16(img + o + 2);
    if ((op & 7) >= 2 && !(og_le16(img + o + 4) & 0x8000))
        a->sprite = (int)og_le16(img + o + 4);        /* arg2 >= 0 overrides */
    list = (size_t)((long)o + arg1);                  /* op address + arg1 */
    for (a->n = 0; a->n < OG_FX_MAXFRAMES; ++a->n) {
        unsigned v;
        if (list + 2 > imglen) return OG_E_SHORT;
        v = og_le16(img + list);
        if (v & 0x8000) break;                        /* command: end */
        a->frame[a->n] = (unsigned char)(v & 0xff);
        a->dur[a->n]   = (unsigned char)(v >> 8);
        list += 2;
    }
    return (a->sprite >= 0 && a->n > 0) ? OG_OK : OG_E_MAGIC;
}

int og_styles_load(const char *dir, og_styles *out)
{
    unsigned char *raw = NULL; size_t len = 0;
    char path[512]; int rc, k;
    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_resolve(dir, "EGA.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    for (k = 0; k < OG_STYLES_MAX; ++k) {
        size_t o = (size_t)OG_STYLES_EGA_OFF + (size_t)k * OG_STYLES_STRIDE;
        if (o + 24 > len) break;
        if (raw[o] < 32 || raw[o] > 126 || raw[o+1] < 32 || raw[o+1] > 126) break;
        out->name[k][0] = (char)raw[o];
        out->name[k][1] = (char)raw[o+1];
        /* The row starts at +8: entry 0 at +8, then the fill at +9 and
         * the outline at +10.  (Taking the outline from +8 makes every
         * border black, which suits the podium but is wrong everywhere
         * else, so the podium's black edge comes from something other
         * than the style row.) */
        out->fill[k]    = raw[o + 9];
        out->outline[k] = raw[o + 10];
        out->n = k + 1;
    }
    free(raw);
    out->loaded = (out->n > 0);
    if (out->loaded)
        og_load_note("EGA.EXE", (long)OG_STYLES_EGA_OFF, (long)out->n * OG_STYLES_STRIDE,
                     "Loaded the text style table: %d styles, fill and outline colours",
                     out->n);
    return out->loaded ? OG_OK : OG_E_SHORT;
}

/* A NUL-terminated string straight out of the loaded data segment.  The
 * podium's text lives there and the code addresses it arithmetically, so
 * there is no table to follow -- the offset is the address. */
/* The offset of the nth page in a run of them -- pages end with 0x00, so
 * the run is walked.  The podium's text is stored this way: each driver's
 * handle and name is its own little page, carrying its own position
 * codes, rather than a plain string. */
unsigned og_ds_page_n(const og_pages *p, unsigned ds_off, int n)
{
    size_t base, i;
    if (!p || !p->loaded) return 0;
    base = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG;
    i = base + ds_off;
    while (n > 0 && i < p->len) {
        if (p->img[i] == 0x00) --n;
        ++i;
    }
    return (unsigned)(i - base);
}

const char *og_ds_string(const og_pages *p, unsigned ds_off)
{
    size_t base;
    if (!p || !p->loaded) return NULL;
    base = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG + ds_off;
    if (base >= p->len) return NULL;
    return (const char *)(p->img + base);
}

int og_pages_load(const char *dir, og_pages *out)
{
    unsigned char *raw = NULL; size_t len = 0;
    char path[512]; int rc;
    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    out->img = raw; out->len = len; out->loaded = 1;
    if (len >= 0x20) {
        long ds = OG_DS_FILE(og_le16(raw + 8) * 16, 0);
        if ((size_t)ds < len)
            og_load_note("GAME.EXE", ds, (long)len - ds,
                         "Loaded the data segment, kept whole: the screen pages, "
                         "their text and the PC speaker effects");
    }
    return OG_OK;
}

void og_pages_free(og_pages *p)
{
    if (!p) return;
    free(p->img); p->img = NULL; p->len = 0; p->loaded = 0;
}


/* How many bytes each control code takes, measured from its handler. */
static const unsigned char og_page_param[0x20] = {
    0,0,2,1,3,2,1,3, 0,1,0,0,1,1,1,1,
    0,0,0,0,0,0,0,4, 2,0,0,0,0,0,0,0
};   /* 0x17 takes a four-character font name */

/* Code 0x0c draws a SUB-PAGE: its byte indexes a table of offsets (the
 * one the engine keeps at [0x73a]), and that page is drawn in the middle
 * of this one.  The registration screens are built this way -- a
 * prologue page sets the font and spacing, then each row of the letter
 * grid is its own entry. */
static int og_page_draw_at(const og_pages *p, unsigned ds_off, og_image *dst,
                           const og_font *iron, const og_font *road,
                           const og_styles *styles, unsigned table,
                           int *x_io, int *y_io, int depth);
/* When >= 0 this replaces every style's outline colour for the duration
 * of a page draw.  The results screen needs it: its styles all name 14,
 * which is the SKIN tone in that palette, but the original's plaques and
 * time board are plainly black-edged.  Every style's row entry 0 is 0,
 * so black is what the hardware ends up with there -- but forcing that
 * globally spoils the title and shop text, so the results screen asks
 * for it explicitly. */
int og_page_outline = -1;
int og_page_fill    = -1;

/* A sub-page inherits the caller's whole text state -- margin, letter
 * spacing, leading, pitch, font and style.  Resetting any of it makes the
 * sub-page draw in the default: the plaque's prize line loses its -1
 * spacing and every line after the first loses the column's colour. */
static struct {
    int margin, letter_sp, lead_adj, fixed, fill, outline, road;
} og_page_state;
static int og_page_draw_at2(const og_pages *p, unsigned ds_off, og_image *dst,
                            const og_font *iron, const og_font *road,
                            const og_styles *styles, unsigned table,
                            int *x_io, int *y_io, int depth, unsigned follow);


int og_page_draw(const og_pages *p, unsigned ds_off, og_image *dst,
                 const og_font *iron, const og_font *road,
                 const og_styles *styles)
{
    int x = 0, y = 0;
    return og_page_draw_at(p, ds_off, dst, iron, road, styles, 0, &x, &y, 0);
}

/* Two pages in sequence, the second carrying on from the first.  A view
 * is built this way: a prologue page sets the font, the pitch, the
 * spacing and the leading (0x8424 does exactly that and ends), and the
 * content page follows.  Drawing the content alone loses all of it. */
/* Patch a template in place: the plaque pages ship as "xxx Place : xxx"
 * under style "Ex", and the game rewrites them before each draw.  Only
 * the first occurrence within this page is replaced, and the replacement
 * must be the same length. */
int og_page_patch(og_pages *p, unsigned ds_off, const char *from, const char *to)
{
    size_t base, i, n = strlen(from), end;
    if (!p || !p->loaded || !from || !to || strlen(to) != n) return OG_E_ARG;
    base = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG + ds_off;
    end = base + 256;
    if (end > p->len) end = p->len;
    /* Do NOT stop at the first 0x00: a page's own control codes contain
     * zero bytes (05 00 18 sets x), so the text would never be reached.
     * Search a fixed window from the page's start instead. */
    for (i = base; i + n <= end; ++i) {
        if (memcmp(p->img + i, from, n) == 0) {
            memcpy(p->img + i, to, n);
            return OG_OK;
        }
    }
    return OG_E_RANGE;
}

/* Write text straight into a page at a known offset -- what the game
 * does: the club's loop puts the place at [si+1] and the initials at
 * [si+5] and [si+7].  Searching for the old text instead can match the
 * wrong bytes ("10" inside the money template's "000", say) and corrupt
 * the page for every later draw. */
int og_page_poke(og_pages *p, unsigned ds_off, unsigned at,
                 const char *text, size_t n)
{
    size_t base;
    if (!p || !p->loaded || !text) return OG_E_ARG;
    base = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG + ds_off + at;
    if (base + n > p->len) return OG_E_RANGE;
    memcpy(p->img + base, text, n);
    return OG_OK;
}

int og_page_add(og_pages *p, size_t n, unsigned *ds_off)
{
    size_t ds;
    unsigned char *img;
    if (!p || !p->loaded || !ds_off || !n || p->len < 0x20) return OG_E_ARG;
    ds = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG;
    if (ds > p->len) return OG_E_RANGE;
    img = (unsigned char *)realloc(p->img, p->len + n);
    if (!img) return OG_E_NOMEM;
    memset(img + p->len, 0, n);
    *ds_off = (unsigned)(p->len - ds);
    p->img = img;
    p->len += n;
    return OG_OK;
}

/* A page drawn from a given pen position -- the club's rows are placed
 * by the caller, which sets [0x97c] and [0x97e] before each. */
int og_page_draw_at_xy(const og_pages *p, unsigned ds_off, int x0, int y0,
                       og_image *dst, const og_font *iron, const og_font *road,
                       const og_styles *styles)
{
    int x = x0, y = y0;
    return og_page_draw_at(p, ds_off, dst, iron, road, styles, 0, &x, &y, 0);
}

/* A page that carries on from the one drawn before it, as the engine's
 * pages always do: the font, style, pitch, letter spacing, leading and
 * margin are whatever the last page left (the club's rows inherit E2
 * and spacing -1 from its heading).  The pen starts at x0, y0. */
int og_page_draw_cont(const og_pages *p, unsigned ds_off, int x0, int y0,
                      og_image *dst, const og_font *iron, const og_font *road,
                      const og_styles *styles)
{
    int x = x0, y = y0;
    return og_page_draw_at(p, ds_off, dst, iron, road, styles, 0, &x, &y, 1);
}

/* 0x42f:0xf34: how far a page moves the pen -- every glyph's advance (or
 * the fixed width) plus the letter spacing, and every 0x05 move -- in
 * the current state, which is restored afterwards as the original
 * restores [0x98a] and [0x984]. */
int og_page_measure(const og_pages *p, unsigned ds_off, const og_font *iron,
                    const og_font *road, const og_styles *styles)
{
    og_image tmp;
    int x = 0, y = -1000;               /* off the image: nothing drawn */
    unsigned char px = 0;
    struct { int margin, letter_sp, lead_adj, fixed, fill, outline, road; } keep;
    memcpy(&keep, &og_page_state, sizeof keep);
    tmp.w = 1; tmp.h = 1; tmp.px = &px;
    og_page_draw_at(p, ds_off, &tmp, iron, road, styles, 0, &x, &y, 1);
    memcpy(&og_page_state, &keep, sizeof keep);
    return x;
}

/* og_page_measure in a state the caller sets, as 0x2eed does before the
 * shop's money: the letter spacing ([0x984]), fixed pitch ([0x98a]) and
 * the font ([0x730], swapped for IRON's [0x160]).  Restored after. */
int og_page_measure_in(const og_pages *p, unsigned ds_off, const og_font *iron,
                       const og_font *road, const og_styles *styles,
                       int letter_sp, int fixed, int road_font)
{
    int w;
    struct { int margin, letter_sp, lead_adj, fixed, fill, outline, road; } keep;
    memcpy(&keep, &og_page_state, sizeof keep);
    og_page_state.letter_sp = letter_sp;
    og_page_state.fixed     = fixed;
    og_page_state.road      = road_font;
    w = og_page_measure(p, ds_off, iron, road, styles);
    memcpy(&og_page_state, &keep, sizeof keep);
    return w;
}

/* A page whose sub-page calls resolve through a given table. */
int og_page_draw_table(const og_pages *p, unsigned ds_off, unsigned table,
                       og_image *dst, const og_font *iron, const og_font *road,
                       const og_styles *styles)
{
    int x = 0, y = 0;
    return og_page_draw_at(p, ds_off, dst, iron, road, styles, table, &x, &y, 0);
}

int og_page_draw_after(const og_pages *p, unsigned pre, unsigned ds_off,
                       og_image *dst, const og_font *iron, const og_font *road,
                       const og_styles *styles, int *end_x, int *end_y)
{
    int x = 0, y = 0, rc;
    rc = og_page_draw_at2(p, pre, dst, iron, road, styles, 0, &x, &y, 0, ds_off);
    /* where the pen finished -- after "INITIALS:", so a caller can put
     * the entered letters on that very line instead of guessing */
    if (end_x) *end_x = x;
    if (end_y) *end_y = y;
    return rc;
}

static int og_page_draw_at(const og_pages *p, unsigned ds_off, og_image *dst,
                           const og_font *iron, const og_font *road,
                           const og_styles *styles, unsigned table,
                           int *x_io, int *y_io, int depth)
{
    return og_page_draw_at2(p, ds_off, dst, iron, road, styles, table,
                            x_io, y_io, depth, 0);
}

static int og_page_draw_at2(const og_pages *p, unsigned ds_off, og_image *dst,
                            const og_font *iron, const og_font *road,
                            const og_styles *styles, unsigned table,
                            int *x_io, int *y_io, int depth, unsigned follow)
{
    const og_font *font = iron;
    const unsigned char *b;
    size_t i, base, end;
    /* 0x0f does not set the line height: it sets a SIGNED ADJUSTMENT
     * that the newline handler adds to the font's own height (0x1058
     * fetches it from the font structure and adds the two), so 254 means
     * two pixels tighter, not a 254-pixel step. */
    /* The margin is carried into a sub-page: its own newline returns x to
     * the margin, and the plaque pages set one per column (7, 115, 223).
     * Starting a sub-page at margin 0 sends every name back to the left
     * edge of the screen. */
    int x = *x_io, y = *y_io;
    int margin    = (depth > 0) ? og_page_state.margin    : 0;
    int lead_adj  = (depth > 0) ? og_page_state.lead_adj  : 0;
    int fill      = (depth > 0) ? og_page_state.fill      : 15;
    int outline   = (depth > 0) ? og_page_state.outline   : 0;
    int fixed     = (depth > 0) ? og_page_state.fixed     : 0;
    int letter_sp = (depth > 0) ? og_page_state.letter_sp  : 0;

    if (!p || !p->loaded || !dst || !iron) return OG_E_ARG;
    if (!road) road = iron;
    if (depth > 0 && og_page_state.road) font = road;
    base = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG + ds_off;
    if (base >= p->len) return OG_E_RANGE;
    b = p->img; end = p->len;

    for (i = base; i < end; ) {
        unsigned c = b[i];
        if (c >= 0x20) {                      /* a run of text */
            /* A glyph steps by its OWN advance and nothing more -- no
             * extra spacing -- unless fixed pitch is on, when every
             * glyph steps by the font's own width instead (0x1005 takes
             * it from the font when [0x98a] is set, which is what 0x11
             * does and 0x10 undoes). */
            for (; i < end && b[i] >= 0x20; ++i) {
                char one[2];
                og_glyph gl;
                one[0] = (char)b[i]; one[1] = 0;
                og_font_draw(font, dst, x, y, one, 0, fill, outline);
                /* 0x1013: the pen moves by the advance PLUS the
                 * letter spacing at [0x984], which code 0x0e sets --
                 * 3 for the headings, 0 for body text. */
                if (fixed) x += font->width + letter_sp;
                else if (og_font_glyph(font, (unsigned char)b[i], &gl) == OG_OK)
                    x += gl.advance + letter_sp;
                else x += font->width + letter_sp;
            }
            continue;
        }
        if (c == 0x00) {                      /* the page ends */
            if (follow) {                     /* ...and the next carries on */
                /* i indexes the WHOLE image, so jump to the next page's
                 * absolute position -- rebasing the pointer and resetting
                 * i to zero mixes the two and lands in the wrong place. */
                size_t nb = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG
                          + follow;
                if (nb < end) { i = nb; follow = 0; continue; }
            }
            break;
        }
        {
            const unsigned char *a = b + i + 1;
            unsigned np = og_page_param[c];
            if (i + 1 + np > end) break;
            switch (c) {
            case 0x02: x = (a[0] << 8) | a[1]; break;
            case 0x03: y = a[0]; break;
            case 0x04: x = (a[0] << 8) | a[1]; y = a[2]; break;
            /* 0x05 and 0x07 add a 16-bit word, so ff fe is -2: the club's
             * money pulls each comma back two pixels with it.  Read as
             * unsigned it threw the comma and the rest off the screen. */
            case 0x05: x += (short)((a[0] << 8) | a[1]); break;
            case 0x06: y += (signed char)a[0]; break;
            case 0x07: x += (short)((a[0] << 8) | a[1]); y += (signed char)a[2]; break;
            case 0x08:                                   /* newline */
                x = margin;
                y += font->height + lead_adj;
                break;
            case 0x09: margin = a[0]; og_page_state.margin = margin; break;
            case 0x0f: lead_adj = (signed char)a[0];
                       og_page_state.lead_adj = lead_adj; break;
            case 0x18: {                       /* the style */
                int k;
                if (styles && styles->loaded)
                    for (k = 0; k < styles->n; ++k)
                        if (styles->name[k][0] == (char)a[0] &&
                            styles->name[k][1] == (char)a[1]) {
                            fill = (og_page_fill >= 0)
                                 ? og_page_fill : styles->fill[k];
                            outline = (og_page_outline >= 0)
                                    ? og_page_outline : styles->outline[k];
                            og_page_state.fill = fill;
                            og_page_state.outline = outline;
                            break;
                        }
                break;
            }
            case 0x0c:                         /* draw a sub-page */
                if (table && depth < 4) {
                    /* 0x10af: si = [0x73a]; si += [bx + si + 6] -- the
                     * entries start SIX bytes into the table and are
                     * offsets relative to the TABLE BASE, not absolute and
                     * not relative to 0x8000. */
                    size_t e = (size_t)og_le16(p->img + 8) * 16 + OG_GAME_DS_IMG
                             + table + 6u + (size_t)a[0] * 2u;
                    if (e + 2 <= p->len) {
                        unsigned sub = (table + (unsigned)og_le16(p->img + e))
                                       & 0xffffu;
                        og_page_draw_at(p, sub, dst, iron, road, styles,
                                        table, &x, &y, depth + 1);
                    }
                }
                break;
            case 0x0e: letter_sp = (signed char)a[0];
                       og_page_state.letter_sp = letter_sp; break;
            case 0x10: fixed = 0; og_page_state.fixed = 0; break;
            case 0x11: fixed = 1; og_page_state.fixed = 1; break;
            case 0x17:                         /* select a font by name */
                /* 0x1125: choosing a font also RESETS the margin and
                 * turns fixed pitch back off, which is why a page can
                 * set fixed pitch for a heading and never clear it --
                 * the next font switch does. */
                font = (a[0] == 'R') ? road : iron;
                og_page_state.road = (a[0] == 'R');
                margin = 0; og_page_state.margin = 0;
                fixed = 0;  og_page_state.fixed = 0;
                break;
            default: break;                    /* modes and the rest */
            }
            i += 1 + np;
        }
    }
    *x_io = x; *y_io = y;
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* The Ironman Club table (IRONHI.BIN), as GAME.EXE keeps it             */
/* ------------------------------------------------------------------ */

/* Fill the parsed view from the raw table: the first ten offsets are the
 * standings, best first (0x36ac keeps them sorted). */
static void og_hiscore_parse(og_hiscore *h)
{
    int k, i;
    h->n = 0;
    for (k = 0; k < OG_HISCORE_ENTRIES; ++k) {
        unsigned o = og_le16(h->raw + k * 2);
        const unsigned char *r;
        unsigned m;
        if (o < OG_CLUB_SLOTS * 2 || o + OG_HISCORE_STRIDE > OG_CLUB_BYTES) break;
        r = h->raw + o;
        memcpy(h->initials[k], r, 3);     h->initials[k][3] = '\0';
        memcpy(h->born[k],     r + 3, 6); h->born[k][6] = '\0';
        h->country[k] = r[9];
        m = og_le16(r + 10);                       /* BCD, units of $10,000 */
        h->money[k] = (long)(((m >> 12) & 15) * 1000 + ((m >> 8) & 15) * 100 +
                             ((m >> 4) & 15) * 10 + (m & 15)) * 10000L;
        for (i = 0; i < OG_HISCORE_TIMES; ++i) h->best[k][i] = r[12 + i];
        h->n = k + 1;
    }
    h->loaded = (h->n > 0);
}

/* 0x3635: the file is "RI" and then the 0x404 bytes of DS:0x98fb. */
static int og_hiscore_read(const char *path, og_hiscore *h)
{
    unsigned char *raw = NULL;
    size_t len = 0;
    int rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 2 + OG_CLUB_BYTES || raw[0] != 'R' || raw[1] != 'I') {
        free(raw);
        return OG_E_MAGIC;
    }
    memcpy(h->raw, raw + 2, OG_CLUB_BYTES);
    free(raw);
    return OG_OK;
}

int og_hiscore_open(const char *own, const char *dir, og_hiscore *h)
{
    char path[512];
    if (!h) return OG_E_ARG;
    memset(h, 0, sizeof *h);
    /* SORP's own copy first, then the game's.  The original reads only
     * ironhi.bin and, when that fails, keeps the table already in its
     * data segment -- the same bytes the shipped file holds. */
    if (own && og_hiscore_read(own, h) == OG_OK)
        h->source = OG_CLUB_FROM_SORP;
    else if (og_resolve(dir, OG_HISCORE_FILE, path, sizeof path) == OG_OK &&
             og_hiscore_read(path, h) == OG_OK) {
        h->source = OG_CLUB_FROM_GAME;
        og_load_note(OG_HISCORE_FILE, 2, OG_CLUB_BYTES,
                     "Loaded the Ironman Club table, after its \"RI\"");
    } else {
        unsigned char *exe = NULL;
        size_t len = 0, d;
        int rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
        if (rc == OG_OK) rc = og_read_file(path, &exe, &len);
        if (rc != OG_OK) return rc;
        d = (size_t)og_le16(exe + 8) * 16 + OG_GAME_DS_IMG + OG_CLUB_DS;
        if (d + OG_CLUB_BYTES > len) { free(exe); return OG_E_SHORT; }
        memcpy(h->raw, exe + d, OG_CLUB_BYTES);
        free(exe);
        h->source = OG_CLUB_FROM_DEFAULT;
        og_load_note("GAME.EXE", (long)d, OG_CLUB_BYTES,
                     "Loaded the default Ironman Club table");
    }
    og_hiscore_parse(h);
    return h->loaded ? OG_OK : OG_E_MAGIC;
}

int og_hiscore_load(const char *dir, og_hiscore *out)
{
    return og_hiscore_open(NULL, dir, out);
}

/* 0x3669: "RI" then the table, 0x406 bytes, written whole. */
int og_hiscore_save(const og_hiscore *h, const char *path)
{
    FILE *f;
    int ok;
    if (!h || !path) return OG_E_ARG;
    f = fopen(path, "wb");
    if (!f) return OG_E_IO;
    ok = fwrite("RI", 1, 2, f) == 2 &&
         fwrite(h->raw, 1, OG_CLUB_BYTES, f) == OG_CLUB_BYTES;
    if (fclose(f) != 0) ok = 0;
    return ok ? OG_OK : OG_E_IO;
}

/* op 86, 0x35c9: look for the player's ten bytes -- initials, the
 * truck's [+8], [+0xa], [+0xc] birthdate words and the country -- in
 * each of the ten records as it is stored. */
void og_hiscore_lookup(const og_hiscore *h, og_club_player *p)
{
    unsigned char key[10];
    int k, i;
    memcpy(key, p->initials, 3);
    memcpy(key + 3, p->mm, 2);
    memcpy(key + 5, p->dd, 2);
    memcpy(key + 7, p->yy, 2);
    key[9] = p->country;
    p->rec = 0;
    for (k = 0; h && k < OG_HISCORE_ENTRIES; ++k) {
        unsigned o = og_le16(h->raw + k * 2);
        if (o + OG_HISCORE_STRIDE > OG_CLUB_BYTES) continue;
        if (memcmp(h->raw + o, key, 10) == 0) {   /* 0x3618: found */
            p->rec = (int)o;
            memcpy(p->times, h->raw + o + 12, OG_CLUB_TIMES_BYTES);
            return;
        }
    }
    /* 0x35f2: a new name -- eight best times of 99.99, the rest zero */
    for (i = 0; i < 8; ++i) { p->times[i * 2] = 0x99; p->times[i * 2 + 1] = 0x99; }
    memset(p->times + 16, 0, OG_CLUB_TIMES_BYTES - 16);
}

/* 0x36ac: bubble the eleven offsets into order, most money first. */
static void og_hiscore_sort(og_hiscore *h)
{
    int swapped, k;
    do {
        swapped = 0;
        for (k = 0; k + 1 < OG_CLUB_SLOTS; ++k) {
            unsigned a = og_le16(h->raw + k * 2), b = og_le16(h->raw + k * 2 + 2);
            if (!b) break;
            if (og_le16(h->raw + a + 10) < og_le16(h->raw + b + 10)) {
                h->raw[k * 2]     = (unsigned char)(b & 0xff);
                h->raw[k * 2 + 1] = (unsigned char)(b >> 8);
                h->raw[k * 2 + 2] = (unsigned char)(a & 0xff);
                h->raw[k * 2 + 3] = (unsigned char)(a >> 8);
                swapped = 1;
            }
        }
    } while (swapped);
}

/* op 85, 0x2f76: the player goes into the SPARE eleventh slot -- the
 * birthdate laid in as [+0xa] [+8] [+0xc], the money from [+0x11] --
 * their best times are copied back into the record op 86 found, if it
 * found one, and the table is sorted. */
void og_hiscore_enter(og_hiscore *h, const og_club_player *p)
{
    unsigned o;
    unsigned char *r;
    if (!h) return;
    o = og_le16(h->raw + (OG_CLUB_SLOTS - 1) * 2);       /* [0x990f] */
    if (o + OG_HISCORE_STRIDE > OG_CLUB_BYTES) return;
    r = h->raw + o;
    memcpy(r, p->initials, 3);
    memcpy(r + 3, p->dd, 2);
    memcpy(r + 5, p->mm, 2);
    memcpy(r + 7, p->yy, 2);
    r[9]  = p->country;
    r[10] = (unsigned char)(p->money_bcd & 0xff);
    r[11] = (unsigned char)(p->money_bcd >> 8);
    memcpy(r + 12, p->times, OG_CLUB_TIMES_BYTES);
    if (p->rec && (unsigned)p->rec + OG_HISCORE_STRIDE <= OG_CLUB_BYTES)
        memcpy(h->raw + p->rec + 12, p->times, OG_CLUB_TIMES_BYTES);
    og_hiscore_sort(h);
    og_hiscore_parse(h);
}

int og_hud_load(const char *dir, og_hud *out)
{
    unsigned char *raw = NULL; size_t len = 0;
    char path[512]; int rc, i;
    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    {
        size_t D = (size_t)og_le16(raw + 8) * 16 + OG_GAME_DS_IMG;
        if (D + OG_HUD_CELLS_DS + OG_HUD_CELLS * 2 > len) { free(raw); return OG_E_SHORT; }
        for (i = 0; i < OG_HUD_CELLS; ++i) {
            out->cx[i] = raw[D + OG_HUD_CELLS_DS + (size_t)i * 2];
            out->cy[i] = raw[D + OG_HUD_CELLS_DS + (size_t)i * 2 + 1];
        }
        for (i = 0; i < 8; ++i) {
            out->ox[i] = og_le16(raw + D + OG_HUD_ORIGIN_DS + (size_t)i * 4);
            out->oy[i] = og_le16(raw + D + OG_HUD_ORIGIN_DS + (size_t)i * 4 + 2);
        }
        og_load_note("GAME.EXE", (long)(D + OG_HUD_CELLS_DS), OG_HUD_CELLS * 2,
                     "Loaded the HUD sprite placement table: %d scoreboard cells",
                     OG_HUD_CELLS);
        og_load_note("GAME.EXE", (long)(D + OG_HUD_ORIGIN_DS), 8 * 4,
                     "Loaded the scoreboard's corner on each of the 8 tracks");
    }
    free(raw);
    out->loaded = 1;
    return OG_OK;
}

int og_flagmap_load(const char *dir, unsigned char map[OG_END_COUNTRIES])
{
    unsigned char *raw = NULL; size_t len = 0;
    char path[512]; int rc, i;
    if (!map) return OG_E_ARG;
    rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    {
        size_t o = (size_t)og_le16(raw + 8) * 16 + OG_GAME_DS_IMG + OG_END_FLAG_MAP_DS;
        if (o + OG_END_COUNTRIES > len) { free(raw); return OG_E_SHORT; }
        for (i = 0; i < OG_END_COUNTRIES; ++i) map[i] = raw[o + (size_t)i];
        og_load_note("GAME.EXE", (long)o, OG_END_COUNTRIES,
                     "Loaded the country -> flag sprite table, %d countries",
                     OG_END_COUNTRIES);
    }
    free(raw);
    return OG_OK;
}

int og_ladder_load(const char *dir, og_ladder *out)
{
    unsigned char *raw = NULL; size_t len = 0;
    char path[512]; int rc, k, i;
    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    {
        size_t D = (size_t)og_le16(raw + 8) * 16 + OG_GAME_DS_IMG;
        if (D + 0x7400 > len) { free(raw); return OG_E_SHORT; }
        for (i = 0; i < 16; ++i) out->map[i] = raw[D + OG_LADDER_MAP_DS + (size_t)i];
        for (k = 0; k < OG_LADDER_CLASSES; ++k) {
            size_t p = D + OG_LADDER_PTR_DS + (size_t)k * 2;
            size_t lp = D + og_le16(raw + p);
            out->bias[k] = (signed char)raw[D + OG_LADDER_BIAS_DS + (size_t)k];
            if (lp + OG_LADDER_STEPS * 2 > len) continue;
            for (i = 0; i < OG_LADDER_STEPS; ++i)
                out->step[k][i] = og_le16(raw + lp + (size_t)i * 2);
            og_load_note("GAME.EXE", (long)lp, OG_LADDER_STEPS * 2,
                         "Loaded difficulty ladder %d: %d lap-time thresholds",
                         k, OG_LADDER_STEPS);
        }
        og_load_note("GAME.EXE", (long)(D + OG_LADDER_MAP_DS), 16,
                     "Loaded the race -> ladder class table, 16 races");
        og_load_note("GAME.EXE", (long)(D + OG_LADDER_BIAS_DS), OG_LADDER_CLASSES,
                     "Loaded the ladder biases, one per class");
        og_load_note("GAME.EXE", (long)(D + OG_LADDER_PTR_DS), OG_LADDER_CLASSES * 2,
                     "Loaded the ladder pointers, one per class");
    }
    free(raw);
    out->loaded = 1;
    return OG_OK;
}

int og_ladder_level(const og_ladder *l, int race, int avg_ticks)
{
    int k, n = 0;
    if (!l || !l->loaded || avg_ticks <= 0) return OG_CPU_BASE;
    k = l->map[(unsigned)race & 15] & (OG_LADDER_CLASSES - 1);
    while (n < OG_LADDER_STEPS && l->step[k][n] >= (unsigned)avg_ticks) ++n;
    n -= l->bias[k];
    if (n < 0) n = 0;
    if (n > OG_CPU_LEVELS - 1) n = OG_CPU_LEVELS - 1;
    return n;
}

int og_slopes_load(const char *dir, og_slopes *out)
{
    unsigned char *raw = NULL; size_t len = 0;
    char path[512]; int rc, i;
    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;
    {
        size_t hdr = (size_t)og_le16(raw + 8) * 16;
        size_t base = hdr + OG_GAME_DS_IMG + OG_SLOPE_LIMIT_DS;
        if (base + 16 > len) { free(raw); return OG_E_SHORT; }
        for (i = 0; i < 8; ++i) {
            out->bias [i] = raw[base + (size_t)i * 2];
            out->limit[i] = raw[base + (size_t)i * 2 + 1];
        }
        og_load_note("GAME.EXE", (long)base, 16,
                     "Loaded the slope limits: 8 entries, a gradient bias and a cap each");
    }
    free(raw);
    out->loaded = 1;
    return OG_OK;
}

int og_effects_load(const char *dir, og_effects *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, D;
    const unsigned char *img;
    int rc, i;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr; D = (size_t)OG_GAME_DS_IMG;
    if (D + OG_FX_SPLASH_DS + 32 > imglen) { free(raw); return OG_E_SHORT; }

    /* far pointers into DS: offset word then segment word */
    rc = og_fx_parse(img, imglen, og_le16(img + D + OG_FX_PTRS_DS), &out->dust);
    if (rc == OG_OK)
        rc = og_fx_parse(img, imglen, og_le16(img + D + OG_FX_PTRS_DS + 4), &out->nitro);
    for (i = 0; i < 4 && rc == OG_OK; ++i) {
        size_t r = D + OG_FX_SPLASH_DS + (size_t)i * 8;
        unsigned ptr_addr = og_le16(img + r);          /* address of a far ptr */
        out->splash_dx[i] = (int)(short)og_le16(img + r + 2);
        out->splash_dy[i] = (int)(short)og_le16(img + r + 4);
        rc = og_fx_parse(img, imglen, og_le16(img + D + ptr_addr), &out->splash[i]);
    }
    for (i = 0; i < 8 && rc == OG_OK; ++i) {      /* the burst's octants */
        size_t o = D + OG_NITRO_OFF_DS + (size_t)i * 4;
        if (o + 4 > imglen) { rc = OG_E_SHORT; break; }
        out->burst_dx[i] = (int)(short)og_le16(img + o);
        out->burst_dy[i] = (int)(short)og_le16(img + o + 2);
    }
    if (rc == OG_OK) {
        og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_FX_PTRS_DS), 8,
                     "Loaded the effect script pointers: %.2s dust and %.2s nitro",
                     out->dust.tag, out->nitro.tag);
        og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_FX_SPLASH_DS), 4 * 8,
                     "Loaded the water splash table: 4 %.2s scripts and their offsets",
                     out->splash[0].tag);
        og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_NITRO_OFF_DS), 8 * 4,
                     "Loaded the nitro burst offsets, 8 octants");
    }
    free(raw);
    if (rc != OG_OK) return rc;
    out->loaded = 1;
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* Truck attitude -> sprite frame (see og_data.h)                      */
/* ------------------------------------------------------------------ */

int og_truck_attitudes_load(const char *dir, og_truck_attitudes *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen;
    const unsigned char *img;
    int rc, i, d;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }
    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr;

    for (i = 0; i < OG_ATTITUDES; ++i) {
        size_t r = (size_t)OG_GAME_DS_IMG + OG_TRUCK_ATT_DS + (size_t)i * 4;
        size_t t;
        int base;
        if (r + 4 > imglen) { free(raw); return OG_E_SHORT; }
        base = og_le16(img + r);
        t = (size_t)OG_GAME_DS_IMG + og_le16(img + r + 2);
        if (t + 32 > imglen) { free(raw); return OG_E_SHORT; }
        for (d = 0; d < 32; ++d)
            out->frame[i][d] = (short)(base + img[t + (size_t)d]);
    }
    out->loaded = 1;
    og_load_note("GAME.EXE", OG_DS_FILE(hdr, OG_TRUCK_ATT_DS), OG_ATTITUDES * 4,
                 "Loaded the truck attitude table: %d attitudes x 32 directions",
                 OG_ATTITUDES);
    free(raw);
    return OG_OK;
}

int og_tilt_index(float diff)
{
    /* high byte of the 8.8 difference, + 10, clamp 0..16, >> 2 */
    int v = (int)floorf(diff) + 10;
    if (v < 0)  v = 0;
    if (v > 16) v = 16;
    return v >> 2;
}

int og_truck_frame(const og_truck_attitudes *a, int pitch, int roll,
                   int heading_byte)
{
    int dir;
    if (!a || !a->loaded) return 0;
    if (pitch < 0) pitch = 0;
    if (pitch > 4) pitch = 4;
    if (roll  < 0) roll  = 0;
    if (roll  > 4) roll  = 4;
    dir = ((-heading_byte) & 0xff) >> 3;         /* neg al ; shr 3 */
    return a->frame[pitch * 5 + roll][dir];
}

/* ------------------------------------------------------------------ */
/* World -> screen projection (see og_data.h)                          */
/* ------------------------------------------------------------------ */

void og_project(long wx, long wy, int height, int ox, int oy,
                int *sx, int *sy)
{
    long d = 0x2C00L - wy;
    /* mul by 0x5555 keeps the high word (/3), then sar dx five times */
    long shear = ((d * 0x5555L) >> 16) >> 5;
    if (sx) *sx = (int)(wx >> 5) + (int)shear + ox;
    if (sy) *sy = (int)((0x471L * wy) >> 16) + (128 - height) + oy;
}

int og_track_origin(const char *dir, int track, int *ox, int *oy)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, px, py;
    const unsigned char *img;
    int rc;

    if (track < 0 || track >= OG_NUM_TRACKS) return OG_E_ARG;
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }

    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr;
    px = (size_t)OG_GAME_DS_IMG + OG_ORIGIN_X_DS + (size_t)track * 2;
    py = (size_t)OG_GAME_DS_IMG + OG_ORIGIN_Y_DS + (size_t)track * 2;
    if (py + 2 > imglen) { free(raw); return OG_E_SHORT; }

    if (ox) *ox = (int)(short)og_le16(img + px);
    if (oy) *oy = (int)(short)og_le16(img + py);
    og_load_note("GAME.EXE", (long)(hdr + px), 2,
                 "Loaded track %d's screen origin X: %d", track, (int)(short)og_le16(img + px));
    og_load_note("GAME.EXE", (long)(hdr + py), 2,
                 "Loaded track %d's screen origin Y: %d", track, (int)(short)og_le16(img + py));
    free(raw);
    return OG_OK;
}


/* ------------------------------------------------------------------ */
/* Checkpoints and the AI's route walk (see og_data.h)                 */
/* ------------------------------------------------------------------ */

int og_checkpoints_load(const char *dir, int layout, og_checkpoints *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen, o, start;
    const unsigned char *img;
    int rc;

    if (!out || layout < 0 || layout >= OG_NUM_LAYOUTS) return OG_E_ARG;
    memset(out, 0, sizeof *out);
    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }

    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr;
    o = (size_t)OG_GAME_DS_IMG + OG_CKPT_TABLE_DS + (size_t)layout * 2;
    if (o + 2 > imglen) { free(raw); return OG_E_SHORT; }
    o = (size_t)OG_GAME_DS_IMG + og_le16(img + o);
    start = o;

    while (out->n < OG_CKPT_MAX && o + 7 <= imglen) {
        og_checkpoint *p = &out->cp[out->n++];
        p->x0 = img[o]; p->x1 = img[o+1]; p->y1 = img[o+2]; p->y0 = img[o+3];
        p->progress = img[o+4]; p->b5 = img[o+5]; p->flags = img[o+6];
        o += 7;
        if (p->flags & 0x80) break;                 /* finish line */
    }
    og_load_note("GAME.EXE", (long)(hdr + start), (long)(o - start),
                 "Loaded layout %d's checkpoints: %d gates, 7 bytes each",
                 layout, out->n);
    free(raw);
    return out->n > 0 ? OG_OK : OG_E_SHORT;
}

/* GAME.EXE image 0x23e2: a table-driven atan2.  Folds (x,y) into one
 * octant, counts ratio thresholds, then reflects back.  Transcribed
 * instruction for instruction, including the 8-bit wrap-arounds. */
static int og_angle(int ax, int bx)
{
    static const unsigned th[8] = { 4, 5, 6, 8, 0x0b, 0x11, 0x1a, 0x51 };
    unsigned cl = 0, ch = 8, q;
    int swapped, i, carry;

    if (ax < 0) { ax = -ax; cl = 0xa0; }
    if (bx < 0) { bx = -bx; cl ^= 0xe0; }
    swapped = (ax < bx);                         /* cmp ax,bx ; pushf  */
    if (swapped) { int t = ax; ax = bx; bx = t; }
    if (bx == 0) bx = 1;
    {
        unsigned long num = (unsigned long)ax * 4ul;       /* dx:ax */
        q = ((unsigned long)bx <= (num >> 16))
            ? 0xffffu                            /* quotient would overflow */
            : (unsigned)(num / (unsigned long)bx);
    }
    for (i = 0; i < 8 && q > th[i]; ++i) --ch;
    if (swapped) ch = (0x10u - ch) & 0xffu;      /* sub ch,10h ; neg ch */
    carry = (cl & 0x80u) != 0;                   /* shl cl,1            */
    cl = (cl << 1) & 0xffu;
    if (carry) ch = (0x20u - ch) & 0xffu;        /* sub ch,20h ; neg ch */
    return (int)(((ch << 1) | cl) & 0xffu);
}

int og_ckpt_progress(const og_checkpoint *p, int cx, int cy)
{
    int a = 0, b = 0, v;
    if (!p) return 0;
    switch (p->flags & 0x0f) {
    case 0:  v = cx - p->x0; break;
    case 1:  v = p->y1 - cy; break;
    case 2:  v = p->x1 - cx; break;
    case 3:  v = cy - p->y0; break;
    case 4:  a = cy - p->y0; b = cx - p->x0; goto bend;
    case 5:  a = cx - p->x0; b = p->y1 - cy; goto bend;
    case 6:  a = p->y1 - cy; b = p->x1 - cx; goto bend;
    case 7:  a = p->x1 - cx; b = cy - p->y0; goto bend;
    case 8:  a = p->y1 - cy; b = cx - p->x0; goto bend;
    case 9:  a = cx - p->x0; b = cy - p->y0; goto bend;
    case 10: a = cy - p->y0; b = p->x1 - cx; goto bend;
    case 11: a = p->x1 - cx; b = p->y1 - cy; goto bend;
    default: return p->progress;
    }
    return (v + p->progress) & 0xff;
bend:
    /* the components are bytes, sign-extended with cwde */
    a = (signed char)(a & 0xff);
    b = (signed char)(b & 0xff);
    v = og_angle(a, b);
    v = ((((unsigned)v * p->b5) << 2) & 0xffffu) >> 8;   /* mul, shl x2, ah */
    return (v + p->progress) & 0xff;
}

/* GAME.EXE's generator at 0x644: seed = (seed+1) * 0xAB mod 0x7673. */
unsigned og_rng(unsigned *seed)
{
    *seed = ((*seed + 1u) * 0xABu) % 0x7673u;
    return (*seed - 1u) & 0xffffu;
}

static unsigned og_rng_ai(og_ai *ai) { return og_rng(&ai->seed); }

/* The range test shared by 0x80 and 0x82 (image 0x2395); it wraps. */
static int og_inrange(int v, int a, int b)
{
    v &= 0xff;
    return (a < b) ? (v >= a && v <= b) : (v >= a || v <= b);
}

void og_ai_reset(og_ai *ai, unsigned seed)
{
    memset(ai, 0, sizeof *ai);
    ai->seed = seed;
}

static int og_s16(const og_route *r, int p)
{
    if (p < 0 || p + 1 >= r->stream_len) return 0;
    return (int)(short)(r->stream[p] | (r->stream[p + 1] << 8));
}

/* Advance the cursor to the next waypoint, evaluating the route program
 * exactly as the cursor-advance handlers do.  Returns its index. */
int og_ai_next(og_ai *ai, const og_route *r)
{
    int pos = ai->cursor, guard;

    for (guard = 0; guard < 512; ++guard) {
        unsigned char op;
        if (pos < 0 || pos >= r->stream_len) return -1;
        op = r->stream[pos];

        if (op < 0x80) {                                  /* waypoint */
            ai->cursor = pos + 2;
            return (pos < OG_ROUTE_STREAM) ? r->idx_at[pos] : -1;
        }
        switch (op) {
        case 0x84: pos = pos + 1 + og_s16(r, pos + 1); break;
        case 0x80:
        case 0x82: {
            int v = (op == 0x80) ? ai->checkpoint : ai->v5b;
            if (og_inrange(v, r->stream[pos + 1], r->stream[pos + 2]))
                pos = pos + 3 + og_s16(r, pos + 3);
            else pos += 5;
            break;
        }
        case 0x81:
            if (r->stream[pos + 1] < (og_rng_ai(ai) >> 8))
                pos = pos + 2 + og_s16(r, pos + 2);
            else pos += 4;
            break;
        case 0x89:
            if (ai->v77) pos = pos + 1 + og_s16(r, pos + 1);
            else pos += 3;
            break;
        case 0x83: case 0x88: pos += 3; break;
        case 0x85:            pos += 1; break;
        case 0x86: case 0x87: pos += 2; break;
        default:              return -1;
        }
    }
    return -1;
}

/* Enter the next checkpoint box if the truck's cell is inside it; the
 * count resets at the finish line.  Returns 1 when a box is crossed. */
int og_ai_cross(og_ai *ai, const og_checkpoints *c, int cx, int cy)
{
    const og_checkpoint *p;
    if (!ai || !c || c->n <= 0) return 0;
    if (ai->next_cp >= c->n) ai->next_cp = 0;

    /* still inside the gate last entered: keep progress current */
    if (ai->next_cp > 0) {
        const og_checkpoint *q = &c->cp[ai->next_cp - 1];
        if (cx >= q->x0 && cx < q->x1 && cy >= q->y0 && cy <= q->y1)
            ai->progress = og_ckpt_progress(q, cx, cy);
    }

    p = &c->cp[ai->next_cp];
    if (cx >= p->x0 && cx < p->x1 && cy >= p->y0 && cy <= p->y1) {
        int finish = (p->flags & 0x80) != 0;
        ai->progress = og_ckpt_progress(p, cx, cy);   /* [di+0x42] */
        ai->checkpoint++;
        ai->next_cp++;
        if (finish) { ai->checkpoint = 0; ai->next_cp = 0; ai->laps++; }
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* AI routes                                                           */
/* ------------------------------------------------------------------ */

static void og_add_edge(og_route *r, int from, int to, int kind)
{
    int k;
    if (from < 0 || to < 0 || from >= r->n || to >= r->n) return;
    for (k = 0; k < r->nsucc[from]; ++k)
        if (r->succ[from][k] == to) return;          /* no duplicates */
    if (r->nsucc[from] >= OG_ROUTE_MAXSUCC) return;
    r->succ[from][r->nsucc[from]] = (short)to;
    r->kind[from][r->nsucc[from]] = (unsigned char)kind;
    ++r->nsucc[from];
}

/* Longest simple cycle through node 0, by bounded depth-first search.
 * Routes have a few dozen nodes and low branching, so this is cheap. */
static short og_path[OG_ROUTE_MAX], og_best[OG_ROUTE_MAX];
static unsigned char og_seen[OG_ROUTE_MAX];
static int og_bestlen;
static long og_budget;

static void og_dfs(const og_route *r, int node, int depth)
{
    int k;
    if (--og_budget < 0) return;
    og_path[depth] = (short)node;
    for (k = 0; k < r->nsucc[node]; ++k) {
        int nx = r->succ[node][k];
        if (nx == 0 && depth + 1 > og_bestlen) {       /* closed a lap */
            og_bestlen = depth + 1;
            memcpy(og_best, og_path, sizeof(short) * (size_t)og_bestlen);
        }
        if (!og_seen[nx]) {
            og_seen[nx] = 1;
            og_dfs(r, nx, depth + 1);
            og_seen[nx] = 0;
        }
    }
}

/* Parse a route program in GAME.EXE's encoding: `img` holds `imglen`
 * bytes and the program starts at `start` within them.  *used gets how
 * far pass 1 read.  og_route_load hands it GAME.EXE's own; the Track-Pak
 * (tp_data.c) hands it the arcade's, re-encoded. */
static int og_route_from(const unsigned char *img, size_t imglen, long start,
                         og_route *out, long *used)
{
    unsigned radius = 0, width = 0;
    unsigned nitro_lo = 0, nitro_hi = 0;
    int      nitro_here = 0, rubber_here = 0;
    long pos;
    int prev = -1, i, k, npend = 0;
    struct { int from; long target; int kind; } pend[OG_ROUTE_MAX * 2];

    memset(out, 0, sizeof *out);

    /* Keep the raw program for the AI walk. */
    {
        long avail = (long)imglen - start;
        out->stream_len = (int)(avail < OG_ROUTE_STREAM ? avail : OG_ROUTE_STREAM);
        if (out->stream_len < 0) out->stream_len = 0;
        memcpy(out->stream, img + start, (size_t)out->stream_len);
        for (i = 0; i < OG_ROUTE_STREAM; ++i) out->idx_at[i] = -1;
    }

    /* Pass 1: read the WHOLE list linearly, recording every waypoint and
     * every edge.  A jump ends the current run; a branch does not. */
    pos = start;
    while (pos >= 0 && (size_t)pos + 1 < imglen && out->n < OG_ROUTE_MAX) {
        unsigned char op = img[pos];

        if (op < 0x80) {                                 /* waypoint */
            og_waypoint *w = &out->wp[out->n];
            w->cx = op; w->cy = img[pos + 1];
            w->radius = (unsigned short)radius;
            /* the gate belongs to the waypoint it was written at, so
             * it does not carry forward the way a radius does */
            w->nitro_lo = nitro_here ? (unsigned char)nitro_lo : 0;
            w->nitro_hi = nitro_here ? (unsigned char)nitro_hi : 0;
            w->rubber   = (unsigned char)rubber_here;
            nitro_here = rubber_here = 0;
            w->width  = (unsigned short)width;
            w->off    = (unsigned short)(pos - start);
            if (pos - start >= 0 && pos - start < OG_ROUTE_STREAM)
                out->idx_at[pos - start] = (short)out->n;
            {
                int idx = out->n++;          /* count it BEFORE linking, */
                if (prev >= 0)               /* or og_add_edge rejects it */
                    og_add_edge(out, prev, idx, OG_EDGE_NEXT);
                prev = idx;
            }
            pos += 2;
            continue;
        }
        switch (op) {
        case 0x84: {                                     /* jump */
            long p = pos + 1;
            if (npend < OG_ROUTE_MAX * 2 && prev >= 0) {
                pend[npend].from = prev;
                pend[npend].target = p + (short)og_le16(img + p);
                pend[npend].kind = OG_EDGE_JUMP; ++npend;
            }
            prev = -1;                                   /* run ends */
            pos += 3;
            break;
        }
        case 0x80: case 0x82: case 0x81: {              /* branch */
            long p = pos + (op == 0x81 ? 2 : 3);
            if (npend < OG_ROUTE_MAX * 2 && prev >= 0) {
                pend[npend].from = prev;
                pend[npend].target = p + (short)og_le16(img + p);
                pend[npend].kind = OG_EDGE_BRANCH; ++npend;
            }
            pos += (op == 0x81) ? 4 : 5;                 /* falls through */
            break;
        }
        case 0x86: radius = (unsigned)(img[pos + 1] >> 1);
                   radius *= radius; pos += 2; break;
        case 0x87: width = ((unsigned)img[pos + 1] + 1u) * 2u;
                   pos += 2; break;
        case 0x88:                                   /* FIRE NITRO here */
                   /* The dispatch table at DS:0x7519 sends 0x88 to the
                    * firing handler at 0x2301.  No layout uses it. */
                   nitro_lo = img[pos + 1]; nitro_hi = img[pos + 2];
                   nitro_here = 1;
                   pos += 3; break;
        case 0x83: case 0x89: pos += 3; break;
        case 0x85: rubber_here = 1;   /* re-weigh the rubber band here */
                   pos += 1; break;
        default:   goto done;                            /* 0x8a: end */
        }
    }
done:
    if (used) *used = pos - start + 1;
    /* Pass 2: resolve targets to the first waypoint at or after them. */
    for (i = 0; i < npend; ++i) {
        long rel = pend[i].target - start;
        int best = -1;
        for (k = 0; k < out->n; ++k)
            if ((long)out->wp[k].off >= rel &&
                (best < 0 || out->wp[k].off < out->wp[best].off)) best = k;
        if (best >= 0) og_add_edge(out, pend[i].from, best, pend[i].kind);
    }

    /* Pass 3: one full lap. */
    og_bestlen = 0; og_budget = 2000000L;
    memset(og_seen, 0, sizeof og_seen);
    if (out->n > 0) { og_seen[0] = 1; og_dfs(out, 0, 0); }
    if (og_bestlen > 1) {
        memcpy(out->lap, og_best, sizeof(short) * (size_t)og_bestlen);
        out->nlap = og_bestlen;
        out->closed = 1;
    } else {                                    /* fall back: list order */
        for (i = 0; i < out->n; ++i) out->lap[i] = (short)i;
        out->nlap = out->n;
    }

    return out->n >= 2 ? OG_OK : OG_E_SHORT;
}

int og_route_parse(const unsigned char *prog, long len, og_route *out)
{
    if (!prog || len <= 0 || !out) return OG_E_ARG;
    return og_route_from(prog, (size_t)len, 0, out, NULL);
}

int og_route_load(const char *dir, int layout, og_route *out)
{
    unsigned char *raw = NULL;
    size_t len = 0, hdr, imglen;
    const unsigned char *img;
    long start, used = 0;
    int rc;

    if (!out || layout < 0 || layout >= OG_NUM_LAYOUTS) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    rc = og_slurp(dir, "GAME.EXE", &raw, &len);
    if (rc != OG_OK) return rc;
    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') { free(raw); return OG_E_MAGIC; }

    hdr = (size_t)og_le16(raw + 8) * 16;
    img = raw + hdr; imglen = len - hdr;
    {
        size_t t = (size_t)OG_GAME_DS_IMG + OG_ROUTE_TABLE_DS + (size_t)layout * 2;
        if (t + 2 > imglen) { free(raw); return OG_E_SHORT; }
        start = (long)OG_GAME_DS_IMG + (long)og_le16(img + t);
    }
    rc = og_route_from(img, imglen, start, out, &used);
    og_load_note("GAME.EXE", (long)hdr + start, used,
                 "Loaded layout %d's route program: %d waypoints for the computer trucks",
                 layout, out->n);
    free(raw);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Sprite banks                                                        */
/* ------------------------------------------------------------------ */

int og_bank_load(const char *dir, const char *file, int cell_bytes,
                 int frame_h, int key, og_bank *out)
{
    unsigned char *buf = NULL;
    size_t len = 0;
    og_pcg pcg;
    int rc, i, n;

    if (!out || frame_h <= 0) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    rc = og_slurp(dir, file, &buf, &len);
    if (rc != OG_OK) return rc;

    rc = og_pcg_open(&pcg, buf, len, cell_bytes, 0);
    if (rc != OG_OK) { free(buf); return rc; }

    n = pcg.rows / frame_h;
    if (n <= 0) { free(buf); return OG_E_SHORT; }
    if (n > OG_BANK_MAX_FRAMES) n = OG_BANK_MAX_FRAMES;

    out->frames = (og_image *)calloc((size_t)n, sizeof *out->frames);
    if (!out->frames) { free(buf); return OG_E_NOMEM; }

    for (i = 0; i < n; ++i) {
        rc = og_pcg_strip(&pcg, i * frame_h, frame_h, &out->frames[i]);
        if (rc != OG_OK) break;
    }

    out->nframes = i;
    out->w       = pcg.cell_w;
    out->h       = frame_h;
    out->key     = key;

    free(buf);          /* og_pcg_strip has already copied the pixels */
    return out->nframes > 0 ? OG_OK : OG_E_SHORT;
}


int og_bank_load_ex(const char *dir, const char *file, int cell_bytes,
                    int nplanes, int frame_h, int key, int complement,
                    og_bank *out)
{
    unsigned char *buf = NULL;
    size_t len = 0, stride;
    int rc, i, y, x, p, k, w, total;

    if (!out || cell_bytes < 1 || nplanes < 1 || nplanes > 4 || frame_h < 1)
        return OG_E_ARG;
    memset(out, 0, sizeof *out);

    rc = og_slurp(dir, file, &buf, &len);
    if (rc != OG_OK) return rc;

    stride = (size_t)cell_bytes * (size_t)nplanes;
    w      = cell_bytes * 8;
    total  = (int)(len / (stride * (size_t)frame_h));
    if (total <= 0) { free(buf); return OG_E_SHORT; }
    if (total > OG_BANK_MAX_FRAMES) total = OG_BANK_MAX_FRAMES;

    out->frames = (og_image *)calloc((size_t)total, sizeof *out->frames);
    if (!out->frames) { free(buf); return OG_E_NOMEM; }

    for (i = 0; i < total; ++i) {
        og_image *im = &out->frames[i];
        if (og_image_init(im, w, frame_h) != OG_OK) break;

        for (y = 0; y < frame_h; ++y) {
            const unsigned char *row =
                buf + ((size_t)i * (size_t)frame_h + (size_t)y) * stride;
            for (p = 0; p < nplanes; ++p) {
                for (x = 0; x < cell_bytes; ++x) {
                    unsigned char v = row[(size_t)p * cell_bytes + x];
                    for (k = 0; k < 8; ++k)
                        im->px[y * w + x * 8 + k] |=
                            (unsigned char)(((v >> (7 - k)) & 1) << p);
                }
            }
        }
        if (complement) {
            int n = w * frame_h;
            for (k = 0; k < n; ++k) im->px[k] = (unsigned char)(15 - im->px[k]);
        }
    }

    out->nframes = i;
    out->w       = w;
    out->h       = frame_h;
    out->key     = key;

    free(buf);
    return out->nframes > 0 ? OG_OK : OG_E_SHORT;
}


/* ------------------------------------------------------------------ */
/* Variable-height sprites                                             */
/* ------------------------------------------------------------------ */

int og_sprite_table_load(const char *dir, const char *driver,
                         int count, og_sprite_table *out)
{
    unsigned char *buf = NULL;
    size_t len = 0;
    int rc, i, row = 0;

    if (!out || count <= 0) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    rc = og_slurp(dir, driver, &buf, &len);
    if (rc != OG_OK) return rc;

    if (len < (size_t)OG_SPRTAB_OFFSET + (size_t)count * 8) {
        free(buf);
        return OG_E_SHORT;
    }

    out->def = (og_sprite_def *)calloc((size_t)count, sizeof *out->def);
    if (!out->def) { free(buf); return OG_E_NOMEM; }

    for (i = 0; i < count; ++i) {
        const unsigned char *e = buf + OG_SPRTAB_OFFSET + (size_t)i * 8;
        og_sprite_def *s = &out->def[i];
        s->ptr_off = (unsigned short)(e[0] | ((unsigned)e[1] << 8));
        s->ptr_seg = (unsigned short)(e[2] | ((unsigned)e[3] << 8));
        s->rows = (int)e[4];
        s->top  = e[5];
        s->w    = e[6];
        s->xoff = e[7];
        /* Running total of rows: equal to the far pointer, which counts
         * the same 9-byte rows (entry 1 = 0006:0003 = 99). */
        s->byte_off = (long)row * 9L;
        row += s->rows;
    }
    out->n = count;
    og_load_note(driver, (long)OG_SPRTAB_OFFSET, (long)count * 8,
                 "Loaded the sprite table: %d entries, 8 bytes each "
                 "(pointer, rows, top, width, x offset)", count);

    free(buf);
    return OG_OK;
}

void og_sprite_table_free(og_sprite_table *t)
{
    if (!t) return;
    free(t->def);
    t->def = NULL;
    t->n = 0;
}

int og_bank_load_varh(const char *dir, const char *file,
                      const og_sprite_table *tab, int cell_bytes,
                      int nplanes, int key, int complement, og_bank *out)
{
    unsigned char *buf = NULL;
    size_t len = 0, stride;
    int rc, i, w, maxh = 0;

    if (!out || !tab || tab->n <= 0) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    rc = og_slurp(dir, file, &buf, &len);
    if (rc != OG_OK) return rc;

    stride = (size_t)cell_bytes * (size_t)nplanes;
    w      = cell_bytes * 8;

    out->frames   = (og_image *)calloc((size_t)tab->n, sizeof *out->frames);
    out->frame_h  = (int *)calloc((size_t)tab->n, sizeof(int));
    out->frame_ox = (int *)calloc((size_t)tab->n, sizeof(int));
    out->frame_oy = (int *)calloc((size_t)tab->n, sizeof(int));
    if (!out->frames || !out->frame_h || !out->frame_ox || !out->frame_oy) {
        free(buf);
        og_bank_free(out);
        return OG_E_NOMEM;
    }

    for (i = 0; i < tab->n; ++i) {
        const og_sprite_def *s = &tab->def[i];
        og_image *im = &out->frames[i];
        int y, x, p, k;

        if (s->rows <= 0) continue;
        if (s->byte_off < 0 ||
            (size_t)s->byte_off + (size_t)s->rows * stride > len) continue;
        if (og_image_init(im, w, s->rows) != OG_OK) break;

        for (y = 0; y < s->rows; ++y) {
            const unsigned char *rowp =
                buf + (size_t)s->byte_off + (size_t)y * stride;
            for (p = 0; p < nplanes; ++p) {
                for (x = 0; x < cell_bytes; ++x) {
                    unsigned char v = rowp[(size_t)p * cell_bytes + x];
                    for (k = 0; k < 8; ++k)
                        im->px[y * w + x * 8 + k] |=
                            (unsigned char)(((v >> (7 - k)) & 1) << p);
                }
            }
        }
        if (complement) {
            int n = w * s->rows;
            for (k = 0; k < n; ++k)
                im->px[k] = (unsigned char)(15 - im->px[k]);
        }

        out->frame_h [i] = s->rows;
        out->frame_ox[i] = (int)s->xoff;          /* +7: x offset in box */
        out->frame_oy[i] = OG_TRUCK_BOX_ROWS / 2 - (int)s->top;
        if (s->rows > maxh) maxh = s->rows;
    }

    out->nframes = i;
    out->w       = w;
    out->h       = maxh;
    out->key     = key;
    og_load_note(file, -1, (long)len,
                 "Loaded %d sprites, %d px wide, up to %d rows, %d bitplanes",
                 out->nframes, w, maxh, nplanes);

    free(buf);
    return out->nframes > 0 ? OG_OK : OG_E_SHORT;
}

void og_bank_free(og_bank *b)
{
    int i;
    if (!b) return;
    if (!b->frames) { free(b->frame_h); free(b->frame_ox); free(b->frame_oy);
                      b->frame_h = b->frame_ox = b->frame_oy = NULL; return; }
    for (i = 0; i < b->nframes; ++i)
        og_image_free(&b->frames[i]);
    free(b->frames);
    free(b->frame_h);
    free(b->frame_ox);
    free(b->frame_oy);
    b->frames   = NULL;
    b->frame_h  = NULL;
    b->frame_ox = NULL;
    b->frame_oy = NULL;
    b->nframes  = 0;
}

/* ------------------------------------------------------------------ */
/* Fonts                                                               */
/* ------------------------------------------------------------------ */

int og_fontfile_load(const char *dir, const char *file, og_fontfile *out)
{
    unsigned char *buf = NULL;
    size_t len = 0;
    int rc;

    if (!out) return OG_E_ARG;
    memset(out, 0, sizeof *out);

    rc = og_slurp(dir, file, &buf, &len);
    if (rc != OG_OK) return rc;

    rc = og_font_open(&out->f, buf, len);
    if (rc != OG_OK) { free(buf); return rc; }

    out->raw    = buf;      /* og_font keeps pointers into this */
    out->loaded = 1;
    og_load_note(file, -1, (long)len,
                 "Loaded the %s font: characters %d to %d, %dx%d cells",
                 out->f.id, out->f.first, out->f.last, out->f.width, out->f.height);
    return OG_OK;
}

void og_fontfile_free(og_fontfile *f)
{
    if (!f) return;
    free(f->raw);
    f->raw    = NULL;
    f->loaded = 0;
}

/* ------------------------------------------------------------------ */
/* The gap table at DS:0x753f, which moves the computer drivers between
 * the eighteen levels og_perf already loads.                          */
/* ------------------------------------------------------------------ */
/* The gap table at DS:0x753f, as xlatb actually reaches it.  The shift
 * at 0x2367 is ARITHMETIC on a signed byte, so a truck BEHIND the human
 * produces a negative index -- and xlatb then reads it UNSIGNED, landing
 * at 0xf0..0xff, well past the sixteen deliberate entries.  Those bytes
 * are large and positive, so a trailing truck is often clamped straight
 * to level 17.  That is the game's real behaviour, quirk and all, which
 * is why the opposition surges when it falls behind. */
const signed char og_ai_gap_full[256] = {
       0,    0,   -1,   -1,   -1,   -2,   -2,   -2,   -2,   -3,   -3,   -3,   -4,   -4,   -5,   -5,
       0,  112,  117, -122,  126,  -20,  127,   60,  120,   74, -127,   32,  123,  -74,  123,   -6,
    -126,   54,  125,  -40,  118,  -19,  120,   78,  127,  -98,  122,  -52, -127,   99, -126,   75,
     124, -121,  127, -122,   40,   10,   27,   15,   27, -123, -128,    5,   13, -121,    0, -125,
       0,    4,   20,   27, -123,   26,   27,   31,   27,   36,   28, -125,    0,   -1,   39,   30,
      40,   32,   40,   35,   39,   37,   36,   38,   31,   38,   27,   38,   24,   37,   24,   35,
    -123,   20,   32, -128,    0,    3,  -88,    0, -125,    5,   14,   20,   27, -123,   20,   22,
    -128,   10,    4,   89,    0, -125,    5,    9,   20,   17,   20,   12,   23,   10, -123, -125,
       0,   -1,   26,    7,   29,    7,   32,    8,   34,   11,   33,   14,   30,   16,   26,   17,
    -123, -128,    0,    4,   64,    0, -128,    5,    9,   72,    0, -126,    0,    5,   75,    0,
    -127, -128,   71,    0, -125,   10,   14,   20,   17,   15,   17, -126,    0,    5,   69,    0,
    -125,   11,   14,   10,   18, -125,    0,   -1,    6,   19,    3,   21, -123,    3,   23,    6,
      26, -124,  111,   -1, -125,    5,   14,   18,   25, -124,  -93,   -1, -125,   10,   14,   18,
      19, -124,  -41,   -1, -125,    0,    4,   22,   19, -123,   20,   22,   22,   25, -124,  101,
      -1, -125,    5,    9,   22,   15, -124, -109,   -1, -125,   10,   14,   21,   16,   16,   15,
    -125,   11,   14,   13,   14,   10,   11,    9,    8, -125,   12,    4,    7,    7,    5,    7,
};

/* the deliberate sixteen, kept for reference */
const signed char og_ai_gap[16] = {
    0, 0, -1, -1, -1, -2, -2, -2, -2, -3, -3, -3, -4, -4, -5, -5
};

/* ------------------------------------------------------------------ */
/* The Speed Shop's three tables (DS:0x954a, 0x9550, 0x9556)           */
/* ------------------------------------------------------------------ */
const unsigned char og_shop_price[OG_SHOP_ITEMS] = { 0x01, 0x08, 0x0a, 0x00, 0x06, 0x04 };
const unsigned char og_shop_field[OG_SHOP_ITEMS] = { 0x5d, 0x60, 0x61, 0x00, 0x5e, 0x5f };
const unsigned char og_shop_max  [OG_SHOP_ITEMS] = { 0x63, 0x05, 0x05, 0x00, 0x05, 0x05 };

/* The prize by place, DS:0x9d0a, in units of $10,000. */
const unsigned char og_prize[OG_PRIZE_PLACES] = { 10, 9, 8, 7 };

static void og_put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

unsigned og_club_bcd(unsigned v)
{
    unsigned r = 0, k;
    for (k = 0; k < 4; ++k) { r |= (v % 10) << (4 * k); v /= 10; }
    return r;
}

static unsigned char *og_club_list(og_hiscore *h, unsigned list, int track)
{
    return h->raw + list + (unsigned)track * OG_CLUB_LIST;
}

/* 0x38b5: bubble a zero-ended list of words into rising order */
static void og_club_sort(unsigned char *w)
{
    int swapped, k;
    do {
        swapped = 0;
        for (k = 0; og_le16(w + k * 2 + 2); ++k) {
            unsigned a = og_le16(w + k * 2), b = og_le16(w + k * 2 + 2);
            if (a > b) {
                og_put16(w + k * 2, b);
                og_put16(w + k * 2 + 2, a);
                swapped = 1;
            }
        }
    } while (swapped);
}

int og_club_rank(const og_hiscore *h, unsigned list, int track, unsigned bcd)
{
    const unsigned char *w;
    int k;
    if (!h || !h->loaded || track < 0 || track > 7) return -1;
    w = h->raw + list + (unsigned)track * OG_CLUB_LIST;
    for (k = 0; og_le16(w + k * 2); ++k)
        if (og_le16(w + k * 2) == bcd) return k;
    return -1;
}

unsigned og_club_best(const og_club_player *c, int track)
{
    return og_le16(c->times + track * 2);
}

/* 0x32b2: the average word [+0x24 + track * 3] times five, as BCD,
 * 99.99 from 9999 up */
unsigned og_club_avg(const og_club_player *c, int track)
{
    unsigned v = og_le16(c->times + 0x11 + track * 3) * 5u;
    return v >= 0x270f ? 0x9999 : og_club_bcd(v);
}

void og_club_race(og_hiscore *h, og_club_player *c, int track, int won,
                  unsigned race_bcd, unsigned race_ticks)
{
    unsigned char *e;
    unsigned long n, avg;
    if (!h || !c || track < 0 || track > 7) return;
    if (won) {                                           /* 0x3758 */
        unsigned char *rec = h->raw + OG_CLUB_RECORD + track * 2;
        unsigned char *spare = og_club_list(h, OG_CLUB_BESTS, track) + 0x14;
        if (race_bcd < og_le16(c->times + track * 2))
            og_put16(c->times + track * 2, race_bcd);
        og_put16(spare, race_bcd);
        og_club_sort(og_club_list(h, OG_CLUB_BESTS, track));
        if (race_bcd < og_le16(rec)) {                   /* 0x378a */
            og_put16(rec, race_bcd);
            memcpy(h->raw + OG_CLUB_HOLDER + track * 4, c->initials, 3);
        }
    }
    /* 0x37a5: count [+0x23 + track * 3] and average [+0x24]; the new
     * average is (average * count + time) / (count + 1), and a count
     * that reaches 128 is halved and one added */
    e   = c->times + 0x10 + track * 3;
    n   = e[0];
    avg = og_le16(e + 1);
    avg = (avg * n + race_ticks) / (n + 1);
    n  += 1;
    if (n & 0x80) n = (n >> 1) + 1;
    e[0] = (unsigned char)n;
    og_put16(e + 1, (unsigned)avg & 0xffff);
    og_put16(og_club_list(h, OG_CLUB_AVGS, track) + 0x14,   /* 0x37d2 */
             avg < 0x7d0 ? og_club_bcd((unsigned)avg * 5u) : 0x9999);
    og_club_sort(og_club_list(h, OG_CLUB_AVGS, track));
}

/* The track record and who holds it, as 0x33d7 reads them for the
 * finish-time board: DS:0x9b4f and 0x9b5f, inside the club table. */
double og_club_record(const og_hiscore *h, int track)
{
    unsigned v;
    if (!h || !h->loaded || track < 0 || track > 7) return 99.99;
    v = og_le16(h->raw + OG_CLUB_RECORD + track * 2);
    return ((v >> 12) & 15) * 10 + ((v >> 8) & 15) +
           (((v >> 4) & 15) * 10 + (v & 15)) / 100.0;
}

const char *og_club_record_who(const og_hiscore *h, int track)
{
    static char who[4];
    if (!h || !h->loaded || track < 0 || track > 7) return "XXX";
    memcpy(who, h->raw + OG_CLUB_HOLDER + track * 4, 3);
    who[3] = '\0';
    return who;
}

/* The per-track seed bonus, DS:0x74f0. */
const unsigned char og_ai_track_base[8] = { 2, 2, 2, 2, 2, 2, 1, 2 };

/* The championship schedule, DS:0x1a3: the layout for each race. */
const unsigned char og_schedule[OG_SCHEDULE_N] = {
    0x03, 0x06, 0x0b, 0x0e, 0x09, 0x03, 0x06, 0x01,
    0x05, 0x0b, 0x0e, 0x09, 0x0d, 0x0f, 0x03, 0x06,
    0x01, 0x05, 0x07, 0x04, 0x0b, 0x0e, 0x09, 0x0d,
    0x0f, 0x0c, 0x08, 0x03, 0x06, 0x01, 0x05, 0x07,
    0x04, 0x00, 0x02, 0x0b, 0x0e, 0x09, 0x0d, 0x0f,
    0x0c, 0x08, 0x0a
};
