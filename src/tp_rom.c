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

/* tp_rom.c -- finding the Track-Pak's ROM chips and laying them out.
 *
 * The player's own copy of the arcade set lives in gamedata/trackpak, as
 * MAME keeps it -- offroadt.zip with its parent offroad.zip -- or as the
 * loose chip files, in that folder or a folder below it.  Names differ
 * between MAME versions and dumps, so a chip is recognised by its size
 * and CRC-32, never by its name: every zip entry (whose CRC the zip
 * stores) and every loose file is offered, and the ones that match are
 * kept.  Nothing of the ROMs ships with SORP.
 *
 * The regions are laid out as MAME's leland.cpp loads them (ROM_START
 * offroadt, or offroad for the base game's own tracks): the master CPU's
 * 256K, the background tiles, the tile map PROMs, and two of the video
 * CPU's chips, u4t and u5t, which hold the track names and the other
 * sprites SORP reads.  The sound board's six, u13t-u15t and u25t-u27t,
 * are optional: with them the arcade's own music, effects and engines
 * play (tp_sound.c); without them the races keep the DOS game's sound.
 */
#include "tp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dirent.h>
#  include <sys/stat.h>
#endif

/* ------------------------------------------------------------------ */
/* The chips                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned long crc;
    unsigned long size;
    int           region;          /* TP_MASTER ... TP_SOUND            */
    unsigned long offset;
    const char   *name;            /* MAME's, for the log               */
    int           step;            /* 2: every other byte (TP_SOUND's
                                    * 16-bit pairs); 0: in a row       */
    int           view;            /* 1: wanted only by F3's sprite
                                    * viewer, so optional               */
} tp_chip;

/* ROM_START( offroadt ) -- the four-player Track-Pak.  u57t is shared
 * with the parent set, so a split MAME set keeps it in offroad.zip. */
static const tp_chip tp_chips_t[] = {
    { 0xadbc6211UL, 0x10000, TP_MASTER,  0x00000, "03-25016-0x.u58t" },
    { 0x296dd3b6UL, 0x10000, TP_MASTER,  0x10000, "03-25017-0x.u59t" },
    { 0xe9f0f175UL, 0x10000, TP_MASTER,  0x20000, "03-22120-01.u57t" },
    { 0x2c1a22b3UL, 0x10000, TP_MASTER,  0x30000, "03-25015-01.u56t" },
    { 0xf0c1d8b0UL, 0x08000, TP_BG_GFX,  0x00000, "03-25012-01.u93"  },
    { 0x7460d8c0UL, 0x08000, TP_BG_GFX,  0x08000, "03-25013-01.u94"  },
    { 0x081ee7a8UL, 0x08000, TP_BG_GFX,  0x10000, "03-25014-01.u95"  },
    { 0xf9988e28UL, 0x04000, TP_BG_PROM, 0x04000, "03-25011-01.u92"  },
    { 0xfe5f8d8fUL, 0x04000, TP_BG_PROM, 0x08000, "03-25009-01.u69"  },
    { 0xbda2ecb1UL, 0x04000, TP_BG_PROM, 0x14000, "03-25010-01.u90"  },
    { 0x38c9bf29UL, 0x04000, TP_BG_PROM, 0x18000, "03-25008-01.u67"  },
    { 0x2276546fUL, 0x10000, TP_SLAVE,   0x20000, "03-25002-01.u3t"  },
    { 0xaa4b5975UL, 0x10000, TP_SLAVE,   0x30000, "03-25003-01.u4t"  },
    { 0x69100b06UL, 0x10000, TP_SLAVE,   0x40000, "03-25004-01.u5t"  },
    { 0x0f735078UL, 0x10000, TP_SLAVE,   0x70000, "03-25007-01.u8t"  },
    /* the rest of the video CPU's sprites, for F3 alone */
    { 0xc46c1627UL, 0x10000, TP_SLAVE,   0x10000, "03-25001-01.u2t", 0, 1 },
    { 0xb75015b8UL, 0x10000, TP_SLAVE,   0x50000, "03-25005-01.u6t", 0, 1 },
    { 0xa5af5b4fUL, 0x10000, TP_SLAVE,   0x60000, "03-25006-01.u7t", 0, 1 },
    /* the sound board's 80186, as it sees its 1 MB (leland.cpp: even
     * bytes from u13t-u15t, odd from u25t-u27t) */
    { 0x7beec9fcUL, 0x10000, TP_SOUND,   0x40000, "03-25018-01.u13t", 2 },
    { 0xf952f800UL, 0x10000, TP_SOUND,   0x40001, "03-25021-01.u25t", 2 },
    { 0x0a44331dUL, 0x10000, TP_SOUND,   0x60000, "03-25019-01.u14t", 2 },
    { 0x6227ea94UL, 0x10000, TP_SOUND,   0x60001, "03-25022-01.u26t", 2 },
    { 0x2a1a1c3cUL, 0x10000, TP_SOUND,   0xe0000, "03-25020-01.u15t", 2 },
    { 0xb80c5f99UL, 0x10000, TP_SOUND,   0xe0001, "03-25023-01.u27t", 2 },
};

/* ROM_START( offroad ) -- the base game, rev 4 */
static const tp_chip tp_chips_a[] = {
    { 0xc5790988UL, 0x10000, TP_MASTER,  0x00000, "03-22121-04.u58t" },
    { 0xae862fdcUL, 0x10000, TP_MASTER,  0x10000, "03-22122-03.u59t" },
    { 0xe9f0f175UL, 0x10000, TP_MASTER,  0x20000, "03-22120-01.u57t" },
    { 0x38642f22UL, 0x10000, TP_MASTER,  0x30000, "03-22119-02.u56t" },
    { 0x4426e367UL, 0x08000, TP_BG_GFX,  0x00000, "03-22105-02.u93"  },
    { 0x687dc1fcUL, 0x08000, TP_BG_GFX,  0x08000, "03-22106-02.u94"  },
    { 0xcee6ee5fUL, 0x08000, TP_BG_GFX,  0x10000, "03-22107-02.u95"  },
    { 0x03e0497dUL, 0x04000, TP_BG_PROM, 0x04000, "03-22104-01.u92"  },
    { 0xc3f2e443UL, 0x04000, TP_BG_PROM, 0x08000, "03-22102-01.u69"  },
    { 0x2266757aUL, 0x04000, TP_BG_PROM, 0x14000, "03-22103-02.u90"  },
    { 0xecab0527UL, 0x04000, TP_BG_PROM, 0x18000, "03-22101-02.u67"  },
    { 0x0d72780aUL, 0x10000, TP_SLAVE,   0x30000, "03-22108-02.u4t"  },
    { 0x5429ce2cUL, 0x10000, TP_SLAVE,   0x40000, "03-22109-02.u5t"  },
    /* the rest of the video CPU's sprites, for F3 alone (the base set
     * has no u2t or u3t: banks 0 to 3 are empty) */
    { 0xf97bad5cUL, 0x10000, TP_SLAVE,   0x50000, "03-22110-02.u6t", 0, 1 },
    { 0xf79157a1UL, 0x10000, TP_SLAVE,   0x60000, "03-22111-01.u7t", 0, 1 },
    { 0x3eef38d3UL, 0x10000, TP_SLAVE,   0x70000, "03-22112-01.u8t", 0, 1 },
    { 0x71b28df6UL, 0x10000, TP_SOUND,   0x40000, "03-22113-03.u13t", 2 },
    { 0x95bb31d3UL, 0x10000, TP_SOUND,   0x40001, "03-22116-03.u25t", 2 },
    { 0xf8b31bf8UL, 0x10000, TP_SOUND,   0x60000, "03-22114-03.u14t", 2 },
    { 0x703d81ceUL, 0x10000, TP_SOUND,   0x60001, "03-22117-03.u26t", 2 },
    { 0xc8439a7aUL, 0x10000, TP_SOUND,   0xe0000, "03-22115-03.u15t", 2 },
    { 0x806ccf8bUL, 0x10000, TP_SOUND,   0xe0001, "03-22118-03.u27t", 2 },
};

static const tp_chip *tp_chips_of(int kind, int *n)
{
    if (kind == TP_ARCADE) { *n = (int)(sizeof tp_chips_a / sizeof tp_chips_a[0]); return tp_chips_a; }
    *n = (int)(sizeof tp_chips_t / sizeof tp_chips_t[0]);
    return tp_chips_t;
}

/* the video CPU's region as far as the sprite banks go: banks 0 to 13
 * at 0x10000 + 0x8000 * bank, u2t to u8t (u4t and u5t for the races,
 * u3t and u8t for the Track-Pak's overlays, the rest for F3); the sound
 * board's, its 80186's whole address space */
static const unsigned long tp_region_size[TP_NREGIONS] = {
    0x40000, 0x18000, 0x20000, 0x80000, 0x100000
};

/* ------------------------------------------------------------------ */
/* CRC-32                                                              */
/* ------------------------------------------------------------------ */

static unsigned long tp_crc_table[256];

static void tp_crc_init(void)
{
    unsigned long c;
    int n, k;
    if (tp_crc_table[1]) return;
    for (n = 0; n < 256; ++n) {
        c = (unsigned long)n;
        for (k = 0; k < 8; ++k)
            c = (c & 1) ? 0xedb88320UL ^ (c >> 1) : c >> 1;
        tp_crc_table[n] = c;
    }
}

unsigned long tp_crc32(const unsigned char *p, size_t n)
{
    unsigned long c = 0xffffffffUL;
    tp_crc_init();
    while (n--) c = tp_crc_table[(c ^ *p++) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffUL;
}

/* ------------------------------------------------------------------ */
/* Inflate (RFC 1951), enough for a zip's deflated entries             */
/* ------------------------------------------------------------------ */

typedef struct {
    const unsigned char *in;
    size_t inlen, inpos;
    unsigned long bitbuf;
    int bitcnt;
    unsigned char *out;
    size_t outlen, outpos;
} tp_inflate_state;

typedef struct {
    short count[16];               /* codes of each length               */
    short symbol[288];             /* symbols, ordered by code           */
} tp_huff;

static int tp_bits(tp_inflate_state *s, int need)
{
    unsigned long v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) return -1;
        v |= (unsigned long)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1UL << need) - 1));
}

static int tp_decode(tp_inflate_state *s, const tp_huff *h)
{
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len <= 15; ++len) {
        int b = tp_bits(s, 1), count;
        if (b < 0) return -1;
        code |= b;
        count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* Returns 0 for a complete code, > 0 for an incomplete one, < 0 if the
 * lengths over-subscribe it. */
static int tp_construct(tp_huff *h, const short *length, int n)
{
    short offs[16];
    int len, sym, left = 1;
    for (len = 0; len < 16; ++len) h->count[len] = 0;
    for (sym = 0; sym < n; ++sym) h->count[length[sym]]++;
    if (h->count[0] == n) return 0;
    for (len = 1; len < 16; ++len) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return left;
    }
    offs[1] = 0;
    for (len = 1; len < 15; ++len) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (sym = 0; sym < n; ++sym)
        if (length[sym]) h->symbol[offs[length[sym]]++] = (short)sym;
    return left;
}

static int tp_codes(tp_inflate_state *s, const tp_huff *lencode, const tp_huff *distcode)
{
    static const short lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                     35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const short lext[29]  = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                     3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const short dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                     257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                     8193, 12289, 16385, 24577 };
    static const short dext[30]  = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                     7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
    for (;;) {
        int sym = tp_decode(s, lencode);
        if (sym < 0) return -1;
        if (sym < 256) {
            if (s->outpos >= s->outlen) return -1;
            s->out[s->outpos++] = (unsigned char)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            int len, dist, e;
            sym -= 257;
            if (sym >= 29) return -1;
            e = tp_bits(s, lext[sym]);
            if (e < 0) return -1;
            len = lbase[sym] + e;
            sym = tp_decode(s, distcode);
            if (sym < 0 || sym >= 30) return -1;
            e = tp_bits(s, dext[sym]);
            if (e < 0) return -1;
            dist = dbase[sym] + e;
            if ((size_t)dist > s->outpos || s->outpos + (size_t)len > s->outlen) return -1;
            while (len--) {
                s->out[s->outpos] = s->out[s->outpos - (size_t)dist];
                s->outpos++;
            }
        }
    }
}

static int tp_inflate(unsigned char *out, size_t outlen,
                      const unsigned char *in, size_t inlen)
{
    tp_inflate_state s;
    int last;
    memset(&s, 0, sizeof s);
    s.in = in; s.inlen = inlen; s.out = out; s.outlen = outlen;
    do {
        int type;
        last = tp_bits(&s, 1);
        type = tp_bits(&s, 2);
        if (last < 0 || type < 0) return -1;
        if (type == 0) {                                   /* stored */
            unsigned len, nlen;
            s.bitbuf = 0; s.bitcnt = 0;
            if (s.inpos + 4 > s.inlen) return -1;
            len  = s.in[s.inpos] | ((unsigned)s.in[s.inpos + 1] << 8);
            nlen = s.in[s.inpos + 2] | ((unsigned)s.in[s.inpos + 3] << 8);
            s.inpos += 4;
            if (len != (~nlen & 0xffffu)) return -1;
            if (s.inpos + len > s.inlen || s.outpos + len > s.outlen) return -1;
            memcpy(s.out + s.outpos, s.in + s.inpos, len);
            s.inpos += len; s.outpos += len;
        } else if (type == 1) {                            /* fixed */
            static tp_huff lencode, distcode;
            static int built;
            if (!built) {
                short lengths[288];
                int i;
                for (i = 0; i < 144; ++i) lengths[i] = 8;
                for (; i < 256; ++i) lengths[i] = 9;
                for (; i < 280; ++i) lengths[i] = 7;
                for (; i < 288; ++i) lengths[i] = 8;
                tp_construct(&lencode, lengths, 288);
                for (i = 0; i < 30; ++i) lengths[i] = 5;
                tp_construct(&distcode, lengths, 30);
                built = 1;
            }
            if (tp_codes(&s, &lencode, &distcode) != 0) return -1;
        } else if (type == 2) {                            /* dynamic */
            static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                             11, 4, 12, 3, 13, 2, 14, 1, 15 };
            short lengths[320];
            tp_huff lencode, distcode;
            int nlen = tp_bits(&s, 5), ndist = tp_bits(&s, 5), ncode = tp_bits(&s, 4);
            int index, err;
            if (nlen < 0 || ndist < 0 || ncode < 0) return -1;
            nlen += 257; ndist += 1; ncode += 4;
            if (nlen > 286 || ndist > 30) return -1;
            for (index = 0; index < ncode; ++index) {
                int b = tp_bits(&s, 3);
                if (b < 0) return -1;
                lengths[order[index]] = (short)b;
            }
            for (; index < 19; ++index) lengths[order[index]] = 0;
            if (tp_construct(&lencode, lengths, 19) != 0) return -1;
            index = 0;
            while (index < nlen + ndist) {
                int sym = tp_decode(&s, &lencode), len = 0, rep;
                if (sym < 0) return -1;
                if (sym < 16) { lengths[index++] = (short)sym; continue; }
                if (sym == 16) {
                    if (index == 0) return -1;
                    len = lengths[index - 1];
                    rep = tp_bits(&s, 2); if (rep < 0) return -1; rep += 3;
                } else if (sym == 17) {
                    rep = tp_bits(&s, 3); if (rep < 0) return -1; rep += 3;
                } else {
                    rep = tp_bits(&s, 7); if (rep < 0) return -1; rep += 11;
                }
                if (index + rep > nlen + ndist) return -1;
                while (rep--) lengths[index++] = (short)len;
            }
            if (lengths[256] == 0) return -1;
            err = tp_construct(&lencode, lengths, nlen);
            if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return -1;
            err = tp_construct(&distcode, lengths + nlen, ndist);
            if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return -1;
            if (tp_codes(&s, &lencode, &distcode) != 0) return -1;
        } else {
            return -1;
        }
    } while (!last);
    return s.outpos == outlen ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Offering files to the chip list                                     */
/* ------------------------------------------------------------------ */

static unsigned long tp_le16(const unsigned char *p) { return p[0] | ((unsigned long)p[1] << 8); }
static unsigned long tp_le32(const unsigned char *p)
{
    return p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* the set's chip this size and CRC are, or -1 */
static int tp_chip_for(const tp_set *tp, unsigned long size, unsigned long crc)
{
    int i, n;
    const tp_chip *c = tp_chips_of(tp->kind, &n);
    for (i = 0; i < n; ++i)
        if (c[i].size == size && c[i].crc == crc) return i;
    return -1;
}

static void tp_place(tp_set *tp, int chip, const unsigned char *data, const char *from)
{
    int n;
    const tp_chip *c = &tp_chips_of(tp->kind, &n)[chip];
    if (c->step == 2) {
        unsigned long i;
        for (i = 0; i < c->size; ++i) tp->region[c->region][c->offset + 2 * i] = data[i];
    } else {
        memcpy(tp->region[c->region] + c->offset, data, c->size);
    }
    tp->have |= 1UL << chip;
    snprintf(tp->found[chip], sizeof tp->found[chip], "%.290s", from);
}

/* Every entry of a zip whose size and stored CRC name a chip still
 * missing: inflated, its CRC checked again, and placed. */
static void tp_offer_zip(tp_set *tp, const char *path, const unsigned char *z, size_t n)
{
    size_t eocd, cd, end;
    unsigned long entries, i;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    if (n < 22) return;
    for (eocd = n - 22; ; --eocd) {               /* the end record, from the back */
        if (tp_le32(z + eocd) == 0x06054b50UL) break;
        if (eocd == 0 || n - eocd > 22 + 65535) return;
    }
    entries = tp_le16(z + eocd + 10);
    cd = tp_le32(z + eocd + 16);
    end = eocd;
    for (i = 0; i < entries && cd + 46 <= end; ++i) {
        unsigned long method = tp_le16(z + cd + 10), crc = tp_le32(z + cd + 16);
        unsigned long csize = tp_le32(z + cd + 20), usize = tp_le32(z + cd + 24);
        unsigned long nlen = tp_le16(z + cd + 28), xlen = tp_le16(z + cd + 30);
        unsigned long clen = tp_le16(z + cd + 32), lho = tp_le32(z + cd + 42);
        int chip = tp_chip_for(tp, usize, crc);
        if (chip >= 0 && !(tp->have & (1UL << chip)) && lho + 30 <= n &&
            tp_le32(z + lho) == 0x04034b50UL) {
            size_t data = lho + 30 + tp_le16(z + lho + 26) + tp_le16(z + lho + 28);
            if (data + csize <= n) {
                unsigned char *buf = (unsigned char *)malloc(usize ? usize : 1);
                int ok = 0;
                if (buf) {
                    if (method == 0 && csize == usize) {
                        memcpy(buf, z + data, usize); ok = 1;
                    } else if (method == 8) {
                        ok = tp_inflate(buf, usize, z + data, csize) == 0;
                    }
                    if (ok && tp_crc32(buf, usize) == crc) {
                        char from[300];
                        snprintf(from, sizeof from, "%.80s: %.*s", base,
                                 (int)(nlen < 200 ? nlen : 200), (const char *)(z + cd + 46));
                        tp_place(tp, chip, buf, from);
                    }
                    free(buf);
                }
            }
        }
        cd += 46 + nlen + xlen + clen;
    }
}

static int tp_ends_with_zip(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && tolower((unsigned char)name[n - 1]) == 'p' &&
           tolower((unsigned char)name[n - 2]) == 'i' &&
           tolower((unsigned char)name[n - 3]) == 'z' && name[n - 4] == '.';
}

static void tp_offer_file(tp_set *tp, const char *dir, const char *name)
{
    char path[1024];
    unsigned char *buf = NULL;
    size_t len = 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (og_read_file(path, &buf, &len) != OG_OK) return;
    if (tp_ends_with_zip(name)) {
        tp_offer_zip(tp, path, buf, len);
    } else {
        int chip = tp_chip_for(tp, (unsigned long)len, len <= 0x10000 ? tp_crc32(buf, len) : 0);
        if (chip >= 0 && !(tp->have & (1UL << chip))) tp_place(tp, chip, buf, name);
    }
    free(buf);
}

/* Every file in `dir`, and (when depth allows) in the folders below it. */
static void tp_scan(tp_set *tp, const char *dir, int depth)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pat[1024];
    snprintf(pat, sizeof pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == '.') continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth > 0) {
                char sub[1024];
                snprintf(sub, sizeof sub, "%s/%s", dir, fd.cFileName);
                tp_scan(tp, sub, depth - 1);
            }
        } else if (fd.nFileSizeHigh == 0 && fd.nFileSizeLow <= 64UL << 20) {
            tp_offer_file(tp, dir, fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d) return;
    while ((e = readdir(d)) != NULL) {
        char path[1024];
        struct stat st;
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth > 0) tp_scan(tp, path, depth - 1);
        } else if (S_ISREG(st.st_mode) && st.st_size <= (64L << 20)) {
            tp_offer_file(tp, dir, e->d_name);
        }
    }
    closedir(d);
#endif
}

/* ------------------------------------------------------------------ */

int tp_open(const char *gamedata, int kind, tp_set **out)
{
    tp_set *tp;
    int r;

    if (!gamedata || !out) return OG_E_ARG;
    *out = NULL;
    if (kind < 0 || kind >= TP_KINDS) return OG_E_ARG;
    tp = (tp_set *)calloc(1, sizeof *tp);
    if (!tp) return OG_E_NOMEM;
    tp->kind = kind;
    if (og_resolve(gamedata, "trackpak", tp->dir, sizeof tp->dir) != OG_OK) {
        free(tp);
        return OG_E_ARG;
    }
    for (r = 0; r < TP_NREGIONS; ++r) {
        tp->region[r] = (unsigned char *)malloc(tp_region_size[r]);
        if (!tp->region[r]) { tp_close(tp); return OG_E_NOMEM; }
        memset(tp->region[r], 0xff, tp_region_size[r]);   /* as MAME fills */
    }
    tp_scan(tp, tp->dir, 1);
    *out = tp;
    return tp_missing(tp) == 0 ? OG_OK : OG_E_SHORT;
}

void tp_close(tp_set *tp)
{
    int r;
    if (!tp) return;
    for (r = 0; r < TP_NREGIONS; ++r) free(tp->region[r]);
    free(tp);
}

int tp_missing(const tp_set *tp)
{
    int i, n, miss = 0;
    const tp_chip *c = tp_chips_of(tp->kind, &n);
    for (i = 0; i < n; ++i)
        if (c[i].region != TP_SOUND && !c[i].view && !(tp->have & (1UL << i))) ++miss;
    return miss;
}

int tp_view_missing(const tp_set *tp)
{
    int i, n, miss = 0;
    const tp_chip *c = tp_chips_of(tp->kind, &n);
    for (i = 0; i < n; ++i)
        if (c[i].view && !(tp->have & (1UL << i))) ++miss;
    return miss;
}

int tp_view_chips(int kind)
{
    int i, n, k = 0;
    const tp_chip *c = tp_chips_of(kind, &n);
    for (i = 0; i < n; ++i) k += c[i].view;
    return k;
}

int tp_bank_present(const tp_set *tp, int bank)
{
    const unsigned long o = 0x10000UL + 0x8000UL * (unsigned long)bank;
    int i, n;
    const tp_chip *c = tp_chips_of(tp->kind, &n);
    if (bank < 0 || bank >= TP_SPRITE_BANKS) return 0;
    for (i = 0; i < n; ++i)
        if (c[i].region == TP_SLAVE && (tp->have & (1UL << i)) &&
            o >= c[i].offset && o < c[i].offset + c[i].size)
            return 1;
    return 0;
}

int tp_sound_missing(const tp_set *tp)
{
    int i, n, miss = 0;
    const tp_chip *c = tp_chips_of(tp->kind, &n);
    for (i = 0; i < n; ++i)
        if (c[i].region == TP_SOUND && !(tp->have & (1UL << i))) ++miss;
    return miss;
}

const unsigned char *tp_sound_rom(const tp_set *tp)
{
    return tp && tp_sound_missing(tp) == 0 ? tp->region[TP_SOUND] : NULL;
}

int tp_chip_optional(int kind, int i)
{
    int n;
    const tp_chip *c = tp_chips_of(kind, &n);
    if (i < 0 || i >= n) return 0;
    return c[i].region == TP_SOUND ? 1 : c[i].view ? 2 : 0;
}

int tp_chip_count(int kind)
{
    int n;
    tp_chips_of(kind, &n);
    return n;
}

const char *tp_chip_name(int kind, int i)
{
    int n;
    const tp_chip *c = tp_chips_of(kind, &n);
    return (i >= 0 && i < n) ? c[i].name : "";
}

const char *tp_chip_found(const tp_set *tp, int i)
{
    int n;
    tp_chips_of(tp ? tp->kind : 0, &n);
    if (!tp || i < 0 || i >= n || !(tp->have & (1UL << i))) return NULL;
    return tp->found[i];
}
