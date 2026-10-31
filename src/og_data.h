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

/* og_data.h -- game-data layer for the Super Off Road SDL3 port.
 *
 * Sits on top of offroad_gfx.h and turns the raw containers into the
 * things the engine actually wants: a track (background picture plus
 * its foreground/occlusion plane), a contour grid, and sprite banks
 * sliced into frames.
 *
 * Nothing here ships any game content.  Every byte comes from the
 * player's own copy of the original game at run time.
 */
#ifndef OG_DATA_H
#define OG_DATA_H

#include "offroad_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OG_NUM_TRACKS   8
#define OG_SCREEN_W     OG_XX_WIDTH      /* 320 */
#define OG_SCREEN_H     OG_XX_HEIGHT     /* 200 */

/* ------------------------------------------------------------------ */
/* Tracks                                                              */
/* ------------------------------------------------------------------ */

/* A TRACKn.XX file is two concatenated chunks, each introduced by a
 * big-endian self-length:
 *
 *   [0]            u32 length of the picture chunk, header included
 *   [4]            four RLE planes -> the 320x200 background
 *   [picture_len]  u32 length of the second chunk
 *   [+4]           ONE more RLE plane -> a 1bpp 320x200 mask
 *
 * The mask marks pixels the original draws *after* the trucks, so that
 * bridges, banks and berms occlude a truck passing behind them.  We
 * reproduce that by building a foreground overlay image from it.
 *
 * (Verified against all eight shipped tracks: the picture chunk always
 * decodes to exactly four planes and the tail to exactly one.)
 */
typedef struct {
    og_image        pic;        /* 320x200 attribute indices          */
    unsigned char  *fg;         /* 320x200, non-zero = foreground     */
    int             has_fg;
} og_track_art;

int  og_track_load(const char *dir, int index, og_track_art *out);
void og_track_free(og_track_art *t);

/* ------------------------------------------------------------------ */
/* Contour grids                                                       */
/* ------------------------------------------------------------------ */

/* CONTOURn.XX is a fixed 3872 bytes = a 44 x 44 grid of 16-bit cells,
 * row-major, 88 bytes per row.  Traced in GAME.EXE: the routine at
 * image offset 0x1e9b walks it with two nested counters of 44 (0x2c),
 * stepping the source pointer by 2 per cell, and reaches back by 0x58
 * (88) to sample the cell one row up.  That fixes the geometry with no
 * guesswork.
 *
 * IMPORTANT: these words are LITTLE-endian, unlike everything else in
 * the game's data.  The artwork was converted from the 68000 arcade
 * original and kept its big-endian layout, but this grid is consumed by
 * plain x86 word loads (`mov bx, es:[di]`), so it is native order.
 * Reading it big-endian yields a plausible-looking "type 0..5 plus
 * signed slope" split that is entirely an artefact.
 *
 * A cell is not a terrain type.  It is a reference into a record table
 * held in another segment:
 *
 *   bit 0 clear -- the value is a direct record offset.
 *   bit 0 set   -- mask it off and indirect once through a second
 *                  table before use.
 *
 * From the resolved record the game takes byte +2 and byte +3.  Each is
 * shifted left 8 then arithmetically right 1 (so scaled by 128, signed)
 * and added to / subtracted from a 16-bit accumulator seeded at 0x8000:
 * +2 is the gradient along +X, +3 the gradient along +Y.  The top byte
 * of that accumulator is written out per cell.
 *
 * In other words the file stores *gradients*, and the game integrates
 * them into an absolute height field -- it expands the 2-byte grid into
 * a 3-byte-per-cell runtime array (cell word plus derived height).
 * That is why fitting the raw file to the artwork found no correlation:
 * the raw values are slopes, not positions.
 *
 * The two record tables are almost certainly POLY.BIN and QUAD.BIN,
 * whose segments are held in the same globals the loop dereferences.
 * Decoding those records is what remains before the height field can be
 * reproduced exactly.
 *
 * The engine does not need this to race: collision comes from the
 * barrier artwork (see og_palette.h).  The grid is loaded and exposed
 * for the asset viewer and for future elevation work.
 */
#define OG_CONTOUR_W 44
#define OG_CONTOUR_H 44

typedef struct {
    unsigned short cell[OG_CONTOUR_W * OG_CONTOUR_H];  /* native order */
    int            loaded;
} og_contour;

/* Split a cell into its record offset and indirection flag. */
#define OG_CONTOUR_INDIRECT(v)  ((v) & 1u)
#define OG_CONTOUR_OFFSET(v)    ((v) & ~1u)

int og_contour_load(const char *dir, int index, og_contour *out);

/* ------------------------------------------------------------------ */
/* Surface records: POLY.BIN and QUAD.BIN                              */
/* ------------------------------------------------------------------ */

/* The contour grid does not hold terrain types -- it holds references
 * to surface patches, and those patches store GRADIENTS which the game
 * integrates into an absolute height field.  Traced in GAME.EXE at
 * image 0x1e9b, which dereferences the two buffer segments kept at
 * DS:0x17e and DS:0x189 -- the words immediately preceding the
 * "poly.bin" and "quad.bin" filename strings at DS:0x180 / DS:0x18b.
 *
 * NOTHING IS LEFT UNREAD in these three.  Each CONTOURn.XX is exactly
 * 3872 bytes, 44 x 44 words with no header or tail.  A cell word spends
 * bit 0 on the indirect flag and bits 1..13 on the offset, which reaches
 * 13256 -- the far end of QUAD -- so no bits are spare.  Every one of
 * QUAD's corner words lands inside POLY, and all 1823 POLY records are
 * reached from the eight tracks, so there is no unused record either.
 * Within a POLY record all four bytes are accounted for, below.
 *
 *   POLY.BIN  7292 bytes = 1823 records of 4 bytes
 *       +0  u8   the surface: 0 ordinary ground, 2 WATER, 0x80
 *                IMPASSABLE.  Those are the only three values in the
 *                file (582, 115 and 1126 records).  0x1949 tests bit
 *                0x80 and jumps to the collision path; the same path is
 *                taken when a gradient exceeds the limit at [0x53e0], so
 *                a wall and an unclimbable slope behave alike.
 *       +1  u8   the surface NORMAL's direction, in sixteenths -- every
 *                value in the file is even.  The collision code
 *                subtracts it from the truck's heading to get the angle
 *                of impact, and 0x1b28 takes its high nibble through the
 *                table at DS:0x5248 to choose the response recorded in
 *                [0x4f7a].
 *       +2  i8   gradient along +X   (-60..60)
 *       +3  i8   gradient along +Y   (-60..60)
 *
 *   QUAD.BIN  13264 bytes = 1658 records of 8 bytes
 *       four u16 POLY offsets -- one per corner, which is what the
 *       name refers to.  The integrator picks a specific corner per
 *       pass: slot +2 for the current cell and +4 for the next cell on
 *       the X walk, slots +0 and +6 for the Y step between rows.
 *
 * A contour cell's low bit selects between them:
 *       bit 0 clear -> the value IS a byte offset into POLY.BIN
 *       bit 0 set   -> mask it off, index QUAD.BIN, take the word at
 *                      +2 to reach POLY
 *
 * Verified against CONTOUR0: of 1936 cells, 1636 are even and every one
 * is a POLY offset divisible by 4; the 300 odd cells all mask to QUAD
 * offsets divisible by 8; and every QUAD +2 word resolves to a legal
 * POLY offset (0 invalid of 126 distinct).
 *
 * Integration, per the routine at image 0x1eab: a 16-bit accumulator
 * starts at 0x8000.  Each cell stores the accumulator's HIGH BYTE
 * first, then adds two X gradients (POLY byte +2, scaled by 128 via
 * shl 8 / sar 1) -- the current cell and the next one along the row.
 *
 * At the end of each row the routine does `pop dx`: everything the row
 * accumulated is DISCARDED and the datum restored, so X gradients never
 * leak between rows.  The datum then steps by two Y gradients from POLY
 * byte +3.  Getting this wrong produces a nearly flat field.
 *
 * HOW THE GAME INDEXES IT  (traced at image 0x190a)
 * -------------------------------------------------
 * The grid is NOT sampled per screen pixel.  Positions are 16-bit
 * fixed point per axis, and the HIGH byte is the cell index while the
 * LOW byte is the fraction within that cell:
 *
 *     cell_x = x >> 8      frac_x = x & 0xff
 *     cell_y = y >> 8      frac_y = y & 0xff
 *
 * At run time the game expands the grid to 3 bytes per cell -- the u16
 * contour cell followed by the integrated height byte -- based at
 * DS:0x5402 with a 132-byte row stride (44 * 3), plus a row offset
 * table at DS:0x6ab2 which sits immediately after the array and holds
 * exactly base + row*132:
 *
 *     record = 0x5402 + cell_y * 132 + cell_x * 3
 *
 * So ONE CELL IS 256 WORLD UNITS and the 44x44 grid spans 0x2C00 units
 * on each axis.  That is why fitting the grid to the artwork by cell
 * size and offset never correlated: the mapping runs through world
 * coordinates, not screen pixels.
 *
 * The fractions are not just interpolation weights -- they choose which
 * QUAD corner applies, i.e. which triangle of the cell the point is in:
 *     frac_x + frac_y does NOT carry -> corner slot +4
 *     frac_x >= frac_y               -> corner slot +2
 * (0x1930: add al,dl ; jb skips the +4.  The arcade agrees: its slots
 * are the same four triangles, its Y running the other way.)
 * then the gradient is scaled by the fraction.
 *
 * POLY byte +0 is a flags byte; bit 0x80 sends the sampler down a
 * separate path (image 0x194c).
 *
 * WHO ASKS: the physics samples FOUR points per truck (image 0x1558),
 * at binary angles 0x2000, 0x6000, 0xa000 and 0xe000 -- 45, 135, 225
 * and 315 degrees, i.e. the four wheels -- each with its own 14-byte
 * state block in the truck structure at offsets 0x8a, 0x98, 0xa6, 0xb4.
 * Per-wheel ground height is what produces pitch and jumps.
 *
 * STILL UNKNOWN: the world-unit to screen-pixel scale.  Everything
 * above is in world units; converting to where a hill appears on the
 * 320x200 track needs the position-to-screen step in the draw path. */

typedef struct {
    unsigned char *poly;
    size_t         poly_len;
    unsigned char *quad;
    size_t         quad_len;
    int            loaded;
} og_surface;

#define OG_POLY_REC 4
#define OG_QUAD_REC 8

int  og_surface_load(const char *dir, og_surface *s);
void og_surface_free(og_surface *s);

/* Resolve a contour cell to a POLY record offset, or -1 if invalid. */
long og_surface_resolve(const og_surface *s, unsigned short cell);

/* Gradients of a resolved cell; returns OG_OK on success. */
int  og_surface_gradient(const og_surface *s, unsigned short cell,
                         int *gx, int *gy);

/* Integrate the grid into 44*44 heights (0..255). */
int  og_contour_heights(const og_contour *c, const og_surface *s,
                        unsigned char *out);

/* ------------------------------------------------------------------ */
/* World -> screen projection                                          */
/* ------------------------------------------------------------------ */

/* READ FROM THE CODE, not fitted.  GAME.EXE image 0x161c / 0x165f,
 * which runs when a pickup spawns and converts its world position to
 * the screen position stored at [si+0x2a] / [si+0x2e]:
 *
 *   screen_x = (world_x >> 5) + (0x2C00 - world_y) / 96    + origin_x
 *   screen_y = (0x471 * world_y) >> 16 + (128 - height)     + origin_y
 *
 *   - X is 8 px per contour cell (256 world units / 32).
 *   - The (0x2C00 - world_y)/96 term is a SHEAR: 2.67 px of X per cell
 *     of Y, +117 px at the top of the grid and 0 at the bottom.  That
 *     is the lean visible in every track's outer rail.  (0x2C00 is the
 *     grid's world extent, 44 * 256.)  In the code it is computed as
 *     (D * 0x5555 >> 16) >> 5, i.e. D / 3 / 32.
 *   - Y is 4.44 px per cell (0x471 / 65536 * 256).
 *   - ELEVATION LIFTS THE IMAGE: one pixel per height unit above the
 *     128 datum.  This is why no 2D mapping of the flat grid could ever
 *     fit the artwork -- screen Y depends on the height field.
 *   - origin_x / origin_y are per track, read at track load (0x1e7e)
 *     from word tables at DS:0x53e2 and DS:0x53f2 indexed by the track
 *     number at DS:0x1a0.  X origin is -70 or -72 on every track.
 *
 * Verified with no free parameters: 270 of the 287 AI waypoints across
 * all eight tracks project onto the racing surface, and an independently
 * measured money bag lands within ~5 px on both axes.  The remainder is
 * the racing line clipping kerb stripes that the artwork-derived
 * barrier map counts as solid. */

#define OG_ORIGIN_X_DS  0x53e2
#define OG_ORIGIN_Y_DS  0x53f2

int  og_track_origin(const char *dir, int track, int *ox, int *oy);
void og_project(long wx, long wy, int height, int ox, int oy,
                int *sx, int *sy);

/* ------------------------------------------------------------------ */
/* AI routes                                                           */
/* ------------------------------------------------------------------ */

/* The computer drivers navigate a per-track route list held in
 * GAME.EXE's data segment, NOT in the contour files.  A pointer table
 * at DS:0x7550 is indexed by the track number (DS:0x19f).
 *
 * The list is a little program.  A byte below 0x80 begins a two-byte
 * waypoint -- (cell_x, cell_y) on the same 44x44 grid the elevation
 * data uses, which is why the sampler builds coordinates as byte<<8.
 * A byte of 0x80 or above is a command:
 *
 *     0x80,0x82  5 bytes total     0x85       1
 *     0x81       4                 0x86       2  -> corner radius
 *     0x83       3  (conditional)  0x87       2  -> corner width
 *     0x84       3  RELATIVE JUMP  0x88,0x89  3  (direction-gated)
 *
 * 0x84 adds the following signed word to the cursor, which is how a
 * circuit closes and how alternate branches rejoin.  0x86 stores
 * (b>>1)^2 and 0x87 stores (b+1)*2 as the active corner parameters.
 *
 * Two dispatch tables exist: DS:0x7500 (jmp) during the nearest-point
 * scan, and DS:0x7519 (call) while advancing the cursor.
 *
 * NOTE: the command lengths above are read from the 0x2253 inc-si
 * fall-through chain and reproduce coherent circuits for seven of the
 * eight tracks; track 0 still desynchronises, so at least one length
 * is not yet right. */

#define OG_GAME_DS_IMG     0x5700   /* DS base within the load image  */
#define OG_ROUTE_TABLE_DS  0x7550
#define OG_ROUTE_MAX       512

/* A route is a GRAPH, not a loop.  Besides fall-through from one
 * waypoint to the next there are two kinds of edge:
 *
 *   0x84 <disp16>                 unconditional jump
 *   0x80/0x82 <c1> <c2> <disp16>  conditional branch
 *   0x81 <c> <disp16>             conditional branch
 *
 * The target is always (address of the disp16 word) + disp, and it
 * lands exactly on a waypoint -- verified for all 171 branches and
 * jumps across the eight tracks.  Figure-of-eight tracks such as
 * Huevos Grande depend on this: the conditional branches choose which
 * half of the crossover a truck takes, and whole sections of the course
 * are reachable ONLY through a branch.  Walking the list as one loop
 * and stopping at the first revisited offset loses them.
 *
 * BOTH BRANCH CONDITIONS are range tests on a truck byte, differing only
 * in which byte: 0x80 (handler 0x22b9) calls 0x238d, which reads the
 * CHECKPOINT at [di+0x41]; 0x82 (handler 0x22e0) calls 0x2392, the
 * sibling entry that reads [di+0x5b] -- the RUBBER-BAND LEVEL, set to 6
 * at the start and rewritten at 0x2385 whenever the band moves a truck,
 * which also reloads its performance.  So a route can steer by where a
 * truck is and by how strong it currently is.  The branch is taken when
 * the byte lies inside the opcode's two-byte range, wrapping when the
 * low bound exceeds the high one.
 *
 * The older note below is kept for the shape of the encoding; og_route
 * records every edge and computes one full lap as the longest simple
 * cycle from the start; that covers both halves of a crossover but is
 * a reconstruction of the lap order, not the game's own choice. */

#define OG_ROUTE_MAXSUCC 4
#define OG_ROUTE_STREAM  1024
enum { OG_EDGE_NEXT = 0, OG_EDGE_JUMP = 1, OG_EDGE_BRANCH = 2 };

typedef struct {
    unsigned char  cx, cy;      /* cell coordinates, 0..43            */
    unsigned short radius;      /* from 0x86, (b>>1)^2                */
    unsigned short width;       /* from 0x87, (b+1)*2: NOT a corridor
                                 * -- it is the TARGET THROTTLE for the
                                 * segment ([di+0x52], compared against
                                 * [di+0x75] at 0x21e2), which is how a
                                 * computer truck lifts off for a corner.
                                 * 256 nearly everywhere, less at corners */
    unsigned char  rubber;      /* 0x85 sits here: re-weigh the band   */
    unsigned char  nitro_lo, nitro_hi;  /* 0x83: the gates where the AI
                                 * fires nitro, a checkpoint range tested
                                 * at 0x238d -- 00,ff means anywhere     */
    unsigned short off;         /* offset within the route stream     */
} og_waypoint;

typedef struct {
    og_waypoint   wp[OG_ROUTE_MAX];
    int           n;
    int           closed;       /* the lap returns to its start       */
    short         succ[OG_ROUTE_MAX][OG_ROUTE_MAXSUCC];
    unsigned char kind[OG_ROUTE_MAX][OG_ROUTE_MAXSUCC];
    unsigned char nsucc[OG_ROUTE_MAX];
    short         lap[OG_ROUTE_MAX];   /* one full lap, waypoint indices */
    int           nlap;
    /* the raw route program, kept for the AI walk */
    unsigned char stream[OG_ROUTE_STREAM];
    int           stream_len;
    short         idx_at[OG_ROUTE_STREAM];  /* offset -> waypoint, or -1 */
} og_route;

/* ------------------------------------------------------------------ */
/* Checkpoints and the AI's route walk                                 */
/* ------------------------------------------------------------------ */

/* BRANCH CONDITIONS, decoded from GAME.EXE's cursor-advance handlers
 * (dispatch table DS:0x7519, walker at image 0x229b):
 *
 *   0x80 c1 c2 d   branch if [di+0x41] in [c1,c2]     (test at 0x238d)
 *   0x82 c1 c2 d   branch if [di+0x5b] in [c1,c2]     (test at 0x2392)
 *   0x81 c     d   branch if c < high byte of the RNG  (random choice)
 *   0x89       d   branch if [di+0x77] != 0
 *   0x84       d   always jump
 *
 * The range test WRAPS: if c1 > c2 it passes when v >= c1 OR v <= c2.
 *
 * [di+0x41] is the truck's CHECKPOINT INDEX.  It is zeroed at the start
 * (image 0x1f7c) and incremented each time the truck enters the next
 * checkpoint box (0x1f9c).  That is what makes a figure-of-eight work:
 * the truck reaches the same crossover twice a lap with a different
 * number of checkpoints behind it, so one branch sends it round the top
 * loop the first time and down the other half the second.  On Huevos
 * Grande the crossover at (24,22) is reached at checkpoint 4 (falls
 * through) and at checkpoint 10 (taken).
 *
 * Checkpoints are 7-byte records per track, via the pointer table at
 * DS:0x6b1a, ended by bit 0x80 of byte 6 (crossing test at 0x1fdc):
 *
 *   +0 x_min  +1 x_max (exclusive)  +2 y_max  +3 y_min   (cells)
 *   +4 lap progress at the start of this gate
 *   +5 arc length of a bend (0 on straight gates)
 *   +6 low nibble: how progress is measured INSIDE the gate
 *                  (handlers via DS:0x725d, image 0x2009..0x20c5);
 *      bit 0x80:   finish line
 *
 * Entering the NEXT box in sequence is what counts as crossing it; the
 * edge code does not gate that.  It gives a continuous progress value,
 * stored at [di+0x42] and used to rank race positions:
 *
 *   0  base + (x - x_min)        forwards is +X
 *   1  base + (y_max - y)        forwards is -Y
 *   2  base + (x_max - x)        forwards is -X
 *   3  base + (y - y_min)        forwards is +Y
 *   4..11  a bend: the angle of the truck round one corner of the box,
 *          from the table-driven atan2 at 0x23e2, then
 *          base + ((angle * arc * 4) >> 8)
 *          -- four corners x two senses of rotation, which is exactly
 *          what running a bend the other way round needs.
 *
 * Direction is therefore carried by the gate ORDER plus these codes. 
 *
 * Simulated, this walk completes laps on all eight tracks with every
 * checkpoint crossed in order.  [di+0x5b] (only track 0 uses 0x82) and
 * [di+0x77] are not identified and are taken as 0, their values on a
 * clean lap. */

/* LAYOUTS.  The route table (DS:0x7550) and checkpoint table (DS:0x6b1a)
 * each hold SIXTEEN entries: eight tracks x two directions.  Read from
 * the code at image 0x1ed:
 *
 *     al = [0x19e]            race number in the championship
 *     xlatb from DS:0x1a3     -> layout 0..15
 *     [0x19f] = al            layout: route + checkpoints
 *     and al,7 ; [0x1a0] = al physical track: art, spawns, origin
 *
 * So physical track = layout & 7 and bit 3 is DIRECTION: layout n+8 is
 * track n raced in reverse.  A reversed course is not the forward one
 * read backwards -- it has its own route program and its own gates
 * (layout 15 shares 9 of its 14 gate boxes with layout 7), and its first
 * gate measures progress towards -X where the forward one measures +X.
 * The table at DS:0x1a3 is the championship schedule, race -> layout. */
/* ------------------------------------------------------------------ */
/* Truck attitude -> sprite frame  (GAME.EXE image 0x1de5 .. 0x1e7d)   */
/* ------------------------------------------------------------------ */

/* The game picks the truck's frame from its four wheel heights:
 *
 *   pitch = (front pair - rear pair), roll = (right pair - left pair),
 *           each in whole height units, + 10, clamped 0..16, >> 2
 *           -> 0..4 with 2 = level          (front = wheels at +-45 deg)
 *   combo = pitch * 5 + roll                 -> 25 attitudes
 *   base  = word[DS:0x511c + combo*4]
 *   table = word[DS:0x511e + combo*4]        -> 32 bytes
 *   dir   = (-heading_byte & 0xff) >> 3      -> 32 directions
 *   frame = base + table[dir]                -> stored at [di+0x87]
 *
 * The negation is why the sprite sweep runs against the angle.  Only the
 * level attitude has 32 frames (0..31); twenty tilted ones have 16 (one
 * frame per two directions) and the four extreme corners 15, which is
 * 32 + 20*16 + 4*15 = 412 -- the whole ETRUCK bank. */
#define OG_TRUCK_ATT_DS  0x511c
#define OG_ATTITUDES     25

typedef struct {
    short frame[OG_ATTITUDES][32];   /* [pitch*5+roll][direction] */
    int   loaded;
} og_truck_attitudes;

int og_truck_attitudes_load(const char *dir, og_truck_attitudes *out);

/* whole-unit height difference -> tilt index 0..4, as image 0x1e01 */
int og_tilt_index(float diff);

/* frame for a tilt and a heading byte (0..255, world angle) */
int og_truck_frame(const og_truck_attitudes *a, int pitch, int roll,
                   int heading_byte);

/* ------------------------------------------------------------------ */
/* Vertical physics: every wheel is a point mass (GAME.EXE 0x1871..)   */
/* ------------------------------------------------------------------ */

/* Each wheel block holds, in 8.8 fixed point:
 *   +4 its height   +6 its vertical velocity   +8 the ground under it
 *   +10 clearance above ground   +12 compression on landing
 *
 * One physics step, as the truck update at image 0x1540 runs it:
 *   1. every wheel: height += velocity                        (0x18be)
 *   2. place the wheel and sample the ground                  (0x18df)
 *      airborne (ground below): velocity -= 1.0 -- gravity    (0x19a3)
 *      touching (ground at/above): velocity = bounce, where
 *        rising  : bounce = velocity + penetration * k
 *        falling : bounce = max(penetration, |velocity|) * k
 *        below 2.0 -> 0, capped at 8.0;  height = ground     (0x19ed)
 *      a landing faster than 2.5 down calls 0x1b96 (effects)
 *   3. rigid chassis: if the diagonal pairs' mean heights differ by 1.0
 *      or more, raise the lower pair by half the gap           (0x1d73)
 *   4. [di+0x7d] = mean wheel height (the truck, which flies)
 *      [di+0x7f] = mean ground height (where its shadow sits)
 *      pitch / roll from the wheel heights                    (0x1de5)
 *
 * k is [di+0x63], from 0x147c: the SHOCKS upgrade, better shocks bouncing
 * less; computer trucks use an 18-level difficulty table instead, indexed
 * by [di+0x5b] -- the same byte route opcode 0x82 tests.  A truck counts
 * as airborne once flight height reaches a per-track threshold [0x4f78].
 *
 * The main loop waits for three ticks of a timer phase-locked to the
 * vertical retrace (0x4d3c, 0x0146), so it steps every third video frame:
 * 20 steps per second at the EGA's 70 Hz. */
#define OG_PHYS_HZ       20.0f
#define OG_SHOCKS_DS     0x4f7c
#define OG_AI_BOUNCE_DS  0x4fe0
#define OG_AIR_DS        0x5240

typedef struct {
    float shocks[6];      /* k by shocks upgrade level, as a multiplier */
    float ai[18];         /* k by computer difficulty                   */
    float air[8];         /* airborne threshold per track, height units */
    int   loaded;
} og_physics;

int og_physics_load(const char *dir, og_physics *out);

/* ------------------------------------------------------------------ */
/* Truck shadows: ESHADOW.PCG                                          */
/* ------------------------------------------------------------------ */

/* 32 silhouettes, one per direction, at sprite-table entries 412..443
 * (straight after ETRUCK's 412): 24 px wide, ONE bitplane, rows from the
 * table -- 514 rows x 3 bytes = the whole 1542-byte file.  Shadow i
 * matches level truck frame i for 31 of 32 directions (the odd one is a
 * near tie), so it takes the same direction as the truck:
 *     shadow = (-heading_byte & 0xff) >> 3.
 *
 * It carries no colour.  The palette pairs every colour with a darker
 * twin -- 2n and 2n+1: dirt 8/9, water 2/3, red 10/11, white 12/13 -- and
 * a shadow sets bit 0 of whatever lies beneath, turning each colour into
 * its twin.  So one set of shadows is dark brown on dirt and dark navy on
 * water.  In captured gameplay every pixel of a truck's visible shadow
 * over dirt is exactly art | 1; no capture shows one over water, so that
 * half follows from the rule rather than being observed. */
#define OG_SHADOW_FIRST  412
#define OG_SHADOWS       32

typedef struct {
    unsigned char *mask[OG_SHADOWS];   /* 24 x rows, 1 = in shadow  */
    int            rows[OG_SHADOWS];
    int            top[OG_SHADOWS];    /* rows into the 32-row box  */
    int            xoff[OG_SHADOWS];   /* x offset into the box     */
    int            loaded;
} og_shadows;

int  og_shadows_load(const char *dir, og_shadows *out);
void og_shadows_free(og_shadows *s);

/* In-game effects: EGAME.PCG.  Sprite-table entries 444..559, 116 sprites
 * of 8, 16 or 32 px, FOUR bitplanes per row (plane-major within the row),
 * stored COMPLEMENTED like ETRUCK (index 15 is the clear one) -- 9268
 * bytes, the whole file.  By
 * appearance: 0-1 the money bag and nitro canister, 2-26 the flagman,
 * 27-31 the flag, 62-69 a red/blue burst (very likely the nitro blast),
 * 70-76 a dark splatter, 77-89 score digits and "NITRO" labels, 92-115
 * water spray.  FOUR of them, chosen at 0x1c08:
 *
 *     bl = 0 for the wheels at offsets 0xa6 and 0x8a, else 1
 *     q  = (heading + 0x20) & 0xc0          -- the quadrant faced
 *     if !(q & 0x40)  bl |= 2
 *     if  (q & 0x80)  bl ^= 1
 *
 * so bl only ever reaches 0..3, and the table at DS:0x53c0 holds four
 * eight-byte descriptors (0x31bc, 0x31b8, 0x31c0, 0x31c4 with their
 * offsets); what follows them is other data, not more variants.
 *
 * Those two wheel offsets are not the front pair: the wheel loop at
 * 0x1552 walks (offset, angle) pairs -- 0x8a at 225 degrees, 0x98 at
 * 135, 0xa6 at 315, 0xb4 at 45 -- so 0xa6 and 0x8a are the 225 and 315
 * degree wheels, indices 2 and 3 in a 45/135/225/315 array.  The spray
 * leans with the truck's quarter of the compass. */
/* The last two banks, both FOUR bitplanes but stored PLAIN -- unlike
 * etruck and egame they are not complemented, which a shop panel matched
 * against captured gameplay confirms exactly.  eshop builds the Speed
 * Shop between races: panel frames, "Insert Coin", national flags, the
 * IRON MAN banner, and the upgrade panels with their level bars (nitro
 * 10K, tires 40K, shocks 60K, accel 80K, top speed 100K) plus START NEXT
 * RACE.  eend is the champion screen: a driver with the trophy in each
 * of the four suit colours, the SUPER OFF ROAD banner and a signature. */
/* The Speed Shop, shown between races.  It is built from scripted
 * objects like the track scenery: the banner across the top (tag "ts",
 * sprite 611 at 0,0 -- ten 32 px pieces, 24..33, exactly 320 wide), then
 * one panel per player, each bound to a truck record (RP -> 0x4c68,
 * YP -> 0x4d2a, BP -> 0x4dec) with its player number in field 0x1e.
 *
 * A panel's children carry their offsets WITHIN the panel in fields 0x36
 * and 0x3a, which is how the same descriptors serve all three columns:
 *
 *     credits "CB" (3,5)    cash "cb" (38,5)
 *     nitro   "nb" (2,30)   accel     "ab" (49,30)
 *     tires   "tb" (2,76)   top speed "tb" (49,76)
 *     shocks  "sb" (2,122)  start     "Sb" (49,122)
 *
 * with each box 32 px wide and its level bar 16 px, immediately right at
 * x+32.  The three panels stand at (8,31), (112,31) and (216,31) -- 104
 * apart -- which reproduces a captured shop screen exactly.  Player 1 is
 * always red, 2 yellow, 3 blue; the fourth truck is the computer's and
 * has no panel. */
#define OG_SHOP_PANEL_X0 8
#define OG_SHOP_PANEL_DX 104
#define OG_SHOP_PANEL_Y  31
#define OG_SHOP_BANNER   24        /* eshop 24..33 across the top      */
/* An object draws field 0x8 consecutive sprites, left to right, each at
 * the next one's width -- the count is absent (so one) for the credits
 * box and the flags, 2 for the cash box (54+55), 2 for every upgrade box
 * (its 32 px face plus the 16 px level bar), 2 for START NEXT RACE
 * (64+65, the second piece carrying the track outline under the text)
 * and 10 for the banner, which is exactly the 320 px width.  op 39
 * (image 0xaf2) is what puts a child at its parent's position plus its
 * own offset, from fields 0x34/0x36 and 0x38/0x3a. */
/* The level bar's 16 px piece carries the surround; the gauge inside it
 * is drawn separately (see OG_SHOP_GAUGE_X below). */
#define OG_SHOP_NITRO_MAX 99
#define OG_SHOP_BAR_DX   32        /* the level bar sits right of a box */
/* The credits and the money are ONE page per panel, DS:0x9574, 0x95a5
 * and 0x95d6 (the panel's [+0x10]), which op 78 (0x2d53 -> 0x2e88)
 * rewrites and draws:
 *
 *   +00 17 "IRON"  18 "W3"  10  0e ff      IRON, W3, proportional, -1
 *   +0b 04 xx xx yy                        at(panel x + 17, y + 13)
 *   +0f "^" 05 00 10 "^^^^^^^" 05 ff fc "^" erase both insets
 *   +1e 02 xx xx  +21 '0'                  x = panel x + 17; the credits
 *   +22 02 xx xx  +25 "$00,000,000"        x = panel x + 92 - width
 *
 * '^' is IRON's solid block of value 15 -- W3's outline colour, black --
 * so the eight blocks repaint the black insets: the credits' x 17..24
 * and the money's 40..92, y 13..24.  The credits are ONE character, '0'
 * + [di+1].  The money's six bytes at +25 are blanked, then the BCD word
 * [di+0xf] is laid in backwards from +2a, a digit a nibble while any is
 * left, a comma before the third; then '$'.  The "0,000" after +2a is
 * the template's own -- the units are $10,000 -- so no money reads
 * "$0,000".  The string from +25, leading blanks and all, is measured
 * in IRON at spacing -1, proportional (0x2eed), and placed to END at
 * panel x + 0x5c.  The panel is its [+0x1c] position, the origin the
 * gauges are placed from (0x2d40). */
#define OG_SHOP_MONEY_PAGE    0x9574
#define OG_SHOP_MONEY_STRIDE  0x31
#define OG_SHOP_MONEY_AT      0x0c   /* x word (big-endian), y byte       */
#define OG_SHOP_MONEY_CRED_X  0x1f
#define OG_SHOP_MONEY_CRED    0x21
#define OG_SHOP_MONEY_CASH_X  0x23
#define OG_SHOP_MONEY_CASH    0x25
#define OG_SHOP_MONEY_LAST    0x2a   /* the money's last digit, laid first */
#define OG_SHOP_MONEY_DX      0x11
#define OG_SHOP_MONEY_DY      0x0d
#define OG_SHOP_MONEY_RIGHT   0x5c
/* W3 (fill 10, outline 14) on this screen, whose indices are the
 * complement of the hardware's (15 - v), as the results screen's are:
 * white and black */
#define OG_SHOP_MONEY_FILL    5
#define OG_SHOP_MONEY_EDGE    1
/* The gauges are separate objects, not a repaint of the sprite: op 77
 * (0x2c2f) places one per box -- panel + (38,31) for nitro, (85,42),
 * (38,88), (85,88), (38,134) for the upgrades -- and EGA.EXE 0x65ab
 * fills them, 7 px wide, in hardware colour 12 (empty, on top) then 5
 * (full, below) -- SORP's 3 and 10, see below.  Nitro is one 40-row column, a row per nitro; an
 * upgrade is five 5-row blocks with a row between. */
#define OG_SHOP_GAUGE_X  4         /* the gauge's column, in the bar     */
#define OG_SHOP_GAUGE_W  7
#define OG_SHOP_GAUGE_Y  12        /* an upgrade's, below "LEVEL"        */
#define OG_SHOP_GAUGE_SEG 5
#define OG_SHOP_GAUGE_RUNS 5
#define OG_SHOP_NITRO_Y  1         /* nitro's: the bar has no heading    */
#define OG_SHOP_NITRO_ROWS 40
/* The driver writes hardware colours 12 and 5, but SORP's ESHOP pixels
 * are the COMPLEMENT of the hardware's, 15 - v, as the results screen's
 * are (the picture decoder inverts every plane) -- so in this palette
 * the empty gauge is 3, the bar's own grey, and the full part 10, green.
 * (Bit reversal gives the same two, but only the complement makes the
 * text's outline, 14, the black the insets are: see OG_SHOP_MONEY_EDGE.) */
#define OG_SHOP_GAUGE_EMPTY 3
#define OG_SHOP_GAUGE_FULL  10
#define OG_SHOP_PANEL_W  96        /* each panel is 96 wide in its own   */
#define OG_SHOP_PANEL_H  169       /* colour -- player 1 red, 2 yellow,  */
                                   /* 3 blue -- and runs to the bottom   */

/* The next track's thumbnail sits in the START NEXT RACE box: eshop 37
 * onwards, two pieces per track (32 px + 16 px = the box's 48), so
 * track t uses 37 + 2t and 38 + 2t.  It is drawn one pixel right and 17
 * down from the box, which a captured screen showing Huevos Grande --
 * sprite 51, matching to the pixel -- puts at (58,170). */
#define OG_SHOP_THUMB    37        /* track 0; +2 per track             */
#define OG_SHOP_THUMB_DX 1
#define OG_SHOP_THUMB_DY 17

/* eshop sprite numbers, relative to the bank */
#define OG_SHOP_CREDITS  53
#define OG_SHOP_CASH     54
#define OG_SHOP_NITRO    56
#define OG_SHOP_TIRES    58
#define OG_SHOP_TOPSPD   60
#define OG_SHOP_SHOCKS   62
#define OG_SHOP_START    64
#define OG_SHOP_ACCEL    66
/* Sprites 68 and 69 are a 48x15 strip reading "START NEXT / RACE" with
 * the colours inverted -- the start box's label, highlighted.  The game
 * swaps it in once that player has chosen to start, which is how a
 * player who is READY is shown while the others still shop. */
#define OG_SHOP_START_HI 68
#define OG_SHOP_START_HI_W 2       /* two pieces, 32 + 16 */
#define OG_SHOP_START_HI_DY 2      /* where the strip sits in the box */

/* ESHOP's first sixteen sprites are the REGISTRATION CURSOR: eight
 * shapes, then the same eight again --
 *
 *     0, 1, 2, 3   32x9    a wide cell, four pieces
 *     4, 5         16x9    a narrow cell, for one letter of the grid
 *     6            32x46   a wide column
 *     7            16x46   a narrow column
 *     8..15        the same eight, the cursor's other frame
 *
 * so the box is drawn from the pair that fits what is being chosen, and
 * alternating the two halves is what makes it flash. */
#define OG_ESHOP_FIRST   587
#define OG_ESHOP_CURSOR_SHAPES 8
#define OG_ESHOP_COUNT   70
/* The results screen, from eend.  Its objects: "bn" tiles the banner
 * across the top -- three pieces, 32+32+16 = 80 px, repeated four times
 * -- and "BL" tiles the checkered backdrop below it in 48 px columns
 * (30+31).  A podium place is six sprites: three for the driver and his
 * companion at y=25 ("TR", which carries that y), then three for the
 * plaque at y=145 ("PL").  The four colour groups run white 0, blue 6,
 * yellow 12, red 18, each followed by its own plaque:
 *
 *     white  figures 0..2   plaque 3..5    (grey plaque)
 *     blue   figures 6..8   plaque 9..11
 *     yellow figures 12..14 plaque 15..17
 *     red    figures 18..20 plaque 21..23
 *
 * The three columns stand at x = 13, 121 and 229, which a captured
 * screen confirms.
 *
 * Each piece of a figure carries its own TOP offset in the sprite table
 * and must be placed at y + top, not flush: a group runs 0, 9, 28 down,
 * which is why a captured screen has the first piece at y=25 and the
 * second at y=34.  Ignoring it splits the driver from his companion.
 * The pieces are 32+32+16 wide for three of the groups and 32+32+32 for
 * the first, so a group is 80 or 96 px across. */
#define OG_END_BANNER    25        /* 25,26,27 -- an 80 px tile        */
#define OG_END_BANNER_W  80
#define OG_END_BACK      30        /* 30,31 -- the solid blue field    */
#define OG_END_SIGN      28        /* 28,29 -- the signature scrawl    */
#define OG_END_BACK_H    48
#define OG_END_BACK_BLUE 8         /* the field colour                 */
#define OG_END_BACK_DARK 1
#define OG_END_BACK_W    48
#define OG_END_BACK_Y    29
/* The layers of the results screen, in the order they are drawn so that
 * each occludes the one before: the Leland field from the top, the
 * orange banner across it, the stage the figures stand on, the three
 * groups of figures, then the winning-time board, the plaques and the
 * flags. */
/* Read from a memory dump taken with the podium on screen:
 *
 *   banner   index 25, four objects at x 0, 80, 160, 240, y 0, 3 pieces
 *   board    index 32 at x 104, y 0, 4 pieces
 *   figures  index  0 at x 13, 121, 229, y 25, 3 pieces
 *   plaques  index  3 at x  7, 115, 223, y 145, 3 pieces
 *   flags    index 36 at x 31, 139, 247, y 122
 *   stage    index 24, THREE objects at x 95, 203, 311, y 154 -- not a
 *            tile across the width
 *   the Leland field: signature pairs on a 48 grid from x 40, y 36
 */
#define OG_END_STAGE     24
#define OG_END_STAGE_Y   154
#define OG_END_STAGE_X0  95
#define OG_END_STAGE_DX  108
#define OG_END_SIGN_X0   40
#define OG_END_SIGN_Y0   36
#define OG_END_BOARD     32        /* 32..35, the time board     */
/* The plaque text, from the data segment while the podium is up:
 *
 *   DS:878f  "$80,000 + $10,000"      third
 *   DS:87a3  "$100,000 + $30,000"     first
 *   DS:87b7  "$90,000 + $20,000"      second
 *
 * -- the place money plus the bonus -- with the drivers' handles at
 * DS:87ce and their names at DS:87ff, and the lap summaries
 * "Avg : xx.xx (Last) " at DS:883a and "Best : xx.xx (Last) " at
 * DS:8876, one of each per column, the xx.xx filled in at runtime. */
/* THE PLAQUE PAGES.  Each place is a complete page in the data segment,
 * carrying its own style and calling sub-pages for the rest:
 *
 *   DS:96eb  font<IRON> spacing=-1 leading=-1 style<E1> "3rd Place"
 *            NL x+=7 subpage[10] NL subpage[13] subpage[17] END
 *   DS:9716  the same with style<E4> and "1st Place"
 *   DS:9741  the same with style<E3> and "2nd Place"
 *
 * so the four lines are the place, then the prize, the nickname and the
 * name as sub-pages.  Their offset table sits just above them at
 * DS:96c4, whose entries (9641, 9716, 97eb, 9616, 9741, 976c, 97a4,
 * 97dc, 9717) are the pages the plaques pull in. */
/* One page per COLUMN, not per place -- each sets its own x: 7, 115, 223.
 * The place and its style are patched in for whoever finished there. */
#define OG_END_PLAQUE_COL1 0x96eb
#define OG_END_PLAQUE_COL2 0x9716
#define OG_END_PLAQUE_COL3 0x9741
/* The three plaque pages are listed in a table at DS:0x96df (at odd
 * alignment): 96eb, 9716, 9741, then 976c, 97a4, 97dc.  Their sub-page
 * calls -- subpage[10], [13], [17] -- index whatever table [0x73a] holds
 * AT THE MOMENT THEY ARE DRAWN, and that is not yet known: the live dump
 * caught [0x73a] pointing at 0x83de, which is inside a page rather than a
 * table, so the engine had moved on.  Until it is known, pass 0 and the
 * sub-pages are skipped: the place line draws, the prize, nickname and
 * name do not. */
/* The sub-page table, installed at startup by 0x42f:0ef9 and kept in
 * [0x73a].  Entries begin 6 bytes in and are relative to the base:
 *   [10..12] the three prize lines   [13..16] MADMAN..IRONMAN
 *   [17..20] Sam Powell..Ivan Stewart                              */
#define OG_END_PLAQUE_TAB  0x83de

/* More pages, read from a dump taken with the podium up (its base is
 * DS:0x8710).  They position themselves, like the plaques:
 *
 *   DS:981c  the finish-time board at the top of the podium
 *   DS:9872  "Save HiScore table"
 *   DS:989c  "The Ironman Club" -- the club screen's own heading
 *
 * and the plaques are patched per column: 96eb carries E1 with "1st
 * Place", 9741 E4 with "2nd Place", 97a4 E5 with "4th Place : ...".
 */
/* The finish-time board, DS:0x981c:
 *
 *     "xxxx Finish Time:  00.00"      at page+16 and +35
 *     "Record (XXX) :      00.00"     at page+52 and +64
 *
 * 0x3428 fills the name of the WINNER -- di = [0x9d2e], the head of the
 * standings, set at 0x33df -- not of any column's truck.
 * For a COMPUTER truck it is the four-character
 * first name from the table at DS:0x9862 (" Sam", "Earl", "John",
 * "Ivan", by truck index * 4); for a HUMAN it is a space followed by
 * that player's three registered initials, [di+5..7].  The same rule
 * belongs on the plaques.
 */
#define OG_END_TIMES_NAME   16
#define OG_END_TIMES_TIME   35
#define OG_END_TIMES_INIT   52
#define OG_END_TIMES_REC    64
#define OG_END_FIRSTNAMES   0x9862   /* " SamEarlJohnIvan", four each */
#define OG_END_PAGE_TIMES   0x981c
#define OG_END_PAGE_SAVE    0x9872
/* The club screen's own pages:
 *   0x989c  font<IRON> style<E5> at(116,12) "The Ironman Club", then
 *           style<E2> spacing=-1 for the rows that follow
 *   0x98c3  FIXED "xx. xxx "        the place and initials
 *   0x98cd  "$xx" , "xx0" , "000"   the money, in three fixed groups
 *   0x98f2  " 1 2 3 4 5 6 7 8 910"  the place numbers, two characters each
 */
#define OG_CLUB_PAGE_TITLE  0x989c
#define OG_CLUB_PAGE_ROW    0x98c3
#define OG_CLUB_PAGE_MONEY  0x98cd
/* The row positions follow the place numbers, as words of y * 4:
 * 76, 128, 180 ... a step of 52, so the rows are 13 apart starting at
 * y 19.  A live dump shows the templates patched in place -- the row
 * reading "10. ABC " and the money split across absolute x codes (162
 * and 170) -- so the game reuses one template per entry. */
/* The draw loop (main segment, 0x34e0) is explicit:
 *
 *     si = 0x989c; draw            the heading
 *     cx = 10                      ten entries
 *     al = 13; mul dl; ax += 0x26  y = 38 + row * 13
 *     [0x97e] = ax                 the pen's Y
 *     [0x97c] = 0x4d               the pen's X = 77
 *     [si+1] = the place pair, [si+5..7] = the initials
 *     draw 0x98c3, then the money through 0x3930
 */
#define OG_CLUB_ROW_Y0      38
#define OG_CLUB_ROW_H       13
#define OG_CLUB_ROW_X       77

#define OG_END_PRIZE_3RD  0x878f
#define OG_END_PRIZE_1ST  0x87a3
#define OG_END_PRIZE_2ND  0x87b7
#define OG_END_AVG        0x883a
#define OG_END_BEST       0x8876
#define OG_END_TEXT_STRIDE 0x14
/* where 0x32b2 and 0x3346 start writing: the sub-page + 6 and + 7 (the
 * "add si, 0x83e4 / 0x83e5"), the first x of "Avg : " and "Best : " */
#define OG_END_AT_AVG      6
#define OG_END_AT_BEST     7
#define OG_END_HANDLES     0x87ce   /* MADMAN, HURRICANE, JAMMIN', IRONMAN */
/* THERE ARE TWO PLAQUE TEMPLATES, chosen at 0x318f by the same flag that
 * picks the name on the time board ([di+2] bit 1):
 *
 *   DS:0x96df  96eb 9716 9741   a COMPUTER truck -- "xxx Place", then
 *                               its nickname and the driver's name
 *   DS:0x96e5  976c 97a4 97dc   a HUMAN -- "xxx Place : xxx", the place
 *                               and the player's initials on one line,
 *                               with no nickname and no driver name
 *
 * So a human's plaque is not the computer's with the name swapped: it is
 * a different page, and the driver name is simply absent. */
#define OG_END_PLAQUE_HUMAN1  0x976c
#define OG_END_PLAQUE_HUMAN2  0x97a4
#define OG_END_PLAQUE_HUMAN3  0x97dc
#define OG_END_NAMES       0x87ff   /* Sam Powell, Earl Stratton, ...      */
/* Where 0x3111 writes into those templates -- fixed offsets, each column
 * rewritten from its own truck before every draw.  [di] is the truck's
 * identity, 0 red .. 3 white, and is what picks the driver:
 *
 *   computer page  +0x0b  '1' + [di]      the style, "E1".."E4"
 *                  +0x16  "1st".."4th"    from DS:0x96b1 by [di+0x3b]
 *                  +0x27  13 + [di]       the nickname sub-page
 *                  +0x29  17 + [di]       the driver-name sub-page
 *   human page     +0x16  the place, as above
 *                  +0x22  [di+5..7]       the registered initials
 *
 * so the white truck, substituted into a column at 0x313d, brings
 * IRONMAN and "Ivan Stewart" with it. */
#define OG_END_AT_STYLE     0x0b
#define OG_END_AT_PLACE     0x16
#define OG_END_AT_HANDLE    0x27
#define OG_END_AT_NAME      0x29
#define OG_END_AT_INITIALS  0x22
#define OG_END_SUB_HANDLE   13
#define OG_END_SUB_NAME     17


#define OG_END_BOARD_X   104
#define OG_END_BOARD_Y   0
/* A column is built by image 0x3111, called with the truck record and
 * the column's x.  It spawns three objects, all taking their frame from
 * the truck's type x 6 -- which is what makes a colour group six sprites:
 *
 *     PL  the plaque, 3 pieces, at x,      y 145 (0x91)
 *     TR  the figures, 3 pieces, at x + 6, y  25 (0x19)
 *     EF  the flag, 1 piece,     at x + 18, y 122 (0x7a), its frame
 *         chosen by a table (0x96a8) from the player's country [di+0x0e]
 *
 * So the plaques sit 6 px left of the figures, which is exactly the gap
 * between the measured plaque at x=115 and figures at x=121. */
#define OG_END_COL_X0    7         /* the plaque column; figures at +6 */
#define OG_END_COL_DX    108
#define OG_END_FIG_DX    6
#define OG_END_FLAG_DX   24        /* flags at 31, 139, 247 */
#define OG_END_FIG_Y     25
#define OG_END_PLAQUE_Y  145
#define OG_END_FLAG_Y    122
#define OG_END_FLAG      36        /* eend 36.. : the country flags    */
/* Which flag a driver flies: his country byte [di+0x0e] goes through the
 * table at DS:0x96a8, whose first nine entries are 4, 1, 5, 3, 0, 2, 6,
 * 7, 8 -- a permutation, so nine countries over nine sprites. */
#define OG_END_FLAG_MAP_DS 0x96a8
#define OG_END_COUNTRIES   9
int og_flagmap_load(const char *dir, unsigned char map[OG_END_COUNTRIES]);
#define OG_END_GROUP     6         /* sprites per colour: 3 + 3        */

#define OG_EEND_FIRST    657
#define OG_EEND_COUNT    45

#define OG_EGAME_FIRST   444
#define OG_EGAME_COUNT   116   /* og_egame_load() is declared with og_bank */

/* ------------------------------------------------------------------ */
/* In-game effects, as GAME.EXE spawns and animates them               */
/* ------------------------------------------------------------------ */

/* Effects are script-driven objects.  0x6cf takes one from a free list,
 * runs an INIT script of (field, value) pairs ending 0x007a -- field 0x04
 * a two-letter tag, 0x5a the first sprite -- and then an ANIMATION script
 * that the per-tick loop (0x78b) steps.  Its first instruction, op 31,
 * names a frame list: words of (frame | duration << 8), each frame shown
 * for duration + 1 ticks, ended by a negative command (-3 = finished).
 * On finishing, op 33's handler sends the script to its cleanup, which
 * uncounts the effect and frees it.  At most 16 run at once ([0x618]).
 *
 *   dust    [0x31b0]  tag DU, sprites 514..520 (EGAME 70-76), called by
 *                     0x1b96 when a wheel lands faster than 2.5 on any
 *                     surface except water; placed at the wheel less
 *                     (0x100, 0x80) world units
 *   splash  0x53c0    tag SP, four variants (EGAME 92-95, 96-99, 100-107,
 *                     108-115), called by 0x1bee when a wheel lands in
 *                     water (POLY flag byte 2) faster than 0.375, with
 *                     sound effect 7.  The variant follows which wheel
 *                     and which quarter the heading is in; each record is
 *                     {pointer, dx, dy, 0}, an offset from the wheel
 *   nitro   [0x31b4]  tag NI, EGAME 62-69; its trigger is not traced yet
 *
 * Effects are projected at the ground height under the wheel, and drawn
 * in a list sorted by their screen-space Y ([si+0x18]). */
#define OG_FX_PTRS_DS    0x31b0
#define OG_FX_SPLASH_DS  0x53c0
#define OG_FX_MAXFRAMES  48

typedef struct {
    char          tag[3];                 /* init field 0x04: "DU" ...  */
    int           sprite;                 /* table index of frame 0     */
    int           n;
    unsigned char frame[OG_FX_MAXFRAMES];
    unsigned char dur[OG_FX_MAXFRAMES];   /* shown for dur + 1 ticks    */
} og_fx_anim;

/* The nitro burst is placed by octant (image 0x17e1): the index is
 * (heading + 0x20) >> 5 & 7, into eight (dx, dy) pairs at DS:0x53a0.
 * They run 512..1024 units up and left, which is what puts the burst
 * behind the truck once the sprite's own down-right offset is applied. */
/* ------------------------------------------------------------------ */
/* The route program's opcodes, all of them                            */
/* ------------------------------------------------------------------ */

/* The interpreter dispatches through a table of ten handlers at
 * DS:0x7519, so opcode 0x80 + i uses entry i.  The mapping is anchored by
 * two that were already known: entry 6 is the radius (shr al,1; mul al
 * into [di+0x50]) and entry 7 the width, which are opcodes 0x86 and 0x87.
 *
 *   0x80  0x22b9  branch on the checkpoint          5 bytes
 *   0x81  0x22ce  branch at random (calls the RNG)  4
 *   0x82  0x22e0  branch on [di+0x5b]               5
 *   0x83  0x22bf  NOTHING -- "add si,2; ret"        3
 *   0x84  0x22ea  jump, "add si,[si]"               3
 *   0x85  0x2314  RE-EVALUATE THE RUBBER BAND: for
 *                 a type-3 truck it runs over all
 *                 four truck structures at 0x4c68,
 *                 0x4d2a, 0x4dec and 0x4eae        1
 *   0x86  0x22ed  cornering radius                  2
 *   0x87  0x22f6  segment width / throttle          2
 *   0x88  0x2301  FIRE NITRO, gated on a checkpoint
 *                 range tested at 0x238d            3
 *   0x89  0x22c3  branch if a nitro burst is running 3
 *
 * Counted across all eight layouts with those lengths:
 *
 *   0x80 281   0x81  14   0x82   4   0x83 115   0x84 396
 *   0x85 217   0x86  35   0x87  45   0x88   0   0x89   0
 *
 * So the two nitro opcodes are never used: a computer truck does not ask
 * for nitro anywhere in this game's data.  And 0x85 shows the original
 * re-weighs the rubber band only where the route says to, 217 times
 * across the eight tracks, rather than every tick. */

/* The slope limit, per layout (image 0x1e98 loads it from DS:0x6b0a into
 * [0x53e0]).  The low byte is a bias and the high byte a limit, and the
 * surface test is ((bias + gradient) & 0xff) >= limit -- with limit at
 * roughly twice bias, that caps the climbable gradient at about +-bias.
 * A cell over the cap takes the SAME path as an impassable one, so the
 * game refuses to drive a truck up a steep face rather than launching it
 * off the edge. */
/* ------------------------------------------------------------------ */
/* The difficulty ladder                                               */
/* ------------------------------------------------------------------ */

/* A computer truck starts every race at level 6 ([di+0x5b] and [di+0x5c]
 * are both set to it), but the game RE-BASES that from the player's pace
 * at 0x2132:
 *
 *     class  = [0x1a3 + race]          -- a race picks one of 8 ladders
 *     ladder = [0x7286 + class*2]      -- 17 descending thresholds
 *     level  = how many of them are >= the truck's average lap time
 *     level -= [0x7276 + class]        -- a bias of +1, 0 or -1
 *     clamp 0..17
 *
 * The average lives at [di+0x48]: 0x3855 divides the accumulated time
 * [di+0x44] by the lap count, and that accumulator is incremented once
 * per game tick (0x1f83) with a single lap capped at 0x7d0 = 2000 ticks.
 * So the thresholds, 176 to 456, are laps of roughly 9 to 23 seconds.
 *
 * The effect is that the field gets stronger as the player's lap times
 * come down, race by race, on top of the within-race rubber band. */
#define OG_LADDER_MAP_DS   0x1a3     /* race -> class                   */
#define OG_LADDER_PTR_DS   0x7286    /* class -> ladder                 */
#define OG_LADDER_BIAS_DS  0x7276
#define OG_LADDER_STEPS    17
#define OG_LADDER_CLASSES  8
#define OG_LAP_TICKS_MAX   0x7d0     /* a lap counts no longer than this */

/* Prize money by finishing place, added to whatever the truck picked up
 * during the race. */
/* ------------------------------------------------------------------ */
/* The drivers, and the plaque's text                                  */
/* ------------------------------------------------------------------ */

/* The names live together at DS:0x87d0 upward, each entry a 05 00 pair,
 * an x offset for centring, then the text.  A NICKNAME ends with 0x08, a
 * line break, so it sits above the driver's name, which ends with 0x00:
 *
 *     truck 0  red     MADMAN     Sam Powell      (x 26 / 21)
 *     truck 1  yellow  HURRICANE  Earl Stratton   (x 24 / 16)
 *     truck 2  blue    JAMMIN'    John Morgan     (x 26 / 14)
 *     truck 3  white   IRONMAN    Ivan Stewart    (x 24 / 15)
 *
 * The rest of a plaque is templates kept beside them, three of each --
 * one per column:
 *
 *     "$xxx,xxx + $xxx,xxx"     the prize, then what was picked up
 *     "Avg : xx.xx (Last) "     the average lap
 *     "Best : xx.xx (Last) "    the best lap
 *
 * which is what 0x33d7 fills in: it runs a number through 0x3930 and
 * lays the digits either side of a '.' into a buffer. */
/* NEW PLAYER REGISTRATION, which happens on the Speed Shop screen: an
 * inactive slot shows "Press  Button" (DS:0x878f) until someone joins,
 * and the same screen runs between races.  A joining player gives three
 * initials, a month, a day and a year of birth -- the year list runs
 * "15 16 17 ... 69" under "BIRTHDATE :" at DS:0x8752 -- and finally a
 * nation, chosen from eight kept in order at DS:0x8760 upward:
 *
 *     0 USA      1 ITALY    2 GERMANY   3 BRITAIN
 *     4 FRANCE   5 JAPAN    6 CANADA    7 AUSTRALIA
 *
 * That index is the country byte [di+0x0e], which picks the flag sprite
 * through the table at DS:0x96a8 -- so USA flies flag 4, Italy 1,
 * Germany 5, Britain 3, France 0, Japan 2, Canada 6, Australia 7 -- and
 * also picks the anthem, song country + 2. */
/* ------------------------------------------------------------------ */
/* The in-race scoreboard                                              */
/* ------------------------------------------------------------------ */

/* GAME.EXE keeps the figures in an object at DS:0x436 and refreshes them
 * through two flags in [0x19c]: bit 0x20 rewrites the LAP numbers --
 * 0x0488 reads all four trucks' [di+0x3c] (0x4ca4, 0x4d66, 0x4e28,
 * 0x4eea) and adds one for display -- and bit 0x40 rewrites the NITRO
 * count from [di+0x5d], split with aam (0x04c2).  Both then call the
 * video driver at es:[0x7c].
 *
 * In EGA.EXE that entry is 0x2dd2.  It restores a 7x6 block of
 * background and stamps each digit at a SCREEN ADDRESS taken from the
 * object's fields ([si+0x14], [si+0x18], [si+0x1c] ...), and a companion
 * routine at 0x2cb0 copies those fields into its own code segment.  The
 * driver therefore carries no layout of its own -- every one of those
 * words is zero in EGA.EXE -- so the board's position comes from
 * GAME.EXE, set per track by the scoreboard object's descriptor.
 *
 * The board's frame, its labels and the decimal point of the elapsed
 * time are painted into each track's artwork; the driver only ever fills
 * the gaps with digits, and the time carries a single decimal place. */
/* THE BOARD'S LAYOUT, and why it moves between tracks.  A loop at
 * 0x03f6 walks 19 (x, y) pairs at DS:0x410, adds the track's own origin
 * from [0x3eb]/[0x3ed], turns each into a screen address through the
 * driver's es:[0x14] and stores them into the object from +8 upward.
 * Then es:[0x74] hands three of them to EGA.EXE, which saves the
 * background behind each so the digit drawer can restore it.
 *
 * The 19 cells, relative to the origin:
 *
 *     0..2    y  3   x 17, 23, 30   the elapsed time -- note the gap
 *                                   from 23 to 30: one cell wide, where
 *                                   the artwork's DECIMAL POINT sits, so
 *                                   the time reads as two digits, the
 *                                   painted point, then one more
 *     3..6    y 12   x 14, 26, 38, 50    a lap digit per truck
 *     7..18   y 22   four groups of three, the nitro counts, and the
 *                    group is used TWO WAYS (0x2ecc):
 *
 *                      tens non-zero -> tens in the first cell, units in
 *                          the third: [si+0x24] then [si+0x2c], five
 *                          pixels apart, the middle cell unused
 *                      tens zero     -> the single digit goes in the
 *                          MIDDLE cell, [si+0x28], centred in the group
 *
 *                    so a count below ten sits centred rather than
 *                    hanging where its tens digit would have been.
 *
 * And the origin per track, from DS:0x3f0, four bytes each:
 *
 *     0: (9,152)   1: (3,136)   2: (1,88)    3: (241,1)
 *     4: (15,79)   5: (123,1)   6: (79,117)  7: (235,78)
 *
 * so the same board is painted into a different corner of every track's
 * artwork -- bottom left on Blaster, top right on Hurricane Gulch. */
#define OG_HUD_CELLS_DS     0x410
#define OG_HUD_ORIGIN_DS    0x3f0
#define OG_HUD_CELLS        19
/* The board's digits are NOT a font file.  The renderer at 0x2f7b in
 * EGA.EXE indexes the ordinary sprite table -- cs:0xa46 is image 0xad6,
 * the same table the PCG banks use -- at entry 0x209 + digit.  Those are
 * entries 521..530: ten 8x6 sprites with top 2 and an x offset of 10,
 * which are EGAME 77..86.  (87 and 88, 32x6, are the words NITRO and
 * NITROS.)  So the scoreboard is drawn from the effects bank we already
 * load, not from IRONFONT or ROADFONT. */
#define OG_HUD_DIGIT0       77        /* in the EGAME bank */
#define OG_HUD_OBJECT_DS    0x436

typedef struct {
    unsigned char cx[OG_HUD_CELLS], cy[OG_HUD_CELLS];   /* from DS:0x410 */
    unsigned short ox[8], oy[8];                        /* from DS:0x3f0 */
    int loaded;
} og_hud;
int og_hud_load(const char *dir, og_hud *out);
#define OG_HUD_LAP_FLAG     0x20      /* in [0x19c] */
#define OG_HUD_NITRO_FLAG   0x40

/* ------------------------------------------------------------------ */
/* IRONHI.BIN -- the Ironman Club, the high score table                */
/* ------------------------------------------------------------------ */

/* 1030 bytes: the tag "RI", then eleven u16 offsets 0x34 apart, each
 * pointing TWO BYTES BEFORE its record.  A record is 52 bytes:
 *
 *     +0   3 bytes   the initials entered at registration ("SRT", "K.B")
 *     +3   6 bytes   the birthdate, as typed: "081066", "112768"
 *     +9   1 byte    the country, indexing the same list as the flags
 *     +10  u16       the money, BCD, in units of $10,000 -- 0x0100 is
 *                    100, so $1,000,000, and 0x0080 is $800,000
 *     +12  16 bytes  the best lap times, one per layout, BCD; 0x99
 *                    means none set
 *     +28  24 bytes  unused, zero throughout
 *
 * A stock file holds ten entries from $1,000,000 down to $100,000, all
 * sharing the birthdate 081066 apart from DOC's 112768.  Written with
 * IRONFONT, the same glyphs as the podium plaques. */
/* ------------------------------------------------------------------ */
/* The title screen                                                    */
/* ------------------------------------------------------------------ */

/* The object tagged "TI" at DS:0x366c runs the whole cycle.  Each view
 * is the same little block -- spawn, op 60 with the PAGE to show, then
 * op 4 with how long to hold it, in frames (70 Hz):
 *
 *     view 1  the logo        page 0x8c34   180 ticks   3.0 s
 *     view 2  the copyright   page 0x88f1   360         6.0 s
 *     view 3  the publisher   page 0x8b0d   360         6.0 s
 *     view 4  racing hints    page 0x8c55   540         9.0 s
 *
 * and the block repeats past the script's end with the other two fourth
 * views, so each pass through shows a different one:
 *
 *     DS:36d8   page 0x8e0f  Awesome Wins    528 ticks  8.8 s
 *     DS:36f0   page 0x904f  Team Credits    528        8.8 s
 *
 * The title plays song 1, the Speed Shop's tune.
 *
 * A PAGE is a byte-coded stream: printable bytes are text and anything
 * below 0x20 is a control code.  The ones in use:
 *
 *     0x00   end of the page -- the next page follows immediately
 *     0x08   line break; several in a row leave blank lines
 *     0x17   followed by text, the banner across the top ("IRON")
 *     0x18   followed by TWO LETTERS, the style for what comes next
 *     0x04, 0x09, 0x0e, 0x0f, 0x10, 0x11  positioning, with parameter
 *            bytes -- a page starts each column with one of these
 *
 * The styles name a fill and an outline, which is the pair og_font_draw
 * takes: Ye yellow, Wh white, Bl blue, Gr green, Re red, and the
 * two-tone WR and WY -- white with a red or a yellow outline.
 *
 * A page is written in COLUMNS, each its own run: the credits page lays
 * out every label in yellow (Arcade Concept:, Technical Advisor: ...),
 * ends that run, then starts a white one at a new position and lists the
 * names (Leland Corp., Ivan Stewart ...), each separated by 0x08.  That
 * is why the labels and the values are different colours.
 *
 * SPRITES ARE NOT IN THE PAGE.  A page is text and layout only; the
 * artwork on a view comes from child objects the TI script spawns with
 * op 34 just before showing the page, each with its own sprite number
 * and position:
 *
 *     view 1   "LO"  sprite 582 at (30, 7)    the Off Road logo
 *     view 3   "VI"  sprite 575 at (62, 42)   Virgin
 *              "GR"  sprite 570 at (24, 94)   Graftgold
 *     view 4   "RH"  sprite 560 at (13, 2)    the hints heading
 *
 * -- which is why view 3 spawns two children and the others one.
 *
 * THE ENGINE that reads a page is reached by a FAR call from op 60's
 * handler -- 0x042f:0x0f99, image 0x5289 -- which is why it never turned
 * up in the main code segment.  Its loop is four instructions: take the
 * next byte, draw it as a glyph if it is 0x20 or above, otherwise call
 * the handler for it from a table of 32 at DS:0x98c.  So every control
 * code eats its own parameters:
 *
 *     0x00   0   end of the page
 *     0x01   0   nothing
 *     0x02   2   set x, a BYTE-SWAPPED word (the order the font's glyph
 *                table uses too)
 *     0x03   1   set y
 *     0x04   3   set x (swapped word) and y
 *     0x05   2   add to x;  0x06  1  add to y;  0x07  3  add to both
 *     0x08   0   newline: x back to the margin at [0x986], y down by the
 *                line height at [0x985]
 *     0x09   1   set the left margin
 *     0x0a   0   remember the position;  0x0b  0  restore it
 *     0x0c   1   draw a sub-page from the list at [0x73a]
 *     0x0d   1   repeat the last glyph this many times
 *     0x0e   1   LETTER SPACING, signed, added to every advance by
 *                0x1013 -- the headings use 3 and body text 0
 *     0x0f   1   set the line height
 *     0x10/0x11  0   a mode off / on;  0x12..0x16  0  more modes
 *     0x17   4   SELECT A FONT by name -- "IRON" or "ROAD", and it also
 *                RESETS the margin and the pitch mode (0x1125), so a
 *                heading can turn fixed pitch on and leave it.  0x1100
 *                walks the list at [0x72e] for the name and stores the
 *                match at [0x730], which is where the glyph drawer
 *                takes its font from.  A page switches between them
 *                freely: the publisher page sets IRON for its headings
 *                and ROAD for the small print.
 *     0x18   2   the style -- the two letters are compared AS A WORD
 *                against a LINKED LIST at [0x732]/[0x734], each node
 *                holding its name at +0 and the next at +2.  A node is
 *                not a colour pair: 0x11d3 copies 512 bytes from it into
 *                DS:0x77c, a translation table the glyphs are mapped
 *                through, so a style can recolour every pixel value.
 *                SORP approximates this with a fill/outline pair.
 *     0x19   0   draws from a list at [0x738]
 *
 * With those counts a page walks cleanly from end to end.  The credits
 * page reads as [Ye], then each label at its own x -- which is how they
 * come out right-aligned -- then [Wh], margin 104, and the names one per
 * newline; Awesome Wins is [Ye], margin 4 and a plain list.
 *
 * and a glyph comes from the font structure at [0x730]: es:[9] is the
 * first character it holds, es:[0xb] the last, and es:[0x12 + 2*(ch -
 * first)] its offset, again byte-swapped. */
/* A loaded copy of GAME.EXE, so pages can be walked out of it. */
typedef struct {
    unsigned char *img;      /* the load image, owned                  */
    size_t         len;
    int            loaded;
} og_pages;

/* The styles live in the VIDEO DRIVER, not the game -- EGA, CGA and
 * Tandy each carry their own, since the mapping is per display mode.  A
 * node is 48 bytes: its two-letter name, then at +8 a sixteen-byte row
 * mapping a glyph's pixel value to a colour.  With the node's flag byte
 * zero, 0x11d3 copies that one row 32 times, so the row is all there is.
 * A glyph uses value 1 for its fill and 2 for its outline, so those two
 * entries are what matter.  In EGA.EXE, from 0x236d: */
/* The nodes are 24 bytes apart, not 48 -- a 48 stride reads every other
 * one and mislabels the rest.  The full set, name at +0, fill at +9 and
 * outline at +10:
 *
 *   Wh 14/13   Bk  0/13   Ye  5/13   Bl 12/13   Gr  8/13   Re  6/13
 *   B2 14/14   W2 12/14   WR 10/6    WB 10/1    W3 10/14   WY  1/3
 *   E1  4/14   E2  2/14   E3  6/14   E4 10/14   E5 13/14
 *
 * "Ex" is NOT one of them: the pages that ask for it are the
 * "xxx Place : xxx" templates, where the x's are placeholders patched at
 * runtime -- the style name is patched the same way, Ex becoming E1..E5
 * for the place being shown. */
#define OG_STYLES_EGA_OFF   0x236d
#define OG_STYLES_STRIDE    0x18
#define OG_STYLES_MAX       24

typedef struct {
    char name[OG_STYLES_MAX][2];
    unsigned char fill[OG_STYLES_MAX], outline[OG_STYLES_MAX];
    int n, loaded;
} og_styles;
int  og_styles_load(const char *dir, og_styles *out);

int  og_pages_load(const char *dir, og_pages *out);
const char *og_ds_string(const og_pages *p, unsigned ds_off);
unsigned og_ds_page_n(const og_pages *p, unsigned ds_off, int n);
int  og_page_patch(og_pages *p, unsigned ds_off, const char *from, const char *to);
extern int og_page_outline;   /* >= 0 overrides every style's outline */
extern int og_page_fill;      /* >= 0 overrides every style's fill    */

/* Each truck's plaque text, sampled from a capture of the original: the
 * fill is a LIGHTER shade of that plaque's own colour, and the outline is
 * black.  None of the seventeen styles carries fills 9 or 11, so these
 * are not reachable by patching a style name -- the colours are set
 * directly.
 *
 *   red MADMAN       fill 11  (243, 81,  0)   plaque 10
 *   yellow HURRICANE fill 13  (243,211,  0)   plaque 15
 *   blue JAMMIN'     fill  9  ( 81,146,243)   plaque  8
 *   white IRONMAN    fill  5  (211,211,211)   plaque  4
 */
/* = 15 - the E1..E4 style fills (4, 2, 6, 10): see end_build */
#define OG_END_TEXT_FILL_RED     11
#define OG_END_TEXT_FILL_YELLOW  13
#define OG_END_TEXT_FILL_BLUE     9
#define OG_END_TEXT_FILL_WHITE    5
/* = 15 - E5's fill 13: a human's plaque and the time board, white */
#define OG_END_TEXT_FILL_HUMAN    2

int  og_page_poke(og_pages *p, unsigned ds_off, unsigned at,
                  const char *text, size_t n);
/* Room for a page of SORP's own, `n` zero bytes after the end of the
 * loaded GAME.EXE, in the same format and drawn by the same engine as
 * the game's: *ds_off gets its offset, for og_page_poke and the draws.
 * GAME.EXE's own bytes are left as they are. */
int  og_page_add(og_pages *p, size_t n, unsigned *ds_off);
/* A page that carries on in the text state the previous page left, and
 * the pen movement a page makes (0x42f:0xf34) in that state. */
int  og_page_draw_cont(const og_pages *p, unsigned ds_off, int x0, int y0,
                       og_image *dst, const og_font *iron, const og_font *road,
                       const og_styles *styles);
int  og_page_measure(const og_pages *p, unsigned ds_off, const og_font *iron,
                     const og_font *road, const og_styles *styles);
/* ... in a state the caller names: letter spacing, fixed pitch, ROAD */
int  og_page_measure_in(const og_pages *p, unsigned ds_off, const og_font *iron,
                        const og_font *road, const og_styles *styles,
                        int letter_sp, int fixed, int road_font);
/* ------------------------------------------------------------------ */
/* The computer drivers' level, and the rubber-band that moves it       */
/* ------------------------------------------------------------------ */
/*
 * Eighteen levels of driver, each a row in four tables at DS:0x4fe0,
 * 0x5004, 0x5028 and 0x504c -- top speed, acceleration and two grip
 * figures.  0x14cc copies a level's row into the truck.  Level 0 is the
 * quickest (top speed 19712); 17 is the slowest (512).
 *
 * The level MOVES during a race, at 0x2321: the standings are sorted,
 * then the human's truck is compared with the leader --
 *     a lap behind      ->  +5   (the field gets slower)
 *     a lap ahead       ->  -5   (the field gets quicker)
 *     the same lap      ->  the signed table at DS:0x753f, indexed by
 *                            (gap >> 4): 0, 0, -1, -1, -1, -2, -2, -2,
 *                            -2, -3, -3, -3, -4, -4, -5, -5
 * added to [di+0x5c] and clamped to 0..17.
 *
 * ALL OF THIS IS SKIPPED IN DEMO MODE: [0x1a1] is non-zero during the
 * attract races and 0x231a jumps the whole block, so the computer
 * drivers hold their base level and never chase the leader.
 */
/* The level indexes the SAME seven tables og_perf already loads
 * (OG_CPU_SHOCK_DS 0x4fe0 through OG_CPU_TOP_DS 0x50b8) -- 0x14cc copies
 * one row of all seven into the truck -- so the rubber band only has to
 * move R->level and racer_perf picks the rest up. */
/* 0x20ee: the level the field is SEEDED from is not the ladder's figure
 * alone.  It is HALVED and a per-track bonus added:
 *
 *     al = track_base[[0x1a0]]      DS:0x74f0 -- 2 everywhere but
 *     al += carried_level >> 1       track 6, which is 1
 *     clamp 0..17
 *
 * and that carried level is itself built at 0x20c6:
 *
 *     al = the HIGHEST [di+0x5c] among the HUMAN trucks   (0x20dc)
 *     al >>= 1                                            (0x20e6)
 *     ah = [0x19e] >> 2        the RACE NUMBER, quartered (0x20e8)
 *     al = track_base[track] + ah + al
 *
 * So the field is seeded from three things: the track, HALF the best
 * human's ladder level, and a QUARTER of the race number.  The last is
 * the season's ramp -- without it the opposition never gets harder as a
 * championship goes on, however well the player drives. */
#define OG_AI_TRACK_BASE_DS  0x74f0
extern const unsigned char og_ai_track_base[8];

#define OG_AI_LEVEL_MAX     (OG_CPU_LEVELS - 1)
#define OG_AI_LAP_BEHIND     5     /* the human a lap down   */
#define OG_AI_LAP_AHEAD     (-5)   /* the human a lap up     */

extern const signed char    og_ai_gap[16];
extern const signed char    og_ai_gap_full[256];

/* ------------------------------------------------------------------ */
/* The Speed Shop: prices, fields and maxima                            */
/* ------------------------------------------------------------------ */
/*
 * Six items (0x2de6 indexes all three tables with the same selection).
 * The ORDER is settled by the prices printed on the box sprites --
 * NITRO 10K, ACCEL 80K, TOP SPD 100K, SHOCKS 60K, TIRES 40K:
 *
 *   index  item             price   field        max
 *     0    Nitro              1     [di+0x5d]     99
 *     1    Acceleration       8     [di+0x60]      5
 *     2    Top Speed         10     [di+0x61]      5
 *     3    Start Next Race    0     --             0
 *     4    Shocks             6     [di+0x5e]      5
 *     5    Tires              4     [di+0x5f]      5
 *
 *   DS:0x954a  01 08 0a 00 06 04   the price
 *   DS:0x9550  5d 60 61 00 5e 5f   the field
 *   DS:0x9556  63 05 05 00 05 05   the maximum
 *
 * A price UNIT IS $10,000 -- the sprites read 10K for a price of 1 and
 * 100K for 10 -- which also fits the prize money, $100,000 being 10.
 *
 * A credit adds 0x20 to the LOW byte of the BCD cash (0x2e17: add al,
 * 0x20; daa; then adc the high byte), so it is worth 20 units = $200,000,
 * not 2000.
 *
 * A truck's money is a BCD WORD at [di+0x0f] and its CREDITS a byte at
 * [di+0x01].  The purchase at 0x2e01:
 *
 *     if (price <= cash)            buy
 *     else if (credits == 0)        refuse -- set 0x40 in [si+0xa]
 *     else { credits--;  cash += 0x2000 BCD;  buy }
 *     buy:  ++the item's field;  cash -= price, in BCD
 *
 * So a credit is worth 2000 and is spent only when the cash will not
 * cover the item.
 */
/* the two effects the shop plays: 0x2bc0 on a move, 0x2beb on a refusal */
#define OG_SFX_SHOP_MOVE    11
#define OG_SFX_SHOP_DENY    13
/* 0x2beb plays 13 on EVERY press of the shop's button (0x2dd1), and
 * 0x2bde plays 14 for a box already at its top (0x2dfc) */
#define OG_SFX_SHOP_PRESS   13
#define OG_SFX_SHOP_FULL    14

#define OG_SHOP_ITEMS        6
#define OG_SHOP_START_ITEM   3      /* free, and raises nothing */
#define OG_SHOP_CREDIT_VALUE 0x20   /* BCD, added to the low byte */
#define OG_SHOP_CREDIT_VALUE_DEC 20     /* 20 units = $200,000 */
#define OG_SHOP_CASH_UNIT    10000      /* a unit of money, for display */
#define OG_CASH_MAX          9999       /* four BCD digits: $99,990,000  */
#define OG_SHOP_CREDITS_NEW  2      /* what a player starts with */

/* 0x1383: when a truck is HUMAN (bit 0 of [di+2]) its race is reset to
 * the player's starting state -- every upgrade back to zero, no money,
 * and a full 25 nitros:
 *
 *     [di+0x5e] [di+0x5f] [di+0x60] [di+0x61] = 0    the four upgrades
 *     [di+0x0f] [di+0x11]           = 0              cash and the total
 *     [di+0x5d]                     = 0x19 = 25      nitro
 *     [di+0x5b] [di+0x5c]           = 6              the driver level
 *
 * A computer truck skips all of it and keeps what it had. */
/* Where the options overlay sits.  The renderer presents a 320x200
 * logical screen, so these are in GAME pixels, not window pixels -- and
 * twelve rows have to fit inside 200.  SDL's debug glyphs are 8 tall,
 * so a 15-pixel pitch leaves a comfortable gap and the whole list ends
 * at y 186. */
#define OG_OPT_X0    8
#define OG_OPT_Y0    8
#define OG_OPT_W   304
#define OG_OPT_DY   15

#define OG_JOIN_NITRO        25
#define OG_JOIN_CASH          0
#define OG_JOIN_LEVEL         6

/* The panel's prompts, beside the shop's other strings:
 *     DS:0x88b4  "Game Over"
 *     DS:0x88c6  "to continue"
 * A player who loses is shown "Press Button / to continue" with a
 * COUNTDOWN; if it runs out the game ends even with credits in hand,
 * which is how an arcade frees the position. */
#define OG_SHOP_GAMEOVER_DS  0x88b4
#define OG_SHOP_CONTINUE_DS  0x88c6
#define OG_SHOP_CONTINUE_N   9      /* the count the prompt starts from */
/* where the two extra lines sit under "Press Button" on the panel.  The
 * count is NOT centred in the column: it sits over the TROPHY, in the
 * figure's right half. */
#define OG_SHOP_CONT_DY     44
#define OG_SHOP_COUNT_DY    72
#define OG_SHOP_COUNT_X     62


extern const unsigned char og_shop_price[OG_SHOP_ITEMS];
extern const unsigned char og_shop_field[OG_SHOP_ITEMS];
extern const unsigned char og_shop_max[OG_SHOP_ITEMS];

/* the club row's fields, as the loop at 0x34e0 writes them */
#define OG_CLUB_AT_PLACE    1
#define OG_CLUB_AT_INITIALS 5
/* the money's digit area, filled backwards from its last position, and
 * the jump the code makes over the comma group (0x3556) */
#define OG_CLUB_MONEY_END   0x98db
/* the "10 05 ff fe 2c 11" group (proportional, x -2, ",", fixed) that
 * 0x3571 copies back over a shorter amount's leftovers */
#define OG_CLUB_MONEY_GROUP 0x98dd
/* 0x35a0: x = 0xdc - width, so every amount ENDS at x 220 */
#define OG_CLUB_MONEY_RIGHT 220

int  og_page_draw_at_xy(const og_pages *p, unsigned ds_off, int x0, int y0,
                        og_image *dst, const og_font *iron, const og_font *road,
                        const og_styles *styles);
int  og_page_draw_table(const og_pages *p, unsigned ds_off, unsigned table,
                        og_image *dst, const og_font *iron, const og_font *road,
                        const og_styles *styles);
void og_pages_free(og_pages *p);

/* Walk the page at `ds_off` and draw it into `dst` with `font`.  Returns
 * OG_OK, or OG_E_RANGE if the offset is outside the image. */
/* Pages switch fonts mid-stream, so both are passed. */
int  og_page_draw(const og_pages *p, unsigned ds_off, og_image *dst,
                  const og_font *iron, const og_font *road,
                  const og_styles *styles);
/* Two pages in sequence, the second continuing the first's font, pitch,
 * spacing, leading and pen position -- how a view is assembled. */
int  og_page_draw_after(const og_pages *p, unsigned pre, unsigned ds_off,
                        og_image *dst, const og_font *iron, const og_font *road,
                        const og_styles *styles, int *end_x, int *end_y);

#define OG_TITLE_OBJECT_DS  0x366c
#define OG_TITLE_V1_TICKS   180
#define OG_TITLE_V2_TICKS   360
#define OG_TITLE_V3_TICKS   360
#define OG_TITLE_V4_TICKS   540       /* 528 for the other two */

#define OG_HISCORE_FILE     "IRONHI.BIN"
#define OG_HISCORE_ENTRIES  10
#define OG_HISCORE_STRIDE   0x34
#define OG_HISCORE_TIMES    16       /* one per layout */

/* THE TABLE ITSELF is GAME.EXE's DS:0x98fb, 0x404 bytes, kept raw as
 * the original keeps it: eleven u16 offsets (relative to the table) and
 * a zero, then eleven 52-byte records.  The first ten offsets are the
 * standings; the eleventh is the spare slot op 85 writes a departing
 * player into before 0x36ac sorts all eleven by money.  A record:
 *   +0  initials (3)    +3  birthdate, day month year (6)
 *   +9  country         +0xa money, BCD in units of $10,000
 *   +0xc 40 bytes from the truck's [+0x13]: eight BCD words, its best
 *        WINNING time on each track (0x3758), then 24 zero bytes
 * IRONHI.BIN is "RI" and these bytes (0x3635, 0x3669).  When the file
 * cannot be read, the original keeps the table in its data segment --
 * the same bytes the shipped file holds. */
#define OG_CLUB_DS          0x98fb
#define OG_CLUB_BYTES       0x404
#define OG_CLUB_SLOTS       11
#define OG_CLUB_TIMES_BYTES 40
enum { OG_CLUB_FROM_SORP, OG_CLUB_FROM_GAME, OG_CLUB_FROM_DEFAULT };

typedef struct {
    char          initials[OG_HISCORE_ENTRIES][4];
    char          born    [OG_HISCORE_ENTRIES][7];
    unsigned char country [OG_HISCORE_ENTRIES];
    long          money   [OG_HISCORE_ENTRIES];
    unsigned char best    [OG_HISCORE_ENTRIES][OG_HISCORE_TIMES];
    int           n, loaded;
    unsigned char raw[OG_CLUB_BYTES];   /* DS:0x98fb as the game has it  */
    int           source;               /* OG_CLUB_FROM_...              */
} og_hiscore;

/* What the club needs from a player's truck, in the truck's own layout:
 * the initials [+5], the birthdate words [+8] month, [+0xa] day, [+0xc]
 * year as the registration writes them, the country [+0xe], the money
 * [+0x11] (BCD) and the 40 bytes at [+0x13].  `rec` is the record op 86
 * found (its offset in the table), or 0. */
typedef struct {
    char          initials[3], mm[2], dd[2], yy[2];
    unsigned char country;
    unsigned      money_bcd;
    unsigned char times[OG_CLUB_TIMES_BYTES];
    int           rec;
} og_club_player;

/* Load SORP's own copy (`own`, may be NULL), else gamedata's IRONHI.BIN,
 * else the default table out of GAME.EXE. */
int  og_hiscore_open(const char *own, const char *dir, og_hiscore *h);
int  og_hiscore_save(const og_hiscore *h, const char *path);
void og_hiscore_lookup(const og_hiscore *h, og_club_player *p);   /* op 86 */
void og_hiscore_enter(og_hiscore *h, const og_club_player *p);    /* op 85 */

/* Past the eleven records the table keeps the per-TRACK figures ([0x1a0],
 * the physical track, not the layout), all BCD seconds.hundredths:
 *
 *   +0x254  DS:9b4f  the track record, a word per track (0x33e3: the
 *                    "[bx - 0x64b1]" is 0x9b4f)
 *   +0x264  DS:9b5f  its holder's initials, four bytes per track
 *   +0x284  DS:9b7f  per track, twelve words: the ten best WINNING times,
 *                    a spare at +0x14 that 0x377a writes, and a zero
 *   +0x344  DS:9c3f  the same for the players' AVERAGE times (0x37f7)
 *
 * The shipped table has every figure at 99.99 and every holder "XXX". */
#define OG_CLUB_RECORD      0x254
#define OG_CLUB_HOLDER      0x264
#define OG_CLUB_BESTS       0x284
#define OG_CLUB_AVGS        0x344
#define OG_CLUB_LIST        0x18
#define OG_CLUB_RANKED      10

/* 0x371a, for a HUMAN truck when the race is over: a win keeps the race
 * time as the player's best on the track if it beats it, enters it in the
 * best list and takes the record if it beats that; every race folds the
 * time into the player's average there and enters the average in the
 * average list.  race_bcd is [di+0x44], race_ticks [di+0x46]. */
void og_club_race(og_hiscore *h, og_club_player *c, int track, int won,
                  unsigned race_bcd, unsigned race_ticks);
/* 0x38d4: where `bcd` stands in a track's list (OG_CLUB_BESTS or
 * OG_CLUB_AVGS) -- the index of the first EQUAL entry, or -1 */
int  og_club_rank(const og_hiscore *h, unsigned list, int track, unsigned bcd);
/* the player's own figures in og_club_player.times (the truck's [+0x13]):
 * the best winning time, and the average converted as 0x32b2 shows it */
unsigned og_club_best(const og_club_player *c, int track);
unsigned og_club_avg(const og_club_player *c, int track);
/* 0x38ec: binary to four BCD digits */
unsigned og_club_bcd(unsigned v);

/* the record for a track, and the initials of whoever holds it */
double      og_club_record(const og_hiscore *h, int track);
const char *og_club_record_who(const og_hiscore *h, int track);

int og_hiscore_load(const char *dir, og_hiscore *out);

#define OG_COUNTRY_NAMES_DS 0x8760
#define OG_PRESS_BUTTON_DS  0x878f

#define OG_DRIVER_NAMES_DS  0x87d0
#define OG_DRIVERS          4

/* The prize table is DS:0x9d0a -- 10 09 08 07 in BCD, by PLACE
 * ([di+0x3b]), so $100,000 / $90,000 / $80,000 / $70,000.  Fourth place
 * is paid too.
 *
 * 0x3727 adds the money PICKED UP during the race ([di+0x62]) to the
 * prize before banking it, which is what the plaque's "$100,000 +
 * $30,000" shows -- the second figure is the pickups, not a separate
 * placing bonus.  The sum goes to BOTH the truck's cash [di+0x0f] and
 * its running total [di+0x11].
 *
 * These are in units of $10,000, like every other money value. */
/* the prize line "$xxx,xxx + $xxx,xxx" is exactly 19 characters */
#define OG_END_PRIZE_LEN  19

#define OG_PRIZE_PLACES   4
extern const unsigned char og_prize[OG_PRIZE_PLACES];

typedef struct {
    unsigned char  map[16];                       /* race -> class      */
    signed char    bias[OG_LADDER_CLASSES];
    unsigned short step[OG_LADDER_CLASSES][OG_LADDER_STEPS];
    int            loaded;
} og_ladder;

int og_ladder_load(const char *dir, og_ladder *out);

/* The level a truck should start a race at, given the player's average
 * lap time in game ticks.  Pass avg <= 0 before any lap has been timed
 * and it returns the initial 6. */
int og_ladder_level(const og_ladder *l, int race, int avg_ticks);

#define OG_SLOPE_LIMIT_DS 0x6b0a

typedef struct { unsigned char bias[8], limit[8]; int loaded; } og_slopes;
int og_slopes_load(const char *dir, og_slopes *out);

#define OG_NITRO_OFF_DS  0x53a0

typedef struct {
    og_fx_anim dust, nitro, splash[4];
    int        splash_dx[4], splash_dy[4];   /* world units from wheel */
    int        burst_dx[8], burst_dy[8];     /* the nitro burst, by octant */
    int        loaded;
} og_effects;

int og_effects_load(const char *dir, og_effects *out);

/* The POLY record for a point inside a cell, choosing the QUAD corner by
 * triangle exactly as the sampler does (image 0x1925): slot +4 when
 * frac_x + frac_y stays within 255, +2 when frac_x >= frac_y.  -1 if bad. */
long og_surface_resolve_at(const og_surface *s, unsigned short cell,
                           int frac_x, int frac_y);

/* ------------------------------------------------------------------ */
/* Pickups: the cash bag and the nitro canister                        */
/* ------------------------------------------------------------------ */

/* A pickup is a scripted object like the effects (descriptor DS:0x3404,
 * tag "BO").  Its script: hide, wait 120 ticks, play sound 9, place
 * itself, show, then wait to be touched -- op 26 arms "any of the four
 * trucks" and points at the collect step -- and start over.
 *
 * Placing it (op 58, image 0x508):
 *     index = select[track * 16 + (random & 15)]    (bytes at DS:0x490)
 *     X, Y  = spawn[track][index]                   (via DS:0x510)
 *     type  = random & 1 -> sprite 0x1bc cash / 0x1bd nitro
 *
 * Collecting it (op 59, image 0x56e): a sound per truck, then for nitro
 * an award counter at [0x48b] rising to 9 is added to the truck's own
 * count at [+0x5d] (capped at 99), the HUD refresh bit 0x40 is set in
 * [0x19c], and a floating label is spawned from [0x31e0]. */
#define OG_SPAWNSEL_DS   0x490     /* 16 selection bytes per track      */
#define OG_SPAWNPTR_DS   0x510     /* per-track table of (X, Y) words   */
#define OG_PICKUP_MAX    16
#define OG_PICKUP_CASH   444       /* EGAME 0, sprite 0x1bc             */
#define OG_PICKUP_NITRO  445       /* EGAME 1, sprite 0x1bd             */
#define OG_PICKUP_WAIT   120       /* op 2 (0x78): ticks out of sight   */
#define OG_PICKUP_SOUND  9         /* op 54 (9) when it appears         */

/* Collecting one spawns two labels from DS:0x3442, each shown 40 ticks:
 *   a digit, sprite 0x209 (EGAME 77 = '0') + the award, 0x10 px left
 *   a word: nitro 0x213 "NITRO" for one, 0x214 "NITROS" for more;
 *           cash 0x215 ",000", one pixel right and down
 * The award climbs by one per pickup to a limit of 9.  Nitro is added to
 * the truck's count at [+0x5d] (max 99); cash is added to its money at
 * [+0x62] in BCD -- so the label reads "3,000" and the money is decimal. */
#define OG_LABEL_DIGIT   521       /* EGAME 77: the character '0'        */
#define OG_LABEL_NITRO   531       /* EGAME 87 "NITRO"                   */
#define OG_LABEL_NITROS  532       /* EGAME 88 "NITROS"                  */
#define OG_LABEL_THOUS   533       /* EGAME 89 "0,000": cash is in tens
                                    * of thousands, 10,000 .. 90,000    */
#define OG_LABEL_TICKS   40

typedef struct {
    int n;                          /* spawn points for this track      */
    int x[OG_PICKUP_MAX];           /* world coordinates                */
    int y[OG_PICKUP_MAX];
    unsigned char sel[16];          /* random 0..15 -> a spawn point    */
    int loaded;
} og_spawns;

int og_spawns_load(const char *dir, int track, og_spawns *out);

/* GAME.EXE's generator at 0x644, for the same choices it makes. */
unsigned og_rng(unsigned *seed);

/* ------------------------------------------------------------------ */
/* Track scenery: flagpoles, bunting and barrels                       */
/* ------------------------------------------------------------------ */

/* Built when a race starts (image 0x239).  A per-LAYOUT list at DS:0x1ee
 * holds 8-byte records, ending at a zero word:
 *
 *     +0 u16  screen X in the low 14 bits; the top two choose the object
 *             0xC000 barrel (EGAME 91)   0x8000 barrel (EGAME 90)
 *             0x4000 bunting             else   flagpole
 *     +2 u16  screen Y
 *     +4 u16  world X   ) >> 5, kept for sorting and collision; the sort
 *     +6 u16  world Y   ) key [si+0x18] is the world Y
 *
 * Scenery carries its screen position outright instead of being
 * projected -- each piece was placed by hand.  Both scripts show one
 * fixed frame and wait: the flagpole frame 2 (EGAME 29), the bunting
 * frame 20 (EGAME 52); being hit by a truck starts an animation. */
/* ------------------------------------------------------------------ */
/* Object collision                                                     */
/* ------------------------------------------------------------------ */

/* Objects do not test each other pairwise.  Each marks its own identity
 * bit into two arrays of buckets, one per axis (DS:0x2b10 for X, 0x2d70
 * for Y, image 0x0d97): a bucket is four bytes, so 32 identities, and it
 * is set across every bucket the object spans.  The test (0x0e2a) ORs
 * the bits over an object's own span in each axis and ANDs the two, so
 * everything overlapping in BOTH axes falls out at once.  Each identity
 * found is looked up (tables 0x2f4e / 0x2f5e) and its CLASS word
 * [si+0x66] is OR'd into [si+0x6e] -- so an object learns what kinds of
 * thing are touching it.  A script waits on that through op 26, whose
 * mask meets [si+0x6c]: the pickup waits on 0xf, a bit per truck, and
 * the flagpole on 0x10, a shared truck bit.
 *
 * A shape is four words in COARSE units (world >> 5), set with op 25:
 *     x_start, x_count, y_start, y_count
 * and spans start .. start + 4*count, the start aligned down to a
 * multiple of 4 as the bucket address is.  The truck's shape is its
 * per-heading footprint from DS:0x52f8; the pickup's (DS:0x3260) runs
 * -4..+28 on both axes, offset down and right to match the fact that its
 * sprite is drawn down and right of its point. */
#define OG_TRUCKBOX_DS   0x52f8
#define OG_CLASS_TRUCK   0x0010    /* the shared "a truck" class bit    */

typedef struct { short x0, xn, y0, yn; } og_shape;

int og_truck_shapes_load(const char *dir, og_shape out[16]);

/* The shape for a heading byte, as image 0x1607 indexes the table. */
#define OG_TRUCKBOX_INDEX(h)  ((((h) + 8) & 0xf0) >> 4)

/* Do two shapes overlap, at the buckets' granularity?  Positions are in
 * coarse units. */
int og_collide(int ax, int ay, const og_shape *a,
               int bx, int by, const og_shape *b);

#define OG_SCENERY_DS    0x1ee
#define OG_SCENERY_MAX   96

typedef struct {
    int   n;
    unsigned char type[OG_SCENERY_MAX];   /* 0 pole, 1 bunting, 2/3 barrel */
    short sx[OG_SCENERY_MAX], sy[OG_SCENERY_MAX];
    short sort[OG_SCENERY_MAX];           /* world Y >> 5                */
    int   base[4], idle[4];               /* first sprite, resting frame */
    og_shape shape[4];                    /* op 25: its collision box    */
    unsigned short mask[4];               /* op 26: what sets it off     */
    short wx[OG_SCENERY_MAX];             /* world X >> 5                */
    og_fx_anim hit[4];                    /* played when a truck hits it */
    /* The flagman (tag FM): placed from DS:0x1ce, drawn last (sort
     * 0x7fff).  His script waits 40 ticks, plays sound 1 and waves --
     * that animation runs off its OWN base sprite (0x1ca, EGAME 14) --
     * then returns to sprite 0x1d6 (EGAME 26) frame 0. */
    int   fm_x, fm_y, fm_idle;
    og_fx_anim fm_wave;        /* the white flag, sprite 0x1ca         */
    og_fx_anim fm_chequer;     /* the checkered, 0x1be, same frames    */
    int   loaded;
} og_scenery;

/* The race itself (image 0x13a8 places the trucks, 0x1f9c counts laps):
 *
 *   start point  X word[0x5258 + layout*2], Y word[0x5278 + layout*2]
 *   heading      word[0x52b8 + layout*2]: 0 forward, 0x8000 reversed
 *   grid         four slots at DS:0x52d8, 768 world units (three cells)
 *                apart; a second set of four follows for a reversed start
 *
 * Crossing the finish gate copies the lap time and increments the truck's
 * lap count at [di+0x3c], then sets bits in [0x19c]: 0x20 to refresh the
 * HUD, 1 on the THIRD lap (the white flag, as a truck starts its fourth)
 * and 2 on the FOURTH (the checkered, so the race is four laps).  The
 * flagman waits on those two bits; his white flag runs off sprite 0x1ca
 * and the checkered off 0x1be, sharing one frame list. */
/* ------------------------------------------------------------------ */
/* The four performance upgrades                                        */
/* ------------------------------------------------------------------ */

/* Image 0x147c copies a truck's four upgrade levels into its working
 * fields; 0x14c7 does the same for a computer truck from 18-step tables.
 *
 *   [di+0x5e] SHOCKS  -> 0x4f7c -> [di+0x63]  suspension stiffness
 *   [di+0x5f] TIRES   -> 0x4fac -> [di+0x65]  grip: the clearance below
 *                                             which a wheel still bites
 *                     -> 0x4f88 -> [di+0x67]  turn rate
 *                     -> 0x4fa0 -> [di+0x6b]  sideways slip
 *                     -> 0x4f94 -> [di+0x6d]  drag
 *   [di+0x60] ACCEL   -> 0x4fb8 -> [di+0x71]
 *   [di+0x61] TOP     -> 0x4fc4 -> [di+0x6f]
 *
 * Steering (0x1c8b) compares each wheel's clearance against the grip
 * figure: with neither FRONT wheel down the truck cannot steer, with one
 * it turns at half rate.  Driving (0x1cf8) needs a REAR wheel down at
 * all; the target speed is top * throttle >> 8, half again on nitro and
 * capped at 0xf0, and zero when the truck is sliding sideways.  Speed
 * closes on it by the acceleration figure, or falls back by the drag
 * figure, each halved unless both rear wheels are down. */
/* ------------------------------------------------------------------ */
/* How a computer truck drives (image 0x2189)                          */
/* ------------------------------------------------------------------ */

/* It holds two waypoints: the one it aims at ([di+0x4c]) and the one
 * after ([di+0x4e], the stream position in [di+0x4a]).  Each tick:
 *
 *  - if the aim point is farther away than the corner radius [di+0x50],
 *    0x21f3 RESCANS the whole route stream and takes the nearest
 *    waypoint as the new aim, picking up the radius in force there.
 *    This is the stateless recovery: a truck knocked off line simply
 *    re-acquires the nearest point rather than retracing its steps.
 *  - if the following point is nearer than the aim, it becomes the aim
 *    and the one after is fetched (0x229b, which runs the route program
 *    so branches resolve as usual).
 *  - the bearing to the aim (atan2 at 0x23e2) minus the heading gives
 *    the error.  Within 8 of 256 -- about 11 degrees -- it holds
 *    straight; otherwise it steers by the tires' rate, halved with one
 *    front wheel down and refused with none.
 *  - the throttle chases the segment's figure (og_waypoint.width).
 *
 * Distances (0x23ad) are squared with each axis shifted down 6, so one
 * cell is 4 and a radius of 400 means 5 cells.
 *
 * The truck STEERS its heading [di+0x81] but MOVES along its travel
 * direction [di+0x83], which chases the heading by (slip angle x
 * [di+0x6b]), doubled when both rear wheels are down (0x1ce7).  That lag
 * is the drift, and it is why heading and travel must be kept apart.
 *
 * RESOLVED: the truck steers at [di+0x4e], NOT at the point it has
 * reached.  The bearing comes from the delta globals [0x74fc]/[0x74fe],
 * which hold whatever the last distance call left there -- and that call
 * is always the one for [di+0x4e] (0x21a4).  So [di+0x4c] only drives the
 * advance and the rescan while the steering runs one point ahead.  Without
 * that lookahead a truck orbits any aim point inside its turning circle,
 * which at 145 speed and 6.5 turn is about 3.9 cells, and never advances.
 *
 * Movement (0x188e): each tick the position gains speed * component * 4
 * >> 16, where the component comes from the 256-entry sine table at
 * DS:0x2fa0 (amplitude 16384, so the step is exactly `speed` world units)
 * indexed by the TRAVEL direction rounded to a multiple of 8 -- movement
 * is quantised to the same 32 directions as the sprites.
 *
 * Driving this model heads-up completes laps on all 16 layouts at 11 to
 * 14 seconds a lap, touching a barrier under 2% of the time.
 *
 * Difficulty is rubber-banded (0x2314): the four trucks are ranked, and
 * a computer truck's level [di+0x5b] moves up or down by 5 when the
 * player is behind or ahead (or by a table at 0x753f on the same lap),
 * clamped to 0..17, then 0x14cc reloads its performance from the
 * 18-step tables.  There is no separate blocking behaviour: a computer
 * truck never steers at another truck.  Contact is ordinary collision.
 */

#define OG_UPG_LEVELS    6
#define OG_UPG_SHOCK_DS  0x4f7c
#define OG_UPG_STEER_DS  0x4f88
#define OG_UPG_DRAG_DS   0x4f94
#define OG_UPG_SLIP_DS   0x4fa0
#define OG_UPG_GRIP_DS   0x4fac
#define OG_UPG_ACCEL_DS  0x4fb8
#define OG_UPG_TOP_DS    0x4fc4
/* the computer's own 18-step tables, indexed by [di+0x5b] (image 0x14c7) */
#define OG_CPU_LEVELS    18
#define OG_CPU_SHOCK_DS  0x4fe0
#define OG_CPU_DRAG_DS   0x5004
#define OG_CPU_STEER_DS  0x5028
#define OG_CPU_SLIP_DS   0x504c
#define OG_CPU_GRIP_DS   0x5070
#define OG_CPU_ACCEL_DS  0x5094
#define OG_CPU_TOP_DS    0x50b8
#define OG_RUBBER_DS     0x753f   /* 16 bytes, 0 down to -5            */
#define OG_CPU_BASE      6        /* [di+0x5c], set at 0x13a8          */
#define OG_NITRO_CAP     0xf0
/* Firing nitro (image 0x17a1): the input's bit 0x40 asks for it, and it
 * only fires when the truck's count at [di+0x5d] is non-zero -- one is
 * spent per firing.  [di+0x77] is then set to 0x14, twenty ticks, and
 * while it runs the speed target is raised by half.  A computer truck
 * presses the button from the ROUTE: opcode 0x83 carries a checkpoint
 * range, tested at 0x238d against [di+0x41], and it will not fire while
 * a burst is still running. */
#define OG_NITRO_TICKS   0x14
#define OG_NITRO_HUMAN   25
#define OG_NITRO_CPU     10

typedef struct {
    int shock[OG_UPG_LEVELS];   /* as og_physics.shocks, in 1/65536 */
    int steer[OG_UPG_LEVELS];
    int drag[OG_UPG_LEVELS];
    int slip[OG_UPG_LEVELS];
    int grip[OG_UPG_LEVELS];
    int accel[OG_UPG_LEVELS];
    int top[OG_UPG_LEVELS];
    /* the computer's tables, one step per difficulty level */
    int c_shock[OG_CPU_LEVELS], c_steer[OG_CPU_LEVELS], c_drag[OG_CPU_LEVELS];
    int c_slip[OG_CPU_LEVELS],  c_grip[OG_CPU_LEVELS],  c_accel[OG_CPU_LEVELS];
    int c_top[OG_CPU_LEVELS];
    signed char rubber[16];     /* DS:0x753f                          */
    int loaded;
} og_perf;

/* The rubber band, 0x2321.  SUPERSEDED by og_ai_rubber_band in sorp.c,
 * which follows the original: the HUMAN is compared with the leader and
 * the whole field moves by one adjustment -- a lap behind is worth +5, a
 * lap ahead -5, and on the same lap the gap (>> 4) indexes DS:0x753f.
 * The result is added to the base [di+0x5c], clamped to 0..17, and
 * 0x14cc reloads the truck's whole row of performance.  The block is
 * SKIPPED in demo mode ([0x1a1] set, 0x231a).
 *
 * This version compares each truck with the leader instead, which is not
 * what the game does; it is kept only so older callers still build. */
int og_rubber_level(const og_perf *p, int base, int laps, int prog,
                    int leader_laps, int leader_prog);

int og_perf_load(const char *dir, og_perf *out);

#define OG_START_X_DS    0x5258
#define OG_START_Y_DS    0x5278
#define OG_START_HEAD_DS 0x52b8
#define OG_START_SLOT_DS 0x52d8
#define OG_RACE_LAPS     4

typedef struct {
    int x, y, heading;          /* world, and the 16-bit start angle    */
    int dx[4], dy[4];           /* the four grid slots                  */
} og_race_start;

int og_race_start_load(const char *dir, int layout, og_race_start *out);

#define OG_FLAGMAN_DS    0x1ce
#define OG_FLAGMAN_WAIT  40        /* op 2 (0x28) before he waves       */
#define OG_FLAGMAN_SOUND 1         /* op 54 (1)                          */
#define OG_FLAGMAN_WHITE 2         /* op 54 (2), the white flag (0x34ae) */

int og_scenery_load(const char *dir, int layout, og_scenery *out);

#define OG_NUM_LAYOUTS   16
#define OG_LAYOUT(track, reverse)  (((track) & 7) | ((reverse) ? 8 : 0))
/* The championship's schedule, DS:0x1a3, indexed by the race number
 * [0x19e] (0x01ed):
 *
 * It is FORTY-THREE entries long, not sixteen -- the first sixteen only
 * reach tracks 1, 3, 5, 6 and 7, and the rest of the championship brings
 * in 4, 0 and 2.  All sixteen layouts appear by race 42, after which the
 * byte at +43 is 0x38, outside the set, so that is the end.
 *
 * The whole byte IS the layout -- low three bits the track, bit 3 the
 * reverse flag -- and 0x01f4 keeps it in [0x19f] while 0x01f9 puts just
 * the track in [0x1a0].  So race 0 is track 3 forwards, race 2 the same
 * track BACKWARDS, race 4 track 1 backwards, and so on. */
#define OG_SCHEDULE_DS   0x1a3
#define OG_SCHEDULE_N    43
extern const unsigned char og_schedule[OG_SCHEDULE_N];
#define OG_LAYOUT_TRACK(layout)    ((layout) & 7)

#define OG_CKPT_TABLE_DS 0x6b1a
#define OG_CKPT_MAX      64

typedef struct {
    unsigned char x0, x1, y1, y0;     /* box in cells                */
    unsigned char progress, b5, flags;
} og_checkpoint;

typedef struct {
    og_checkpoint cp[OG_CKPT_MAX];
    int           n;
} og_checkpoints;

int og_checkpoints_load(const char *dir, int layout, og_checkpoints *out);

/* Progress through a gate at cell (cx,cy), exactly as image 0x2009. */
int og_ckpt_progress(const og_checkpoint *p, int cx, int cy);

typedef struct {
    int      cursor;        /* offset into the route program          */
    int      checkpoint;    /* [di+0x41]                              */
    int      next_cp;       /* next box to enter                      */
    int      laps;
    int      progress;      /* [di+0x42]: progress through the lap    */
    unsigned seed;          /* RNG state, as GAME.EXE 0x644           */
    int      v5b, v77;      /* other branch inputs, 0 on a clean lap  */
} og_ai;

void og_ai_reset(og_ai *ai, unsigned seed);
int  og_ai_next (og_ai *ai, const og_route *r);         /* -> waypoint */
int  og_ai_cross(og_ai *ai, const og_checkpoints *c, int cx, int cy);

/* `layout` is 0..15: OG_LAYOUT(track, reverse). */
int og_route_load(const char *dir, int layout, og_route *out);
/* The same from a program already in hand, in GAME.EXE's encoding, its
 * jumps relative to `prog` (the Track-Pak's, re-encoded). */
int og_route_parse(const unsigned char *prog, long len, og_route *out);

/* ------------------------------------------------------------------ */
/* Sprite banks                                                        */
/* ------------------------------------------------------------------ */

/* A .PCG holds planar cells as consecutive vertical strips with no
 * header.  Cell width and frame height differ per bank, so both are
 * passed in:
 *
 *   ETRUCK.PCG   cell_bytes 3 (24 px), 4608 scanlines
 *                frame height 9 divides it exactly -> 512 frames
 *   ETITLE.PCG   cell_bytes 4 (32 px), 830 scanlines
 *   EGAME/ESHOP/EEND  cell_bytes 4
 *
 * These were measured from the shipped files; they are also settable
 * from superoffroad.cfg so they can be corrected without a rebuild.
 */
#define OG_BANK_MAX_FRAMES 1024

/* ETRUCK.PCG does NOT use the 4-plane layout the other banks use.
 *
 * Established by dumping DOSBox-X memory while the attract demo ran and
 * aligning the loaded buffer against the file: the loader emits 3 mask
 * bytes followed by 9 data bytes, repeating (467 nine-byte match runs,
 * 560 three-byte insert runs, ratio 1.3330 = 4/3 over 6000 bytes).
 *
 * So a row is 9 bytes: THREE bitplanes of 3 bytes = 24 pixels.  The
 * driver ORs the three planes into a 3-byte mask and stores
 * [mask][p0][p1][p2] = 12 bytes per row in RAM.  EGA.EXE's own record
 * for etruck carries the decoder pointer 0009:2287 and flags 0x87,
 * where bit 7 enables that mask pass; eshadow uses the same decoder
 * with flags 0x01 and is copied verbatim (confirmed: ESHADOW.PCG sits
 * unmodified in RAM, ETRUCK.PCG does not).
 *
 * Decoding at 9 bytes/row lifts row-to-row coherence from 0.551 (the
 * old 12-byte reading) to 0.747.
 *
 * Three planes give 8 colours; the fourth EGA plane is supplied at blit
 * time and is almost certainly the per-player colour remap, which is
 * why one shape appears as white, yellow and blue trucks on screen.
 *
 * The complement that pictures and 4-plane cells need does NOT apply
 * here.  The file uses only values 0..5 (0 is background, 60.8% of the
 * bank; 6 and 7 occur once between them).  Applying 15-v would force
 * everything into 8..15, which is why trucks rendered a uniform red.
 *
 * Player colour is NOT stored.  Captured gameplay at native resolution
 * shows the blue truck using indices 2,3 -- the raw file values -- and
 * the other players using those same values with high bits OR'd in:
 *
 *     blue    2,3          (colour bits 0)
 *     yellow  6,7   = 2,3|4
 *     red    10,11  = 2,3|8
 *     white  12,13  = 4,5|8
 *
 * So the driver ORs a per-player value into the upper planes at blit
 * time.  One shape set, four colours, from two bits. */
/* Player indices, NOT palette offsets.  Recolouring is a per-value
 * substitution (see og_truck_remap in demo.c): only the body and
 * shading values change between players, so an offset or an OR cannot
 * express it.  These index the pair table in that order. */
#define OG_TRUCK_COLOUR_RED    0
#define OG_TRUCK_COLOUR_YELLOW 1
#define OG_TRUCK_COLOUR_BLUE   2
#define OG_TRUCK_COLOUR_WHITE  3
#define OG_ETRUCK_CELL_BYTES 3
#define OG_ETRUCK_PLANES     3
#define OG_ETRUCK_ROW_BYTES  9
#define OG_ETRUCK_KEY        0

typedef struct {
    og_image  *frames;
    int        nframes;
    int        w, h;           /* h = max height when frames vary     */
    int        key;            /* attribute index treated as clear    */
    int       *frame_h;        /* per-frame height, or NULL           */
    int       *frame_ox;       /* per-frame hotspot, or NULL          */
    int       *frame_oy;
} og_bank;

int  og_bank_load(const char *dir, const char *file, int cell_bytes,
                  int frame_h, int key, og_bank *out);

/* General form: `nplanes` bitplanes of `cell_bytes` bytes per row, so
 * the row stride is cell_bytes*nplanes.  `complement` applies the
 * bit-inversion the 4-plane formats need; pass 0 for ETRUCK. */
int  og_bank_load_ex(const char *dir, const char *file, int cell_bytes,
                     int nplanes, int frame_h, int key, int complement,
                     og_bank *out);

/* ------------------------------------------------------------------ */
/* Variable-height sprites                                             */
/* ------------------------------------------------------------------ */

/* EGA.EXE's sprite table: 702 eight-byte records, one per sprite of all
 * six banks in load order (etruck 412, eshadow 32, egame 116, etitle 27,
 * eshop 70, eend 45 -- the counts in the driver's own load records).
 *
 * The driver addresses it as CS:0x0a46 + 8 * index (e.g. the shadow draw
 * at 0x6150, the truck draw at 0x586b), which is image 0x0ad6, FILE
 * 0x0cd6.  An earlier reading started two bytes later, which shifted every
 * field name; this is the layout the draw routines actually use:
 *
 *     +0  u16  pointer offset, 0..15  } a normalised far pointer to the
 *     +2  u16  pointer segment        } sprite, relative to its bank:
 *                                       entry 1 = 0006:0003 = 99 bytes =
 *                                       entry 0's 11 rows x 9, exactly
 *     +4  u8   rows                     the clip height ("+ bp - 0xc8")
 *     +5  u8   top: rows into the 32-row box        (y = +5 + box y)
 *     +6  u8   width in pixels
 *     +7  u8   x offset into the box, 3..11 for trucks (x = +7 + box x)
 *
 * The x offset matters: sprites are stored at the LEFT of their cell, so
 * the narrow end-on truck frames need up to 11 px to sit centred, while
 * side-on ones need 4.  Each bank's pointers restart at 0000:0000. */
#define OG_SPRTAB_OFFSET  0x0cd6   /* into EGA.EXE, header included    */
#define OG_SPRTAB_ETRUCK  412

typedef struct {
    unsigned short ptr_off;     /* +0: far pointer, offset 0..15       */
    unsigned short ptr_seg;     /* +2: far pointer, paragraphs         */
    unsigned char  top;         /* +5: rows into the 32-row box        */
    unsigned char  w;           /* +6: width in pixels                 */
    unsigned char  xoff;        /* +7: x offset into the box           */
    long           byte_off;    /* start in the file, rows * 9         */
    int            rows;        /* +4: row count                       */
} og_sprite_def;

typedef struct {
    og_sprite_def *def;
    int            n;
} og_sprite_table;

int  og_sprite_table_load(const char *dir, const char *driver,
                          int count, og_sprite_table *out);

#define OG_TRUCK_BOX_ROWS 32   /* nominal box; +5 (top) centres within */
void og_sprite_table_free(og_sprite_table *t);

/* Decode a bank into per-sprite images of differing heights, using the
 * table for geometry.  Frame i has size w x rows of def[i]. */
int  og_bank_load_varh(const char *dir, const char *file,
                       const og_sprite_table *tab, int cell_bytes,
                       int nplanes, int key, int complement, og_bank *out);

#define OG_SPRTAB_PARA 16      /* offset field unit, in bytes         */

/* Per-frame heights and hotspots, parallel to og_bank.frames. */
typedef struct {
    int h, ox, oy;
} og_frame_info;
void og_bank_free(og_bank *b);

/* EGAME.PCG effects sprites -- see OG_EGAME_FIRST */
int og_egame_load(const char *dir, og_bank *out);

/* Any four-plane bank named by its span of sprite-table entries.
 * `complement` is 1 for egame, 0 for eshop and eend. */
int og_pcg_bank_load(const char *dir, const char *file, int first, int count,
                     int complement, og_bank *out);
/* ETITLE.PCG: sprite table entries 560..586, the 27 pieces the title
 * screen's child objects use -- the Off Road logo (582), Virgin (575),
 * Graftgold (570) and the page headings. */
#define OG_ETITLE_FIRST  560
#define OG_ETITLE_COUNT  27
int og_title_bank_load(const char *dir, og_bank *out);

int og_shop_load(const char *dir, og_bank *out);
int og_end_load(const char *dir, og_bank *out);

/* ------------------------------------------------------------------ */
/* Fonts                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *raw;
    og_font        f;
    int            loaded;
} og_fontfile;

int  og_fontfile_load(const char *dir, const char *file, og_fontfile *out);
void og_fontfile_free(og_fontfile *f);

/* ------------------------------------------------------------------ */
/* Locating the game files                                             */
/* ------------------------------------------------------------------ */

/* Case-insensitively resolve `name` inside `dir` (the originals are
 * upper case, but installs from ZIPs often are not).  Writes a usable
 * path into `out`; returns OG_OK if something was found. */
int og_resolve(const char *dir, const char *name, char *out, size_t cap);

/* True if `dir` looks like a Super Off Road install. */
int og_dir_looks_valid(const char *dir);

/* Check every file SORP needs against its SHA-256: the 31 files of the
 * DOS release that SORP reads.  `check` is called once for each, with
 * the file's name, its size (-1 when it cannot be read) and why it
 * failed -- missing, unreadable or different -- or NULL when it is
 * correct; returns how many failed (0 = all present and correct). */
typedef void (*og_file_check_fn)(const char *name, long bytes,
                                 const char *why, void *user);
int og_gamedata_verify(const char *dir, og_file_check_fn check, void *user);

/* Every table SORP takes from the original's files is reported as it is
 * read: the file, where in it (a byte offset into the file as it lies on
 * disk, MZ header included, or -1 when the whole file is used), how many
 * bytes, and what it is, as a sentence ("Loaded ...").  Nothing is
 * reported until a function is set. */
typedef void (*og_load_note_fn)(const char *file, long offset, long bytes,
                                const char *what);
void og_set_load_note(og_load_note_fn fn);
void og_load_note(const char *file, long offset, long bytes,
                  const char *fmt, ...);

/* SHA-256 of `len` bytes into `out` (32 bytes). */
void og_sha256(const unsigned char *data, size_t len, unsigned char out[32]);

#ifdef __cplusplus
}
#endif
#endif /* OG_DATA_H */
