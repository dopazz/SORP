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

/* tp.h -- the arcade Track-Pak's eight tracks, read from the player's own
 * ROMs and handed to SORP in the shapes GAME.EXE's data takes.
 *
 * The DOS game is the arcade game's data carried over: the same 44x44
 * contour grid split along the same diagonals, the same 7-byte
 * checkpoint gates, the same route programs with every command moved up
 * by 0x40, sixteen layouts of eight tracks raced both ways.  What
 * differs is that DOS runs Y the other way (DOS row r is arcade row
 * 43 - r, y = 0x2C00 - Y) and draws 200 lines where the arcade drew
 * 240.  Each loader here makes that conversion, checked against the
 * DOS game's own files for the tracks both games have.
 *
 * The ROMs come from gamedata/trackpak (see tp_rom.c); nothing of them
 * ships with SORP.
 */
#ifndef SORP_TP_H
#define SORP_TP_H

#include "og_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the master CPU's program, the background tiles, the tile map, the
 * part of the video CPU's ROM that holds the sprites SORP draws, and the
 * sound board's 80186 program (optional: tp_sound_rom) */
enum { TP_MASTER, TP_BG_GFX, TP_BG_PROM, TP_SLAVE, TP_SOUND, TP_NREGIONS };

/* The two arcade sets SORP reads: the Track-Pak (offroadt), whose eight
 * tracks are new, and the base game (offroad), whose eight are the DOS
 * game's own -- its art, terrain, routes and the rest in the arcade's
 * versions, numbered as the arcade numbers them (TP_DOS_TRACK). */
enum { TP_TRACKPAK, TP_ARCADE, TP_KINDS };

#define TP_TRACKS   8
#define TP_LAYOUTS  16
#define TP_ART_W    OG_SCREEN_W           /* 320 */
#define TP_ART_H    OG_SCREEN_H           /* 200, squashed from 240 */
#define TP_ROWS_IN  240
#define TP_SCHEDULE_MAX 64

typedef struct tp_set {
    int            kind;                  /* TP_TRACKPAK or TP_ARCADE    */
    char           dir[1024];             /* gamedata/trackpak, as found */
    unsigned char *region[TP_NREGIONS];
    unsigned long  have;                  /* bit per chip found          */
    char           found[32][300];        /* where each chip came from   */
} tp_set;

/* Find gamedata/trackpak and every chip of the set in it.  OG_OK when
 * all are there; OG_E_SHORT when some are missing (tp_missing says how
 * many; the set is still returned so the log can name them); OG_E_ARG
 * when there is no trackpak folder at all.  tp_close it either way.
 * The sound board's chips are optional and counted apart: a set without
 * them still races, with the DOS game's sound. */
int  tp_open(const char *gamedata, int kind, tp_set **out);
void tp_close(tp_set *tp);
int  tp_missing(const tp_set *tp);
int  tp_sound_missing(const tp_set *tp);
/* F3's sprite viewer wants the rest of the video CPU's ROM: optional too,
 * counted apart (tp_view_chips of them in a set) */
int  tp_view_missing(const tp_set *tp);
int  tp_view_chips(int kind);
/* 0 a chip the races need; 1 the sound board's; 2 the viewer's */
int  tp_chip_optional(int kind, int i);
/* The 80186's 1 MB, as tp_sound_new takes it, or NULL unless all six of
 * the sound board's chips were found.  It lives as long as the set. */
const unsigned char *tp_sound_rom(const tp_set *tp);
int  tp_chip_count(int kind);
const char *tp_chip_name(int kind, int i);
const char *tp_chip_found(const tp_set *tp, int i);   /* NULL if missing */
unsigned long tp_crc32(const unsigned char *p, size_t n);

/* --- the track's terrain ------------------------------------------ */

/* CONTOURn.XX, POLY.BIN and QUAD.BIN for one track, as GAME.EXE would
 * hold them.  The surface's poly and quad are this track's own, malloc'd
 * (og_surface_free them).  `datum`, if given, gets the arcade height of
 * DOS cell (0,0) in DOS height units -- the screen origin needs it. */
int  tp_contour_load(const tp_set *tp, int track, og_contour *c, og_surface *s,
                     double *datum);

/* The slope limits: the arcade has none (only a plane's own steepness
 * makes it a wall), so every gradient passes. */
void tp_slopes(og_slopes *out);

/* --- the art ------------------------------------------------------- */

/* The art is the background tiles in the track's palette, with the
 * track's name painted in where the arcade draws it (its video CPU's
 * sprite, tp_data.c), squashed from 240 lines to 200 by area. */
typedef struct {
    unsigned char rgb[TP_ART_W * TP_ART_H * 3];     /* the background   */
    unsigned char shadow[TP_ART_W * TP_ART_H * 3];  /* under a shadow:
                                                     * palette bank 15 */
    unsigned char fg[TP_ART_W * TP_ART_H];          /* 1: drawn over the
                                                     * trucks           */
    unsigned char over[TP_ART_W * TP_ART_H];        /* the overlays: the
                                                     * depth of scenery
                                                     * drawn over a truck
                                                     * whose own depth is
                                                     * less; 0 none    */
} tp_art;

/* A truck's depth, as the master gives the video CPU (0x58bf): 255 less
 * its arcade Y >> 6, the arcade's Y being 0x2C00 less DOS's y (world
 * units, 256 a cell).  The video CPU draws its sprites in depth order,
 * the overlays among them. */
#define TP_DEPTH(dos_y) (255 - ((0x2C00 - (int)(dos_y)) >> 6))

int  tp_art_load(const tp_set *tp, int track, tp_art *out);

/* The 240-line original, for tools: pens 0..63 and the 1024 colours. */
int  tp_art_pens(const tp_set *tp, int track, unsigned char pens[320 * 240],
                 unsigned char rgb[1024 * 3], unsigned char patched[64]);

/* --- the race ------------------------------------------------------ */

/* The screen origin og_project takes, worked out from the arcade's own
 * projection, squashed to 200 lines. */
int  tp_track_origin(const tp_set *tp, int track, int *ox, int *oy);

/* The computer trucks' route, in GAME.EXE's encoding, then parsed by
 * og_route_parse as GAME.EXE's are. */
int  tp_route_bytes(const tp_set *tp, int layout, unsigned char *out, int cap);
int  tp_route_load(const tp_set *tp, int layout, og_route *out);

int  tp_checkpoints_load(const tp_set *tp, int layout, og_checkpoints *out);

/* Each truck's own start, from the arcade's per-truck tables: x and y
 * are 0 and dx/dy carry the four positions outright. */
int  tp_race_start_load(const tp_set *tp, int layout, og_race_start *out);

/* The cash and nitro spots.  The arcade keeps them in SCREEN pixels;
 * they are taken back onto the ground through the projection. */
int  tp_spawns_load(const tp_set *tp, int layout, og_spawns *out);

/* Where the flagman stands, in screen pixels (og_scenery's fm_x, fm_y):
 * the arcade's own place for him, changed as GAME.EXE changed the base
 * game's (tp_data.c). */
int  tp_flagman(const tp_set *tp, int layout, int *sx, int *sy);

/* The pennant poles and banners: where GAME.EXE's scenery pieces would
 * stand (sx, sy, as og_scenery's) and the middle of the cells a truck
 * sets them off in (wx, wy, DOS world units).  Returns the count. */
typedef struct { int type, sx, sy, wx, wy; } tp_deco;   /* 0 pole, 1 banner */
int  tp_scenery(const tp_set *tp, int layout, tp_deco *out, int cap);

/* The scoreboard's top left corner (og_hud's origin), per track. */
int  tp_board_at(const tp_set *tp, int track, int *x, int *y);

/* The course's box on the screen, rails and all: the cells reachable
 * from the start, projected.  For a map of it, as the shop's are. */
int  tp_course_box(const tp_set *tp, int track, int box[4]);

/* The race order (offroadt 0x5710): layouts, until the list loops.
 * Returns the count; 0 for the base game, which has GAME.EXE's. */
int  tp_schedule(const tp_set *tp, unsigned char *layouts, int cap);

/* The sound command of a track's race tune, as the master sends it at
 * the race's start (offroad 0x4fbe: the table at 0x54BA by the arcade
 * track, [0xe045] & 7; offroadt's at 0x56C0), or -1. */
int  tp_race_tune(const tp_set *tp, int track);

/* The arcade's nations, as registration lists them (alphabetical, the
 * names at offroad 0x157F0): a player's is (ix-2) of the truck's block,
 * 0x10 for none. */
enum {
    TP_AUSTRALIA, TP_BELGIUM, TP_BRITAIN, TP_CANADA, TP_FINLAND, TP_FRANCE,
    TP_GERMANY, TP_HOLLAND, TP_ITALY, TP_JAPAN, TP_MEXICO, TP_NORWAY,
    TP_SPAIN, TP_SWEDEN, TP_SWISS, TP_USA, TP_NATIONS
};

/* The sound command of a nation's anthem, or -1.  The podium (offroad
 * bank 2 0x3795, offroadt 0x2345) plays the winner's: a computer truck's
 * (bit 2 of (ix-0x19)) is the jingle 0x56 instead, and a human's is the
 * table at 0x3AD2 (offroadt 0x2682) by the nation -- the same commands in
 * both sets.  In the attract mode ([0xe03e] set, [0xe047] clear) the
 * podium is silent. */
int  tp_anthem(const tp_set *tp, int nation);

/* A nation's name as registration lists it ("AUSTRALIA" .. "USA"), into
 * `out`, trimmed.  OG_OK, or an error for no such nation. */
#define TP_NATION_NAME 11
int  tp_nation_name(const tp_set *tp, int nation, char out[TP_NATION_NAME]);

/* A nation's flag, the sprite registration and the podium show, as pens
 * (0 clear, 1..15) into `pens`, a row of *w after another, *h rows; 32 x
 * 20 in both sets, the very flags GAME.EXE's are converted from (ESHOP 16
 * to 23, EEND 36 to 44).  `nation` TP_NATIONS
 * is the flag of a driver with no nation.  `addr`, if given, gets the
 * sprite's address, for tp_sprite_palette.  Needs the sprite chip u5t
 * (banks 6 and 7), which the races need too. */
#define TP_FLAG_W 32
#define TP_FLAG_H 20
int  tp_flag_pens(const tp_set *tp, int nation, unsigned char *pens,
                  int *w, int *h, unsigned *addr);

/* --- the video CPU's sprites, all of them (F3) --------------------- */

/* Banks 0 to 13 (0x10000 + 0x8000 * bank of the video region); the base
 * set has no 0 to 3.  1 when the chip holding `bank` was found. */
#define TP_SPRITE_BANKS 14
int  tp_bank_present(const tp_set *tp, int bank);

/* One sprite: its address HHLB as the master names it, and its 16-byte
 * descriptor's fields (tp_data.c) */
typedef struct {
    unsigned short addr;
    unsigned char  kind;        /* +0 & 0x0E: 2 a fill, else pixels      */
    unsigned char  flags;       /* +0's other bits (0x40, 0x08 seen)     */
    unsigned char  w, h;        /* the even block, in pixels; rows       */
    unsigned char  w_odd;       /* the odd block, in pixels              */
    signed char    ax, ay;      /* the anchor: X - ax, Y - ay is the
                                 * top left                              */
} tp_sprite;

/* Every sprite in the set's video ROM.  The ROM has no index, but each
 * bank holds one table of descriptors from 0x4000, and a descriptor
 * checks itself well enough to find the table's end (a rule checked
 * against every sprite MAME drew).
 * Fills `out` up to `cap` and returns how many there are in all (pass
 * NULL, 0 to count). */
int  tp_sprite_list(const tp_set *tp, tp_sprite *out, int cap);

/* Draw sprite `addr` into an RGB picture `rgb` (w x h, 3 bytes a pixel)
 * with its top left at (x, y): its even block, or with `odd` its odd
 * one, the picture a pixel over.  A pixel of pen f shows palette entry
 * f * 64 + `bg` of `pal` (1024 RGB triples, as tp_art_pens makes them);
 * pen 0 is clear.  Returns OG_OK, or OG_E_ARG for no such sprite. */
int  tp_sprite_draw(const tp_set *tp, unsigned addr, int odd,
                    const unsigned char *pal, int bg,
                    unsigned char *rgb, int w, int h, int x, int y);

/* The palette the arcade shows sprite `addr` in, into `pal` (1024 RGB
 * triples): a screen's own list for the sprites drawn on one (the
 * podium, the flags, the shop...: from MAME, tp_data.c), else track
 * `track`'s race palette.  Returns the screen's list, or 0 for a race's. */
unsigned tp_sprite_palette(const tp_set *tp, unsigned addr, int track,
                           unsigned char pal[1024 * 3]);

/* The base game's numbering against GAME.EXE's: DOS rows run the other
 * way, so the arcade's tracks 2 and 3 are DOS 3 and 2, and on four
 * tracks the arcade's forward is DOS's reverse. */
#define TP_DOS_TRACK(t)  ((t) == 2 ? 3 : (t) == 3 ? 2 : (t))
extern const unsigned char tp_layout_from_dos[16];

#ifdef __cplusplus
}
#endif

#endif /* SORP_TP_H */
