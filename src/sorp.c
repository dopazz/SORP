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

/* sorp.c -- SORP, a Super Off Road port on SDL3.
 *
 *   Looks for the original game files in ./gamedata (override with
 *   argv[1]).  Nothing is redistributed with this source; you need your
 *   own copy of the game.
 *
 * What it shows:
 *
 *   1. The title screen, drawn under the title-screen palette.
 *   2. A track, drawn under the racing palette, with a truck driven along
 *      the game's own AI route program.  The truck sits on the height
 *      field through the game's projection, and its sprite frame comes
 *      from its four wheel heights through the game's attitude table.
 *      The foreground plane is composited *over* the truck, so it passes
 *      behind bridges and banks exactly as the original does.
 *
 * Keys:  JOINING        every truck drives itself until its player
 *                       presses the button, as the Speed Shop's "Press
 *                       Button" panels invite; from then on it is theirs
 *        PLAYERS        any controller joins with its button and takes
 *                       the first truck no player has -- red, yellow,
 *                       blue -- keeping it until that player's game is
 *                       over.  Controllers: the keyboard three times (A
 *                       and D steer, W throttle, SPACE nitro; the arrow
 *                       keys with right ctrl; the keypad's 4 6 8 2 with 0)
 *                       and up to four gamepads or joysticks, found at
 *                       launch.
 *        ESC            asks "Exit SORP?" -- ESC again, or a pad's Start,
 *                       leaves; any other key or a player's button
 *                       resumes.  F1's "Exit SORP" asks the same.  A
 *                       pad's Guide + Start (Hotkey + Start on a
 *                       handheld) leaves at once, and so does Back +
 *                       Start (Select + Start).
 *        F1             THE OPTIONS OVERLAY -- sound, volume, full
 *                       screen, the gamepad controls (up or a button
 *                       to accelerate), the screen, sprite and background
 *                       shaders, writing sorp.log and the log's level,
 *                       "Exit SORP", and the build stamp,
 *                       which shows "About"; the settings are kept
 *                       in sorp.cfg beside the program, with a desktop
 *                       window's size and position.  A pad's Back
 *                       button (Select on a handheld) opens and closes
 *                       it too.  Up and down choose a
 *                       row, left and right change it, and the players'
 *                       own buttons work too.  While any overlay is open
 *                       the game behind it gets no input at all.  A race
 *                       stands still while F1 is up (its "About" and
 *                       "Exit SORP?" too, and ESC's), the music playing
 *                       on, as the DOS game's F1 pauses it.
 *        F10            with F1 up in a race with players, asks "Back
 *                       to the title?" -- ESC or a pad's Start ends the
 *                       game there, as F10 in the DOS game's pause does;
 *                       any other key or button resumes.
 *        F11           full screen on and off, in a desktop window.
 *        F2             THE AUDIO OVERLAY -- the sound mode, the
 *                       driver's tick rate and the other sound aids,
 *                       the nine OPL channels' meters and the PC
 *                       speaker's, and tests: any effect, song or note,
 *                       a channel muted, the chip silenced.  The button
 *                       (or ENTER) plays what the row names.
 *        F3             THE SPRITE VIEWER -- every .PCG bank as the game
 *                       loads it.  Up and down choose the bank (the
 *                       truck's 25 attitudes first, then ETRUCK, EGAME,
 *                       ESHADOW, ESHOP, EEND and ETITLE); left and right
 *                       steer the truck round in the attitude view, held
 *                       to keep turning, and page through the others; the
 *                       button (or ENTER) repaints the truck red, yellow,
 *                       blue or white.  Each sprite is numbered in its
 *                       bank, from 0, and by its EGA.EXE sprite table
 *                       entry.  After the banks: IRONFONT and ROADFONT,
 *                       each with sample lines and every character it
 *                       has; the pictures LOADB, TITLES and, when it is
 *                       there, LOADA; each TRACKn.XX's picture and
 *                       its mask, one page each; and, with arcade sets
 *                       in gamedata/trackpak, each arcade track's
 *                       picture, mask, shadow and overlays (whatever
 *                       F1's "arcade roms" says).
 *        F4            THE DEBUG OVERLAY -- the track and its direction
 *                       (changing either starts that race afresh), "next
 *                       stage" -- a button that ends a race there and
 *                       then (chequered flag, fanfare, results), takes the
 *                       podium on to the shop, starts the next race from
 *                       the shop, and moves attract mode on a stage -- the
 *                       debug console, the race
 *                       overlays (navigation waypoints, progress gates,
 *                       physics wheel dots), the contour mesh and barrier
 *                       reports, each row worked as in F1.
 *
 * The log: stdout (on Windows, F4's debug console), each line timed and
 * tagged -- see THE LOG BUFFER -- and, with F1's "write sorp.log" on,
 * sorp.log beside the program.  F1's "log level" chooses how much: ERROR,
 * WARN, INFO (the default), DEBUG or TRACE; SORP_LOG sets it for one run.
 *
 * Build (Linux): make, which runs
 *   cc -O2 -std=gnu11 -pthread -Wall -Isrc -o sorp src/sorp.c \
 *      src/offroad_gfx.c src/og_data.c src/og_course.c src/og_palette.c \
 *      src/og_sound.c src/opl3.c $(pkg-config --cflags --libs sdl3) -lm
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   /* supplies WinMain for /SUBSYSTEM:WINDOWS */

#include "offroad_gfx.h"
#include "og_data.h"
#include "og_course.h"
#include "og_palette.h"
#include "og_sound.h"
#include "opl3.h"        /* Nuked OPL3, in place of the hand-written core */
#include "tp.h"          /* the arcade Track-Pak (tp_*.c) */
#include "tp_sound.h"    /* ...and its sound board */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <sys/stat.h>    /* og_log_prepare: is stdout sorp.log already? */
#include "truck_icon_png.h"   /* the window icon: Windows has sorp.rc's */
#include <dirent.h>      /* og_open_files */
#include <signal.h>      /* og_on_crash */
#include <time.h>
#include <unistd.h>
#endif

/* The log, by severity -- see THE LOG BUFFER for where the lines go.
 *   ERROR  something failed: a file, a device, a save
 *   WARN   SORP carries on another way: a fallback, a setting not used
 *   INFO   what a player would want in a report: the build, the machine,
 *          the screens, races and results, settings changed
 *   DEBUG  what was loaded and from where, the field's levels, the window
 *   TRACE  the race tick by tick: gates, laps, pickups, flight, effects
 * A run shows INFO and above unless F1's "log level" (kept in sorp.cfg)
 * or SORP_LOG asks for more or less. */
#define OG_ERR(...)   SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define OG_WARN(...)  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define OG_INFO(...)  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define OG_DEBUG(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define OG_TRACE(...) SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

/* F1's "log level", in its order */
#define OG_LOG_LEVELS 5
static const char *const og_log_level_nm[OG_LOG_LEVELS] = {
    "error", "warn", "info", "debug", "trace"
};
static const SDL_LogPriority og_log_level_p[OG_LOG_LEVELS] = {
    SDL_LOG_PRIORITY_ERROR, SDL_LOG_PRIORITY_WARN, SDL_LOG_PRIORITY_INFO,
    SDL_LOG_PRIORITY_DEBUG, SDL_LOG_PRIORITY_TRACE
};
static int og_log_level = 2;          /* F1's, and sorp.cfg's: INFO      */

/* F1's "arcade roms" and "arcade mode": the arcade game's tracks, from
 * the player's ROMs in gamedata/trackpak.  See "The arcade sets" with the
 * per-track state below. */
enum { OG_ARCADE_ORIGINAL, OG_ARCADE_TRACKPAK, OG_ARCADE_ALL16, OG_ARCADE_MODES };
static int      og_arcade_roms = 0;       /* F1, kept in sorp.cfg          */
static int      og_arcade_mode = OG_ARCADE_ORIGINAL;   /* F1, sorp.cfg     */
static int      og_tp_live    = 0;        /* the races come from a set now */
static og_image og_tp_thumb[TP_TRACKS];   /* START NEXT RACE, shop palette */
static int og_load_route(const char *dir, int layout, og_route *r);
static int og_load_gates(const char *dir, int layout, og_checkpoints *c);
static int og_log_env = -1;           /* SORP_LOG's, for this run, or -1 */
static int og_log_write = 0;          /* F1's "write sorp.log"           */
static const char *og_exit_why = "asked to quit";               /* the last line */
static const char *volatile og_crash_where = "the start";      /* a crash's screen */
static long og_crash_utc = 0;         /* seconds east of UTC, for a crash's time */

/* "debug" -> 3; -1 if it is none of them */
static int og_log_level_named(const char *s)
{
    int i;
    for (i = 0; s && i < OG_LOG_LEVELS; ++i)
        if (SDL_strcasecmp(s, og_log_level_nm[i]) == 0) return i;
    return -1;
}

/* a file beside the program by its name alone, for the log; any other
 * path as it is */
static const char *og_rel(const char *path)
{
    const char *base = SDL_GetBasePath();
    const size_t n = base ? SDL_strlen(base) : 0;
    if (!path) return "";
    return (n && SDL_strncmp(path, base, n) == 0) ? path + n : path;
}

/* how many files SORP holds open, on Linux; -1 elsewhere */
static int og_open_files(void)
{
#if defined(__linux__)
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    int n = 0;
    if (!d) return -1;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.') ++n;
    closedir(d);
    return n - 1;                      /* not the one reading the list */
#else
    return -1;
#endif
}

#define SCALE 3

/* Sprite anchor, from GAME.EXE 0x15d6 / 0x15dc: the box is drawn with its
 * top-left 15 px left of and 14 px above the projected point. */
#define OG_TRUCK_ANCHOR_X  15.0f
#define OG_TRUCK_ANCHOR_Y  14.0f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* og_image -> SDL_Texture                                             */
/* ------------------------------------------------------------------ */

/* `key` is the attribute index to treat as transparent, or -1 for an
 * opaque texture.  `mask`, when given, is one byte per pixel: zero
 * means the pixel is left fully transparent regardless of its colour.
 * That is how the track's foreground plane becomes an overlay. */
/* ETRUCK colour mapping.
 *
 * NOT bit arithmetic.  The sprite's five values split into two groups:
 * raw 1..3 are fixed details, and only raw 4..5 carry the player's
 * colour.  Evidence, all from captured gameplay:
 *
 *   red    body 10, shading 11, roll bar 12 white, tyres 14 black
 *   blue   body  2, shading  3 -- yet 12/13 still present in the blob,
 *          i.e. the white roll bar survives the recolour
 *   yellow body  6, shading  7
 *   white  body  4, shading  5  (pale blue / blue grey)
 *
 * So the per-player part is a pair (light, dark) substituted for raw 5
 * and 4, while raw 3/2/1 stay white / dim blue / black for everyone.
 * An offset or an OR cannot express that, which is why both failed.
 *
 * The real table lives in EGA.EXE, which owns the ETRUCK bank; this
 * reproduces its observed effect rather than reading it. */
static const unsigned char og_truck_pair[4][2] = {
    { 10, 11 },   /* red    */
    {  6,  7 },   /* yellow */
    {  2,  3 },   /* blue   */
    {  4,  5 }    /* white  -- pale blue / blue grey, not 12/13.  The
                   * greyish truck reads as the light pair in the track
                   * palette; 12/13 was too bright and too blue. */
};

static int og_truck_remap(int v, int player)
{
    if (player < 0 || player > 3) player = 0;
    switch (v) {
    case 5:  return og_truck_pair[player][0];   /* body      */
    case 4:  return og_truck_pair[player][1];   /* shading   */
    case 3:  return 12;                         /* roll bar  */
    case 2:  return 13;                         /* trim      */
    case 1:  return 14;                         /* tyres     */
    default: return 15;                         /* background */
    }
}

/* Generic converter: palette lookup only.  Used for the title screen,
 * track background and foreground plane, so it must
 * not apply any sprite-specific colour transform. */
static SDL_Texture *texture_from_x(SDL_Renderer *ren, const og_image *img,
                                   const unsigned char *pal, int key,
                                   const unsigned char *mask, int truck_off)
{
    SDL_Texture *tex;
    unsigned char *rgba;
    int i, n;

    if (!img || !img->px) return NULL;
    n = img->w * img->h;

    rgba = (unsigned char *)malloc((size_t)n * 4);
    if (!rgba) return NULL;

    for (i = 0; i < n; ++i) {
        int v = img->px[i] & 15;
        int clear = (key >= 0 && v == key) || (mask && !mask[i]);
        const unsigned char *c;
        /* Only ETRUCK sprites get remapped; everything else is a
         * straight palette lookup.  truck_off is a player index here. */
        if (!clear && truck_off >= 0) v = og_truck_remap(v, truck_off);
        c = pal + v * 3;
        rgba[i * 4 + 0] = c[0];
        rgba[i * 4 + 1] = c[1];
        rgba[i * 4 + 2] = c[2];
        rgba[i * 4 + 3] = (unsigned char)(clear ? 0 : 255);
    }

    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                            SDL_TEXTUREACCESS_STATIC, img->w, img->h);
    if (tex) {
        SDL_UpdateTexture(tex, NULL, rgba, img->w * 4);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    free(rgba);
    return tex;
}

/* The shaded converters (defined with MMPX below): a screen image as a
 * texture, at 4x through MMPX when the shader for its KIND says so --
 *   OG_LAYER_BACK    full-screen artwork      the background shader
 *   OG_LAYER_SPRITE  sprites, logos, text     the sprite shader
 *   OG_LAYER_MIXED   a screen built of both   either
 * texture_from_kept also keeps a copy of the 1x image, so og_reshade can
 * make the texture again when a shader is changed while it is up. */
enum { OG_LAYER_BACK, OG_LAYER_SPRITE, OG_LAYER_MIXED };
static SDL_Texture *texture_from_shaded(SDL_Renderer *ren, const og_image *img,
                                        const unsigned char *pal, int key, int kind);
static SDL_Texture *texture_from_kept(SDL_Renderer *ren, const og_image *img,
                                      const unsigned char *pal, int key, int kind);
static void og_reshade(SDL_Renderer *ren, SDL_Texture **t);

/* Plain palette conversion -- pictures, overlays, anything not ETRUCK. */
static SDL_Texture *texture_from(SDL_Renderer *ren, const og_image *img,
                                 const unsigned char *pal, int key,
                                 const unsigned char *mask)
{
    return texture_from_x(ren, img, pal, key, mask, -1);
}

/* ETRUCK sprites: display = 15 - raw, stepped down by the player's
 * offset.  Verified against captured gameplay -- the complement alone
 * gives the RED truck (body 10, dark red shading 11, white roll bar 12,
 * black tyres 14, background 15).  Blue's observed body indices 2,3 are
 * those values less 8 and yellow's 6,7 less 4, so the player colour
 * subtracts.  This applies to the truck bank ONLY. */
static SDL_Texture *texture_from_truck(SDL_Renderer *ren, const og_image *img,
                                       const unsigned char *pal, int key,
                                       int colour_off)
{
    return texture_from_x(ren, img, pal, key, NULL,
                          colour_off < 0 ? 0 : colour_off);
}

/* ------------------------------------------------------------------ */
/* Sprite bank explorer                                                */
/* ------------------------------------------------------------------ */

/* All 25 attitudes at one direction, as the game lays them out: pitch
 * down the rows (nose up at the top), roll across the columns (right
 * side high on the right), level in the middle.  Frames come straight
 * from the game's attitude table, so this is the bank's real shape --
 * 32 level frames, then 16 per tilted attitude and 15 at the corners. */
static int bank_compose_attitudes(og_image *sheet, const og_bank *b,
                                  const og_truck_attitudes *a, int dir,
                                  int *level_idx)
{
    const int cw = b->w + 6, ch = OG_TRUCK_BOX_ROWS + 4;
    int x0 = (OG_SCREEN_W - 5 * cw) / 2, y0 = (OG_SCREEN_H - 5 * ch) / 2;
    int p, r, shown = 0;

    memset(sheet->px, 0, (size_t)sheet->w * sheet->h);   /* 0 = clear */
    if (!a || !a->loaded) return 0;
    dir &= 31;
    for (p = 0; p < 5; ++p)
        for (r = 0; r < 5; ++r) {
            int idx = a->frame[p * 5 + r][dir];
            const og_image *f;
            if (idx < 0 || idx >= b->nframes) continue;
            f = &b->frames[idx];
            if (!f->px || f->h <= 0) continue;
            /* each sprite sits `top` rows into its 32-row box */
            og_image_blit(sheet, f, x0 + r * cw,
                          y0 + (4 - p) * ch
                             + (OG_TRUCK_BOX_ROWS / 2 - b->frame_oy[idx]), 0);
            ++shown;
        }
    if (level_idx) *level_idx = a->frame[12][dir];
    return shown;
}





/* ------------------------------------------------------------------ */
/* Effects: dust and splashes, as GAME.EXE spawns and animates them     */
/* ------------------------------------------------------------------ */

/* A live effect, mirroring the object fields the game uses: the frame
 * list and position [+0x56/+0x58], the current frame's remaining ticks
 * [+0x5e], the projected point [+0x2a/+0x2e], and the sort key [+0x18]. */
/* [0x618] counts the animated effects and caps them at 16 (0x696 and
 * 0x69b count them in and out); everything else -- the pickup's labels
 * among them -- comes from the 64-object pool 0x6a0 builds, and never
 * competes with them.  Sharing one 16-slot pool let a burst of dust
 * starve a label of its second half: the "1" without its "0,000". */
#define OG_FX_POOL      32
#define OG_FX_ANIM_MAX  16
typedef struct {
    const og_fx_anim *a;
    int  live, ended, i, hold;
    int  px, py, sort_y;
    int  fixed;                 /* a label: this sprite, no animation  */
    int  life;                  /* its remaining ticks                 */
} og_fx_obj;

/* A label spawned on collection (DS:0x3442): one sprite, 40 ticks. */
static int og_fx_label(og_fx_obj *pool, int sprite, int px, int py, int life)
{
    int k;
    for (k = 0; k < OG_FX_POOL; ++k) if (!pool[k].live) break;
    if (k == OG_FX_POOL) return 0;
    memset(&pool[k], 0, sizeof pool[k]);
    pool[k].live = 1; pool[k].fixed = sprite; pool[k].life = life;
    pool[k].px = px; pool[k].py = py;
    pool[k].sort_y = 0;                     /* [si+0x18] = 0 */
    return 1;
}

/* 0x6cf + op 31: start an animation at a world point, projected at the
 * ground height given.  Returns 0 when all 16 are in use, as the game
 * refuses a 17th. */
static int og_fx_spawn(og_fx_obj *pool, const og_fx_anim *a,
                       long wx, long wy, int ground, int ox, int oy)
{
    int k, anims = 0;
    for (k = 0; k < OG_FX_POOL; ++k)
        if (pool[k].live && !pool[k].fixed) ++anims;
    if (anims >= OG_FX_ANIM_MAX) return 0;
    for (k = 0; k < OG_FX_POOL; ++k) if (!pool[k].live) break;
    if (k == OG_FX_POOL || !a || a->n <= 0) return 0;
    memset(&pool[k], 0, sizeof pool[k]);
    pool[k].a = a;
    pool[k].live = 1; pool[k].ended = 0; pool[k].i = 0;
    pool[k].hold = a->dur[0];
    og_project(wx, wy, ground, ox, oy, &pool[k].px, &pool[k].py);
    pool[k].sort_y = (int)(wy >> 5);            /* [si+0x18] */
    return 1;
}

/* One tick of the animation step at 0xc9e: a frame shows for its
 * duration + 1 ticks; past the last one the -3 command ends it, the last
 * frame stays up for that tick, and the cleanup frees it on the next. */
static void og_fx_tick(og_fx_obj *pool)
{
    int k;
    for (k = 0; k < OG_FX_POOL; ++k) {
        og_fx_obj *f = &pool[k];
        if (!f->live) continue;
        if (f->fixed) { if (--f->life <= 0) f->live = 0; continue; }
        if (f->ended) { f->live = 0; continue; }
        if (--f->hold < 0) {
            if (f->i + 1 >= f->a->n) f->ended = 1;
            else { ++f->i; f->hold = f->a->dur[f->i]; }
        }
    }
}

/* One EGAME sprite at a projected point, with its table offsets. */
static void og_sprite_draw(SDL_Renderer *ren, const og_bank *eg,
                           SDL_Texture **cache, int index, int px, int py)
{
    int e = index - OG_EGAME_FIRST;
    SDL_FRect d;
    if (e < 0 || e >= eg->nframes) return;
    if (!cache[e]) cache[e] = texture_from(ren, &eg->frames[e], og_pal_race, eg->key, NULL);
    if (!cache[e]) return;
    d.x = (float)(px + eg->frame_ox[e]);
    d.y = (float)(py + OG_TRUCK_BOX_ROWS / 2 - eg->frame_oy[e]);
    d.w = (float)eg->frames[e].w;
    d.h = (float)eg->frames[e].h;
    SDL_RenderTexture(ren, cache[e], NULL, &d);
}

/* One of the four trucks.  The game starts them stopped on the grid,
 * waves the white flag, and counts each truck's laps at [di+0x3c]; the
 * race is over when one completes OG_RACE_LAPS. */
#define OG_RACERS 4
typedef struct {
    og_ai ai;
    int   from, to;
    float t;
    float wh[4], wv[4];
    int   live, flying;
    int   laps, colour, place;
    int   nitro, cash;          /* [di+0x5d] and [di+0x0f], BCD         */
    int   credits;              /* [di+0x01]: each worth 20 at the shop  */
    int   race_cash;            /* [di+0x62]: picked up THIS race        */
    int   game_over;            /* out of credits: back to the machine   */
    int   finished;             /* it has taken the chequered flag       */
    int   continue_t;           /* the losing player's countdown, 9..0   */
    int   total;                /* [di+0x11]: the running total          */
    int   nitro_t;              /* [di+0x77]: ticks of boost left       */
    int   nitro_lap;            /* the lap it last spent one on         */
    int   ai_base;              /* [di+0x5c]: the level it drifts from --
                                 * the ladder's level for this race     */
    int   rubber_at;            /* the last 0x85 waypoint it passed     */
    int   country;              /* [di+0x0e]: his flag and his anthem   */
    /* The club's view of the truck, from op 86 at the end of
     * registration: the initials and birthdate as the truck holds them,
     * the record it matched, and [+0x13], its best winning time on each
     * track.  club_ok says registration has run. */
    og_club_player club;
    int   club_ok;
    /* Each truck carries its OWN upgrades, as the original does --
     * [di+0x5e] shocks, [di+0x5f] tires, [di+0x60] acceleration and
     * [di+0x61] top speed -- so three players in the shop buy for
     * themselves rather than from one shared set. */
    int   up_shock, up_tire, up_accel, up_top;
    float body_h;               /* [di+0x7d]: the mean wheel height     */
    float ground_m;             /* [di+0x7f]: the mean of the grounds   */
    /* timing, as the game keeps it per truck: [di+0x44] counts a tick at
     * a time, [di+0x46] marks where the current lap began and [di+0x48]
     * is the average, each lap counting no more than OG_LAP_TICKS_MAX */
    long  time_ticks, lap_mark;
    int   best_lap, last_lap;
    long  fin_ticks;            /* time_ticks as it crossed the line   */
    int   club_raced;           /* 0x371a has counted this race        */
    int   computer;             /* [di] == 3: driven by the machine     */
    unsigned touched;           /* [di+0x59]: who it was against last tick */
    int   in_wall;              /* [0x4f7a]: already against a barrier  */
    int   block_axes;           /* its low bits: 1 blocks x, 2 blocks y */
    int   level;                /* [di+0x5b], rubber-banded 0..17       */
    float speed;                /* [di+0x85], world units per tick */
    float wx, wy;               /* world position in cells, [di+0x79/7b] */
    float head;                 /* [di+0x81] heading, 0..256           */
    float trav;                 /* [di+0x83] travel direction          */
    float thr;                  /* [di+0x75] throttle                  */
    float vx, vy;               /* [di+0x55/57]: the last step 0x1871
                                 * formed, in speed units -- what a bump
                                 * (0x169d) exchanges */
} og_racer;

/* The results screen, composed from the eend bank.  `order[]` lists the
 * colour group standing in each of the three columns, left to right. */
static unsigned char og_flagmap[OG_END_COUNTRIES] = { 4, 1, 5, 3, 0, 2, 6, 7, 8 };
/* No nation: what 0x12ae gives every truck, and all a computer driver
 * ever has.  Its flag is EEND 44, the signature banner, and its anthem
 * song 10. */
#define OG_COUNTRY_NONE  8

/* SORP'S NATIONS.  GAME.EXE's eight are countries 0-7 and "none" is 8,
 * as it has them.  With the arcade's ROMs (F1 "arcade roms"),
 * registration has a second page with the arcade's other eight, 9-16,
 * in the arcade's alphabetical order -- SORP's own addition: the arcade
 * lists eight at a time, an operator's choice of its sixteen.  A club
 * record keeps the byte as it is; GAME.EXE only compares it (op 86), so
 * 9-16 harm neither IRONHI.BIN nor the DOS game.  og_nation_tp is each
 * one's place in the arcade's list, for its name, flag and anthem. */
#define OG_NATIONS       17
#define OG_NATION_MORE    9       /* the first of the second page's      */
static const unsigned char og_nation_tp[OG_NATIONS] = {
    TP_USA, TP_ITALY, TP_GERMANY, TP_BRITAIN, TP_FRANCE, TP_JAPAN,
    TP_CANADA, TP_AUSTRALIA, TP_NATIONS,                  /* 8: none   */
    TP_BELGIUM, TP_FINLAND, TP_HOLLAND, TP_MEXICO, TP_NORWAY, TP_SPAIN,
    TP_SWEDEN, TP_SWISS
};
/* A nation's flag in the DOS game's colours, from the arcade's (see
 * og_nation_flag, with the arcade sets below) */
enum { OG_FLAG_SHOP, OG_FLAG_PODIUM };
static int og_nation_flag(int code, int where, og_image *out);

/* The Ironman Club: the same blue-and-signature backdrop the podium
 * uses, its heading, then the ten entries as place, initials and money.
 * Written in IRONFONT, as the plaques are. */
static SDL_Texture *club_build(SDL_Renderer *ren, const og_bank *eb,
                               const og_hiscore *h, const og_font *f,
                               const og_pages *pg, const og_styles *st,
                               int ask)
{
    og_image scr;
    SDL_Texture *t;
    int k;

    if (!h || !h->loaded || !f) return NULL;
    if (og_image_init(&scr, OG_SCREEN_W, OG_SCREEN_H) != OG_OK) return NULL;
    memset(scr.px, 0, (size_t)OG_SCREEN_W * OG_SCREEN_H);

    /* The same Leland-signature field the podium uses: alternating blue
     * and black cells, each carrying the two-piece signature in whichever
     * colour contrasts with its cell. */
    if (eb && eb->nframes > OG_END_SIGN + 1) {
        int cx, cy, col, i, fills_only;
        /* two passes: every cell's fill, THEN every signature.  A
         * signature straddles the 48 grid, so drawing them cell by cell
         * lets the next cell's fill erase the previous one's. */
        for (fills_only = 1; fills_only >= 0; --fills_only)
        for (cx = 0, col = 0; cx < OG_SCREEN_W; cx += OG_END_BACK_W, ++col) {
            int row = 0;
            for (cy = 0; cy < OG_SCREEN_H; cy += OG_END_BACK_H, ++row) {
                int inverted = ((col + row) & 1);
                int px2, py2;
                if (!inverted) {
                    for (py2 = cy; py2 < cy + OG_END_BACK_H && py2 < OG_SCREEN_H; ++py2)
                        for (px2 = cx; px2 < cx + OG_END_BACK_W && px2 < OG_SCREEN_W; ++px2)
                            scr.px[py2 * OG_SCREEN_W + px2] = OG_END_BACK_BLUE;
                }
                if (fills_only) continue;
                for (i = 0; i < 2; ++i) {
                    const og_image *f = &eb->frames[OG_END_SIGN + i];
                    int sx = cx + (i ? eb->frames[OG_END_SIGN].w : 0);
                    int sy = cy + OG_TRUCK_BOX_ROWS / 2
                           - (eb->frame_oy ? eb->frame_oy[OG_END_SIGN + i] : 0);
                    int yy, xx;
                    if (!f->px) continue;
                    for (yy = 0; yy < f->h; ++yy)
                        for (xx = 0; xx < f->w; ++xx) {
                            int tx = sx + xx, ty = sy + yy;
                            if (f->px[yy * f->w + xx] == eb->key) continue;
                            if (tx < 0 || tx >= OG_SCREEN_W ||
                                ty < 0 || ty >= OG_SCREEN_H) continue;
                            scr.px[ty * OG_SCREEN_W + tx] =
                                inverted ? OG_END_BACK_BLUE : OG_END_BACK_DARK;
                        }
                }
            }
        }
    }

    /* The screen's own pages, as 0x34e3 draws them.  The heading places
     * itself and ends by selecting style E2 with letter spacing -1, and
     * every row CARRIES ON in that state -- drawing the rows as fresh
     * pages lost the colour and the spacing.  The styles all outline in
     * 14, the skin tone in this palette; the hardware shows black, so
     * the outline is forced to 0 as the podium's is. */
    if (pg && pg->loaded) {
        og_pages *wp = (og_pages *)pg;     /* the templates are patched */
        og_page_outline = 0;
        og_page_draw_table(pg, OG_CLUB_PAGE_TITLE, 0, &scr, f, f, st);
        for (k = 0; k < h->n && k < OG_HISCORE_ENTRIES; ++k) {
            /* y = 13 * row + 38, x = 77 (0x3503..0x3510) */
            int y = OG_CLUB_ROW_Y0 + k * OG_CLUB_ROW_H;
            char place[3], who[4], d[4];
            unsigned v = (unsigned)(h->money[k] / 10000L);  /* BCD units */
            unsigned bx = OG_CLUB_MONEY_END;
            int z, w;
            /* the place pair from " 1 2 3 ... 910" and the initials,
             * into "xx. xxx " at +1 and +5 */
            SDL_snprintf(place, sizeof place, "%2d", k + 1);
            SDL_snprintf(who, sizeof who, "%-3.3s", h->initials[k]);
            og_page_poke(wp, OG_CLUB_PAGE_ROW, OG_CLUB_AT_PLACE, place, 2);
            og_page_poke(wp, OG_CLUB_PAGE_ROW, OG_CLUB_AT_INITIALS, who, 3);
            og_page_draw_cont(pg, OG_CLUB_PAGE_ROW, OG_CLUB_ROW_X, y,
                              &scr, f, f, st);

            /* The money, 0x3532..0x35b4.  0x3930 gives four digits of
             * [di+0xa] (units of $10,000); leading zeros become spaces,
             * as far as the first that is not one.  They are laid in
             * BACKWARDS from 0x98db into "$xx,xx0,000":
             *   d3 at 0x98db, d2 before it; with a third digit the
             *   ", " group is put back (copied from 0x98dd, since a
             *   shorter amount overwrote it) and d1, d0 go before that;
             * then "$", and fixed pitch (0x11) in front of the "$". */
            d[0] = (char)('0' + (v / 1000) % 10);
            d[1] = (char)('0' + (v / 100) % 10);
            d[2] = (char)('0' + (v / 10) % 10);
            d[3] = (char)('0' + v % 10);
            for (z = 0; z < 4 && d[z] == '0'; ++z) d[z] = ' ';
            if (d[3] != ' ') {
                og_page_poke(wp, bx--, 0, &d[3], 1);
                if (d[2] != ' ') {
                    og_page_poke(wp, bx--, 0, &d[2], 1);
                    if (d[1] != ' ') {
                        char grp[6];
                        bx -= 7;
                        memcpy(grp, og_ds_string(pg, OG_CLUB_MONEY_GROUP), 6);
                        og_page_poke(wp, bx + 2, 0, grp, 6);
                        og_page_poke(wp, bx + 1, 0, &d[1], 1);
                        if (d[0] != ' ') og_page_poke(wp, bx--, 0, &d[0], 1);
                    }
                }
            }
            {
                char dollar = '$', fixed = 0x11, setx[3];
                og_page_poke(wp, bx--, 0, &dollar, 1);
                og_page_poke(wp, bx, 0, &fixed, 1);
                /* measured from the 0x11 (0x42f:0xf34), and a "set x" put
                 * three bytes before it so the amount ENDS at x 220 */
                w = og_page_measure(pg, bx, f, f, st);
                w = OG_CLUB_MONEY_RIGHT - w;
                bx -= 3;
                setx[0] = 0x02;
                setx[1] = (char)((w >> 8) & 0xff);
                setx[2] = (char)(w & 0xff);
                og_page_poke(wp, bx, 0, setx, 3);
            }
            og_page_draw_cont(pg, bx, 0, y, &scr, f, f, st);
        }
        /* 0x34bc: Ctrl+S puts "Save HiScore table (Y/N)" over it -- a
         * page that sets its own font, style E5 and place (92, 176) */
        if (ask) og_page_draw_table(pg, OG_END_PAGE_SAVE, 0, &scr, f, f, st);
        og_page_outline = -1;
    }

    t = texture_from_kept(ren, &scr, og_pal_end, -1, OG_LAYER_MIXED);
    og_image_free(&scr);
    return t;
}

/* EVERY tick in the game is a display frame, and the EGA's 320x200 mode
 * runs at 70 Hz -- not 60, and not the BIOS timer's 18.2 (the game
 * restores that divisor on exit, so it never drives anything).  Music
 * tempo, effect durations, title-view holds, the attract cycle and lap
 * times all count in these frames, so they share one constant. */
#define OG_FRAME_HZ  70
#define OG_TICK_HZ   OG_FRAME_HZ /* the sequencer runs at the same rate */

/* what the plaque's time lines need from each truck */
/* what the plaque's time lines need from each truck: `total` is the
 * race time [di+0x46] in ticks; a human's plaque shows its CLUB figures
 * for the track -- the average race time and the best winning time, BCD
 * -- each with its place in the club's list for the track, or -1 */
typedef struct {
    int laps, total, best, pickups;
    int club;
    unsigned avg_bcd, best_bcd;
    int avg_rank, best_rank;
} og_endtime;

/* The podium gives way to the Speed Shop on a TIMER, not a keypress: its
 * objects carry a countdown at [si+0x12], ticked at 0x0782, and the loop
 * at 0x30be runs until it expires.  SORP holds the screen for
 * OG_END_HOLD frames -- long enough for the winner's anthem -- and then
 * raises this flag.  F4's "next stage" skips it. */
#define OG_END_HOLD   (OG_FRAME_HZ * 8)
static int   og_end_to_shop = 0;
static float og_end_t       = 0.0f;

static SDL_Texture *end_build(SDL_Renderer *ren, const og_bank *eb,
                              const int order[3], const int country[3],
                              og_pages *pages, const og_font *font,
                              const og_styles *styles, const og_endtime *times,
                              const int human[4], const char reg[3][4],
                              double record_time, const char *record_who,
                              const int places[4])
{
    og_image scr;
    SDL_Texture *t;
    int i, c, x;

    /* The plaques and the time board are BLACK-edged in the original.
     * Their styles name 14, which is the skin tone in this palette, so
     * the outline is taken from the styles' entry 0 instead -- 0, black
     * -- for the whole of this screen. */
    og_page_outline = 0;

    if (og_image_init(&scr, OG_SCREEN_W, OG_SCREEN_H) != OG_OK) {
        og_page_outline = -1;
        og_page_fill    = -1;
        return NULL;
    }
    memset(scr.px, 0, (size_t)OG_SCREEN_W * OG_SCREEN_H);

    /* The checkerboard.  Sprites 30/31 are a solid blue field and 28/29
     * the signature scrawl, so a cell drawn normally is blue with a dark
     * scrawl and a cell drawn INVERTED -- field and scrawl swapped -- is
     * dark with a blue one.  Alternating the two gives the backdrop, and
     * both kinds are visible in captured gameplay.  The cell grid itself
     * is taken to be the tile's own 48x48; the figures cover too much of
     * a captured screen to measure it. */
    {
        int cx, cy, col = 0;
        for (cx = 0; cx < OG_SCREEN_W; cx += OG_END_BACK_W, ++col) {
            int row = 0;
            /* the field runs from the top of the screen; the banner is
             * drawn over it, not around it */
            for (cy = 0; cy < OG_SCREEN_H; cy += OG_END_BACK_H, ++row) {
                int inverted = ((col + row) & 1);
                int px2, py2;
                if (!inverted) {                 /* the blue field */
                    for (py2 = cy; py2 < cy + OG_END_BACK_H && py2 < OG_SCREEN_H; ++py2)
                        for (px2 = cx; px2 < cx + OG_END_BACK_W && px2 < OG_SCREEN_W; ++px2)
                            scr.px[py2 * OG_SCREEN_W + px2] = OG_END_BACK_BLUE;
                }
                /* the signature, in whichever colour contrasts */
                for (i = 0; i < 2; ++i) {
                    const og_image *f = &eb->frames[OG_END_SIGN + i];
                    int sx = cx + (i ? eb->frames[OG_END_SIGN].w : 0);
                    /* Each piece carries its own top offset -- 18 for the
                     * 32 px half, 16 for the 16 px one -- so centring
                     * them by height drops the right-hand piece four
                     * rows below the left. */
                    int sy = cy + OG_TRUCK_BOX_ROWS / 2
                           - (eb->frame_oy ? eb->frame_oy[OG_END_SIGN + i] : 0);
                    int yy, xx;
                    for (yy = 0; yy < f->h; ++yy)
                        for (xx = 0; xx < f->w; ++xx) {
                            int tx = sx + xx, ty = sy + yy;
                            if (f->px[yy * f->w + xx] == eb->key) continue;
                            if (tx < 0 || tx >= OG_SCREEN_W || ty < 0 || ty >= OG_SCREEN_H) continue;
                            scr.px[ty * OG_SCREEN_W + tx] =
                                inverted ? OG_END_BACK_BLUE : OG_END_BACK_DARK;
                        }
                }
            }
        }
    }
    for (x = 0; x < OG_SCREEN_W; x += OG_END_BANNER_W) {   /* the banner */
        int bx = x;
        for (i = 0; i < 3; ++i) {
            const og_image *f = &eb->frames[OG_END_BANNER + i];
            og_image_blit(&scr, f, bx, 0, eb->key);
            bx += f->w;
        }
    }
    /* the stage, tiled across the lower part -- drawn BEFORE the figures
     * so they stand on it rather than behind it */
    if (eb->nframes > OG_END_STAGE) {
        const og_image *f = &eb->frames[OG_END_STAGE];
        int s;
        /* four pieces on the 108 pitch: the first sits at 95 - 108 = -13,
         * its right edge just showing at the screen's left */
        for (s = -1; s < 3; ++s)
            og_image_blit(&scr, f, OG_END_STAGE_X0 + s * OG_END_STAGE_DX,
                          OG_END_STAGE_Y, eb->key);
    }
    /* the figures first, then the winning-time board over them */
    for (c = 0; c < 3; ++c) {
        int base = order[c] * OG_END_GROUP;
        int cx = OG_END_COL_X0 + c * OG_END_COL_DX;
        int fx = cx + OG_END_FIG_DX;
        for (i = 0; i < 3; ++i) {             /* driver and companion */
            const og_image *f = &eb->frames[base + i];
            int top;
            if (base + i >= eb->nframes) break;
            /* each piece sits at its own top offset (table +5) */
            top = OG_TRUCK_BOX_ROWS / 2 - (eb->frame_oy ? eb->frame_oy[base + i] : 0);
            og_image_blit(&scr, f, fx, OG_END_FIG_Y + top, eb->key);
            fx += f->w;
        }
    }

    /* the winning-time board, four pieces, over the figures */
    if (eb->nframes > OG_END_BOARD + 3) {
        int bx = OG_END_BOARD_X;
        for (i = 0; i < 4; ++i) {
            const og_image *f = &eb->frames[OG_END_BOARD + i];
            og_image_blit(&scr, f, bx, OG_END_BOARD_Y, eb->key);
            bx += f->w;
        }
    }

    for (c = 0; c < 3; ++c) {                 /* the plaques and flags */
        int base = order[c] * OG_END_GROUP;
        int cx = OG_END_COL_X0 + c * OG_END_COL_DX;
        int px2 = cx;
        for (i = 0; i < 3; ++i) {             /* the plaque */
            const og_image *f = &eb->frames[base + 3 + i];
            if (base + 3 + i >= eb->nframes) break;
            og_image_blit(&scr, f, px2, OG_END_PLAQUE_Y, eb->key);
            px2 += f->w;
        }
        {                                     /* the driver's country flag */
            int ctry = country[c];
            og_image fl;
            /* the second page's nations: the arcade's flag in this
             * screen's colours, as GAME.EXE converted its eight; with the
             * arcade's ROMs gone since, the no-nation banner */
            if (ctry >= OG_NATION_MORE && ctry < OG_NATIONS &&
                og_nation_flag(ctry, OG_FLAG_PODIUM, &fl) == OG_OK) {
                og_image_blit(&scr, &fl, cx + OG_END_FLAG_DX, OG_END_FLAG_Y, 0);
                og_image_free(&fl);
            } else {
                int fr = OG_END_FLAG + og_flagmap[ctry >= 0 && ctry < OG_END_COUNTRIES
                                                  ? ctry : OG_COUNTRY_NONE];
                if (fr < eb->nframes)
                    og_image_blit(&scr, &eb->frames[fr],
                                  cx + OG_END_FLAG_DX, OG_END_FLAG_Y, eb->key);
            }
        }
        /* The plaque's own text, from the game's strings: the driver's
         * handle and name, the prize for the place, and the lap summaries.
         * The strings are the game's; where they sit on the plaque is
         * measured from a screenshot, since the code builds their
         * addresses arithmetically and no object carries a position. */
        if (font && pages && pages->loaded) {

        }
    }

    /* Every plaque's text AFTER every plaque's artwork: the three pages
     * are one per COLUMN -- their own x is 7, 115 and 223 -- so drawing a
     * column's text inside the loop let the next column's artwork paint
     * over it.  Each is patched with the place of whoever finished there. */
    if (pages && pages->loaded && font) {
        static const unsigned page_col[3] = {
            OG_END_PLAQUE_COL1, OG_END_PLAQUE_COL2, OG_END_PLAQUE_COL3
        };
        /* 0x30d6 gives a plaque to trucks 0, 1 and 2 -- in that order, at
         * x 7, 115 and 223 -- and the white truck gets none.  The column
         * is therefore fixed to a TRUCK, and what varies is the PLACE
         * printed on it, which comes from that truck's own [di+0x3b].
         * Patching "1st/2nd/3rd" by column instead made column one read
         * "1st Place" whoever had actually won. */
        static const char *place_name[5] = { "1st", "2nd", "3rd", "4th", "---" };
        /* The plaque's two figures are the PLACE PRIZE (DS:0x9d0a) and
         * what that truck picked up during the race -- 0x3727 adds them
         * before banking.  Both are in units of $10,000. */
        /* The style follows the TRUCK, not the column or the place.
         * 0x31d5 patches the plaque page's style to "E" + ('1' + the
         * truck's colour number [di]): E1 red, E2 yellow, E3 blue, E4
         * white -- fills 4, 2, 6, 10, outline 14 -- and a human's plaque
         * (0x3220) "E5", fill 13.  The driver draws a glyph pixel as the
         * style's value exactly (EGA.EXE 9:2fca), but this screen's
         * indices here are the COMPLEMENT of the original's (its picture
         * decoder inverts every plane, 9:256d; og_pal_end was read with
         * the registers reversed), so a style value v shows as 15 - v:
         * 11, 13, 9, 5 for the four trucks, as the capture measured, and
         * the outline 14 as 1, black.  E5 shows as 2, white: a human's
         * plaque reads white on black, whatever the truck. */
        /* the fill per truck, measured in a capture (see og_data.h) */
        static const int tfill[4] = {
            OG_END_TEXT_FILL_RED,    OG_END_TEXT_FILL_YELLOW,
            OG_END_TEXT_FILL_BLUE,   OG_END_TEXT_FILL_WHITE
        };
        int col;
        for (col = 0; col < 3; ++col) {
            int who  = order[col];
            const char *pname = place_name[(places && places[who] >= 1 &&
                                            places[who] <= 4) ? places[who] - 1 : 4];
            og_page_fill = (human && who >= 0 && who < 4 && human[who])
                         ? OG_END_TEXT_FILL_HUMAN : tfill[who % 4];
            /* the three prize pages are 878f, 87a3, 87b7 -- the sub-pages
             * the columns call as 10, 11 and 12 */
            {
                /* The prize line is one fixed page, "$xxx,xxx + $xxx,xxx"
                 * -- 19 characters.  Write it whole: the place prize
                 * always, and the pickups only when there were any, the
                 * rest blanked so no empty "+ $0" is left showing. */
                char line[OG_END_PRIZE_LEN + 1];
                int  pp = (places && places[who] >= 1) ? places[who] : col + 1;
                int  pl = (pp <= OG_PRIZE_PLACES) ? og_prize[pp - 1] : 0;
                int  pk = times ? times[who].pickups : 0;
                if (pk > 0) {
                    SDL_snprintf(line, sizeof line, "$%3d,000 + $%3d,000",
                                 pl * (OG_SHOP_CASH_UNIT / 1000),
                                 pk * (OG_SHOP_CASH_UNIT / 1000));
                    og_page_poke(pages,
                                 OG_END_PRIZE_3RD + col * OG_END_TEXT_STRIDE,
                                 0, line, OG_END_PRIZE_LEN);
                } else {
                    /* No pickups: show the prize alone, centred.  The page
                     * engine has no centring code -- every handler from
                     * 0x101e to 0x1088 only moves the pen -- so do it the
                     * way the plaque's own lines do, with a RELATIVE move
                     * (code 0x05, a big-endian word) before the text.
                     *
                     *     05 00 NN  "$100,000"  00
                     *
                     * which fits easily in the 19-byte template. */
                    char one[12], both[OG_END_PRIZE_LEN + 1], buf[24];
                    int  len, dx = 0, n = 0;
                    SDL_snprintf(one, sizeof one, "$%3d,000",
                                 pl * (OG_SHOP_CASH_UNIT / 1000));
                    len = (int)SDL_strlen(one);
                    if (font) {
                        SDL_snprintf(both, sizeof both, "%s + %s", one, one);
                        dx = (og_font_text_width(font, both, -1)
                              - og_font_text_width(font, one, -1)) / 2;
                    }
                    if (dx < 0) dx = 0;
                    buf[n++] = 0x05;                 /* x += (big endian) */
                    buf[n++] = (char)((dx >> 8) & 0xff);
                    buf[n++] = (char)(dx & 0xff);
                    memcpy(buf + n, one, (size_t)len); n += len;
                    buf[n++] = 0x00;                 /* the page ends here */
                    og_page_poke(pages,
                                 OG_END_PRIZE_3RD + col * OG_END_TEXT_STRIDE,
                                 0, buf, (size_t)n);
                }
            }
            /* 0x32b2 and 0x3346 rewrite the human plaque's two time
             * lines from the "xx.xx" on: the player's AVERAGE race time
             * on this track and best WINNING time there, from the club
             * record -- not this race's laps -- then " (Nth)" with the
             * time's place in the club's list for the track, but only in
             * the top ten (DS:0x96b1, "1st" .. "10th"), and the line ends
             * there.  The template's "(Last)" is never shown.  A best of
             * 99.99, no win yet, prints " ---" and no place. */
            if (times && times[who].club) {
                static const char *nth[OG_CLUB_RANKED] = {
                    "1st", "2nd", "3rd", "4th", "5th",
                    "6th", "7th", "8th", "9th", "10th"
                };
                char line[2][16];
                int  k;
                for (k = 0; k < 2; ++k) {
                    unsigned v = k ? times[who].best_bcd : times[who].avg_bcd;
                    int      r = k ? times[who].best_rank : times[who].avg_rank;
                    if (k && v == 0x9999)
                        SDL_snprintf(line[k], sizeof line[k], " ---");
                    else if (r >= 0 && r < OG_CLUB_RANKED)
                        SDL_snprintf(line[k], sizeof line[k], "%x%x.%x%x (%s)",
                                     (v >> 12) & 15, (v >> 8) & 15,
                                     (v >> 4) & 15, v & 15, nth[r]);
                    else
                        SDL_snprintf(line[k], sizeof line[k], "%x%x.%x%x",
                                     (v >> 12) & 15, (v >> 8) & 15,
                                     (v >> 4) & 15, v & 15);
                }
                og_page_poke(pages, OG_END_AVG + col * OG_END_TEXT_STRIDE,
                             OG_END_AT_AVG, line[0], SDL_strlen(line[0]) + 1);
                og_page_poke(pages, OG_END_BEST + col * (OG_END_TEXT_STRIDE + 1),
                             OG_END_AT_BEST, line[1], SDL_strlen(line[1]) + 1);
            }
            /* 0x3428's rule applies to the plaque too: a HUMAN truck
             * shows the initials entered at registration, not one of the
             * four fixed driver names. */
            /* 0x318f picks a different TEMPLATE for a human: "xxx Place
             * : xxx", the place and the player's initials, with no
             * nickname or driver name at all. */
            {
                static const unsigned page_human[3] = {
                    OG_END_PLAQUE_HUMAN1, OG_END_PLAQUE_HUMAN2,
                    OG_END_PLAQUE_HUMAN3
                };
                int   is_human = human && who < 3 && human[who] &&
                                 reg && reg[who][0];
                unsigned pg = is_human ? page_human[col] : page_col[col];
                /* Written at 0x3111's fixed offsets, every podium.  The
                 * old search-and-replace of "xxx" found nothing once the
                 * first podium had overwritten it, so later podiums kept
                 * the first one's places and initials; and the computer
                 * plaque's nickname and driver-name sub-pages were never
                 * touched, so a column always named its OWN truck's
                 * driver -- even with the white truck, Ivan Stewart,
                 * standing in it. */
                char style = (char)('1' + (is_human ? 0 : who));  /* any
                                     * for a human: the fill is overridden */
                og_page_poke(pages, pg, OG_END_AT_STYLE, &style, 1);
                og_page_poke(pages, pg, OG_END_AT_PLACE, pname, 3);
                if (is_human) {
                    og_page_poke(pages, pg, OG_END_AT_INITIALS, reg[who], 3);
                } else {
                    char sub[2];
                    sub[0] = (char)(OG_END_SUB_HANDLE + who);
                    sub[1] = (char)(OG_END_SUB_NAME   + who);
                    og_page_poke(pages, pg, OG_END_AT_HANDLE, &sub[0], 1);
                    og_page_poke(pages, pg, OG_END_AT_NAME,   &sub[1], 1);
                }
                og_page_draw_table(pages, pg, OG_END_PLAQUE_TAB,
                                   &scr, font, font, styles);
            }
        }
    }

    /* the board's page is style E5 (DS:0x981c): white, as a human's */
    og_page_fill = OG_END_TEXT_FILL_HUMAN;

    /* The finish-time board (0x3428): the winner's name and time, and the
     * track record with its holder's initials.  A COMPUTER truck shows
     * its four-character first name from DS:0x9862; a HUMAN shows a space
     * and that player's three registered initials. */
    if (pages && pages->loaded && font) {
        /* the WINNER, the head of the standings ([0x9d2e] at 0x33df) --
         * not column one's truck, which is red whatever its place */
        int win = order[0], z;
        char who4[5], tstr[6], rstr[6];
        if (places)
            for (z = 0; z < 4; ++z)
                if (places[z] == 1) { win = z; break; }
        if (win >= 0 && win < 3 && human && human[win] &&
            reg && reg[win][0]) {
            who4[0] = ' ';
            who4[1] = reg[win][0];
            who4[2] = reg[win][1];
            who4[3] = reg[win][2];
        } else {
            const char *nm = og_ds_string(pages, OG_END_FIRSTNAMES
                                                 + (unsigned)win * 4u);
            memcpy(who4, nm ? nm : " ???", 4);
        }
        who4[4] = '\0';
        SDL_snprintf(tstr, sizeof tstr, "%5.2f",
                     times ? times[win].total / (double)OG_PHYS_HZ : 0.0);
        SDL_snprintf(rstr, sizeof rstr, "%5.2f", record_time);
        og_page_poke(pages, OG_END_PAGE_TIMES, OG_END_TIMES_NAME, who4, 4);
        og_page_poke(pages, OG_END_PAGE_TIMES, OG_END_TIMES_TIME, tstr, 5);
        og_page_poke(pages, OG_END_PAGE_TIMES, OG_END_TIMES_INIT, record_who, 3);
        og_page_poke(pages, OG_END_PAGE_TIMES, OG_END_TIMES_REC,  rstr, 5);
        og_page_draw_table(pages, OG_END_PAGE_TIMES, OG_END_PLAQUE_TAB,
                           &scr, font, font, styles);
    }


    og_page_outline = -1;        /* every other screen keeps its styles */
    og_page_fill    = -1;

    t = texture_from_kept(ren, &scr, og_pal_end, -1, OG_LAYER_MIXED);
    og_image_free(&scr);
    return t;
}

/* The Speed Shop, composed from the eshop bank into one screen image.
 * `levels[player][0..3]` are that player's shocks, tires, accel and top
 * speed, drawn as filled segments up the bar beside each box. */
/* A panel shows ONE of three things, never the shop's boxes underneath
 * the others:
 *
 *   OG_PANEL_INVITE   the plain colour, "Player N" and "Press  Button"
 *                     (DS:0x88d5 and DS:0x8781) over the girl and trophy
 *   OG_PANEL_REGISTER the plain colour; the registration page is drawn
 *                     over it by the caller
 *   OG_PANEL_SHOP     the six upgrade boxes
 *   OG_PANEL_READY    the girl and trophy with "Get Ready!": a player who
 *                     has just registered.  Red's panel script ends its
 *                     registration (DS:0x3fae) by spawning the panel
 *                     colour (0x377e) and the figure "lg" (0x3874, sprite
 *                     621 in three pieces at +0, +56) again, showing page
 *                     0x91a8 -- sub-page 4, "Get Ready!", at (24, 56) where
 *                     the invite's "Press Button" is at (28, 56) -- and
 *                     ending itself (op 43).  So a new player never sees
 *                     the upgrade boxes before their first race, and the
 *                     shop no longer waits on them.
 *   OG_PANEL_OVER     the girl and trophy with "Game Over": a player who
 *                     is out with no credits left.  Red's panel script
 *                     (DS:0x458a) runs op 85 -- the truck goes back to the
 *                     machine -- and, its credits being 0, spawns the
 *                     colour and the figure and shows page 0x91b7: sub-
 *                     page 0x1b, "Game Over", at (28, 56), where the
 *                     invite's "Press Button" is.  It then only yields
 *                     (0x4674): the page stays for the rest of the visit
 *                     and nobody joins on that panel.
 */
#define OG_PANEL_INVITE    0
#define OG_PANEL_REGISTER  1
#define OG_PANEL_SHOP      2
#define OG_PANEL_READY     3
#define OG_PANEL_OVER      4
#define OG_SHOP_GIRL      34       /* three pieces: the figure and trophy */
#define OG_SHOP_FIGURE_DY 56       /* y 87, the panel top being 31        */

/* The Speed Shop's purchase, 0x2e01.  Money is BCD in the original; SORP
 * keeps it as a plain integer, so the BCD add and subtract become normal
 * arithmetic on the same values.  Returns 1 if the item was bought, -1
 * if it is already at its top (0x2df8), 0 if it was refused. */
static int og_shop_buy(og_racer *R, int item)
{
    int price, max, *field = NULL;

    if (item < 0 || item >= OG_SHOP_ITEMS) return 0;
    if (item == OG_SHOP_START_ITEM) return 0;         /* free, buys nothing */

    price = og_shop_price[item];
    max   = og_shop_max[item];
    switch (og_shop_field[item]) {                    /* DS:0x9550 */
    case 0x5d: field = &R->nitro;    break;   /* 10K  */
    case 0x60: field = &R->up_accel; break;   /* 80K  */
    case 0x61: field = &R->up_top;   break;   /* 100K */
    case 0x5e: field = &R->up_shock; break;   /* 60K  */
    case 0x5f: field = &R->up_tire;  break;   /* 40K  */
    default:   return 0;
    }
    if (*field >= max) return -1;                     /* already at its top */

    if (price > R->cash) {                            /* 0x2e04 */
        if (R->credits <= 0) return 0;                /* 0x2e38: refused */
        --R->credits;                                 /* 0x2e0f */
        R->cash += OG_SHOP_CREDIT_VALUE_DEC;          /* 0x2e14: +2000 */
    }
    ++*field;                                         /* 0x2e21 */
    R->cash -= price;                                 /* 0x2e23 */
    if (R->cash < 0) R->cash = 0;
    return 1;
}

static SDL_Texture *shop_build(SDL_Renderer *ren, const og_bank *shop,
                               const int levels[3][4], const int nitro[3],
                               const int cash[3], const int credits[3],
                               int next_track, const int state[3],
                               const og_font *font, const int cont[3],
                               const int ready[3], og_pages *pages,
                               const og_styles *styles)
{
    /* sprite, offset in the panel, how many pieces (field 0x8), and which
     * upgrade the bar shows (-1 for none) */
    static const struct { int sprite, x, y, pieces, upgrade; } box[] = {
        { OG_SHOP_CREDITS, 3,   5, 1, -1 },
        { OG_SHOP_CASH,   38,   5, 2, -1 },
        { OG_SHOP_NITRO,   2,  30, 2, -1 },   /* nitro is spent, not bought */
        { OG_SHOP_ACCEL,  49,  30, 2,  2 },
        { OG_SHOP_TIRES,   2,  76, 2,  1 },
        { OG_SHOP_TOPSPD, 49,  76, 2,  3 },
        { OG_SHOP_SHOCKS,  2, 122, 2,  0 },
        { OG_SHOP_START,  49, 122, 2, -1 }
    };
    og_image scr;
    SDL_Texture *t;
    int p, i;


    if (og_image_init(&scr, OG_SCREEN_W, OG_SCREEN_H) != OG_OK) return NULL;
    memset(scr.px, 0, (size_t)OG_SCREEN_W * OG_SCREEN_H);   /* index 0 surround */

    for (i = 0; i < 10; ++i) {                  /* the banner, 24..33 */
        int s = OG_SHOP_BANNER + i;
        if (s < shop->nframes)
            og_image_blit(&scr, &shop->frames[s], i * 32, 0, shop->key);
    }
    for (p = 0; p < 3; ++p) {
        static const unsigned char panel_col[3] = { 9, 12, 14 };  /* red, yellow, blue */
        int px0 = OG_SHOP_PANEL_X0 + p * OG_SHOP_PANEL_DX;
        int py0 = OG_SHOP_PANEL_Y;
        int x, y;
        for (y = py0; y < py0 + OG_SHOP_PANEL_H && y < OG_SCREEN_H; ++y)
            for (x = px0; x < px0 + OG_SHOP_PANEL_W && x < OG_SCREEN_W; ++x)
                scr.px[y * OG_SCREEN_W + x] = panel_col[p];
        if (state && state[p] != OG_PANEL_SHOP) {
            /* the invitation, "Get Ready!", or a bare panel for
             * registration */
            if (state[p] == OG_PANEL_INVITE || state[p] == OG_PANEL_READY ||
                state[p] == OG_PANEL_OVER) {
                static const char *who[3] = { "Player 1", "Player 2", "Player 3" };
                int g;
                /* From the dump: DS:0e74 and DS:0fe2 both hold sprite 621
                 * -- ESHOP index 34 -- with THREE pieces, at x 8 and 216
                 * (each panel's own origin) and y 87, which is 56 below
                 * the panel top. */
                for (g = 0; g < 3; ++g) {
                    /* Each piece carries its OWN top offset in the bank
                     * table -- 1, 0 and 28 rows for the three -- so the
                     * trophy half sits 28 rows lower than the figure.
                     * Blitting them all at one y misaligns it. */
                    const og_image *f = &shop->frames[OG_SHOP_GIRL + g];
                    /* frame_oy holds 16 - top, the truck-box convention,
                     * so the piece's own top comes back out of it */
                    int dy = shop->frame_oy
                           ? OG_TRUCK_BOX_ROWS / 2 - shop->frame_oy[OG_SHOP_GIRL + g]
                           : 0;
                    if (f->px)
                        og_image_blit(&scr, f, px0 + g * 32,
                                      py0 + OG_SHOP_FIGURE_DY + dy, shop->key);
                }
                if (font && state[p] == OG_PANEL_READY) {
                    /* page 0x91a8: "Get Ready!" alone, 4 px left of
                     * where the invite puts "Press Button" */
                    og_font_draw(font, &scr, px0 + 6 - 4, py0 + 26,
                                 "Get Ready!", 0, 5, -1);
                } else if (font && state[p] == OG_PANEL_OVER) {
                    /* page 0x91b7: "Game Over" alone, where the invite
                     * puts "Press Button" */
                    og_font_draw(font, &scr, px0 + 6, py0 + 26,
                                 "Game Over", 0, 5, -1);
                } else if (font) {
                    /* white with no border: fill and outline the same,
                     * index 5 being white in the shop palette */
                    og_font_draw(font, &scr, px0 + 14, py0 + 10, who[p], 0, 5, -1);
                    og_font_draw(font, &scr, px0 + 6,  py0 + 26,
                                 "Press  Button", 0, 5, -1);
                    /* A losing player is asked to continue: the game's
                     * own "to continue" (DS:0x88c6) under the invite,
                     * with the count below it.  Both are centred in the
                     * panel, as the reference capture shows. */
                    if (cont && cont[p] > 0) {
                        char n[4];
                        int w;
                        w = og_font_text_width(font, "to continue", 0);
                        og_font_draw(font, &scr,
                                     px0 + (OG_SHOP_PANEL_W - w) / 2,
                                     py0 + OG_SHOP_CONT_DY, "to continue",
                                     0, 5, -1);
                        SDL_snprintf(n, sizeof n, "%d", cont[p]);
                        w = og_font_text_width(font, n, 0);
                        og_font_draw(font, &scr,
                                     px0 + OG_SHOP_COUNT_X - w / 2,
                                     py0 + OG_SHOP_COUNT_DY, n, 0, 5, -1);
                    }
                }
            }
            continue;                       /* no upgrade boxes on this panel */
        }
        for (i = 0; i < (int)(sizeof box / sizeof box[0]); ++i) {
            int bx = px0 + box[i].x, by = py0 + box[i].y, k;
            for (k = 0; k < box[i].pieces; ++k) {      /* field 0x8 pieces */
                const og_image *f = &shop->frames[box[i].sprite + k];
                if (box[i].sprite + k >= shop->nframes) break;
                og_image_blit(&scr, f, bx, by, shop->key);
                bx += f->w;
            }
            /* The gauge.  op 77 (0x2c2f) spawns one object per box (record
             * DS:0x3ace, draw type 0x18) and EGA.EXE's 0x65ab draws it:
             * [+0xc] runs of hardware colour 12, each [+0xe] rows tall,
             * then [+0xd] runs of colour 5, each [+0xf] rows tall -- SORP's
             * 3 (grey) and 10 (green), its planes being bit-reversed --
             * with [+0x10] rows
             * left untouched after every run -- 7 px wide (the masks at
             * 0x659b), straight onto the screen.
             *
             *   nitro     one run each: 40 - n rows empty over n rows
             *             full, n = nitro capped at 40 -- ONE ROW A
             *             NITRO, from the bar's row 1
             *   upgrades  5 - level empty runs over level full ones, 5
             *             rows each with a 1-row gap that leaves the
             *             sprite's dividers showing, from row 12
             *
             * The nitro bar has no LEVEL heading, so its column runs the
             * whole 40 rows; painting it from row 12 like the others left
             * the sprite's own colour 15 showing above. */
            if (box[i].pieces > 1 && box[i].sprite != OG_SHOP_CASH &&
                box[i].sprite != OG_SHOP_START) {
                const og_image *face = &shop->frames[box[i].sprite];
                int gx = px0 + box[i].x + face->w + OG_SHOP_GAUGE_X;
                /* per colour: how many runs, and how tall each is */
                int gy, runs[2], rows[2], gap, r, k2, x, y;
                if (box[i].upgrade >= 0) {
                    int lv = levels[p][box[i].upgrade];
                    if (lv < 0) lv = 0;
                    if (lv > OG_SHOP_GAUGE_RUNS) lv = OG_SHOP_GAUGE_RUNS;
                    gy = py0 + box[i].y + OG_SHOP_GAUGE_Y;
                    runs[0] = OG_SHOP_GAUGE_RUNS - lv;  runs[1] = lv;
                    rows[0] = rows[1] = OG_SHOP_GAUGE_SEG;
                    gap = 1;
                } else {
                    int n = nitro[p];
                    if (n < 0) n = 0;
                    if (n > OG_SHOP_NITRO_ROWS) n = OG_SHOP_NITRO_ROWS;
                    gy = py0 + box[i].y + OG_SHOP_NITRO_Y;
                    runs[0] = runs[1] = 1;
                    rows[0] = OG_SHOP_NITRO_ROWS - n;  rows[1] = n;
                    gap = 0;
                }
                for (k2 = 0; k2 < 2; ++k2)
                    for (r = 0; r < runs[k2]; ++r) {
                        for (y = 0; y < rows[k2]; ++y, ++gy)
                            for (x = 0; x < OG_SHOP_GAUGE_W; ++x) {
                                int sx = gx + x;
                                if (sx < 0 || sx >= OG_SCREEN_W || gy < 0 || gy >= OG_SCREEN_H) continue;
                                scr.px[gy * OG_SCREEN_W + sx] =
                                    k2 ? OG_SHOP_GAUGE_FULL : OG_SHOP_GAUGE_EMPTY;
                            }
                        gy += gap;
                    }
            }
            /* A player who has chosen to start wears the HIGHLIGHTED
             * label: sprites 68 and 69, the same "START NEXT / RACE"
             * strip with its colours inverted. */
            if (box[i].sprite == OG_SHOP_START && ready && ready[p]) {
                int hx = px0 + box[i].x, k2;
                for (k2 = 0; k2 < OG_SHOP_START_HI_W &&
                             OG_SHOP_START_HI + k2 < shop->nframes; ++k2) {
                    og_image_blit(&scr, &shop->frames[OG_SHOP_START_HI + k2],
                                  hx, py0 + box[i].y + OG_SHOP_START_HI_DY,
                                  shop->key);
                    hx += shop->frames[OG_SHOP_START_HI + k2].w;
                }
            }

            /* the next track's thumbnail, inside the start box */
            if (box[i].sprite == OG_SHOP_START && next_track >= 0) {
                int t0 = OG_SHOP_THUMB + 2 * (next_track % OG_NUM_TRACKS);
                int tx = px0 + box[i].x + OG_SHOP_THUMB_DX;
                int ty = py0 + box[i].y + OG_SHOP_THUMB_DY, k2;
                for (k2 = 0; k2 < 2 && t0 + k2 < shop->nframes; ++k2) {
                    og_image_blit(&scr, &shop->frames[t0 + k2], tx, ty, shop->key);
                    tx += shop->frames[t0 + k2].w;
                }
                /* a Track-Pak track's own map, over the DOS picture
                 * (og_shop_next_track counts them 8..15) */
                if (next_track >= OG_NUM_TRACKS && og_tp_thumb[next_track % TP_TRACKS].px)
                    og_image_blit(&scr, &og_tp_thumb[next_track % TP_TRACKS],
                                  px0 + box[i].x + OG_SHOP_THUMB_DX, ty, -1);
            }
            bx = px0 + box[i].x;
            if (0) {
                /* fill the bar from the bottom, one segment per level */
                const og_image *b = &shop->frames[box[i].sprite + 1];
                int lv = levels[p][box[i].upgrade], seg;
                for (seg = 0; seg < lv && seg < OG_UPG_LEVELS; ++seg) {
                    int y0 = by + b->h - 4 - seg * (b->h / (OG_UPG_LEVELS + 1));
                    int x, y;
                    for (y = y0 - 4; y < y0; ++y)
                        for (x = bx + OG_SHOP_BAR_DX + 4;
                             x < bx + OG_SHOP_BAR_DX + b->w - 4; ++x)
                            if (x >= 0 && x < OG_SCREEN_W && y >= 0 && y < OG_SCREEN_H)
                                scr.px[y * OG_SCREEN_W + x] = 10;   /* green */
                }
            }
        }

        /* The CREDITS and CASH boxes' numbers: the panel's own page,
         * rewritten as op 78 does (0x2e88) and drawn whole -- see
         * OG_SHOP_MONEY_PAGE. */
        if (pages && pages->loaded && font) {
            const unsigned pg = OG_SHOP_MONEY_PAGE + (unsigned)p * OG_SHOP_MONEY_STRIDE;
            const int ax = px0 + OG_SHOP_MONEY_DX, ay = py0 + OG_SHOP_MONEY_DY;
            unsigned v = (unsigned)(cash ? cash[p] : 0), bcd = 0, at;
            int n, w;
            char c, at3[3];
            /* the truck's money is a BCD word: four nibbles */
            for (n = 0; n < 4; ++n, v /= 10) bcd |= (v % 10) << (4 * n);
            c = (char)('0' + (credits ? credits[p] : 0));           /* +21 */
            og_page_poke(pages, pg, OG_SHOP_MONEY_CRED, &c, 1);
            og_page_poke(pages, pg, OG_SHOP_MONEY_CASH, "      ", 6); /* +25..2a */
            at = OG_SHOP_MONEY_LAST;
            for (n = 4; n > 0 && bcd; --n, bcd >>= 4) {             /* 0x2ea6 */
                if (n == 2) { og_page_poke(pages, pg, at, ",", 1); --at; }
                c = (char)('0' + (bcd & 15));
                og_page_poke(pages, pg, at, &c, 1);
                --at;
            }
            og_page_poke(pages, pg, at, "$", 1);
            at3[0] = (char)((ax >> 8) & 0xff);                       /* at() */
            at3[1] = (char)(ax & 0xff);
            at3[2] = (char)ay;
            og_page_poke(pages, pg, OG_SHOP_MONEY_AT, at3, 3);
            og_page_poke(pages, pg, OG_SHOP_MONEY_CRED_X, at3, 2);   /* +1f */
            /* measured from +25, blanks and all, in IRON at -1 (0x2eed) */
            w = og_page_measure_in(pages, pg + OG_SHOP_MONEY_CASH, font, font,
                                   styles, -1, 0, 0);
            w = px0 + (OG_SHOP_MONEY_RIGHT - w);
            at3[0] = (char)((w >> 8) & 0xff);
            at3[1] = (char)(w & 0xff);
            og_page_poke(pages, pg, OG_SHOP_MONEY_CASH_X, at3, 2);   /* +23 */
            og_page_fill    = OG_SHOP_MONEY_FILL;
            og_page_outline = OG_SHOP_MONEY_EDGE;
            og_page_draw_table(pages, pg, 0, &scr, font, font, styles);
            og_page_fill    = -1;
            og_page_outline = -1;
        }
    }
    t = texture_from_kept(ren, &scr, og_pal_shop, -1, OG_LAYER_MIXED);
    og_image_free(&scr);
    return t;
}

/* What a truck is running with this tick: the player's four upgrades, or
 * a computer truck's single rubber-banded level (image 0x147c / 0x14c7). */
typedef struct { int shock, steer, drag, slip, grip, accel, top; } og_tperf;

static void racer_perf(og_tperf *o, const og_perf *p, const og_racer *R)
{
    if (R->computer) {
        int L = R->level;
        if (L < 0) L = 0;
        if (L >= OG_CPU_LEVELS) L = OG_CPU_LEVELS - 1;
        o->shock = p->c_shock[L]; o->steer = p->c_steer[L]; o->drag = p->c_drag[L];
        o->slip  = p->c_slip[L];  o->grip  = p->c_grip[L];  o->accel = p->c_accel[L];
        o->top   = p->c_top[L];
    } else {
        o->shock = p->shock[R->up_shock]; o->steer = p->steer[R->up_tire];
        o->drag  = p->drag[R->up_tire];   o->slip  = p->slip[R->up_tire];
        o->grip  = p->grip[R->up_tire];   o->accel = p->accel[R->up_accel];
        o->top   = p->top[R->up_top];
    }
}

/* Acquire the nearest waypoint, as the rescan at image 0x21f3 does. */
/* The rubber-band at 0x2321.  The standings are sorted, then the HUMAN's
 * truck is compared with the leader and the computer drivers' level moves
 * to suit: a lap down makes them slower, a lap up makes them quicker, and
 * within a lap the signed table at DS:0x753f scales with the gap.
 *
 * It is skipped entirely in demo mode -- 0x231a jumps the block when
 * [0x1a1] is set -- so an attract race never rubber-bands. */
/* The average lap 0x2132 compares against the ladder's step tables.
 * Those thresholds run 456 down to 176, which at OG_PHYS_HZ is 22.8 s
 * down to 8.8 s -- real lap times -- so they are in the same units
 * SORP already counts in.  No conversion. */
static int og_ladder_avg(long ticks, int laps)
{
    return laps > 0 ? (int)(ticks / laps) : 0;
}

/* 0x2105: seed the field's levels at the start of a race.  In DEMO mode
 * every truck starts from 17; otherwise from the ladder's level for this
 * race.  Each successive computer truck is ONE LOWER than the last,
 * floored at 0 -- the field is not uniform.
 *
 * Both the player's and the computer's tables RISE with the level (the
 * player's top speed runs 134..154 as upgrades are bought, the
 * computer's 126..186), so a higher level is a STRONGER driver.
 *
 * The walk goes from the LAST truck down: truck 3 is white, IRONMAN, who
 * is the strongest opponent in the original, and seeding forward made
 * him the weakest. */
static void og_ai_seed_levels(og_racer *R, int n, int attract, int base,
                              int track, int race)
{
    /* 0x20c6: the field's starting level is the track's own base, plus
     * HALF the best human's ladder level, plus a QUARTER of the race
     * number -- the season's ramp. */
    int i, lv;
    if (attract) lv = OG_AI_LEVEL_MAX;
    else {
        lv = og_ai_track_base[track & 7] + (base >> 1) + (race >> 2);
        OG_DEBUG("[OG_AI] Field base: track %d gives %d, ladder %d -> %d, race %d -> %d",
                track & 7, og_ai_track_base[track & 7], base, base >> 1,
                race, race >> 2);
        if (lv > OG_AI_LEVEL_MAX) lv = OG_AI_LEVEL_MAX;
    }
    for (i = n - 1; i >= 0; --i) {
        if (!R[i].computer) continue;
        R[i].ai_base = lv;
        R[i].level   = lv;
        OG_DEBUG("[OG_AI] Truck %d seeded at level %d", i, lv);
        if (--lv < 0) lv = 0;
    }
}

/* The rubber band at 0x2321.  Each COMPUTER truck is banded INDIVIDUALLY
 * against the human: di is the truck being adjusted, si the first truck
 * in the list at DS:0x9d2e whose computer bit is clear.
 *
 *     di a lap behind si  ->  +5          stronger
 *     di a lap ahead      ->  -5          weaker
 *     the same lap        ->  al = [di+0x43] - [si+0x43]
 *                             sar al,1 x4        SIGNED
 *                             xlatb from DS:0x753f   UNSIGNED index
 *
 * That last pair is the quirk that gives the game its bite: the shift is
 * arithmetic, so a truck BEHIND the human yields a negative value, and
 * xlatb then reads it as 0xf0..0xff -- past the sixteen deliberate
 * entries, into bytes worth +4 to +14.  A trailing truck is therefore
 * often clamped to the top levels and comes back very fast, while the
 * deliberate entries only ever peg back a truck that is AHEAD.
 *
 * Skipped entirely in demo mode (0x231a). */
/* THE STANDINGS are a single list, DS:0x9d2e -- four truck pointers,
 * first place first -- and a truck's place [di+0x3b] is only ever its
 * index in it.  Two trucks therefore can never share a place.
 *
 * 0x12f9 lays the list out WHITE, blue, yellow, red -- but only from
 * 0x1e4, where the race number goes back to 0 (attract starting, a game
 * starting).  Between races the list CARRIES OVER, so each race starts
 * in the last one's finishing order.  0x3878 bubble-sorts it by lap [+0x3c] then progress
 * [+0x42], and swaps a pair only when the lower one is STRICTLY ahead
 * (0x389c ja / 0x38a6 jae), so trucks level on both keep the order they
 * already had -- a dead heat goes to whoever stood higher before.  The
 * sort runs with the rubber band (0x233b, not in demo mode) and once
 * more as the race ends (0x36ec); the places it leaves are what the
 * prizes (0x3727), the club's "won" (0x3751) and the plaques use.  The
 * order trucks cross the line in is not recorded anywhere.
 *
 * SORP counted the trucks ahead of each one, so two level trucks shared
 * a place, and it also numbered trucks 1, 2, 3 as they crossed -- mixing
 * the two put two trucks on the same step of the podium. */
static int og_standing[OG_RACERS] = { 3, 2, 1, 0 };

static void og_standings_reset(og_racer *R)          /* 0x12f9 */
{
    static const int order[OG_RACERS] = { 3, 2, 1, 0 };
    int i;
    for (i = 0; i < OG_RACERS; ++i) {
        og_standing[i] = order[i];
        R[order[i]].place = i + 1;
    }
}

static void og_race_positions(og_racer *R, int n)    /* 0x3878 */
{
    int i, swapped;
    (void)n;
    do {
        swapped = 0;
        for (i = 0; i + 1 < OG_RACERS; ++i) {
            const og_racer *a = &R[og_standing[i]], *b = &R[og_standing[i + 1]];
            int t;
            if (a->laps > b->laps ||
                (a->laps == b->laps && a->ai.progress >= b->ai.progress))
                continue;
            t = og_standing[i];
            og_standing[i] = og_standing[i + 1];
            og_standing[i + 1] = t;
            swapped = 1;
        }
    } while (swapped);
    for (i = 0; i < OG_RACERS; ++i) R[og_standing[i]].place = i + 1;
}

static void og_ai_rubber_band(og_racer *R, int n, int attract, int base,
                              int lap_span)
{
    int i, human = -1;

    (void)base;
    if (attract) return;

    for (i = 0; i < n; ++i)
        if (!R[i].computer) { human = i; break; }
    if (human < 0) return;

    for (i = 0; i < n; ++i) {
        int adj, lv;
        if (!R[i].computer) continue;
        if (R[i].laps < R[human].laps)      adj = OG_AI_LAP_BEHIND;
        else if (R[i].laps > R[human].laps) adj = OG_AI_LAP_AHEAD;
        else {
            /* [di+0x43] is the HIGH byte of the progress word, so the
             * lap spans 0..255 there; scale ours to match. */
            int span = lap_span ? lap_span : 255;
            int a = R[i].ai.progress     * 255 / span;
            int b = R[human].ai.progress * 255 / span;
            signed char diff = (signed char)(a - b);
            diff = (signed char)(diff >> 4);
            adj = og_ai_gap_full[(unsigned char)diff];
        }
        lv = R[i].ai_base + adj;
        if (lv < 0) lv = 0;
        if (lv > OG_AI_LEVEL_MAX) lv = OG_AI_LEVEL_MAX;
        R[i].level = lv;
    }
}

static void og_ai_acquire(og_racer *R, const og_route *route)
{
    int q, best = -1;
    float bd = 1e9f;
    for (q = 0; q < route->n; ++q) {
        float ex = (float)route->wp[q].cx + 0.5f - R->wx;
        float ey = (float)route->wp[q].cy + 0.5f - R->wy;
        float e = ex * ex + ey * ey;
        if (e < bd) { bd = e; best = q; }
    }
    if (best < 0) return;
    R->ai.cursor = route->wp[best].off;       /* [di+0x4a] */
    R->from = best;                            /* [di+0x4c] */
    q = og_ai_next(&R->ai, route);
    R->to = (q < 0) ? best : q;                /* [di+0x4e] */
}

static const int og_racer_colour[OG_RACERS] = {
    OG_TRUCK_COLOUR_RED, OG_TRUCK_COLOUR_YELLOW,
    OG_TRUCK_COLOUR_BLUE, OG_TRUCK_COLOUR_WHITE
};

/* The track's scenery, drawn at the screen positions its list gives and
 * sorted with everything else by world Y. */
/* Each piece rests on its idle frame until a truck hits it, then runs its
 * animation once and settles again -- the flagpole swinging through
 * 2,3,4,3,2,1,0,1 with each pass held longer, the bunting rippling
 * through its even frames. */
typedef struct { unsigned char on, i; signed char hold; } og_sc_state;

static void og_scenery_tick(const og_scenery *s, og_sc_state *st)
{
    int i;
    if (!s || !s->loaded) return;
    for (i = 0; i < s->n; ++i) {
        const og_fx_anim *a = &s->hit[s->type[i]];
        if (!st[i].on || a->n <= 0) continue;
        if (--st[i].hold < 0) {
            if (st[i].i + 1 >= a->n) { st[i].on = 0; st[i].i = 0; }
            else { ++st[i].i; st[i].hold = (signed char)a->dur[st[i].i]; }
        }
    }
}

static void og_draw_sprite_q(int key, int sprite, int x, int y);
static void og_draw_label_q(int sprite, int x, int y);

/* Queue the scenery for the race screen (og_screen_compose), each piece
 * at its own sort key. */
static void og_scenery_queue(const og_scenery *s, const og_bank *eg,
                             const og_sc_state *st)
{
    int i;
    if (!s || !s->loaded) return;
    for (i = 0; i < s->n; ++i) {
        int t = s->type[i];
        int frame = (st && st[i].on && s->hit[t].n > 0)
                    ? s->hit[t].frame[st[i].i] : s->idle[t];
        int sp = s->base[t] + frame, e = sp - OG_EGAME_FIRST;
        og_draw_sprite_q(s->sort[i], sp, s->sx[i], s->sy[i]);
        /* Bunting is drawn as a PAIR: the 32 px piece at the record's X
         * and the 16 px piece that follows it, butted against its right
         * edge (52 with 53, 56 with 57 while animating).  The bunting is
         * draw type 8 ([si+0x1a], the flagpole's is 4), EGA.EXE 9:6394:
         * ONE sprite-table entry, 0x1dc + frame, blitted 32 px wide and
         * then -- di + 4, 32 px on -- 16 px wide, the source running on
         * into the next frame's data. */
        if (t == 1 && e >= 0 && e + 1 < eg->nframes)
            og_draw_sprite_q(s->sort[i], sp + 1,
                             s->sx[i] + eg->frames[e].w, s->sy[i]);
    }
}

/* Queue the live effects and labels, each at its own sort key [si+0x18]
 * -- a label's is 0, so it goes under everything else.  Each sprite sits
 * at the projected point plus its table x offset (+7) and top (+5). */
static void og_fx_queue(const og_fx_obj *pool)
{
    int k;
    for (k = 0; k < OG_FX_POOL; ++k) {
        const og_fx_obj *f = &pool[k];
        if (!f->live) continue;
        if (f->fixed) og_draw_label_q(f->fixed, f->px, f->py);
        else          og_draw_sprite_q(f->sort_y, f->a->sprite + f->a->frame[f->i],
                                       f->px, f->py);
    }
}

/* ------------------------------------------------------------------ */
/* The race screen, composed as the EGA screen is                       */
/* ------------------------------------------------------------------ */

/* Every tick 0x07e0 inserts each visible object into ONE display list
 * (head at DS:0x29a6), ascending by its sort key [si+0x18] -- a new
 * object goes after any with the same key -- and EGA.EXE 0x257c walks
 * that list drawing each by its type ([si+0x1a] through cs:0xa2a).
 * A truck is type 0, 0x57aa, which draws its SHADOW (0x60a0) and then
 * the truck, one object after another: so every truck, its shadow, the
 * scenery, the effects, the labels and the pickup interleave by key.
 *
 * Two consequences SORP's SDL drawing could not give, so the race is
 * composed here in palette indices, as the screen memory is:
 *
 *   - the shadow writes plane 0 only (Map Mask 1), setting bit 0 of
 *     WHATEVER is already on the screen.  Over another truck it turns
 *     that truck's colours to their dark twins; it does not paint the
 *     track over it.
 *   - only the truck bank carries the foreground mask (ETRUCK loads
 *     with flags 0x87, bit 7); EGAME and ESHADOW are painted straight
 *     over the foreground plane.
 *   - and a truck is masked only while it is near the ground.  0x15e8
 *     sets bit 0 of [si+0xa] and clears it once the body is the track's
 *     threshold [0x4f78] or more above the ground; EGA.EXE 0x582e picks
 *     the blitters 7 on for bit 0 -- 0x5af5, which ANDs each byte with
 *     the mask at ss:[di] -- and otherwise 0x59ea, a plain copy.  So a
 *     truck in the air flies OVER bridges and banks.
 *
 * Ties go in the order the objects are queued here -- scenery, then
 * trucks, then the pickup and effects -- which follows the order they
 * are created, not a traced list. */
enum { OG_DI_SPRITE, OG_DI_TRUCK };
typedef struct {
    int key, seq, kind;
    int sprite, x, y;            /* EGAME sprite and point, or truck frame
                                  * and its top-left                       */
    int colour;                  /* the truck's colour offset              */
    int sh, sh_x, sh_y;          /* its shadow: index (-1 none) and corner */
    int high;                    /* a truck lifted clear of the ground:
                                  * drawn OVER the foreground (below)      */
    int depth;                   /* the arcade's depth (TP_DEPTH), for its
                                  * overlays; -1 on the DOS art            */
} og_draw_item;
#define OG_DRAW_MAX 160
static og_draw_item og_draws[OG_DRAW_MAX];
static int          og_ndraws;
static unsigned char og_screen[OG_SCREEN_W * OG_SCREEN_H];

/* A Track-Pak track's background is the arcade's own, in full colour,
 * not palette indices.  The screen then starts as 16 everywhere, "the
 * background here"; a shadow's bit 0 makes it 17, "the background in
 * shadow"; sprites and trucks write their indices over it as ever.  The
 * last pass takes 16 and 17 from the art and its shadow (the arcade's
 * palette bank 15), everything else from the race palette. */
#define OG_SCREEN_BG 16
static const tp_art *og_tp_bg;

static Uint32 og_screen_rgb(unsigned v, int pixel)
{
    const unsigned char *c;
    if (v >= OG_SCREEN_BG && og_tp_bg)
        c = (v == OG_SCREEN_BG ? og_tp_bg->rgb : og_tp_bg->shadow) + pixel * 3;
    else
        c = og_pal_race + (v & 15) * 3;
    return 0xff000000u | ((Uint32)c[0] << 16) | ((Uint32)c[1] << 8) | c[2];
}

static og_draw_item *og_draw_add(int kind, int key)
{
    og_draw_item *d;
    if (og_ndraws >= OG_DRAW_MAX) return NULL;
    d = &og_draws[og_ndraws];
    memset(d, 0, sizeof *d);
    d->kind = kind; d->key = key; d->seq = og_ndraws++;
    d->sh = -1;
    d->depth = -1;
    return d;
}

static void og_draw_sprite_q(int key, int sprite, int x, int y)
{
    og_draw_item *d = og_draw_add(OG_DI_SPRITE, key);
    if (d) {
        d->sprite = sprite; d->x = x; d->y = y;
        /* On the arcade's art, every sprite is in the video CPU's depth
         * order, the overlays among them: dust, splashes, pickups and
         * the rest behind the scenery that stands in front of them.
         * The key is world y >> 5, so its depth is the truck's formula.
         * A label (key 0, under everything) and the flagman (0x7fff,
         * over everything) are placed by no world y: no depth. */
        if (og_tp_bg && key > 0 && key < 0x7fff)
            d->depth = TP_DEPTH((long)key << 5);
    }
}

/* A pickup's label ("10,000", "2 NITROS"): at key 0, under everything
 * else, and never masked -- neither by the foreground nor by the
 * arcade's overlays (`high`, as a truck in the air is over the mask). */
static void og_draw_label_q(int sprite, int x, int y)
{
    og_draw_item *d = og_draw_add(OG_DI_SPRITE, 0);
    if (d) { d->sprite = sprite; d->x = x; d->y = y; d->high = 1; }
}

static int og_draw_cmp(const void *a, const void *b)
{
    const og_draw_item *p = (const og_draw_item *)a, *q = (const og_draw_item *)b;
    if (p->key != q->key) return p->key < q->key ? -1 : 1;
    return p->seq - q->seq;               /* the later one goes after */
}

/* og_sprite_draw's placement, into the indexed screen */
static void og_screen_sprite_fg(const og_bank *eg, int index, int px, int py,
                                const unsigned char *fg, int depth)
{
    const unsigned char *ov = og_tp_bg && depth >= 0 ? og_tp_bg->over : NULL;
    int e = index - OG_EGAME_FIRST, x, y;
    const og_image *f;
    if (e < 0 || e >= eg->nframes) return;
    f = &eg->frames[e];
    px += eg->frame_ox[e];
    py += OG_TRUCK_BOX_ROWS / 2 - eg->frame_oy[e];
    for (y = 0; y < f->h; ++y)
        for (x = 0; x < f->w; ++x) {
            int sx = px + x, sy = py + y, v = f->px[y * f->w + x] & 15;
            if (v == eg->key) continue;
            if (sx < 0 || sx >= OG_SCREEN_W || sy < 0 || sy >= OG_SCREEN_H) continue;
            if (fg && fg[sy * OG_SCREEN_W + sx]) continue;
            if (ov && ov[sy * OG_SCREEN_W + sx] > depth) continue;
            og_screen[sy * OG_SCREEN_W + sx] = (unsigned char)v;
        }
}

static void og_screen_sprite(const og_bank *eg, int index, int px, int py)
{
    og_screen_sprite_fg(eg, index, px, py, NULL, -1);
}

/* Sort the queue and draw it over `art`, then hand back the screen as a
 * texture.  `fg` is the foreground mask, applied to trucks only (NULL to
 * leave them unmasked) -- the DOS game's way (0x15e8), and a truck in
 * the air goes over it.
 *
 * On the arcade's art the mask is the arcade's: its palette's priority
 * patch, which shows the background in every sprite bank over the
 * patched pens.  So EVERY sprite is under it -- the trucks, their
 * shadows, the dust and the rest -- in the air or not.  Checked in MAME
 * on Cuttoff Pass and Boulder Hill: in 232 frames of racing, not one
 * sprite pixel over a masked one. */
static void og_screen_compose(SDL_Texture *tex, const unsigned char *art,
                              const unsigned char *fg, const og_bank *eg,
                              const og_bank *trucks, const og_shadows *sh)
{
    static Uint32 rgba[OG_SCREEN_W * OG_SCREEN_H];
    int i, x, y;
    for (i = 0; i < OG_SCREEN_W * OG_SCREEN_H; ++i)
        og_screen[i] = og_tp_bg ? OG_SCREEN_BG : (unsigned char)(art[i] & 15);
    qsort(og_draws, (size_t)og_ndraws, sizeof og_draws[0], og_draw_cmp);
    for (i = 0; i < og_ndraws; ++i) {
        const og_draw_item *d = &og_draws[i];
        if (d->kind == OG_DI_SPRITE) {
            og_screen_sprite_fg(eg, d->sprite, d->x, d->y,
                                og_tp_bg && !d->high ? fg : NULL, d->depth);
            continue;
        }
        /* the arcade's overlays (tp_data.c): scenery over a truck
         * further up the course, its shadow with it */
        const unsigned char *ov = og_tp_bg && d->depth >= 0 ? og_tp_bg->over : NULL;
        if (d->sh >= 0 && sh && sh->loaded) {          /* 0x60a0: plane 0 */
            int rows = sh->rows[d->sh];
            if (rows > OG_TRUCK_BOX_ROWS) rows = OG_TRUCK_BOX_ROWS;
            for (y = 0; y < rows; ++y)
                for (x = 0; x < 24; ++x) {
                    int sx = d->sh_x + x, sy = d->sh_y + y;
                    if (!sh->mask[d->sh][y * 24 + x]) continue;
                    if (sx < 0 || sx >= OG_SCREEN_W || sy < 0 || sy >= OG_SCREEN_H) continue;
                    if (og_tp_bg && fg && fg[sy * OG_SCREEN_W + sx]) continue;
                    if (ov && ov[sy * OG_SCREEN_W + sx] > d->depth) continue;
                    og_screen[sy * OG_SCREEN_W + sx] |= 1;
                }
        }
        if (d->sprite >= 0 && d->sprite < trucks->nframes) {   /* the truck */
            const og_image *f = &trucks->frames[d->sprite];
            for (y = 0; y < f->h; ++y)
                for (x = 0; x < f->w; ++x) {
                    int sx = d->x + x, sy = d->y + y, v = f->px[y * f->w + x] & 15, p;
                    if (v == trucks->key) continue;
                    if (sx < 0 || sx >= OG_SCREEN_W || sy < 0 || sy >= OG_SCREEN_H) continue;
                    p = sy * OG_SCREEN_W + sx;
                    if (fg && fg[p] && (!d->high || og_tp_bg)) continue;
                    if (ov && ov[p] > d->depth) continue;
                    og_screen[p] = (unsigned char)og_truck_remap(v, d->colour);
                }
        }
    }
    for (i = 0; i < OG_SCREEN_W * OG_SCREEN_H; ++i)
        rgba[i] = og_screen_rgb(og_screen[i], i);
    SDL_UpdateTexture(tex, NULL, rgba, OG_SCREEN_W * 4);
}

/* The shaders.  The SCREEN shader works on the finished frame,
 * the overlays (F1-F3, the exit prompt, the meters) being drawn over its
 * result untouched:
 *   none       the frame as it is, scaled to the window
 *   scanlines  the lower half of every source row darkened, as a CRT's
 *              gaps between lines show at this resolution
 *   eagle      the Eagle 2x pixel-art scaler, then scaled to the window
 *   MMPX 2x    MMPX on the finished frame's colours, as Eagle is run
 * Eagle and MMPX are both 2x scalers; they come last, so a screen that
 * cannot show 2x drops them together (og_shader_limit).
 * The SPRITE shader upscales the sprites themselves:
 *   none       as drawn
 *   MMPX 2x    every EGAME sprite, ETRUCK frame and shadow put through
 *              MMPX once, and the race screen composed at 2x (640x400)
 *   MMPX 4x    ... through MMPX twice, composed at 4x (1280x800)
 * The setting is the number of MMPX passes, and the race is composed at
 * the larger of the sprite and background shaders' scales. */
enum { OG_SHADER_NONE, OG_SHADER_SCANLINES, OG_SHADER_EAGLE, OG_SHADER_MMPX,
       OG_SCREEN_SHADERS };
static const char *const og_screen_shader_nm[] = { "none", "scanlines", "eagle",
                                                   "MMPX 2x" };
#define OG_SHADER_2X(m) ((m) == OG_SHADER_EAGLE || (m) == OG_SHADER_MMPX)
enum { OG_SPRITES_NONE, OG_SPRITES_MMPX2, OG_SPRITES_MMPX4, OG_SPRITE_SHADERS };
static const char *const og_sprite_shader_nm[] = { "none", "MMPX 2x", "MMPX 4x" };
static int og_screen_shader = OG_SHADER_NONE;
static int og_sprite_shader = OG_SPRITES_NONE;
/* The BACKGROUND shader scales the full-screen artwork:
 *   none       pixels made 4x4 (when the screen is composed at 4x at all)
 *   MMPX 4x    the track artwork put through MMPX twice, once per track
 * Either shader turns the 4x race screen on; the other part of it is then
 * simply enlarged.  The still screens are covered by their layer kind
 * (texture_from_kept): the title and other full-screen pictures by this
 * shader, logos and text by the sprite shader. */
enum { OG_BACK_NONE, OG_BACK_MMPX2, OG_BACK_MMPX4, OG_BACK_SHADERS };
static const char *const og_back_shader_nm[] = { "none", "MMPX 2x", "MMPX 4x" };
static int og_back_shader = OG_BACK_NONE;
#define OG_COMPOSE4() (og_sprite_shader != OG_SPRITES_NONE || \
                       og_back_shader != OG_BACK_NONE)
/* the scale the race is composed at: 2 to the larger number of passes */
#define OG_UP_MAX 4
static int og_up_k(void)
{
    const int p = og_sprite_shader > og_back_shader ? og_sprite_shader
                                                     : og_back_shader;
    return 1 << p;
}
/* the framebuffer's window is at most this many times 320x200 */
#define OG_FB_MAXK 2

/* ------------------------------------------------------------------ */
/* MMPX, and the race screen at 4x                                      */
/* ------------------------------------------------------------------ */

/* MMPX 2x (Morgan McGuire and Mara Gagiu, "MMPX Style-Preserving Pixel
 * Art Magnification", JCGT 2021), after the paper's reference code.  It
 * only ever COPIES a neighbour into an output pixel -- it never blends
 * -- so it runs here straight on palette indices, and the race screen's
 * index tricks (the shadow's bit 0, the truck colour remap, the
 * foreground mask) still work on its output.  `luma` gives each index's
 * brightness, as the reference's luma(): r + g + b + 1, and 256 times
 * that for the transparent key, so a sprite's outline extends into
 * clear pixels rather than the reverse.  Edges are clamped.
 *
 * The core works on 32-bit pixels: palette indices, or a finished
 * frame's colours for the screen shader.  With no `luma` table a pixel's
 * brightness is its own r + g + b + 1, and only its colour bits are
 * compared. */
static Uint32 og_mmpx_luma(Uint32 v, const Uint32 *luma)
{
    return luma ? luma[v & 15]
                : (v & 255) + ((v >> 8) & 255) + ((v >> 16) & 255) + 1;
}

static void og_mmpx2x_px(const Uint32 *src, int w, int h, Uint32 *dst,
                         const Uint32 *luma)
{
    int x, y;
#define M(dx, dy) (src[(((y) + (dy)) < 0 ? 0 : ((y) + (dy)) >= h ? h - 1 : ((y) + (dy))) * w + \
                       (((x) + (dx)) < 0 ? 0 : ((x) + (dx)) >= w ? w - 1 : ((x) + (dx)))] \
                   & 0xffffffu)
#define EQ3(b, a0, a1, a2)      ((b) == (a0) && (b) == (a1) && (b) == (a2))
#define EQ4(b, a0, a1, a2, a3)  ((b) == (a0) && (b) == (a1) && (b) == (a2) && (b) == (a3))
#define ANY3(b, a0, a1, a2)     ((b) == (a0) || (b) == (a1) || (b) == (a2))
#define NONE2(b, a0, a1)        ((b) != (a0) && (b) != (a1))
#define NONE4(b, a0, a1, a2, a3) ((b) != (a0) && (b) != (a1) && (b) != (a2) && (b) != (a3))
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x) {
            const unsigned A = M(-1, -1), B = M(0, -1), C = M(1, -1);
            const unsigned D = M(-1,  0), E = M(0,  0), F = M(1,  0);
            const unsigned G = M(-1,  1), H = M(0,  1), I = M(1,  1);
            unsigned J = E, K = E, L = E, N = E;     /* N is the paper's M */
            Uint32 *o = dst + (size_t)(2 * y) * (size_t)(2 * w) + 2 * x;

            if (A != E || B != E || C != E || D != E ||
                F != E || G != E || H != E || I != E) {
                const unsigned P = M(0, -2), S = M(0, 2), Q = M(-2, 0), R = M(2, 0);
                const Uint32 Bl = og_mmpx_luma(B, luma), Dl = og_mmpx_luma(D, luma),
                             El = og_mmpx_luma(E, luma), Fl = og_mmpx_luma(F, luma),
                             Hl = og_mmpx_luma(H, luma);

                /* 1:1 slopes */
                if ((D == B && D != H && D != F) && (El >= Dl || E == A) &&
                    ANY3(E, A, C, G) && ((El < Dl) || A != D || E != P || E != Q)) J = D;
                if ((B == F && B != D && B != H) && (El >= Bl || E == C) &&
                    ANY3(E, A, C, I) && ((El < Bl) || C != B || E != P || E != R)) K = B;
                if ((H == D && H != F && H != B) && (El >= Hl || E == G) &&
                    ANY3(E, A, G, I) && ((El < Hl) || G != H || E != S || E != Q)) L = H;
                if ((F == H && F != B && F != D) && (El >= Fl || E == I) &&
                    ANY3(E, C, G, I) && ((El < Fl) || I != H || E != R || E != S)) N = F;

                /* intersections */
                if ((E != F && EQ4(E, C, I, D, Q) && F == B && F == H) && (F != M(3, 0))) K = N = F;
                if ((E != D && EQ4(E, A, G, F, R) && D == B && D == H) && (D != M(-3, 0))) J = L = D;
                if ((E != H && EQ4(E, G, I, B, P) && H == D && H == F) && (H != M(0, 3))) L = N = H;
                if ((E != B && EQ4(E, A, C, H, S) && B == D && B == F) && (B != M(0, -3))) J = K = B;
                if (Bl < El && EQ4(E, G, H, I, S) && NONE4(E, A, D, C, F)) J = K = B;
                if (Hl < El && EQ4(E, A, B, C, P) && NONE4(E, D, G, I, F)) L = N = H;
                if (Fl < El && EQ4(E, A, D, G, Q) && NONE4(E, B, C, I, H)) K = N = F;
                if (Dl < El && EQ4(E, C, F, I, R) && NONE4(E, B, A, G, H)) J = L = D;

                /* 2:1 slopes */
                if (H != B) {
                    if (H != A && H != E && H != C) {
                        if (EQ3(H, G, F, R) && NONE2(H, D, M(2, -1))) L = N;
                        if (EQ3(H, I, D, Q) && NONE2(H, F, M(-2, -1))) N = L;
                    }
                    if (B != I && B != G && B != E) {
                        if (EQ3(B, A, F, R) && NONE2(B, D, M(2, 1))) J = K;
                        if (EQ3(B, C, D, Q) && NONE2(B, F, M(-2, 1))) K = J;
                    }
                }
                if (F != D) {
                    if (D != I && D != E && D != C) {
                        if (EQ3(D, A, H, S) && NONE2(D, B, M(1, 2))) J = L;
                        if (EQ3(D, G, B, P) && NONE2(D, H, M(1, -2))) L = J;
                    }
                    if (F != E && F != A && F != G) {
                        if (EQ3(F, C, H, S) && NONE2(F, B, M(-1, 2))) K = N;
                        if (EQ3(F, I, B, P) && NONE2(F, H, M(-1, -2))) N = K;
                    }
                }
            }
            o[0] = J;  o[1] = K;
            o[2 * w] = L;  o[2 * w + 1] = N;
        }
#undef M
#undef EQ3
#undef EQ4
#undef ANY3
#undef NONE2
#undef NONE4
}

/* A frame scaled up k times (2 or 4): `passes` of MMPX, each doubling
 * it, and what is left of k by each pixel simply repeated -- MMPX 2x on
 * a race composed at 4x is MMPX once, then 2x2 pixels. */
typedef struct { unsigned char *px; int w, h; } og_px4;

static og_px4 og_upscale(const unsigned char *src, int w, int h,
                         const Uint32 luma[16], int passes, int k)
{
    og_px4 r;
    Uint32 *a, *b;
    int cw = w, ch = h, i, x, y, f;
    memset(&r, 0, sizeof r);
    if (passes > 0 && (1 << passes) > k) passes = 0;
    a = (Uint32 *)malloc((size_t)w * h * k * k * sizeof *a);
    b = (Uint32 *)malloc((size_t)w * h * k * k * sizeof *b);
    r.px = (unsigned char *)malloc((size_t)w * h * k * k);
    if (!a || !b || !r.px) { free(a); free(b); free(r.px); r.px = NULL; return r; }
    for (i = 0; i < w * h; ++i) a[i] = src[i];
    for (i = 0; i < passes && luma; ++i) {
        Uint32 *t;
        og_mmpx2x_px(a, cw, ch, b, luma);
        cw *= 2; ch *= 2;
        t = a; a = b; b = t;
    }
    f = k * w / cw;                        /* the repeat left over */
    r.w = k * w; r.h = k * h;
    for (y = 0; y < r.h; ++y)
        for (x = 0; x < r.w; ++x)
            r.px[y * r.w + x] = (unsigned char)a[(y / f) * cw + x / f];
    free(a); free(b);
    return r;
}

static og_px4 *og_up_egame, *og_up_truck, *og_up_shadow;
static int     og_up_ready = 0;
static int     og_up_mode  = -1;   /* passes * 8 + scale they were made for */
static int     og_up_shadow_n = 0;

/* index brightness for MMPX, from a palette; `key` is the clear index */
static void og_luma_from(Uint32 luma[16], const unsigned char *pal,
                         const unsigned char map[16], int key)
{
    int i;
    for (i = 0; i < 16; ++i) {
        const unsigned char *c = pal + (map ? map[i] : i) * 3;
        luma[i] = (Uint32)c[0] + c[1] + c[2] + 1;
        if (i == key) luma[i] *= 256;
    }
}

static SDL_Window *og_window;       /* defined with the F11 code below */

/* The shaders' generation: bumped whenever one changes, so whatever was
 * made under the old setting is made again. */
static int og_shade_gen = 1;

static int og_shade_wants(int kind)
{
    const int sp = (og_sprite_shader != OG_SPRITES_NONE);
    const int bg = (og_back_shader != OG_BACK_NONE);
    return kind == OG_LAYER_BACK ? bg : kind == OG_LAYER_SPRITE ? sp : (sp || bg);
}

static SDL_Texture *texture_from_shaded(SDL_Renderer *ren, const og_image *img,
                                        const unsigned char *pal, int key, int kind)
{
    Uint32 luma[16];
    unsigned char *t;
    og_px4 r;
    og_image big;
    SDL_Texture *tex;
    int i, n, passes;
    if (!img || !img->px) return NULL;
    if (!og_shade_wants(kind)) return texture_from(ren, img, pal, key, NULL);
    /* the layer's own shader's passes, or the larger of the two */
    passes = kind == OG_LAYER_BACK ? og_back_shader
           : kind == OG_LAYER_SPRITE ? og_sprite_shader
           : og_sprite_shader > og_back_shader ? og_sprite_shader : og_back_shader;
    n = img->w * img->h;
    t = (unsigned char *)malloc((size_t)n);
    if (!t) return texture_from(ren, img, pal, key, NULL);
    for (i = 0; i < n; ++i) t[i] = (unsigned char)(img->px[i] & 15);
    og_luma_from(luma, pal, NULL, key);
    r = og_upscale(t, img->w, img->h, luma, passes, 1 << passes);
    free(t);
    if (!r.px) return texture_from(ren, img, pal, key, NULL);
    big.w = r.w; big.h = r.h; big.px = r.px;
    tex = texture_from(ren, &big, pal, key, NULL);
    free(r.px);
    /* drawn at the 1x size in the 320x200 space, and rarely on whole
     * pixels of the window: smoothed, as the composed race is -- but not
     * on the framebuffer, whose window is the texture's own scale, where
     * smoothing only sends the software renderer down its slow path */
    if (tex) SDL_SetTextureScaleMode(tex, og_window ? SDL_SCALEMODE_LINEAR
                                                    : SDL_SCALEMODE_NEAREST);
    return tex;
}

typedef struct {
    SDL_Texture         *tex;
    og_image             img;
    const unsigned char *pal;
    int                  key, kind, gen;
} og_kept;
#define OG_KEPT_MAX 32
static og_kept og_kept_tab[OG_KEPT_MAX];

static SDL_Texture *texture_from_kept(SDL_Renderer *ren, const og_image *img,
                                      const unsigned char *pal, int key, int kind)
{
    SDL_Texture *tex = texture_from_shaded(ren, img, pal, key, kind);
    int i, slot = -1;
    if (!tex) return NULL;
    /* a slot whose texture has been destroyed is free again: SDL3 knows
     * its own live objects */
    for (i = 0; i < OG_KEPT_MAX; ++i) {
        og_kept *k = &og_kept_tab[i];
        if (k->tex && (k->tex == tex || !SDL_GetTextureSize(k->tex, NULL, NULL))) {
            og_image_free(&k->img);
            k->tex = NULL;
        }
        if (!k->tex && slot < 0) slot = i;
    }
    if (slot < 0) return tex;                 /* full: simply not kept */
    if (og_image_init(&og_kept_tab[slot].img, img->w, img->h) != OG_OK) return tex;
    memcpy(og_kept_tab[slot].img.px, img->px, (size_t)img->w * img->h);
    og_kept_tab[slot].tex  = tex;
    og_kept_tab[slot].pal  = pal;
    og_kept_tab[slot].key  = key;
    og_kept_tab[slot].kind = kind;
    og_kept_tab[slot].gen  = og_shade_gen;
    return tex;
}

/* make a kept texture again if the shaders have changed since */
static void og_reshade(SDL_Renderer *ren, SDL_Texture **t)
{
    int i;
    if (!t || !*t) return;
    for (i = 0; i < OG_KEPT_MAX; ++i) {
        og_kept *k = &og_kept_tab[i];
        SDL_Texture *n;
        if (k->tex != *t) continue;
        if (k->gen == og_shade_gen) return;
        n = texture_from_shaded(ren, &k->img, k->pal, k->key, k->kind);
        if (!n) return;
        SDL_DestroyTexture(*t);
        *t = k->tex = n;
        k->gen = og_shade_gen;
        return;
    }
}

static og_px4 *og_up_bank(const og_bank *b, const Uint32 luma[16], int passes, int k)
{
    og_px4 *u = (og_px4 *)calloc((size_t)(b->nframes > 0 ? b->nframes : 1), sizeof *u);
    int i;
    if (!u) return NULL;
    for (i = 0; i < b->nframes; ++i) {
        const og_image *f = &b->frames[i];
        unsigned char *t;
        int j;
        if (!f->px || f->w <= 0 || f->h <= 0) continue;
        t = (unsigned char *)malloc((size_t)f->w * f->h);
        if (!t) continue;
        for (j = 0; j < f->w * f->h; ++j) t[j] = f->px[j] & 15;
        u[i] = og_upscale(t, f->w, f->h, luma, passes, k);
        free(t);
    }
    return u;
}

static void og_up_free_set(og_px4 **set, int n)
{
    int i;
    if (!*set) return;
    for (i = 0; i < n; ++i) free((*set)[i].px);
    free(*set);
    *set = NULL;
}

/* Upscale the race screen's three banks to the scale the race is
 * composed at, through the sprite shader's MMPX passes -- none when only
 * the background shader has the screen up, the pixels simply repeated.
 * Made once, and again only when the shader or the scale changes.  The
 * truck bank's brightness is judged in red's colours. */
static void og_up_build(const og_bank *eg, const og_bank *trucks, const og_shadows *sh)
{
    Uint32 luma[16];
    unsigned char tmap[16];
    const int passes = og_sprite_shader, k = og_up_k();
    int i;
    if (og_up_ready && og_up_mode == passes * 8 + k) return;
    if (og_up_ready) {
        og_up_free_set(&og_up_egame, eg->nframes);
        og_up_free_set(&og_up_truck, trucks->nframes);
        og_up_free_set(&og_up_shadow, og_up_shadow_n);
    }
    og_luma_from(luma, og_pal_race, NULL, eg->key);
    og_up_egame = og_up_bank(eg, luma, passes, k);
    for (i = 0; i < 16; ++i) tmap[i] = (unsigned char)og_truck_remap(i, 0);
    og_luma_from(luma, og_pal_race, tmap, trucks->key);
    og_up_truck = og_up_bank(trucks, luma, passes, k);
    og_up_shadow = (og_px4 *)calloc(OG_SHADOWS, sizeof *og_up_shadow);
    og_up_shadow_n = OG_SHADOWS;
    if (og_up_shadow && sh && sh->loaded) {
        /* a mask: 0 clear (bright, as a key), 1 in shadow (dark) */
        Uint32 ml[16];
        for (i = 0; i < 16; ++i) ml[i] = 256;
        ml[1] = 1;
        for (i = 0; i < OG_SHADOWS; ++i)
            if (sh->mask[i] && sh->rows[i] > 0)
                og_up_shadow[i] = og_upscale(sh->mask[i], 24, sh->rows[i], ml, passes, k);
    }
    og_up_ready = 1;
    og_up_mode  = passes * 8 + k;
}

/* The track artwork at the composed scale through the background
 * shader's MMPX passes, made once per track: kept with a hash of the
 * 320x200 it came from and the setting, so a new track (whose art may
 * land at the same address) or a new setting is noticed.  Every index
 * is opaque here. */
static unsigned char *og_art4;
static Uint32         og_art4_hash;

static const unsigned char *og_art4_for(const unsigned char *art)
{
    Uint32 h = 2166136261u, luma[16];
    static unsigned char t[OG_SCREEN_W * OG_SCREEN_H];
    const int k = og_up_k();
    og_px4 r;
    int i;
    for (i = 0; i < OG_SCREEN_W * OG_SCREEN_H; ++i) {
        t[i] = (unsigned char)(art[i] & 15);
        h = (h ^ t[i]) * 16777619u;                   /* FNV-1a */
    }
    h = (h ^ (Uint32)(og_back_shader * 8 + k)) * 16777619u;
    if (og_art4 && h == og_art4_hash) return og_art4;
    free(og_art4);
    og_luma_from(luma, og_pal_race, NULL, -1);
    r = og_upscale(t, OG_SCREEN_W, OG_SCREEN_H, luma, og_back_shader, k);
    og_art4 = r.px;
    og_art4_hash = h;
    return og_art4;
}

/* og_screen_compose at 2x or 4x (k): the same display list, the same
 * rules, with every position times k and the upscaled frames. */
static unsigned char og_screen4[OG_SCREEN_W * OG_UP_MAX * OG_SCREEN_H * OG_UP_MAX];

static void og_screen4_blit(const og_px4 *u, int px, int py, int key,
                            const unsigned char *fg, int colour, int mode, int k,
                            int depth)
{
    const unsigned char *ov = og_tp_bg && depth >= 0 ? og_tp_bg->over : NULL;
    const int W = OG_SCREEN_W * k, H = OG_SCREEN_H * k;
    int x, y;
    if (!u || !u->px) return;
    for (y = 0; y < u->h; ++y) {
        int sy = py + y;
        if (sy < 0 || sy >= H) continue;
        for (x = 0; x < u->w; ++x) {
            int sx = px + x, v = u->px[y * u->w + x], p;
            if (sx < 0 || sx >= W) continue;
            p = sy * W + sx;
            if (fg && fg[(sy / k) * OG_SCREEN_W + sx / k]) continue;
            if (ov && ov[(sy / k) * OG_SCREEN_W + sx / k] > depth) continue;
            if (mode == 1) {                         /* a shadow: bit 0 */
                if (v) og_screen4[p] |= 1;
                continue;
            }
            if (v == key) continue;
            if (mode == 2) {                         /* a truck */
                og_screen4[p] = (unsigned char)og_truck_remap(v, colour);
            } else
                og_screen4[p] = (unsigned char)v;
        }
    }
}

static void og_screen_compose4(SDL_Texture *tex, const unsigned char *art,
                               const unsigned char *fg, const og_bank *eg,
                               const og_bank *trucks, const og_shadows *sh)
{
    static Uint32 rgba[OG_SCREEN_W * OG_UP_MAX * OG_SCREEN_H * OG_UP_MAX];
    const int k = og_up_k(), W = OG_SCREEN_W * k, H = OG_SCREEN_H * k;
    const unsigned char *a4 = NULL;
    int i, x, y;
    og_up_build(eg, trucks, sh);
    if (og_back_shader != OG_BACK_NONE && !og_tp_bg) a4 = og_art4_for(art);
    if (og_tp_bg)                                    /* the arcade's art */
        memset(og_screen4, OG_SCREEN_BG, (size_t)W * H);
    else if (a4)                                     /* the art, MMPX */
        memcpy(og_screen4, a4, (size_t)W * H);
    else                                             /* the art, kxk */
        for (y = 0; y < H; ++y)
            for (x = 0; x < W; ++x)
                og_screen4[y * W + x] =
                    (unsigned char)(art[(y / k) * OG_SCREEN_W + x / k] & 15);
    qsort(og_draws, (size_t)og_ndraws, sizeof og_draws[0], og_draw_cmp);
    for (i = 0; i < og_ndraws; ++i) {
        const og_draw_item *d = &og_draws[i];
        if (d->kind == OG_DI_SPRITE) {
            int e = d->sprite - OG_EGAME_FIRST;
            if (e < 0 || e >= eg->nframes || !og_up_egame) continue;
            og_screen4_blit(&og_up_egame[e],
                            (d->x + eg->frame_ox[e]) * k,
                            (d->y + OG_TRUCK_BOX_ROWS / 2 - eg->frame_oy[e]) * k,
                            eg->key, og_tp_bg && !d->high ? fg : NULL, 0, 0, k, d->depth);
            continue;
        }
        if (d->sh >= 0 && d->sh < OG_SHADOWS && sh && sh->loaded && og_up_shadow)
            og_screen4_blit(&og_up_shadow[d->sh], d->sh_x * k, d->sh_y * k,
                            -1, og_tp_bg ? fg : NULL, 0, 1, k, d->depth);
        if (d->sprite >= 0 && d->sprite < trucks->nframes && og_up_truck)
            og_screen4_blit(&og_up_truck[d->sprite], d->x * k, d->y * k,
                            trucks->key, d->high && !og_tp_bg ? NULL : fg, d->colour, 2, k,
                            d->depth);
    }
    for (i = 0; i < W * H; ++i)
        rgba[i] = og_screen_rgb(og_screen4[i],
                                ((i / W) / k) * OG_SCREEN_W + (i % W) / k);
    SDL_UpdateTexture(tex, NULL, rgba, W * 4);
}

/* Project a route waypoint to the screen with the game's own formula
 * (og_project), using that cell's integrated height.  Waypoints are
 * cell indices; the AI builds world coordinates as byte << 8. */
static void og_wp_screen(const og_route *r, int i, const unsigned char *H,
                         int ox, int oy, float *x, float *y)
{
    int cx = r->wp[i].cx, cy = r->wp[i].cy, sx, sy;
    int h = (H && cx < OG_CONTOUR_W && cy < OG_CONTOUR_H)
            ? H[cy * OG_CONTOUR_W + cx] : 128;
    og_project((long)cx << 8, (long)cy << 8, h, ox, oy, &sx, &sy);
    *x = (float)sx; *y = (float)sy;
}


/* Is this screen pixel somewhere a truck can drive?
 *
 * Judged at the pixel rather than from the course map, which keeps only
 * the largest connected stretch of road and so cuts off dirt beside a
 * dip.  Two cases the palette alone gets wrong:
 *
 *   - Under the foreground plane the artwork shows scenery drawn OVER
 *     the truck -- a bridge, the lip of a dip, the frame of a window in
 *     a wall -- not the road beneath, so it says nothing about
 *     drivability.  Treated as drivable.
 *
 *   - Puddles are drawn in navy (index 2), the same colour as the
 *     shadowed faces of retaining walls.  water[] marks navy regions
 *     that touch no barrier colour; trucks drive straight through them.
 */
static int og_drivable(const og_track_art *a, const unsigned char *water,
                       int x, int y)
{
    int v;
    if (!a || x < 0 || x >= OG_SCREEN_W || y < 0 || y >= OG_SCREEN_H) return 0;
    if (a->has_fg && a->fg[y * OG_SCREEN_W + x]) return 1;
    if (water && water[y * OG_SCREEN_W + x]) return 1;
    v = a->pic.px[y * OG_SCREEN_W + x] & 15;
    return og_race_class[v] != OG_CLS_SOLID;
}

/* Mark puddles: connected navy (2/3) regions that never touch a barrier
 * face or stripe (4,5,10,11,12,13).  Wall shadows always do. */
static unsigned char *og_find_water(const og_track_art *a)
{
    const int W = OG_SCREEN_W, N = OG_SCREEN_W * OG_SCREEN_H;
    unsigned char *water = (unsigned char *)calloc((size_t)N, 1);
    unsigned char *seen  = (unsigned char *)calloc((size_t)N, 1);
    int *stack = (int *)malloc(sizeof(int) * (size_t)N);
    int *comp  = (int *)malloc(sizeof(int) * (size_t)N);
    int i;

    if (!water || !seen || !stack || !comp) {
        free(water); free(seen); free(stack); free(comp); return NULL;
    }
    for (i = 0; i < N; ++i) {
        int v = a->pic.px[i] & 15, sp = 0, nc = 0, touches = 0, k;
        if (seen[i] || (v != 2 && v != 3)) continue;
        seen[i] = 1; stack[sp++] = i;
        while (sp) {
            int p = stack[--sp], px = p % W, py = p / W, d;
            comp[nc++] = p;
            for (d = 0; d < 4; ++d) {
                int nx = px + (d == 0) - (d == 1), ny = py + (d == 2) - (d == 3), q, u;
                if (nx < 0 || nx >= W || ny < 0 || ny >= OG_SCREEN_H) continue;
                q = ny * W + nx; u = a->pic.px[q] & 15;
                if (u == 4 || u == 5 || u >= 10) touches = 1;
                if ((u == 2 || u == 3) && !seen[q]) { seen[q] = 1; stack[sp++] = q; }
            }
        }
        if (!touches) for (k = 0; k < nc; ++k) water[comp[k]] = 1;
    }
    free(seen); free(stack); free(comp);
    return water;
}


/* Start (or restart) a race.  The route program and the gates come from
 * the LAYOUT -- track | reverse<<3, as GAME.EXE derives [0x19f] -- while
 * the artwork, height field and screen origin stay with the physical
 * track, [0x1a0] = layout & 7.  A reversed race is not the forward route
 * read backwards: it has its own program and its own gates. */
/* Put the four trucks on the grid: each takes its own walk of the route
 * program, staggered along the first segment, and its own colour. */
/* Prize money, added to whatever the truck picked up during the race. */
/* four BCD digits, as the game keeps money and times */
static unsigned og_bcd4(long v)
{
    if (v < 0) v = 0;
    if (v > 9999) v = 9999;
    return (unsigned)(((v / 1000) << 12) | (((v / 100) % 10) << 8) |
                      (((v / 10) % 10) << 4) | (v % 10));
}

/* op 82 (0x2f3a): was this truck beaten by the MACHINE?  It walks the
 * standings ([bx + 0x9d2e]) from the place above this truck's up to the
 * top, and answers yes if any truck there is a computer's ([di+2] bit
 * value 2).  A winner, or a human beaten only by other humans, is not. */
static int og_beaten_by_machine(const og_racer *all, const og_racer *r)
{
    int i;
    for (i = 0; i < r->place - 1 && i < OG_RACERS; ++i)
        if (all[og_standing[i]].computer) return 1;
    return 0;
}

static void og_award_prize(og_racer *all, og_racer *r, int trk)
{
    int won;
    (void)trk;
    if (!r || r->place < 1 || r->place > OG_PRIZE_PLACES) return;
    /* 0x371a: a COMPUTER truck ([di+2] bit value 2) is paid nothing -- no
     * prize, no pickups, no total; 0x371f jumps straight to the ret at
     * 0x37fd.  Paying them let attract mode, every truck a computer's,
     * pile up millions race after race. */
    if (r->computer) return;
    /* The winner's best time on the track is kept by og_racer_club, with
     * the rest of 0x371a's figures. */
    /* 0x3727: the place's prize PLUS what the truck picked up during the
     * race, banked into its cash and its running total alike. */
    won = og_prize[r->place - 1] + r->race_cash;
    r->cash  += won;
    r->total += won;
    /* Both are four BCD digits of $10,000 there (0x3734..0x374e), and the
     * carry out of the last is dropped: past 9999 the money would wrap
     * round to almost nothing.  SORP stops at 9999 instead, $99,990,000,
     * the most the shop, the club and IRONHI.BIN can show. */
    if (r->cash  > OG_CASH_MAX) r->cash  = OG_CASH_MAX;
    if (r->total > OG_CASH_MAX) r->total = OG_CASH_MAX;
    /* race_cash is NOT cleared here: the plaque is drawn after the trucks
     * finish and shows it as the second figure.  racers_reset clears it
     * at the start of each race, which is where [di+0x62] is zeroed. */

    /* The shop panel's script (red's at DS:0x458a) settles the credit:
     *   op 82 (0x45b8)  beaten by a computer truck?  If not -- a win, or
     *                   only humans ahead -- straight to the upgrade
     *                   boxes, nothing paid
     *   op 83 (0x4604)  no credits: op 85, the game is over
     *   op 84 (0x460a)  a credit spent (dec [bx+1]) BEFORE the prompt,
     *                   then "Press Button / to continue" counts down:
     *                   a press goes on to the boxes, running out is op
     *                   85 -- the invite if credits are left, Game Over
     *                   if not */
    if (!r->computer && og_beaten_by_machine(all, r)) {
        if (r->credits > 0) {
            --r->credits;                              /* op 84 */
            r->continue_t = OG_SHOP_CONTINUE_N;
            OG_INFO("[OG_Game] Finished %d behind the machine -- a credit spent, %d left; "
                    "press to continue", r->place, r->credits);
        } else {
            r->game_over = 1;
            OG_INFO("[OG_Game] Finished %d behind the machine, no credits -- game over",
                    r->place);
        }
    } else if (!r->computer && r->place > 1)
        OG_INFO("[OG_Game] Finished %d behind humans only -- no credit spent", r->place);
}

/* THE CONTROLS.  Left and right rotate the truck, up is the throttle and
 * the button fires nitro, as the original maps a stick.
 *
 * A CONTROLLER is not tied to a truck.  There are six:
 *
 *     keyboard 0   A and D steer, W is the throttle, SPACE fires nitro
 *     keyboard 1   the arrow keys, with right ctrl for nitro
 *     keyboard 2   the numeric keypad: 4 and 6 steer, 8 is the throttle,
 *                  2 is down and 0 fires nitro
 *     pad 0..3     up to four gamepads or joysticks, found at launch
 *
 * and each is bound to a truck when it JOINS: the first press of its
 * button takes the first truck no player has -- red, then yellow, then
 * blue -- and keeps it until that player's game is over, when the
 * controller is free again.  So whoever presses first is red, whatever
 * they press with.  og_player[] is what each TRUCK's controller is doing
 * (nothing, for a truck no one has).
 *
 * A RACE reads only gas, the throttle, and nitro; every other screen, and
 * the overlays, read the directions and fire.  On the keyboards and a
 * plain joystick gas is up and nitro is fire.  A gamepad's are F1's
 * "gamepad controls" (og_pad_scheme):
 *
 *     default   up is the throttle, A or B fires nitro, as above
 *     B accel   B is the throttle, A fires nitro, and up does nothing
 *     A accel   A is the throttle, B fires nitro, and up does nothing
 *
 * fire stays A or B either way, so either button joins, chooses and
 * presses on.  Which button IS A, og_pad_ab says. */
#define OG_PLAYERS 3
#define OG_PADS    4               /* gamepads looked for at launch       */
enum { OG_DEV_KEYS0, OG_DEV_KEYS1, OG_DEV_KEYS2, OG_DEV_PAD0,
       OG_DEVICES = OG_DEV_PAD0 + OG_PADS };

typedef struct { int left, right, up, down, fire, gas, nitro; } og_input;

enum { OG_PAD_UP_GAS, OG_PAD_B_GAS, OG_PAD_A_GAS, OG_PAD_SCHEMES };
static const char *const og_pad_scheme_nm[] = { "default", "B accel", "A accel" };
static int og_pad_scheme = OG_PAD_UP_GAS;

static SDL_Gamepad  *og_pad  [OG_PADS];
static SDL_Joystick *og_stick[OG_PADS];
static og_input      og_dev[OG_DEVICES];      /* every controller, raw   */
static int           og_dev_truck[OG_DEVICES] = { -1, -1, -1, -1, -1, -1, -1 };
static int           og_truck_dev[OG_PLAYERS] = { -1, -1, -1 };
static og_input      og_player[OG_PLAYERS];   /* by truck                */

static const char *og_dev_name(int d)
{
    static char b[64];
    if (d == OG_DEV_KEYS0) return "keyboard 0 (WASD, space)";
    if (d == OG_DEV_KEYS1) return "keyboard 1 (arrows, right ctrl)";
    if (d == OG_DEV_KEYS2) return "keyboard 2 (keypad 4 6 8 2, 0)";
    d -= OG_DEV_PAD0;
    SDL_snprintf(b, sizeof b, "pad %d (%s)", d,
                 og_pad[d] ? SDL_GetGamepadName(og_pad[d])
                 : og_stick[d] ? SDL_GetJoystickName(og_stick[d]) : "none");
    return b;
}

/* A truck drives itself until its player presses the button -- the same
 * way the Speed Shop's panels say "Press  Button" until someone joins.
 * Once in, a player stays in for the rest of the season. */
static int og_joined[OG_PLAYERS];

/* ------------------------------------------------------------------ */
/* Attract mode                                                        */
/* ------------------------------------------------------------------ */

/* With nobody playing, the game shows itself off and loops until
 * someone presses a button:
 *
 *     the title screen, with its own four views
 *     a demo race of four laps -- Sidewinder first, then usually Wipeout
 *     the podium for that race
 *     the Ironman Club, the high score table          5 seconds
 *     the Speed Shop demo (0x2bf8 with [0x1a1] set, script 0x48ec):
 *         each panel shows ITS truck's shop -- red, yellow, blue, as
 *         the demo race left them -- with the cursor wandering at
 *         random                                       480 frames
 *         then the invite, "Player N / Press Button"   240 frames
 *
 * and then round again.  No input is needed to advance any of it. */
typedef enum {
    OG_ATTRACT_TITLE = 0,
    OG_ATTRACT_RACE,
    OG_ATTRACT_PODIUM,
    OG_ATTRACT_CLUB,
    OG_ATTRACT_SHOP_DEMO,
    OG_ATTRACT_SHOP_REAL
} og_attract_stage;

#define OG_ATTRACT_HOLD   350      /* 5 seconds, as 70 Hz frames        */
#define OG_ATTRACT_TITLE_HOLD (180 + 360 + 360 + 540)   /* one full pass */
/* the attract shop's two parts, the panels' "wait 0x1e0" and "wait 0xf0" */
#define OG_ATTRACT_SHOP_BOXES  480
#define OG_ATTRACT_SHOP_INVITE 240

/* The demo panels' cursors (op 76, 0x2e40): [si+0xd] counts down and
 * every 16th frame (& 0xf == 0) the cursor steps +1 or -1 at random
 * (bit 0 of 0x42f:0x644), wrapping over the six boxes, with effect 0xb.
 * The three counters start at 5, 10 and 15 (the panels' 0c = 0500,
 * 0a00, 0f00), so the cursors move at different moments. */
static int og_demo_ctr[3];
static int og_shop_demo = 0;       /* the panels are the demo's        */

/* Attract mode races the championship's own schedule, DS:0x1a3, one
 * entry a cycle: 0x1e4 sets the race number [0x19e] to 0 as the attract
 * loop starts (0xe3) and the loop's 0x1d5 moves it on after the shop
 * (0x102), so the demo walks 03 06 0b 0e 09 ... -- Sidewinder, Wipeout,
 * Sidewinder reversed, Wipeout reversed ... -- and not just
 * the first two.  A game starting (0x107) and a game ending (0xd9) both
 * put it back to 0. */

static int og_attract = 1;         /* nobody has joined yet             */
static og_attract_stage og_stage = OG_ATTRACT_TITLE;
static float og_stage_t = 0.0f;    /* SECONDS spent in this stage       */
static int og_join_pending = 0;    /* bringing the shop up for a joiner */

/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */

/* A joining player gives three initials, a month, a day and a year of
 * birth, then a nation -- the prompts and lists the game keeps together
 * at DS:0x8752 upward.  The nation is the country byte [di+0x0e], which
 * picks the flag on the podium plaque and the anthem that plays.
 *
 * Each step is driven by that player's own controls: left and right move
 * through the choices, the button accepts and goes on to the next. */
typedef enum {
    OG_REG_NONE = 0, OG_REG_INITIALS, OG_REG_MONTH, OG_REG_DAY,
    OG_REG_YEAR, OG_REG_NATION, OG_REG_DONE
} og_reg_step;

static const char *og_nation[OG_NATIONS] = {
    "USA", "ITALY", "GERMANY", "BRITAIN", "FRANCE", "JAPAN", "CANADA",
    "AUSTRALIA", "-",
    "BELGIUM", "FINLAND", "HOLLAND", "MEXICO", "NORWAY", "SPAIN",
    "SWEDEN", "SWISS"
};
/* Registration's nation list: cells 0-7 are GAME.EXE's page, 8-15 the
 * second page's, nations 9-16 -- offered only while an arcade set is
 * open (og_nations_more, with the arcade sets below). */
static int og_nations_more(void);
static int og_nation_of_cell(int cell) { return cell < 8 ? cell : cell + 1; }
static int og_cell_of_nation(int n)
{
    return n >= 0 && n < OG_COUNTRY_NONE ? n
         : n >= OG_NATION_MORE && n < OG_NATIONS && og_nations_more() ? n - 1 : 0;
}

typedef struct og_reg_s {
    og_reg_step step;
    char  initials[4];
    int   letter;            /* which of the three is being set */
    int   cell;              /* where the cursor sits in the grid */
    int   month, day, year, nation;
    int   ytab;              /* year table: 0 is 30..99, 1 is 00..69   */
    int   skipped;           /* SKIP ended it: the nation was not set   */
    int   held;              /* the button must be released first      */
    int   frame;             /* [si+0xf]: counts down a frame at a time */
    float acc;               /* time not yet run as 70 Hz frames       */
} og_reg;

static og_reg og_regs[OG_PLAYERS];

/* op 86 (0x35c9), at the end of registration -- SKIP included, which
 * jumps to the same place: find the player in the club by the ten bytes
 * the truck holds, initials, month [+8], day [+0xa], year [+0xc] and
 * country.  A match hands the truck that record's best times; a new
 * name starts at 99.99 on every track. */
static void og_club_register(const og_hiscore *h, og_racer *R, const og_reg *r)
{
    og_club_player *c = &R->club;
    char t[8];
    int k;
    for (k = 0; k < 3; ++k)
        c->initials[k] = r->initials[k] ? r->initials[k] : 'X';
    SDL_snprintf(t, sizeof t, "%02d", r->month % 100); memcpy(c->mm, t, 2);
    SDL_snprintf(t, sizeof t, "%02d", r->day % 100);   memcpy(c->dd, t, 2);
    SDL_snprintf(t, sizeof t, "%02d", r->year % 100);  memcpy(c->yy, t, 2);
    c->country = (unsigned char)R->country;
    og_hiscore_lookup(h, c);
    R->club_ok = 1;
    OG_INFO("[OG_Club] %.3s %s", c->initials,
            c->rec ? "found -- their best times come back" : "is a new name");
}

/* op 85 (0x2f76), when a player's game ends and the truck goes back to
 * the machine: into the club's spare slot with the money [+0x11] and
 * the country as they stand now, then the table is sorted. */
static int og_club_changed = 0;     /* rebuild the club screen */
static void og_club_leave(og_hiscore *h, og_racer *R)
{
    og_club_player c;
    if (!R->club_ok) return;
    c = R->club;
    c.country   = (unsigned char)R->country;
    c.money_bcd = og_bcd4(R->total);
    og_hiscore_enter(h, &c);
    R->club_ok = 0;
    og_club_changed = 1;
    OG_INFO("[OG_Club] %.3s entered with $%d0,000", c.initials, R->total);
}

/* 0x381c: a truck's race time [di+0x46], in ticks, when the race ends.
 * One that crossed the line has its time.  One still running has its
 * time PROJECTED from the lap under way: the lap's fraction done, as
 * 0x37fe turns the progress [di+0x42] into 1/65536ths, gives the lap
 * time as elapsed / fraction (dx:ax = elapsed:0x7d1, div), added to
 * where the lap began and capped at 0x7d0, 100 seconds.  A fraction no
 * bigger than the elapsed ticks also gives 0x7d0; no fraction at all
 * leaves the count as it stands. */
static long og_race_ticks(const og_racer *R, int lap_span)
{
    long el, frac, t;
    if (R->fin_ticks) return R->fin_ticks;
    el   = R->time_ticks - R->lap_mark;
    frac = lap_span > 0 ? (long)R->ai.progress * 65536L / lap_span : 0;
    if (frac > 0xffff) frac = 0xffff;
    if (frac <= 0) return R->time_ticks;
    t = (frac > el) ? R->lap_mark + (el * 65536L + 0x7d1) / frac : 0x7d0;
    return t < 0x7d0 ? t : 0x7d0;
}

/* 0x385e: the ticks as [di+0x44] keeps them, BCD hundredths */
static unsigned og_race_bcd(long ticks)
{
    return ticks < 0x7d0 ? og_club_bcd((unsigned)ticks * 5u) : 0x9999;
}

/* 0x371a's club figures, once a race for a HUMAN truck (0x371d skips
 * the computers): see og_club_race.  `own` is 0 when the race is not on
 * the player's club's tracks (a Track-Pak race in "All 16 Tracks"): its
 * records go into that table, and the player's own bests are left as
 * they are. */
static void og_racer_club(og_hiscore *h, og_racer *R, int trk, int lap_span, int own)
{
    og_club_player scratch, *c = &R->club;
    long t;
    if (R->computer || !R->club_ok || R->club_raced) return;
    if (!own) { scratch = R->club; c = &scratch; }
    t = og_race_ticks(R, lap_span);
    og_club_race(h, c, trk, R->place == 1, og_race_bcd(t),
                 (unsigned)t);
    R->club_raced = 1;
}

/* ------------------------------------------------------------------ */
/* The Speed Shop                                                      */
/* ------------------------------------------------------------------ */

/* A panel serves three purposes: it invites a player who has not joined,
 * it runs registration for one who just has, and it is the upgrade shop
 * for one already racing.
 *
 * The shop's six buttons form a LOOP.  Steering left walks it one way and
 * right the other, and the nitro button presses whatever is selected:
 *
 *     Nitro -> Acceleration -> Top Speed -> Start Next Race
 *           -> Shocks -> Tires -> back to Nitro
 *
 * The order and the prices are the game's own (DS:0x954a, 0x9550,
 * 0x9556) and the sprites print them: NITRO 10K, ACCEL 80K, TOP SPD
 * 100K, SHOCKS 60K, TIRES 40K.  og_shop_price[] holds them in units of
 * $10,000, so the old table here -- a flat $30,000 for every upgrade --
 * was invented and is gone.
 */
typedef enum {
    OG_BUY_NITRO = 0, OG_BUY_ACCEL, OG_BUY_TOPSPD,
    OG_BUY_START, OG_BUY_SHOCKS, OG_BUY_TIRES, OG_BUY_COUNT
} og_shop_button;

static char og_reg_line[OG_PLAYERS][40];  /* a prompt per player's panel */

/* what each panel is showing right now */
static int og_panel_state[3];
/* registered during THIS shop visit: "Get Ready!" until the race starts */
static int og_get_ready[OG_PLAYERS];
/* out with no credits left during THIS shop visit: "Game Over" until
 * the race starts, and the truck is not to be joined meanwhile */
static int og_shop_over[OG_PLAYERS];
/* A player waiting to continue shows the INVITE panel again -- "Player N
 * / Press Button" -- with "to continue" and the count added beneath, as
 * the reference capture shows.  Their upgrade boxes are not drawn while
 * the count runs. */
static const int *pstate(const og_racer *R)
{
    int p;
    for (p = 0; p < 3; ++p)
        og_panel_state[p] = og_shop_demo ? 2                  /* demo shop */
                          : og_shop_over[p] ? 4               /* Game Over */
                          : (R && R[p].continue_t > 0) ? 0    /* continue */
                          : !og_joined[p] ? 0                 /* invite   */
                          : (og_regs[p].step != OG_REG_NONE &&
                             og_regs[p].step != OG_REG_DONE) ? 1  /* register */
                          : og_get_ready[p] ? 3               /* Get Ready! */
                          : 2;                                /* the shop */
    return og_panel_state;
}

/* The registration screens are PAGES, drawn by the same engine as the
 * title: DS:0x842f is the letter grid -- five rows of five, then
 * "Z _ &  SKIP" with & as the back arrow, and "INITIALS:" beneath --
 * and the birthdate's month list follows at DS:0x8480.  Both are set in
 * ROADFONT at fixed pitch, which the prologue right before the grid
 * (17 'ROAD', 11, 0e 02, 0f 08) establishes. */
/* Every registration grid is a page of its own, all in ROADFONT at fixed
 * pitch.  Two prologues set the metrics: the initials grid is spaced 2
 * with leading 8, the date and nation grids spaced -2 with leading 0.
 *
 *   initials  'A B C D E' .. 'Z _ &'  x+=4 spacing=-2 ' SKIP'  'INITIALS:'
 *   months    'JAN  FEB  MAR' .. 'OCT  NOV  DEC' ' &   SKIP'  'BIRTHDATE :'
 *   days      '01 02 03 04 05' .. '31 &  SKIP'                'BIRTHDATE :'
 *   years     TWO tables of fourteen rows, each ending 'BIRTHDATE :':
 *               DS:8566  '30 31 32 33 34' .. '95 96 97 98 99'  1930..1999
 *               DS:8648  '00 01 02 03 04' .. '65 66 67 68 69'  2000..2069
 *               Running off one lands on the other's far end, so the two
 *               behave as a single list of 140 years.
 *   nations   'USA' 'ITALY' 'GERMANY' 'BRITAIN' 'FRANCE' 'JAPAN'
 *             'CANADA' 'AUSTRALIA', each on its own line
 */
#define OG_REG_PAGE_PROLOGUE  0x8424   /* ROAD, fixed, spacing 2, leading 8 */
#define OG_REG_PAGE_PRO_DATE  0x847d   /* ROAD, fixed, spacing -2, leading 0 */
/* The grids are SUB-PAGES 1, 3, 5, 6, 7 and 8 of the table at 0x83de,
 * exactly as the panel's wrapper pages (0x9232, 0x92a1, 0x9310, 0x93b4,
 * 0x93c7, 0x950c) call them: the month list opens with a line break and
 * the day list with its own leading of 8, and the nation list with an
 * indent for USA -- starting a byte or two late lost all three. */
#define OG_REG_PAGE_INITIALS  0x842f   /* sub-page 1 */
#define OG_REG_PAGE_MONTHS    0x8488   /* sub-page 3 */
#define OG_REG_PAGE_DAYS      0x84ef   /* sub-page 5 */
#define OG_REG_PAGE_YEARS     0x8566   /* sub-page 6: 30..99 */
#define OG_REG_PAGE_YEARS2    0x8648   /* sub-page 7: 00..69 */
#define OG_REG_YEARS_PER      70       /* cells in each table */
#define OG_REG_PAGE_NATIONS   0x872a   /* sub-page 8 */
#define OG_REG_PAGE_BIRTH     0x8480

/* Read out of a live memory dump taken with player 2 registering and the
 * cursor on A:
 *
 *     DS:0c8c  sprite 587 (index 0)  x   8, y 31   player 1's panel
 *     DS:0d06  sprite 595 (index 8)  x 112, y 31   player 2's, registering
 *     DS:0d80  sprite 587 (index 0)  x 216, y 31   player 3's panel
 *     DS:105c  sprite 595 (index 8)  x 115, y 48   the cursor on A
 *
 * so a panel's origin is (8 + 104*p, 31) and the cursor sits +3, +17
 * inside it.  The step between cells comes from the page's own metrics:
 * ROADFONT at fixed pitch is 8 wide with letter spacing 2, so "A B C D E"
 * spaces letters 20 apart, and its 9-row height plus the leading
 * adjustment of 8 puts rows 17 apart -- the same 17 the cursor is
 * offset by. */
#define OG_PANEL_X0       8
#define OG_PANEL_STEP     104
#define OG_PANEL_Y        31
#define OG_CURSOR_DX      3
#define OG_CURSOR_DY      17
#define OG_CURSOR_COL_W   20
#define OG_CURSOR_ROW_H   17
#define OG_GRID_COLS      5

/* The cursor's shape is [si+0x5c] of the input child, an index into
 * ESHOP's first eight (32x9 for 0-3, 16x9 for 4-5):
 *
 *   initials  5 on a letter, 2 on SKIP                 0x2700
 *   month     3 on a month or the back arrow, 2 SKIP   0x2853
 *   day       4 on a day or the back arrow,   2 SKIP   0x2973
 *   year      4                                        init 5c=4
 *   nation    0, in TWO pieces -- 64 wide              init 08=2
 */
#define OG_CURSOR_FLASH   8        /* add for the other frame      */

/* Where each grid's page pen starts inside the panel: the 0x04 of its
 * wrapper page, less the panel origin (8 + 104p, 31). */
static const signed char og_reg_page_at[7][2] = {
    { 0, 0 },          /* none */
    { 4, 17 },         /* initials, 0x9232 */
    { 8, 17 },         /* month,    0x92a1 */
    { 4, 17 },         /* day,      0x9310 */
    { 4, 17 },         /* year,     0x93b4 / 0x93c7 */
    { 2, 6 },          /* nation,   0x950c */
    { 0, 0 }
};

/* The flags beside the nation list: eight "fl" children (0x3898 ..
 * 0x3978) at panel x 63, y 1 + 21n, sprites 607, 604, 608, 606, 603,
 * 605, 609, 610 -- ESHOP 20, 17, 21, 19, 16, 18, 22, 23. */
static const unsigned char og_reg_flag[8] = { 20, 17, 21, 19, 16, 18, 22, 23 };
#define OG_REG_FLAG_X     63
#define OG_REG_FLAG_Y     1
#define OG_REG_NATION_DY  21      /* DS:0x9536: 6, 27, 48 ... 153 */
#define OG_REG_ENTRY_DY   155     /* the bottom line, y 186 on screen */

/* A registration step's cursor, inside the panel: the arithmetic of the
 * input handlers op 70..74 (0x26b2, 0x2805, 0x2925, 0x2a45, 0x2b4d). */
static void og_reg_cursor(const struct og_reg_s *r, int *x, int *y,
                          int *shape, int *pieces);
static int og_shop_sel[OG_PLAYERS];    /* the button each player is on */
static int og_shop_ready[OG_PLAYERS];  /* has chosen "Start Next Race"  */
static int og_shop_held[OG_PLAYERS];   /* debounce                     */

/* One step of a player's registration.  Returns 1 while it still has the
 * screen.  Each step is the original's input child -- one handler per
 * grid, op 70..74 -- and they share a protocol: [si+0x4c] = 4 when a value
 * is chosen, 8 for SKIP and 0xc for the back arrow.  The panel's script
 * (DS:0x3e28 ..) answers SKIP by jumping straight to the record lookup and
 * "Get Ready!" (0x3fa4) from ANY step, and the back arrow by returning to
 * the step before. */
/* One frame's cursor move: the handlers' direction step, 0x26db .. */
static void og_reg_move(og_reg *r, int move)
{
    switch (r->step) {
    case OG_REG_INITIALS: r->cell = (r->cell + move + 29) % 29; break;
    case OG_REG_MONTH:    r->cell = (r->cell + move + 14) % 14; break;
    case OG_REG_DAY:      r->cell = (r->cell + move + 33) % 33; break;
    case OG_REG_YEAR: {
        /* 0x2a45: 70 cells a table, no back and no SKIP.  Stepping off
         * an end flips tables when the YEAR stays within 0..99 (0x2ac6):
         * left of 30 on the 30..99 table lands on 69 at the far end of
         * the 00..69 one (0x3f3c), right of 69 on that table lands on 30
         * (0x3ed6); the outer ends, 00 and 99, simply stop. */
        int c = r->cell + move;
        if (c >= 0 && c < OG_REG_YEARS_PER) r->cell = c;
        else if (r->ytab == 0 && c < 0)                 { r->ytab = 1; r->cell = 69; }
        else if (r->ytab == 1 && c >= OG_REG_YEARS_PER) { r->ytab = 0; r->cell = 0;  }
        break;
    }
    case OG_REG_NATION: {
        /* 0x2b4d: eight, wrapping; SORP's second page makes it sixteen,
         * the cursor running off one page onto the other */
        const int n = og_nations_more() ? 16 : 8;
        if (r->cell >= n) r->cell = 0;
        r->cell = (r->cell + move + n) % n;
        break;
    }
    default: break;
    }
}

/* *sfx gets 1 for a move (effect 0xb) and 2 for the button (0xc) */
static int og_reg_tick(int p, float dt, int *sfx)
{
    og_reg *r = &og_regs[p];
    const og_input *in = &og_player[p];
    int move = 0, fire, n;

    if (r->step == OG_REG_NONE || r->step == OG_REG_DONE) return 0;
    /* left and right walk the grids, right first (0x26e0 tests 1, then
     * 2); the nation list also takes up and down -- 0x2b71 tests 5,
     * right or down, before 0xa */
    if (r->step == OG_REG_NATION)
        move = (in->right || in->down) ? 1 : (in->left || in->up) ? -1 : 0;
    else
        move = in->right ? 1 : in->left ? -1 : 0;

    /* A HELD direction keeps moving.  Each handler runs once a frame,
     * counts [si+0xf] down, and reads the stick as it stands -- no wait
     * for a release -- on every 2nd frame for the initials, month and day
     * (test 1), every frame for the year, and every 4th for the nation
     * (test 3), playing effect 0xb (0x2bc0) as it moves.
     *
     * SORP DOUBLES those periods (every 4th, 2nd and 8th frame): the
     * authentic rates are too quick to stop on the letter you want. */
    r->acc += dt;
    n = (int)(r->acc * OG_FRAME_HZ);
    r->acc -= (float)n / OG_FRAME_HZ;
    while (n-- > 0) {
        int mask = (r->step == OG_REG_YEAR) ? 1 : (r->step == OG_REG_NATION) ? 7 : 3;
        r->frame = (r->frame - 1) & 0xff;
        if (move && !(r->frame & mask)) {
            og_reg_move(r, move);
            *sfx |= 1;
        }
    }

    /* The button is taken once a press, bit 0x40, with effect 0xc
     * (0x2bd1). */
    if (r->held) {
        if (!in->fire) r->held = 0;
        return 1;
    }
    fire = in->fire;
    if (!fire) return 1;
    r->held = 1;
    *sfx |= 2;

    switch (r->step) {
    case OG_REG_INITIALS:
        /* 0x26b2: 29 cells, wrapping -- A..Z, _, the back arrow (27)
         * and SKIP (28); three letters move on */
        {
            if (r->cell == 28) { r->skipped = 1; r->step = OG_REG_DONE; break; }
            if (r->cell == 27) {                  /* the back arrow */
                if (r->letter > 0) r->initials[--r->letter] = ' ';
            } else {
                char ch = (r->cell < 26) ? (char)('A' + r->cell) : '_';
                if (r->letter < 3) r->initials[r->letter++] = ch;
            }
            if (r->letter >= 3) { r->cell = 10; r->step = OG_REG_MONTH; }
        }
        break;
    case OG_REG_MONTH:
        /* 0x2805: 14 cells -- twelve months, back (12), SKIP (13); the
         * child starts on cell 10 (init 0c=0x0a) */
        {
            if (r->cell == 13) { r->skipped = 1; r->step = OG_REG_DONE; break; }
            if (r->cell == 12) {                  /* back to the initials */
                r->letter = 0; r->cell = 0; r->step = OG_REG_INITIALS;
                break;
            }
            r->month = r->cell + 1;
            r->cell = 26; r->step = OG_REG_DAY;   /* init 0c=0x1a */
        }
        break;
    case OG_REG_DAY:
        /* 0x2925: 33 cells -- 31 days, back (31), SKIP (32) */
        {
            if (r->cell == 32) { r->skipped = 1; r->step = OG_REG_DONE; break; }
            if (r->cell == 31) { r->cell = r->month - 1; r->step = OG_REG_MONTH; break; }
            r->day = r->cell + 1;
            /* the panel sets 0c=0x26 before the year child (0x3ecc):
             * table 30..99, on 68 */
            r->ytab = 0; r->cell = 38; r->step = OG_REG_YEAR;
        }
        break;
    case OG_REG_YEAR:
        /* 0x2a45: 70 cells a table, no back and no SKIP */
        {
            r->year = (r->ytab == 0) ? 30 + r->cell : r->cell;
            r->cell = og_cell_of_nation(r->nation); r->step = OG_REG_NATION;
        }
        break;
    case OG_REG_NATION:
        /* 0x2b4d: eight, wrapping (sixteen with the second page) */
        if (r->cell >= (og_nations_more() ? 16 : 8)) r->cell = 0;
        r->nation = og_nation_of_cell(r->cell); r->step = OG_REG_DONE;
        break;
    default: break;
    }
    /* The button is still DOWN as the panel changes, so arm the shop's
     * debounce: otherwise this same press reaches whatever comes next. */
    if (r->step == OG_REG_DONE) og_shop_held[p] = 1;
    return r->step != OG_REG_DONE;
}

static void og_reg_cursor(const og_reg *r, int *x, int *y,
                          int *shape, int *pieces)
{
    int c = r->cell;
    *pieces = 1;
    switch (r->step) {
    case OG_REG_INITIALS:                         /* 0x2700..0x272a */
        *shape = (c == 28) ? 2 : 5;
        *x = (c % 5) * 20 + 3;  *y = (c / 5) * 17 + 17;  break;
    case OG_REG_MONTH:                            /* 0x2853..0x287d */
        *shape = (c == 13) ? 2 : 3;
        *x = (c % 3) * 30 + 7;  *y = (c / 3) * 18 + 26;  break;
    case OG_REG_DAY:                              /* 0x2973..0x299d */
        *shape = (c == 32) ? 2 : 4;
        *x = (c % 5) * 18 + 3;  *y = (c / 5) * 17 + 17;  break;
    case OG_REG_YEAR:                             /* 0x2a83..0x2a9f */
        *shape = 4;
        *x = (c % 5) * 18 + 3;  *y = (c / 5) * 9 + 17;   break;
    case OG_REG_NATION:                           /* 0x2b93..0x2ba3 */
        *shape = 0;  *pieces = 2;
        *x = 2;  *y = 6 + (c % 8) * OG_REG_NATION_DY;    break;
    default:
        *shape = 5;  *x = *y = 0;                        break;
    }
}

/* Draw one sprite of a bank straight to the screen in the shop palette,
 * dropped by the piece's own top offset as the shop figure is. */
static void og_blit_sprite(SDL_Renderer *ren, const og_bank *b, int idx,
                           int x, int y)
{
    const og_image *f;
    og_image im;
    if (idx < 0 || idx >= b->nframes) return;
    f = &b->frames[idx];
    if (!f->px) return;
    if (b->frame_oy) y += OG_TRUCK_BOX_ROWS / 2 - b->frame_oy[idx];
    if (og_image_init(&im, f->w, f->h) == OG_OK) {
        SDL_Texture *t;
        memset(im.px, 0, (size_t)f->w * f->h);
        og_image_blit(&im, f, 0, 0, b->key);
        t = texture_from_shaded(ren, &im, og_pal_shop, b->key, OG_LAYER_SPRITE);
        if (t) {
            SDL_FRect dst = { (float)x, (float)y, (float)f->w, (float)f->h };
            SDL_RenderTexture(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
        }
        og_image_free(&im);
    }
}

static void og_reg_start(int p)
{
    og_reg *r = &og_regs[p];
    char keep[4];
    /* The initials live in the truck ([di+5..7]) and the entry overwrites
     * them a letter at a time (0x2758), so SKIP keeps whatever is left:
     * "XXX" from 0x12d8 at start-up, or an earlier player's. */
    memcpy(keep, r->initials, sizeof keep);
    if (!keep[0]) memcpy(keep, "XXX", 4);
    memset(r, 0, sizeof *r);
    memcpy(r->initials, keep, sizeof keep);
    r->cell = 0;                          /* init 0c=0: on A */
    r->month = 11; r->day = 27; r->year = 68; r->nation = 0;
    r->step = OG_REG_INITIALS;
    r->held = 1;                          /* the joining press must clear */
}

/* The original polls FOUR joysticks -- descriptors at DS:0x5378, 0x538c,
 * 0x676 and 0x688, the last two calibrated at launch by 0x42f:0x7e8 after
 * the "ARE YOU USING A JOYSTICK (Y\N)" question.  Its control bits are
 * 1 right, 2 left, 4 down, 8 up, 0x10 fire (0x42f:0x83c).
 *
 * SORP looks for four too, at launch: gamepads first, then any joystick
 * SDL does not know as a gamepad, in the order SDL lists them. */
static void og_pad_log(int i);

/* The buttons PRINTED A and B.  SDL's mapping puts a button where it sits
 * -- SOUTH the bottom one, EAST the right -- and SDL_GetGamepadButtonLabel
 * gives the letters from the pad's TYPE.  For a pad SDL knows (Xbox,
 * PlayStation, Switch) those are the pad's own.  A handheld's built-in pad
 * is type "standard", which SDL letters as an Xbox pad's, A at the bottom;
 * but the handhelds print theirs as Nintendo does, A on the right.  On the
 * Anbernic RG DS (Knulli, PortMaster's mapping), pressing the buttons
 * printed A, B, X, Y gave EAST, SOUTH, NORTH, WEST, which SDL labelled B,
 * A, Y, X, in a small SDL3 test program.  So a "standard" pad is taken
 * to be Nintendo's: A is EAST and B SOUTH. */
static void og_pad_ab(SDL_Gamepad *g, SDL_GamepadButton *a, SDL_GamepadButton *b)
{
    int east_a = SDL_GetGamepadType(g) == SDL_GAMEPAD_TYPE_STANDARD ||
                 SDL_GetGamepadButtonLabel(g, SDL_GAMEPAD_BUTTON_EAST) ==
                     SDL_GAMEPAD_BUTTON_LABEL_A;
    *a = east_a ? SDL_GAMEPAD_BUTTON_EAST  : SDL_GAMEPAD_BUTTON_SOUTH;
    *b = east_a ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
}

static void og_input_open(void)
{
    SDL_JoystickID *pads = NULL, *sticks = NULL;
    int npad = 0, nstick = 0, i, n = 0;

    pads   = SDL_GetGamepads(&npad);
    sticks = SDL_GetJoysticks(&nstick);
    for (i = 0; pads && i < npad && n < OG_PADS; ++i)
        if ((og_pad[n] = SDL_OpenGamepad(pads[i])) != NULL) ++n;
    for (i = 0; sticks && i < nstick && n < OG_PADS; ++i)
        if (!SDL_IsGamepad(sticks[i]) &&
            (og_stick[n] = SDL_OpenJoystick(sticks[i])) != NULL) ++n;
    SDL_free(pads); SDL_free(sticks);
    for (i = 0; i < OG_DEVICES; ++i)
        if (i < OG_DEV_PAD0 || og_pad[i - OG_DEV_PAD0] || og_stick[i - OG_DEV_PAD0])
            OG_INFO("[OG_Input] Controller: %s", og_dev_name(i));
    for (i = 0; i < OG_PADS; ++i) og_pad_log(i);
}

/* what SDL takes pad slot i to be: the GUID a mapping must name, and the
 * mapping it is using -- for SDL_GAMECONTROLLERCONFIG */
static void og_pad_log(int i)
{
    SDL_Joystick *j = og_pad[i] ? SDL_GetGamepadJoystick(og_pad[i]) : og_stick[i];
    char guid[64];
    char *map;
    if (!j) return;
    SDL_GUIDToString(SDL_GetJoystickGUID(j), guid, (int)sizeof guid);
    map = og_pad[i] ? SDL_GetGamepadMapping(og_pad[i]) : NULL;
    OG_INFO("[OG_Input] Pad %d: GUID %s, mapping %s", i, guid, map ? map : "(none)");
    SDL_free(map);
    if (og_pad[i]) {
        SDL_GamepadButton a, b;
        og_pad_ab(og_pad[i], &a, &b);
        OG_INFO("[OG_Input] Pad %d: type %s, so A is the %s button and B the %s", i,
                SDL_GetGamepadStringForType(SDL_GetGamepadType(og_pad[i])),
                a == SDL_GAMEPAD_BUTTON_EAST ? "right" : "bottom",
                b == SDL_GAMEPAD_BUTTON_EAST ? "right" : "bottom");
    }
}

/* A controller plugged in or pulled out while SORP runs.  A new one takes
 * the first free slot of the four; one pulled out frees its slot, and a
 * truck it drove keeps the slot, so plugging a pad back in gives the
 * truck back to it.  SDL also announces the ones there at launch, which
 * og_input_open has already opened. */
static void og_input_hotplug(const SDL_Event *ev)
{
    int i;
    switch (ev->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_JOYSTICK_ADDED: {
        const SDL_JoystickID id = ev->type == SDL_EVENT_GAMEPAD_ADDED ?
                                  ev->gdevice.which : ev->jdevice.which;
        const int pad = SDL_IsGamepad(id);
        if (ev->type == SDL_EVENT_JOYSTICK_ADDED && pad) return;  /* the gamepad event's */
        for (i = 0; i < OG_PADS; ++i) {
            SDL_Joystick *j = og_pad[i] ? SDL_GetGamepadJoystick(og_pad[i]) : og_stick[i];
            if (j && SDL_GetJoystickID(j) == id) return;           /* ours already */
        }
        for (i = 0; i < OG_PADS && (og_pad[i] || og_stick[i]); ++i) {}
        if (i == OG_PADS) {
            OG_WARN("[OG_Input] %s plugged in, but all four slots are taken",
                    pad ? SDL_GetGamepadNameForID(id) : SDL_GetJoystickNameForID(id));
            return;
        }
        if (pad) og_pad[i] = SDL_OpenGamepad(id);
        else     og_stick[i] = SDL_OpenJoystick(id);
        if (!og_pad[i] && !og_stick[i]) {
            OG_ERR("[OG_Input] A controller plugged in could not be opened: %s",
                   SDL_GetError());
            return;
        }
        OG_INFO("[OG_Input] Plugged in: %s", og_dev_name(OG_DEV_PAD0 + i));
        og_pad_log(i);
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
    case SDL_EVENT_JOYSTICK_REMOVED: {
        const SDL_JoystickID id = ev->type == SDL_EVENT_GAMEPAD_REMOVED ?
                                  ev->gdevice.which : ev->jdevice.which;
        for (i = 0; i < OG_PADS; ++i) {
            SDL_Joystick *j = og_pad[i] ? SDL_GetGamepadJoystick(og_pad[i]) : og_stick[i];
            if (!j || SDL_GetJoystickID(j) != id) continue;
            OG_WARN("[OG_Input] Pulled out: %s", og_dev_name(OG_DEV_PAD0 + i));
            if (og_pad[i])   { SDL_CloseGamepad(og_pad[i]);   og_pad[i]   = NULL; }
            if (og_stick[i]) { SDL_CloseJoystick(og_stick[i]); og_stick[i] = NULL; }
        }
        break;
    }
    default:
        break;
    }
}

static void og_input_close(void)
{
    int p;
    for (p = 0; p < OG_PADS; ++p) {
        if (og_pad[p])   { SDL_CloseGamepad(og_pad[p]);   og_pad[p]   = NULL; }
        if (og_stick[p]) { SDL_CloseJoystick(og_stick[p]); og_stick[p] = NULL; }
    }
}

/* Bind controller `d` to the first truck no player has; returns the
 * truck, or -1 when every truck is a player's. */
static int og_dev_join(int d)
{
    int p;
    for (p = 0; p < OG_PLAYERS; ++p)
        if (!og_joined[p] && og_truck_dev[p] < 0 && !og_shop_over[p]) {
            og_truck_dev[p] = d;
            og_dev_truck[d] = p;
            og_player[p] = og_dev[d];        /* the press that joined */
            OG_INFO("[OG_Input] %s takes truck %d", og_dev_name(d), p);
            return p;
        }
    return -1;
}

/* the settings the overlay drives, file-scope so it can reach them.
 * The three track overlays, each on or off in F1:
 *   waypoints  the route program's nodes -- green on drivable ground,
 *              red inside a wall, a blue box on the first and yellow
 *              on the current target
 *   gates      the checkpoints that count progress round the lap, and
 *              each crossing logged with the truck's place
 *   wheels     the four wheels, with a cross on the ground under the
 *              truck joined to it in flight, and takeoffs, landings and
 *              attitude changes logged */
static int ovl_waypoints = 0;
static int ovl_gates     = 0;
static int ovl_wheels    = 0;
static int mesh_mode = 0;          /* contour: off/height/slope/surface */

static int og_options   = 0;       /* which overlay: 0 none, 1 F1, 2 F2,
                                    * 3 F3, 4 F4 */
static int og_quit_ask  = 0;       /* a prompt is showing: 1 "Exit SORP?",
                                    * 2 F10's "Back to the title?" -- the
                                    * same keys answer both */
static int og_quit_held = 0;       /* ... and the press that asked is
                                    * still held, so it is not an answer */
static int og_about      = 0;      /* F1's "About" is showing           */
#define OG_ABOUT_URL "https://ko-fi.com/ddopaz"
static int og_about_held = 0;      /* ... and the press that opened it  */
static SDL_Window *og_window = NULL;  /* the window full screen applies to;
                                       * none on the framebuffer */
static int og_fullscreen = 0;      /* the window is full screen         */
static float og_fps = 0.0f;        /* frames a second, for F1           */
/* How the frame reaches the screen, for F1: "GPU gles fb" (the GPU
 * presenter on the framebuffer), "CPU fb copy", or SDL's own renderer --
 * "GPU direct3d11", "GPU opengl", "CPU software" and so on. */
static char og_render_path[32] = "";
static int  og_render_gpu = 0;      /* ... and whether the GPU draws    */
static int og_console   = 0;       /* the debug console is open         */
static void og_console_set(int on);
static int og_opt_held  = 0;       /* debounce for the players' buttons */

/* the option table sits further down, after the settings it points at */
static void og_opt_move(int dir);
static void og_opt_nudge(int dir);
static void og_opt_fire(void);

/* F3, THE SPRITE VIEWER.  Up and down choose the bank; the steering
 * turns the truck in the attitude view -- held, it keeps turning, as it
 * turns a truck -- and pages through the other banks; the button
 * repaints the truck red, yellow, blue or white.  After the banks come
 * the two fonts, the pictures, each track's picture and mask, the
 * arcade's tracks: each one's picture, mask and shadow, and the arcade's
 * sprites, every one in its video ROM. */
enum { OG_SPR_ATTITUDES, OG_SPR_ETRUCK, OG_SPR_EGAME, OG_SPR_ESHADOW,
       OG_SPR_ESHOP, OG_SPR_EEND, OG_SPR_ETITLE,
       OG_SPR_FONTS, OG_SPR_PICTURES, OG_SPR_TRACKS, OG_SPR_ARCADE, OG_SPR_ASPRITES,
       OG_SPR_VIEWS };
#define OG_SPR_PAGES    64
#define OG_SPR_CAPTION  10         /* px above and below a grid page   */
#define OG_SPR_TURN_MS  300        /* a held turn: the first step ...  */
#define OG_SPR_REPEAT_MS 60        /* ... and every one after it       */
static int    og_spr_view  = OG_SPR_ATTITUDES;
static int    og_spr_dir   = 0;    /* the attitude view's direction, of 32 */
static int    og_spr_page[OG_SPR_VIEWS];
static int    og_spr_start[OG_SPR_VIEWS][OG_SPR_PAGES];  /* first sprite */
static int    og_spr_more  = 0;    /* the page showing has another after */
static int    og_spr_dirty = 1;
static Uint64 og_spr_rep   = 0;    /* when a held turn steps again       */
static int    og_spr_colour = OG_TRUCK_COLOUR_RED;   /* ETRUCK's paint  */
static int    og_spr_apal   = 0;   /* the arcade sprites' palette: a track's */

static void og_spr_input(const og_input *in)
{
    int turn = in->right ? 1 : in->left ? -1 : 0;
    if (!in->up && !in->down && !in->left && !in->right && !in->fire) {
        og_opt_held = 0;
        return;
    }
    if (og_spr_view == OG_SPR_ATTITUDES && turn && !in->up && !in->down) {
        /* left steers anticlockwise, as R->head -= rate does */
        Uint64 now = SDL_GetTicks();
        if (!og_opt_held || now >= og_spr_rep) {
            og_spr_rep = now + (og_opt_held ? OG_SPR_REPEAT_MS : OG_SPR_TURN_MS);
            og_spr_dir = (og_spr_dir + turn) & 31;
            og_spr_dirty = 1;
        }
        og_opt_held = 1;
        return;
    }
    if (og_opt_held) return;
    og_opt_held = 1;
    if (in->up || in->down)
        og_spr_view = (og_spr_view + (in->down ? 1 : OG_SPR_VIEWS - 1)) % OG_SPR_VIEWS;
    else if (in->fire && og_spr_view == OG_SPR_ASPRITES)
        og_spr_apal = (og_spr_apal + 1) % TP_TRACKS;   /* whose palette */
    else if (in->fire)               /* the four trucks' colours, 0..3 */
        og_spr_colour = (og_spr_colour + 1) & 3;
    else if (turn > 0 && og_spr_more && og_spr_page[og_spr_view] + 1 < OG_SPR_PAGES)
        ++og_spr_page[og_spr_view];
    else if (turn < 0 && og_spr_page[og_spr_view] > 0)
        --og_spr_page[og_spr_view];
    og_spr_dirty = 1;
}

static void og_input_read(void)
{
    const bool *k = SDL_GetKeyboardState(NULL);
    const int dead = 8000;
    int p, d;

    /* A player whose game is over has let the truck go: the controller
     * is free to join again. */
    for (p = 0; p < OG_PLAYERS; ++p)
        if (!og_joined[p] && og_truck_dev[p] >= 0) {
            og_dev_truck[og_truck_dev[p]] = -1;
            og_truck_dev[p] = -1;
        }

    for (d = 0; d < OG_DEVICES; ++d) {
        og_input in;
        memset(&in, 0, sizeof in);
        p = d - OG_DEV_PAD0;                  /* the pad, for a pad */

        /* DOWN is not used in a race -- a truck has no reverse -- but
         * every controller has the direction and the options overlay
         * needs it, so it is read like any other. */
        if (d == OG_DEV_KEYS0) {
            in.left  = k[SDL_SCANCODE_A]     ? 1 : 0;
            in.right = k[SDL_SCANCODE_D]     ? 1 : 0;
            in.up    = k[SDL_SCANCODE_W]     ? 1 : 0;
            in.down  = k[SDL_SCANCODE_S]     ? 1 : 0;
            in.fire  = k[SDL_SCANCODE_SPACE] ? 1 : 0;
        } else if (d == OG_DEV_KEYS1) {
            in.left  = k[SDL_SCANCODE_LEFT]  ? 1 : 0;
            in.right = k[SDL_SCANCODE_RIGHT] ? 1 : 0;
            in.up    = k[SDL_SCANCODE_UP]    ? 1 : 0;
            in.down  = k[SDL_SCANCODE_DOWN]  ? 1 : 0;
            in.fire  = k[SDL_SCANCODE_RCTRL] ? 1 : 0;
        } else if (d == OG_DEV_KEYS2) {       /* the same keys, NumLock or not */
            in.left  = k[SDL_SCANCODE_KP_4]  ? 1 : 0;
            in.right = k[SDL_SCANCODE_KP_6]  ? 1 : 0;
            in.up    = k[SDL_SCANCODE_KP_8]  ? 1 : 0;
            in.down  = k[SDL_SCANCODE_KP_2]  ? 1 : 0;
            in.fire  = k[SDL_SCANCODE_KP_0]  ? 1 : 0;
        } else if (og_pad[p]) {
            int ax = SDL_GetGamepadAxis(og_pad[p], SDL_GAMEPAD_AXIS_LEFTX);
            int ay = SDL_GetGamepadAxis(og_pad[p], SDL_GAMEPAD_AXIS_LEFTY);
            if (ax < -dead || SDL_GetGamepadButton(og_pad[p], SDL_GAMEPAD_BUTTON_DPAD_LEFT))  in.left = 1;
            if (ax >  dead || SDL_GetGamepadButton(og_pad[p], SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) in.right = 1;
            if (ay < -dead || SDL_GetGamepadButton(og_pad[p], SDL_GAMEPAD_BUTTON_DPAD_UP))    in.up = 1;
            if (ay >  dead || SDL_GetGamepadButton(og_pad[p], SDL_GAMEPAD_BUTTON_DPAD_DOWN))  in.down = 1;
            {
                SDL_GamepadButton ba, bb;
                int a, b;
                og_pad_ab(og_pad[p], &ba, &bb);
                a = SDL_GetGamepadButton(og_pad[p], ba) ? 1 : 0;
                b = SDL_GetGamepadButton(og_pad[p], bb) ? 1 : 0;
                in.fire = a || b;
                if (og_pad_scheme == OG_PAD_B_GAS)      { in.gas = b; in.nitro = a; }
                else if (og_pad_scheme == OG_PAD_A_GAS) { in.gas = a; in.nitro = b; }
                else                                    { in.gas = in.up; in.nitro = in.fire; }
            }
        } else if (og_stick[p]) {
            int ax = SDL_GetJoystickAxis(og_stick[p], 0);
            int ay = SDL_GetJoystickAxis(og_stick[p], 1);
            int b;
            if (ax < -dead) in.left = 1;
            if (ax >  dead) in.right = 1;
            if (ay < -dead) in.up = 1;
            if (ay >  dead) in.down = 1;
            for (b = 0; b < 4; ++b)
                if (SDL_GetJoystickButton(og_stick[p], b)) in.fire = 1;
        }
        if (d < OG_DEV_PAD0 || !og_pad[p]) { in.gas = in.up; in.nitro = in.fire; }
        og_dev[d] = in;
    }
    /* each truck sees its own controller, and a truck no one has sees
     * nothing */
    for (p = 0; p < OG_PLAYERS; ++p) {
        if (og_truck_dev[p] >= 0) og_player[p] = og_dev[og_truck_dev[p]];
        else memset(&og_player[p], 0, sizeof og_player[p]);
    }

    /* While the options overlay is up the players drive IT, and nothing
     * reaches the game behind: a truck must not wander off while its
     * driver is changing a setting.  The inputs are combined, so any
     * controller works. */
    /* While "Exit SORP?" is up the game gets no input either, and any
     * player's button puts it away. */
    /* A press already held when it came up -- the button that chose
     * F1's "Exit SORP", a throttle held through ESC -- is not an answer:
     * only one made after everything is let go puts it away. */
    if (og_quit_ask) {
        int held = 0;
        for (d = 0; d < OG_DEVICES; ++d)
            if (og_dev[d].fire || og_dev[d].left || og_dev[d].right ||
                og_dev[d].up   || og_dev[d].down)
                held = 1;
        if (!held)             og_quit_held = 0;
        else if (!og_quit_held) og_quit_ask = 0;
        memset(og_player, 0, sizeof og_player);
        memset(og_dev, 0, sizeof og_dev);
    }

    /* F1's "About" the same way: a new press puts it away, and the
     * overlay behind takes nothing until that press is let go */
    if (og_about) {
        int held = 0;                  /* ENTER is its link's, not a close */
        for (d = 0; d < OG_DEVICES; ++d)
            if (og_dev[d].fire || og_dev[d].left || og_dev[d].right ||
                og_dev[d].up   || og_dev[d].down)
                held = 1;
        if (!held)               og_about_held = 0;
        else if (!og_about_held) og_about = 0;
        og_opt_held = 1;
        memset(og_player, 0, sizeof og_player);
        memset(og_dev, 0, sizeof og_dev);
        return;
    }

    if (og_options) {
        og_input any;
        memset(&any, 0, sizeof any);
        /* every controller, joined or not */
        for (d = 0; d < OG_DEVICES; ++d) {
            if (og_dev[d].up)    any.up = 1;
            if (og_dev[d].down)  any.down = 1;
            if (og_dev[d].left)  any.left = 1;
            if (og_dev[d].right) any.right = 1;
            if (og_dev[d].fire)  any.fire = 1;
        }
        memset(og_player, 0, sizeof og_player);
        memset(og_dev, 0, sizeof og_dev);     /* nor may anyone join */
        if (k[SDL_SCANCODE_RETURN]) any.fire = 1;
        /* UP and DOWN walk the rows, LEFT and RIGHT change the setting,
         * and the button runs the row's action -- play, mute, stop -- or,
         * on a row without one, does what right does, so a single button
         * is enough.  Every controller and both keyboard sets work. */
        if (og_options == 3)
            og_spr_input(&any);
        else if (!any.up && !any.down && !any.left && !any.right && !any.fire)
            og_opt_held = 0;
        else if (!og_opt_held) {
            og_opt_held = 1;
            if (any.down)       og_opt_move(1);
            else if (any.up)    og_opt_move(-1);
            else if (any.left)  og_opt_nudge(-1);
            else if (any.right) og_opt_nudge(1);
            else                og_opt_fire();
        }
    }
}

/* Set a truck up for a race.  What a truck OWNS -- its nitro and its
 * winnings -- survives into the next race; only the racing state is
 * cleared.  Pass fresh = 1 for the first race of a season, which is when
 * the tanks are filled: 25 for a player, 10 for a computer truck
 * ([di+0x5d] is written with 0x19 at 0x13a5 and 0xa at 0x14c9).  The 25
 * is gated on [di+2] bit 0 -- a human has joined (0x138b) -- and never on
 * WHICH truck it is, so red driving itself in attract mode gets 10. */
static void racers_reset_ex(og_racer *r, const og_route *route, int fresh)
{
    int i;
    for (i = 0; i < OG_RACERS; ++i) {
        int keep_nitro = r[i].nitro, keep_cash = r[i].cash;
        int keep_cr = r[i].credits, keep_total = r[i].total;
        /* The UPGRADES are a truck's own, bought with its winnings, and
         * they last the whole season -- the memset below wiped them, so
         * every race started on stock parts however much had been spent
         * in the shop. */
        int keep_tire = r[i].up_tire,  keep_accel = r[i].up_accel;
        int keep_top  = r[i].up_top,   keep_shock = r[i].up_shock;
        /* The COUNTRY is the truck's for good: 0x12ae sets every truck's
         * to 8 -- no nation -- once, at start-up, and only the nation
         * picker (0x2bb3) ever writes it again.  Resetting it here to the
         * truck's index threw a registered player's nation away before
         * the podium could show its flag or play its anthem. */
        int keep_country = r[i].country;
        /* ...and so is its place in the club, set at registration */
        og_club_player keep_club = r[i].club;
        int keep_club_ok = r[i].club_ok;
        memset(&r[i], 0, sizeof r[i]);
        r[i].country = keep_country;
        r[i].club = keep_club;
        r[i].club_ok = keep_club_ok;
        if (!fresh) { r[i].nitro = keep_nitro; r[i].cash = keep_cash;
                      r[i].credits = keep_cr; r[i].total = keep_total;
                      r[i].up_tire = keep_tire; r[i].up_accel = keep_accel;
                      r[i].up_top  = keep_top;  r[i].up_shock = keep_shock; }
        else         r[i].credits = OG_SHOP_CREDITS_NEW;   /* two to start */
        r[i].race_cash = 0;            /* [di+0x62] is per race */
        og_ai_reset(&r[i].ai, (unsigned short)(0x1234 + i * 0x111));
        r[i].from   = og_ai_next(&r[i].ai, route);
        r[i].to     = og_ai_next(&r[i].ai, route);
        r[i].t      = (float)(OG_RACERS - 1 - i) * 0.2f;
        r[i].colour   = og_racer_colour[i];
        /* A truck is a player's when that player has a stick; truck 0
         * is always playable because the keyboard stands in for one. */
        /* computer-driven until that player has joined */
        r[i].computer = !(i < OG_PLAYERS && og_joined[i]);
        /* 0x146b: a COMPUTER truck is given 10 at every race start
         * (0x14c7); a human's carries over */
        if (r[i].computer)  r[i].nitro = OG_NITRO_CPU;
        else if (fresh)     r[i].nitro = OG_NITRO_HUMAN;
        r[i].nitro_t   = 0;
        r[i].nitro_lap = -1;
        r[i].rubber_at = -1;
        r[i].body_h    = 128.0f;
        r[i].ground_m  = 128.0f;
        r[i].time_ticks = r[i].lap_mark = 0;
        r[i].best_lap = r[i].last_lap = 0;
        r[i].fin_ticks = 0;
        r[i].club_raced = 0;
        /* The level is NOT set here: og_ai_seed_levels gives each truck
         * its own, walking DS:0x74e6 -- white, blue, yellow, red -- so
         * white is the strongest.  Setting a single OG_CPU_BASE for all
         * of them here flattened that spread again after every race. */
    }
    /* the places carry over with the standings (0x12f9 runs only with
     * race 0, from 0x1e4) */
    for (i = 0; i < OG_RACERS; ++i) r[og_standing[i]].place = i + 1;
}

static void racers_reset(og_racer *r, const og_route *route)
{
    racers_reset_ex(r, route, 0);      /* a truck keeps what it owns */
}

static int race_start(const char *dir, int track, int reverse,
                      og_route *route, og_checkpoints *cps, og_ai *ai,
                      int *from, int *to, float *t)
{
    int layout = OG_LAYOUT(track, reverse);

    /* the shop's "Game Over" panels last until the next race */
    memset(og_shop_over, 0, sizeof og_shop_over);
    if (og_load_route(dir, layout, route) != OG_OK) {
        OG_ERR("[OG_Race] Layout %d: no route", layout);
        return 0;
    }
    if (og_load_gates(dir, layout, cps) != OG_OK)
        cps->n = 0;
    og_ai_reset(ai, 0x1234);
    *from = og_ai_next(ai, route);
    *to   = og_ai_next(ai, route);
    *t    = 0.0f;
    OG_DEBUG("[OG_Race] Track %d %s (layout %d): %d waypoints, %d gates",
            track, reverse ? "REVERSED" : "forward", layout,
            route->n, cps->n);
    return 1;
}


/* Ground height at a point in cell units.
 *
 * The integrator stores each height BEFORE adding that cell's gradient,
 * so H[y][x] is the height at grid VERTEX (x, y) -- the cell's corner,
 * not its centre.  Within a cell the game's surface is planar per
 * triangle, so it is linear along every cell edge; bilinear between the
 * four surrounding vertices matches it exactly on edges and closely
 * inside. */
static float og_ground(const unsigned char *H, float cx, float cy)
{
    int x0, y0, x1, y1;
    float fx, fy, a, b;
    if (!H) return 128.0f;
    if (cx < 0.0f) cx = 0.0f;
    if (cy < 0.0f) cy = 0.0f;
    if (cx > OG_CONTOUR_W - 1) cx = (float)(OG_CONTOUR_W - 1);
    if (cy > OG_CONTOUR_H - 1) cy = (float)(OG_CONTOUR_H - 1);
    x0 = (int)cx; y0 = (int)cy;
    x1 = x0 + 1 < OG_CONTOUR_W ? x0 + 1 : x0;
    y1 = y0 + 1 < OG_CONTOUR_H ? y0 + 1 : y0;
    fx = cx - x0; fy = cy - y0;
    a = H[y0 * OG_CONTOUR_W + x0] * (1 - fx) + H[y0 * OG_CONTOUR_W + x1] * fx;
    b = H[y1 * OG_CONTOUR_W + x0] * (1 - fx) + H[y1 * OG_CONTOUR_W + x1] * fx;
    return a * (1 - fy) + b * fy;
}

/* The ground under one point, the way the original computes it (0x1963):
 * take the CELL's own height and add its two gradients scaled by the
 * position within that cell --
 *
 *     al = the gradient;  cl >>= 1;  imul cl;  ax <<= 1;  ... sub cx
 *     ax = height + the x term + the y term
 *
 * -- so each cell is its own little plane and nothing is blended with
 * the neighbours.  og_ground() below interpolates between four cells,
 * which is smoother but rounds every ridge off over a cell's width and,
 * at the edge of a wall, tilts the two cells outside it upward into a
 * ramp a truck can launch from.  Use this for the wheels. */
static float og_ground_plane(const unsigned char *H, const og_surface *s,
                             const og_contour *c, float px, float py)
{
    int cx, cy;
    long po;
    float fx, fy, h;
    if (!H || !s || !s->loaded || !c || !c->loaded)
        return og_ground(H, px, py);
    if (px < 0.0f) px = 0.0f;
    if (py < 0.0f) py = 0.0f;
    if (px > OG_CONTOUR_W - 1) px = (float)(OG_CONTOUR_W - 1);
    if (py > OG_CONTOUR_H - 1) py = (float)(OG_CONTOUR_H - 1);
    cx = (int)px; cy = (int)py;
    fx = px - cx; fy = py - cy;
    h  = (float)H[cy * OG_CONTOUR_W + cx];
    po = og_surface_resolve_at(s, c->cell[cy * OG_CONTOUR_W + cx],
                               (int)(fx * 256.0f), (int)(fy * 256.0f));
    if (po >= 0) {
        /* 0x1963: al = the gradient; cl >>= 1; imul cl; ax <<= 1; then
         * sub cx, where cx is the gradient << 7 -- so the term is
         *
         *     gradient * (frac - 128)
         *
         * in the height's fixed point, which here is gradient * (f-0.5).
         * The stored height is the CELL CENTRE and the gradient tilts
         * the plane about it, so the planes meet at the cell edges
         * instead of stepping. */
        float gx = (float)(signed char)s->poly[po + 2];
        float gy = (float)(signed char)s->poly[po + 3];
        h += gx * (fx - 0.5f) + gy * (fy - 0.5f);
    }
    return h;
}


/* Project a ground point given in cell units. */
static SDL_FPoint og_cell_screen(float cx, float cy, const unsigned char *H,
                                 int ox, int oy)
{
    SDL_FPoint p;
    int sx, sy;
    og_project((long)(cx * 256.0f), (long)(cy * 256.0f),
               (int)(og_ground(H, cx, cy) + 0.5f), ox, oy, &sx, &sy);
    p.x = (float)sx; p.y = (float)sy;
    return p;
}


/* The whole contour grid, projected.  Every cell is drawn on its four
 * vertex heights, exactly like the gates, so it lines up with the art.
 *
 *   mode 1  HEIGHT   integrated elevation, coloured per vertex (blue low,
 *                    green middle, red high, scaled to this track) with a
 *                    faint wireframe of the grid
 *   mode 2  SLOPE    each cell's own POLY gradients (+2, +3): the raw
 *                    slopes the height is integrated from; flat cells are
 *                    clear, steep ones bright
 *   mode 3  SURFACE  POLY byte +0 -- 0x80 orange, 0x02 cyan -- and an X on
 *                    every cell that goes through QUAD.  The sampler splits
 *                    those cells along both diagonals (slot +4 if
 *                    frac_x + frac_y does not carry, +2 if frac_x >= frac_y), so
 *                    the X is the four-triangle split, one per QUAD slot. */
static void og_draw_contour(SDL_Renderer *ren, const og_contour *ct,
                            const og_surface *surf, const unsigned char *H,
                            int ox, int oy, int mode)
{
    static SDL_FPoint vp[(OG_CONTOUR_W + 1) * (OG_CONTOUR_H + 1)];
    static SDL_FColor vc[(OG_CONTOUR_W + 1) * (OG_CONTOUR_H + 1)];
    static SDL_Vertex v[OG_CONTOUR_W * OG_CONTOUR_H * 6];
    const int VW = OG_CONTOUR_W + 1;
    int x, y, nv = 0, hmin = 255, hmax = 0;

    if (!H || mode <= 0) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    for (y = 0; y < OG_CONTOUR_H * OG_CONTOUR_W; ++y) {
        if (H[y] < hmin) hmin = H[y];
        if (H[y] > hmax) hmax = H[y];
    }
    if (hmax <= hmin) hmax = hmin + 1;

    /* every grid vertex, projected once, with its height colour */
    for (y = 0; y <= OG_CONTOUR_H; ++y)
        for (x = 0; x <= OG_CONTOUR_W; ++x) {
            float t = (og_ground(H, (float)x, (float)y) - hmin) / (float)(hmax - hmin);
            SDL_FColor *k = &vc[y * VW + x];
            vp[y * VW + x] = og_cell_screen((float)x, (float)y, H, ox, oy);
            if (t < 0.5f) { k->r = 0.10f; k->g = 0.20f + 1.4f * t;        k->b = 0.95f - 1.6f * t; }
            else          { k->r = 0.20f + 1.6f * (t - 0.5f); k->g = 0.90f - 1.4f * (t - 0.5f); k->b = 0.15f; }
            k->a = 0.42f;
        }

    for (y = 0; y < OG_CONTOUR_H; ++y)
        for (x = 0; x < OG_CONTOUR_W; ++x) {
            int i00 = y * VW + x, i10 = i00 + 1, i11 = i00 + VW + 1, i01 = i00 + VW;
            SDL_FColor k00 = vc[i00], k10 = vc[i10], k11 = vc[i11], k01 = vc[i01];
            unsigned short cell = ct && ct->loaded ? ct->cell[y * OG_CONTOUR_W + x] : 0;

            if (mode == 2 || mode == 3) {
                SDL_FColor f;
                f.r = f.g = f.b = 0.0f; f.a = 0.0f;
                if (mode == 2) {
                    int gx = 0, gy = 0, m;
                    og_surface_gradient(surf, cell, &gx, &gy);
                    m = abs(gx) + abs(gy);                     /* 0..120 */
                    if (m > 0) {
                        float s = m / 90.0f; if (s > 1.0f) s = 1.0f;
                        f.r = 0.30f + 0.70f * s; f.g = 0.85f - 0.65f * s; f.b = 0.20f;
                        f.a = 0.12f + 0.50f * s;
                    }
                } else {
                    long off = og_surface_resolve(surf, cell);
                    int flag = (off >= 0) ? surf->poly[off] : 0;
                    if (flag & 0x80)     { f.r = 1.00f; f.g = 0.55f; f.b = 0.10f; f.a = 0.40f; }
                    else if (flag == 2)  { f.r = 0.20f; f.g = 0.85f; f.b = 1.00f; f.a = 0.40f; }
                    else                 { f.r = 1.00f; f.g = 1.00f; f.b = 1.00f; f.a = 0.06f; }
                }
                k00 = k10 = k11 = k01 = f;
            }
            v[nv+0].position = vp[i00]; v[nv+0].color = k00;
            v[nv+1].position = vp[i10]; v[nv+1].color = k10;
            v[nv+2].position = vp[i11]; v[nv+2].color = k11;
            v[nv+3].position = vp[i00]; v[nv+3].color = k00;
            v[nv+4].position = vp[i11]; v[nv+4].color = k11;
            v[nv+5].position = vp[i01]; v[nv+5].color = k01;
            {
                int k;
                for (k = 0; k < 6; ++k) v[nv + k].tex_coord.x = v[nv + k].tex_coord.y = 0.0f;
            }
            nv += 6;
        }
    SDL_RenderGeometry(ren, NULL, v, nv, NULL, 0);

    if (mode == 1) {                                 /* faint wireframe */
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 38);
        for (y = 0; y <= OG_CONTOUR_H; ++y) SDL_RenderLines(ren, &vp[y * VW], VW);
        for (x = 0; x <= OG_CONTOUR_W; ++x) {
            static SDL_FPoint col[OG_CONTOUR_H + 1];
            for (y = 0; y <= OG_CONTOUR_H; ++y) col[y] = vp[y * VW + x];
            SDL_RenderLines(ren, col, OG_CONTOUR_H + 1);
        }
    }
    if (mode == 3 && ct && ct->loaded) {             /* QUAD triangle split */
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 120);
        for (y = 0; y < OG_CONTOUR_H; ++y)
            for (x = 0; x < OG_CONTOUR_W; ++x) {
                int i00 = y * VW + x;
                if (!(ct->cell[y * OG_CONTOUR_W + x] & 1u)) continue;
                SDL_RenderLine(ren, vp[i00].x, vp[i00].y, vp[i00 + VW + 1].x, vp[i00 + VW + 1].y);
                SDL_RenderLine(ren, vp[i00 + 1].x, vp[i00 + 1].y, vp[i00 + VW].x, vp[i00 + VW].y);
            }
    }
}

/* Draw the checkpoint gates.
 *
 * A gate is a flat box in the 44x44 grid -- cells x0 <= x < x1,
 * y0 <= y <= y1 -- and the game only ever tests which cell the truck is
 * in.  It has no meaningful outline on screen: its edges run through
 * walls and scenery where the ground jumps 60-90 height units (= pixels)
 * within a cell or two, so any faithful trace of the boundary is a row
 * of spikes, and joining only the four corners folds it into slivers.
 *
 * What matters is where the gate covers the RACING SURFACE, since only
 * there can a truck trigger it.  So each of its cells that lies on
 * drivable ground is shaded, projected on its own four vertex heights.
 *
 *   yellow   the NEXT gate -- the only one that can be crossed now
 *   magenta  the finish line (flag 0x80); always a one-cell strip
 *   cyan     a straight gate; its arrow shows which way progress runs
 *   violet   a bend; the dot marks the corner progress is measured
 *            around (codes 4-11), so it swaps sides when reversed      */
static void og_draw_gates(SDL_Renderer *ren, const og_checkpoints *c,
                          const og_track_art *art, const unsigned char *water,
                          const unsigned char *H, int ox, int oy, int next)
{
    static const signed char pivx[12] = { 0,0,0,0, 0,0,1,1, 0,0,1,1 };
    static const signed char pivy[12] = { 0,0,0,0, 0,1,1,0, 1,0,0,1 };
    static const signed char dirx[4]  = {  1, 0, -1, 0 };
    static const signed char diry[4]  = {  0,-1,  0, 1 };
    static SDL_Vertex v[OG_CONTOUR_W * OG_CONTOUR_H * 6];
    float sx0, sy0;
    int i;

    if (!c || c->n <= 0) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_GetRenderScale(ren, &sx0, &sy0);

    for (i = 0; i < c->n; ++i) {
        const og_checkpoint *p = &c->cp[i];
        int code = p->flags & 0x0f;
        int x0 = p->x0, x1 = p->x1, y0 = p->y0, y1 = p->y1 + 1;
        int nv = 0, ncell = 0, x, y;
        float cxs = 0.0f, cys = 0.0f, mx, my;
        SDL_FColor col;
        SDL_FPoint m;
        char label[8];

        if (i == next)            { col.r = 1.00f; col.g = 0.90f; col.b = 0.00f; col.a = 0.45f; }
        else if (p->flags & 0x80) { col.r = 1.00f; col.g = 0.24f; col.b = 0.78f; col.a = 0.40f; }
        else if (code >= 4)       { col.r = 0.75f; col.g = 0.51f; col.b = 1.00f; col.a = 0.20f; }
        else                      { col.r = 0.43f; col.g = 0.82f; col.b = 1.00f; col.a = 0.20f; }

        for (y = y0; y < y1 && y < OG_CONTOUR_H; ++y)
            for (x = x0; x < x1 && x < OG_CONTOUR_W; ++x) {
                SDL_FPoint mid = og_cell_screen(x + 0.5f, y + 0.5f, H, ox, oy);
                SDL_FPoint a, b2, cc, d;
                int k;
                if (!og_drivable(art, water, (int)mid.x, (int)mid.y)) continue;
                a  = og_cell_screen((float)x,     (float)y,     H, ox, oy);
                b2 = og_cell_screen((float)x + 1, (float)y,     H, ox, oy);
                cc = og_cell_screen((float)x + 1, (float)y + 1, H, ox, oy);
                d  = og_cell_screen((float)x,     (float)y + 1, H, ox, oy);
                v[nv].position = a;  v[nv+1].position = b2; v[nv+2].position = cc;
                v[nv+3].position = a;  v[nv+4].position = cc; v[nv+5].position = d;
                for (k = 0; k < 6; ++k) {
                    v[nv + k].color = col;
                    v[nv + k].tex_coord.x = v[nv + k].tex_coord.y = 0.0f;
                }
                nv += 6;
                cxs += x + 0.5f; cys += y + 0.5f; ++ncell;
            }
        if (nv) SDL_RenderGeometry(ren, NULL, v, nv, NULL, 0);

        /* mark it at the middle of its on-track part, which may be far
         * from the middle of the box */
        if (ncell) { mx = cxs / ncell; my = cys / ncell; }
        else       { mx = (x0 + x1) * 0.5f; my = (y0 + y1) * 0.5f; }
        m = og_cell_screen(mx, my, H, ox, oy);

        SDL_SetRenderDrawColor(ren, (Uint8)(col.r * 255), (Uint8)(col.g * 255),
                               (Uint8)(col.b * 255), 255);
        if (code < 4) {                              /* progress arrow */
            float len = 0.35f * ((code & 1) ? (float)(y1 - y0) : (float)(x1 - x0));
            SDL_FPoint t;
            SDL_FRect tip;
            if (len > 3.0f) len = 3.0f;
            t = og_cell_screen(mx + dirx[code] * len, my + diry[code] * len, H, ox, oy);
            SDL_RenderLine(ren, m.x, m.y, t.x, t.y);
            tip.x = t.x - 1.5f; tip.y = t.y - 1.5f; tip.w = 3.0f; tip.h = 3.0f;
            SDL_RenderFillRect(ren, &tip);
        } else if (code <= 11) {                     /* bend pivot */
            SDL_FPoint pv = og_cell_screen((float)(pivx[code] ? x1 : x0),
                                           (float)(pivy[code] ? y1 : y0), H, ox, oy);
            SDL_FRect dot;
            dot.x = pv.x - 2.0f; dot.y = pv.y - 2.0f; dot.w = 4.0f; dot.h = 4.0f;
            SDL_RenderFillRect(ren, &dot);
        }

        SDL_snprintf(label, sizeof label, "%d", i);
        SDL_SetRenderScale(ren, sx0 * 0.5f, sy0 * 0.5f);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 235);
        SDL_RenderDebugText(ren, m.x * 2.0f - 4.0f, m.y * 2.0f - 4.0f, label);
        SDL_SetRenderScale(ren, sx0, sy0);
    }
}

/* Draw the route straight through the renderer rather than baking a
 * texture, so it re-lays out the instant the scale or origin changes.
 * Each waypoint is colour coded against the barrier map: green where it
 * lands on drivable ground, red where it lands inside a wall.  A
 * correct mapping shows no red at all. */
static void og_draw_route(SDL_Renderer *ren, const og_track_art *art,
                          const unsigned char *water,
                          const og_route *r, const unsigned char *H,
                          int ox, int oy, int current)
{
    int i;

    if (!r || r->n < 1) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    /* The route is a GRAPH.  Draw every edge: fall-through in white,
     * unconditional jumps in cyan, conditional branches in orange --
     * the orange ones are where a crossover chooses its half. */
    for (i = 0; i < r->n; ++i) {
        int k;
        for (k = 0; k < r->nsucc[i]; ++k) {
            float ax, ay, bx, by;
            switch (r->kind[i][k]) {
            case OG_EDGE_JUMP:   SDL_SetRenderDrawColor(ren,  80, 220, 255, 150); break;
            case OG_EDGE_BRANCH: SDL_SetRenderDrawColor(ren, 255, 160,  40, 170); break;
            default:             SDL_SetRenderDrawColor(ren, 255, 255, 255, 110); break;
            }
            og_wp_screen(r, i, H, ox, oy, &ax, &ay);
            og_wp_screen(r, r->succ[i][k], H, ox, oy, &bx, &by);
            SDL_RenderLine(ren, ax, ay, bx, by);
        }
    }

    for (i = 0; i < r->n; ++i) {
        float x, y;
        SDL_FRect box;
        og_wp_screen(r, i, H, ox, oy, &x, &y);
        int wall = !og_drivable(art, water, (int)x, (int)y);

        if (wall) SDL_SetRenderDrawColor(ren, 255,  40,  40, 255);
        else      SDL_SetRenderDrawColor(ren,  40, 255, 120, 255);

        box.x = x - 1.5f; box.y = y - 1.5f; box.w = 3.0f; box.h = 3.0f;
        SDL_RenderFillRect(ren, &box);

        if (i == current) {          /* the one being driven to */
            SDL_SetRenderDrawColor(ren, 255, 230, 0, 255);
            box.x = x - 3.5f; box.y = y - 3.5f; box.w = 7.0f; box.h = 7.0f;
            SDL_RenderRect(ren, &box);
        }
        if (i == 0) {                /* start of the list */
            SDL_SetRenderDrawColor(ren, 80, 160, 255, 255);
            box.x = x - 5.5f; box.y = y - 5.5f; box.w = 11.0f; box.h = 11.0f;
            SDL_RenderRect(ren, &box);
        }
    }
}


/* A page of a bank's sprites, laid out in rows, each at its own height
 * and dropped by its own TOP -- sprite table byte +5, the rows it starts
 * into its 32-row box (kept as frame_oy = 16 - top) -- so the pieces of
 * a sprite drawn in several parts line up as the game puts them
 * together.
 *
 * `group`, when given, is the number of pieces in the sprite starting at
 * each index (0 or 1 for a sprite of one).  A group's pieces are drawn
 * side by side with no gap, as the game draws them, and never split
 * across rows: one that will not fit on the row starts the next. */
static int bank_compose_grid(og_image *sheet, const og_bank *b,
                             const unsigned char *group,
                             int first, int *shown_out)
{
    int i = first, x = 0, y = 0, rowh = 0, shown = 0;
    const int gap = 2;

    memset(sheet->px, (unsigned char)b->key, (size_t)sheet->w * sheet->h);

    while (i < b->nframes) {
        int n = (group && group[i] > 1) ? group[i] : 1, k, gw = 0, gh = 0;
        if (i + n > b->nframes) n = b->nframes - i;
        for (k = 0; k < n; ++k) {           /* the group's width and depth */
            const og_image *f = &b->frames[i + k];
            int top = b->frame_oy ? OG_TRUCK_BOX_ROWS / 2 - b->frame_oy[i + k] : 0;
            if (!f->px || f->h <= 0) continue;
            if (top < 0) top = 0;
            gw += f->w;
            if (top + f->h > gh) gh = top + f->h;
        }
        if (gw == 0) { i += n; continue; }  /* nothing to draw */

        if (x > 0 && x + gw > sheet->w) {   /* wrap to the next row */
            x = 0;
            y += rowh + gap;
            rowh = 0;
        }
        if (y + gh > sheet->h) break;       /* page is full */

        for (k = 0; k < n; ++k) {
            const og_image *f = &b->frames[i + k];
            int top = b->frame_oy ? OG_TRUCK_BOX_ROWS / 2 - b->frame_oy[i + k] : 0;
            if (!f->px || f->h <= 0) continue;
            if (top < 0) top = 0;
            og_image_blit(sheet, f, x, y + top, b->key);
            x += f->w;
            ++shown;
        }
        x += gap;
        if (gh > rowh) rowh = gh;
        i += n;
    }
    if (shown_out) *shown_out = shown;
    return i;                                /* first frame of the next page */
}

/* The multi-part sprites, by bank index, as the game draws them -- a
 * first piece and how many follow it side by side.  The sprite table
 * says nothing of this; it is in the code and scripts that place each
 * one, traced where SORP draws the same thing. */
typedef struct { int first, pieces; } og_spr_group;
static const og_spr_group og_spr_title_groups[] = {
    { 0, 5 }, { 5, 5 },           /* "Racing Hints", "Awesome Wins"   */
    { 10, 5 },                    /* Graftgold                        */
    { 15, 2 },                    /* Virgin                           */
    { 17, 5 },                    /* the Virgin Mastertronic wordmark,
                                   * which the game never draws        */
    { 22, 5 }                     /* the Super Off Road logo          */
};
static const og_spr_group og_spr_shop_groups[] = {
    { 0, 2 }, { 8, 2 },           /* the nation cursor, both frames   */
    { 24, 10 },                   /* the banner, 320 across           */
    { 34, 3 },                    /* the girl and the trophy          */
    { 37, 2 }, { 39, 2 }, { 41, 2 }, { 43, 2 },   /* the eight track  */
    { 45, 2 }, { 47, 2 }, { 49, 2 }, { 51, 2 },   /* thumbnails       */
    { 54, 2 }, { 56, 2 }, { 58, 2 }, { 60, 2 },   /* cash and the     */
    { 62, 2 }, { 64, 2 }, { 66, 2 },              /* upgrade boxes    */
    { 68, 2 }                     /* START NEXT RACE, highlighted     */
};
static const og_spr_group og_spr_end_groups[] = {
    { 0, 3 }, { 3, 3 },           /* white: figures, plaque           */
    { 6, 3 }, { 9, 3 },           /* blue                             */
    { 12, 3 }, { 15, 3 },         /* yellow                           */
    { 18, 3 }, { 21, 3 },         /* red                              */
    { 25, 3 },                    /* the banner tile                  */
    { 28, 2 },                    /* the signature                    */
    { 30, 2 },                    /* a backdrop column                */
    { 32, 4 }                     /* the winning-time board           */
};
/* fill `out` (one byte per sprite) from a group list */
static void og_spr_groups(unsigned char *out, int n,
                          const og_spr_group *g, int ng)
{
    int k;
    memset(out, 1, (size_t)n);
    for (k = 0; k < ng; ++k)
        if (g[k].first < n) out[g[k].first] = (unsigned char)g[k].pieces;
}

/* ------------------------------------------------------------------ */
/* Collision against the barrier map                                   */
/* ------------------------------------------------------------------ */

/* The surface map comes from the track artwork: barrier stripes and
 * retaining walls classify as OG_SURF_WALL (see og_palette.h), which is
 * solid, per-pixel and verified against gameplay capture.
 *
 * A single point would let the sprite's corners overlap a barrier, so
 * test a small footprint around the truck's hotspot. */
/* How many of the route's waypoints currently land inside a barrier.
 * The AI's racing line is by definition drivable, so a correct mapping
 * drives this to zero; it is the objective test for the Y scale. */
static int og_route_in_walls(const og_track_art *art,
                             const unsigned char *water,
                             const og_route *r,
                             const unsigned char *H, int ox, int oy,
                             int *offscreen)
{
    int i, bad = 0, off = 0;

    if (!art || !r) return -1;
    for (i = 0; i < r->n; ++i) {
        float x, y;
        og_wp_screen(r, i, H, ox, oy, &x, &y);
        if (x < 0 || x >= OG_SCREEN_W || y < 0 || y >= OG_SCREEN_H) { ++off; continue; }
        if (!og_drivable(art, water, (int)x, (int)y)) ++bad;
    }
    if (offscreen) *offscreen = off;
    return bad;
}



/* ------------------------------------------------------------------ */
/* Audio: the game's effect data driven through the OPL2 core          */
/* ------------------------------------------------------------------ */

#define OG_VOICES     4          /* simultaneous effects              */
#define OG_AUDIO_RATE 44100

typedef struct {
    og_sound        sfx;
    opl3_chip       chip;          /* Nuked OPL3, run as an OPL2       */
    og_voice        voice[OG_VOICES];
    SDL_AudioStream *stream;
    float           *chunk;
    int              chunk_len;   /* samples rendered per tick        */
    int              ready;
    int              next_voice;
    int              threaded;    /* SDL's audio thread renders it    */
    double           tick_due;    /* samples to the driver's next tick */
    /* The arcade's sound board (THE ARCADE'S SOUND, below), in place of
     * the OPL while a set with its sound chips is in use. */
    tp_sound        *arc;
    int              arc_kind;    /* TP_ARCADE or TP_TRACKPAK; -1 none */
    unsigned long long arc_ready; /* board ticks: commands wait for it */
    unsigned long long arc_due;   /* ...and give up on an answer then  */
    int              arc_busy;    /* a command sent and not answered   */
    unsigned char    arc_sent, arc_toggle;
    unsigned short   arc_q[256];  /* commands queued: param << 8 | cmd */
    int              arc_qh, arc_qt;
    float            arc_x1, arc_y1;   /* the DC blocker               */
    int              arc_song;    /* the song command last sent, or 0  */
    int              arc_race;    /* the race's sound on (0x02 sent)   */
    int              arc_live;    /* a race under way (its end not sent) */
    int              arc_eng[3];  /* each engine's countdown, in ticks */
} og_audio;

#define OG_AUDIO_CHUNK_MAX 2048   /* samples rendered at once         */

/* The sound runs on SDL's audio thread (og_audio_feed), which holds the
 * stream's lock while it renders; everything the game does to the driver
 * -- an effect, a song, a mode -- takes the same lock.  SDL's mutexes are
 * recursive, so these nest.  SORP_AUDIO_THREAD=0 renders on the main
 * thread instead, as before, for comparison. */
static void og_snd_lock(og_audio *a)
{
    if (a && a->threaded) SDL_LockAudioStream(a->stream);
}

static void og_snd_unlock(og_audio *a)
{
    if (a && a->threaded) SDL_UnlockAudioStream(a->stream);
}

static void SDLCALL og_audio_feed(void *user, SDL_AudioStream *st,
                                  int additional, int total);

/* og_sound emits (register, value); hand them straight to the chip.
 *
 * Nuked is an OPL3, but with OPL3 mode left off (register 0x105 never
 * written) it behaves as an OPL2: the low bank is the YM3812's own
 * register space, so the game's writes go through unchanged, rhythm mode
 * and all. */
static void og_audio_sink(void *user, unsigned reg, unsigned val)
{
    OPL3_WriteRegBuffered((opl3_chip *)user, (unsigned short)(reg & 0xff),
                          (unsigned char)val);
}

static int og_audio_open(og_audio *a, const char *dir)
{
    SDL_AudioSpec spec;
    int rc;

    memset(a, 0, sizeof *a);
    a->arc_kind = -1;

    rc = og_sound_load(&a->sfx, dir);
    if (rc != OG_OK) {
        OG_ERR("[OG_Audio] Sound data unavailable: %s", og_strerror(rc));
        return rc;
    }

    OPL3_Reset(&a->chip, OG_AUDIO_RATE);
    og_sound_set_sink(&a->sfx, og_audio_sink, &a->chip);
    og_sound_reset(&a->sfx);

    a->chunk_len = OG_AUDIO_RATE / OG_TICK_HZ;
    a->chunk = (float *)malloc(sizeof(float) * OG_AUDIO_CHUNK_MAX);
    if (!a->chunk) return OG_E_NOMEM;

    SDL_zero(spec);
    spec.format   = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq     = OG_AUDIO_RATE;

    {
        const char *e = SDL_getenv("SORP_AUDIO_THREAD");
        a->threaded = !(e && SDL_strcmp(e, "0") == 0);
    }
    a->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                          a->threaded ? og_audio_feed : NULL,
                                          a->threaded ? a : NULL);
    if (!a->stream) {
        OG_ERR("[OG_Audio] SDL_OpenAudioDeviceStream: %s", SDL_GetError());
        a->threaded = 0;
        return OG_E_ARG;
    }
    a->ready = 1;
    SDL_ResumeAudioStreamDevice(a->stream);

    OG_INFO("[OG_Audio] %d patches, %d effects, %d Hz, %d samples/tick, %s",
            OG_NUM_PATCHES, a->sfx.neffects, OG_AUDIO_RATE, a->chunk_len,
            a->threaded ? "on the audio thread" : "on the main thread");
    return OG_OK;
}

static void og_audio_close(og_audio *a)
{
    if (a->stream) SDL_DestroyAudioStream(a->stream);
    if (a->arc) tp_sound_free(a->arc);
    a->arc = NULL;
    free(a->chunk);
    og_sound_free(&a->sfx);
    a->ready = 0;
}

/* The driver's tick is the display's: the game restores the BIOS timer
 * divisor on exit, so INT 8 never drives the sound, and everything
 * counts in EGA frames at 70 Hz.  The F2 overlay steps it for comparison. */
#define OG_SND_HZ_DEFAULT ((double)OG_FRAME_HZ)

/* The sound modes: effects, music, both, the PC speaker's own effects,
 * and (F2 only) silent.  Both is the default -- it is what the game
 * itself does with an AdLib fitted.  "Effects" and "music" are the
 * AdLib's, or the arcade's sound board's while it plays. */
#define OG_AUDIO_SFX    1
#define OG_AUDIO_MUSIC  2
#define OG_AUDIO_SPK    4
#define OG_AUDIO_MODES  5
static int audio_mode = OG_AUDIO_SFX | OG_AUDIO_MUSIC;
/* F1's "sound": the modes but silent (the volume does that), so
 * audio_mode - 1; og_ovl_sound_sel keeps the two together */
static int og_sound_sel = (OG_AUDIO_SFX | OG_AUDIO_MUSIC) - 1;
static og_pcspk pcspk;

static int og_force_effects = 0;   /* play the ones AdLib disables      */
static int og_wall_debug    = 0;   /* report every barrier contact      */
static int og_meters        = 0;   /* the meters over the game, too     */

/* One row of an overlay: a label, the value it shows, and how a nudge
 * changes it.  A row whose label starts with a dash is a heading, drawn
 * in colour and skipped when moving.  `changed` runs after a nudge;
 * `act` is what the BUTTON does on that row (without one the button is
 * the same as right); `show` formats a value that has no names. */
typedef struct {
    const char *label;
    int        *value;
    int         modulo;        /* how many states; 2 is a plain on/off */
    const char *const *names;  /* what each state is called            */
    void      (*changed)(void);
    void      (*act)(void);
    const char *(*show)(int v);
} og_option;

static const char *const og_on_off[]   = { "off", "on" };
static const char *const og_audio_nm[] = { "silent", "effects", "music",
                                           "music & effects", "PC speaker" };
static const char *const og_mesh_nm[]  = { "off", "height", "slope",
                                           "surface records" };
static const char *const og_voice_nm[] = { "both", "first only",
                                           "second only" };
static const char *const og_ops_nm[]   = { "normal", "swapped" };

/* the audio overlay's test selections */
#define OG_TICK_RATES 7
static const int og_tick_rate[OG_TICK_RATES] = { 35, 50, 60, 70, 80, 100, 140 };
static int og_tick_i   = 3;        /* 70 Hz, the EGA frame rate         */
static int og_test_fx  = 0;        /* effect 0..14                      */
static int og_test_song = 0;       /* song 1..19, held as 0..18         */
static int og_test_ins = 0;        /* instrument 0..63                  */
/* The pitch table at DS:0xb2c9 is equal-tempered from C0 (16.35 Hz):
 * note n is n semitones up, so 48 is middle C.  It runs 96 notes, eight
 * octaves; past 95 the words are something else. */
#define OG_TEST_NOTES 96
static int og_test_note = 48;      /* note 0..95, middle C              */
static int og_test_ch  = 0;        /* OPL channel 0..8, for muting      */
static int og_dummy    = 0;        /* the value of a row that only acts */

/* defined with the sound code below */
static void og_ovl_audio_mode(void);
static void og_ovl_tick_rate(void);
static void og_ovl_silence(void);
static void og_ovl_play_fx(void);
static void og_ovl_play_song(void);
static void og_ovl_stop_song(void);
static void og_ovl_play_note(void);
static void og_ovl_mute(void);
static const char *og_ovl_show_rate(int v);
static const char *og_ovl_show_fx(int v);
static const char *og_ovl_show_song(int v);
static const char *og_ovl_show_ins(int v);
static const char *og_ovl_show_note(int v);
static const char *og_ovl_show_ch(int v);
static const char *og_ovl_show_act(int v);
static void og_ovl_volume(void);
static void og_ovl_shader(void);
static void og_ovl_track(void);
static void og_ovl_next_stage(void);
static const char *og_ovl_show_stage(int v);
static void og_ovl_fullscreen(void);
static const char *og_ovl_show_full(int v);
static void og_ovl_exit(void);
static void og_ovl_about(void);
static const char *og_ovl_show_none(int v);
static void og_ovl_log_write(void);
static const char *og_ovl_show_log_write(int v);
static void og_ovl_log_level(void);
static void og_ovl_arcade_roms(void);
static void og_ovl_sound_sel(void);
static void og_ovl_arcade_mode(void);
static int og_arcade_mode_usable(void);
static const char *const og_arcade_mode_nm[] = { "original", "track-pak", "all 16" };

/* F4's "next stage": moves whatever is on screen on.  In attract mode
 * the attract machine ends the stage it is in, as the stage's own timer
 * would (the demo race, which has no timer, is ended early).  Otherwise
 * a running race is ended early, the podium goes on to the Speed Shop,
 * and the shop starts the next race.  og_next_nm is what the row shows:
 * main names the screen each frame. */
static int og_next_req = 0;
static const char *og_next_nm = "-";

/* SORP's own event that moves a screen on (registered at startup) */
static Uint32 og_ev_advance = 0;
static void og_advance(void)
{
    SDL_Event e;
    if (!og_ev_advance) return;
    SDL_zero(e);
    e.type = og_ev_advance;
    SDL_PushEvent(&e);
}

/* F4's track and direction.  Changing either asks main to load that race
 * (the same as the attract cycle does when it leaves the title): the
 * request is og_race_req, for og_req_track / og_req_reverse.  The rows
 * otherwise follow whatever race is loaded. */
static int og_dbg_track = 0, og_dbg_reverse = 0;
static int og_race_req = 0, og_req_track = 0, og_req_reverse = 0;

/* THE PAUSE.  In the DOS game F1 pauses a race and F10 then ends the
 * game.  0x030a, called each frame of the race loop (0x189): nothing in a
 * demo race ([0x1a1] set); otherwise F1 down waits for its release, then
 * loops reading the keys -- F1 again, once let go, carries on (clc); F10
 * returns carry, and 0x018c -> 0x01d0 turns the music off (0x3ac8) and
 * leaves the race with carry, which the game loop (0x0115) takes back
 * to 0x00d9: the demo flag, race 0 and the standings (0x1e4), and the
 * title (0x2468) -- no podium, no shop, no club entry.  Nothing is drawn
 * while it waits, and the sound driver, ticked from the timer interrupt
 * (0x39d6), plays on.
 *
 * SORP's F1 is the options overlay, so the race stands still while it is
 * up -- with its "About" and the "Exit SORP?" it asks, and ESC's "Exit
 * SORP?" too -- the music playing on.  Unlike the DOS game it pauses a
 * demo race as well, the overlay being the same on every screen.  F10
 * with F1 up in a race with players asks "Back to the title?" -- ESC or
 * Start to go, as "Exit SORP?" is answered -- and leaves for it. */
static int og_title_req = 0;
static int og_race_paused(void)
{
    return og_options == 1 || og_about || og_quit_ask;
}
/* ...and from which arcade set: 0 the original's, 1 the Track-Pak's, -1
 * whichever is in use (F4) */
static int og_req_tp = -1;
/* The tracks by file, TRACK0.XX .. TRACK7.XX.  The game's own numbering
 * runs 1..8 with Blaster as 8, so each file is its number and Blaster is
 * TRACK0: Sidewinder (3) is the demo's first race and Wipeout (6) its
 * second, and Blaster's score board sits bottom left, as TRACK0's does. */
static const char *const og_track_file_nm[] = {
    "Blaster", "Big Dukes", "Cliffhanger", "Sidewinder",
    "Fandango", "Hurricane Gulch", "Wipeout", "Huevos Grande"
};
static const char *const og_direction_nm[] = { "normal", "reversed" };
static const char *og_ovl_show_volume(int v);

/* The master volume, 0..100 in steps of 5, set on the output stream as a
 * whole -- AdLib and PC speaker alike.  The gain goes as the square of
 * the level, so the steps sound roughly even.  The overlay draws it as a
 * level bar with the figure beside it. */
#define OG_VOLUME_STEP  5
#define OG_VOLUME_STEPS (100 / OG_VOLUME_STEP + 1)
static int og_volume = 75 / OG_VOLUME_STEP;      /* held as steps: 75 */

/* F1, the options overlay.  A label beginning with '-' is a heading. */
static og_option og_opt_main[] = {
    { "- SETTINGS",          NULL,               0, NULL },
    { "sound",               &og_sound_sel,      4, og_audio_nm + 1, og_ovl_sound_sel },
    { "volume",              &og_volume,         OG_VOLUME_STEPS, NULL,
                             og_ovl_volume, NULL, og_ovl_show_volume },
    { "full screen",         &og_fullscreen,     2, NULL,
                             og_ovl_fullscreen, NULL, og_ovl_show_full },
    { "gamepad controls",    &og_pad_scheme,     OG_PAD_SCHEMES, og_pad_scheme_nm },
    { "screen shader",    &og_screen_shader,  OG_SCREEN_SHADERS, og_screen_shader_nm },
    { "sprite shader",       &og_sprite_shader,  OG_SPRITE_SHADERS, og_sprite_shader_nm,
                             og_ovl_shader },
    { "background shader",   &og_back_shader,    OG_BACK_SHADERS, og_back_shader_nm,
                             og_ovl_shader },
    { "write sorp.log",      &og_log_write,      2, og_on_off,
                             og_ovl_log_write, NULL, og_ovl_show_log_write },
    { "log level",           &og_log_level,      OG_LOG_LEVELS, og_log_level_nm,
                             og_ovl_log_level },
    { "arcade roms",         &og_arcade_roms,    2, og_on_off, og_ovl_arcade_roms },
    { "arcade mode",         &og_arcade_mode,    OG_ARCADE_MODES, og_arcade_mode_nm,
                             og_ovl_arcade_mode },
    /* drawn apart, in the bottom left corner: a pad has no ESC */
    { "Exit SORP",           &og_dummy,          1, NULL,
                             NULL, og_ovl_exit, og_ovl_show_none },
    /* drawn apart too, as the build stamp in the bottom right corner */
    { "About",               &og_dummy,          1, NULL,
                             NULL, og_ovl_about, og_ovl_show_none }
};

/* F4, the debug overlay: the console and the aids drawn over the race. */
static og_option og_opt_debug[] = {
    { "- DEBUG",             NULL,               0, NULL },
    { "track",               &og_dbg_track,      8, og_track_file_nm, og_ovl_track },
    { "direction",           &og_dbg_reverse,    2, og_direction_nm, og_ovl_track },
    { "next stage",          &og_dummy,          1, NULL,
                             NULL, og_ovl_next_stage, og_ovl_show_stage },
    { "debug console",       &og_console,        2, og_on_off },
    { "navigation waypoints", &ovl_waypoints,    2, og_on_off },
    { "progress gates",      &ovl_gates,         2, og_on_off },
    { "physics wheel dots",  &ovl_wheels,        2, og_on_off },
    { "contour mesh",        &mesh_mode,         4, og_mesh_nm },
    { "barrier reports",     &og_wall_debug,     2, og_on_off }
};

/* F2, the audio overlay: every sound setting and debugging aid, and
 * controls to sound any effect, song or note.  Its meters are drawn
 * below the rows. */
static og_option og_opt_audio[] = {
    { "- AUDIO",             NULL,               0, NULL },
    { "sound",               &audio_mode,        5, og_audio_nm, og_ovl_audio_mode },
    { "tick rate",           &og_tick_i,         OG_TICK_RATES, NULL,
                             og_ovl_tick_rate, NULL, og_ovl_show_rate },
    { "forced effects",      &og_force_effects,  2, og_on_off },
    { "instrument voices",   &og_voice_mode,     3, og_voice_nm, og_ovl_silence },
    { "voice operators",     &og_sound_swap_ops, 2, og_ops_nm,   og_ovl_silence },
    { "meters in game",      &og_meters,         2, og_on_off },
    { "- TEST: the button plays",  NULL,         0, NULL },
    { "effect",              &og_test_fx,        OG_NUM_EFFECTS, NULL,
                             NULL, og_ovl_play_fx, og_ovl_show_fx },
    { "song",                &og_test_song,      OG_MUSIC_SONGS, NULL,
                             NULL, og_ovl_play_song, og_ovl_show_song },
    { "stop the music",      &og_dummy,          1, NULL,
                             NULL, og_ovl_stop_song, og_ovl_show_act },
    { "instrument",          &og_test_ins,       OG_NUM_PATCHES, NULL,
                             NULL, og_ovl_play_note, og_ovl_show_ins },
    { "note",                &og_test_note,      OG_TEST_NOTES, NULL,
                             NULL, og_ovl_play_note, og_ovl_show_note },
    { "mute channel",        &og_test_ch,        9, NULL,
                             NULL, og_ovl_mute, og_ovl_show_ch },
    { "silence",             &og_dummy,          1, NULL,
                             NULL, og_ovl_silence, og_ovl_show_act }
};

#define OG_ROWS(t) ((int)(sizeof (t) / sizeof (t)[0]))
typedef struct { og_option *rows; int n, row; } og_overlay_tab;
/* indexed by og_options: 1 the options (F1), 2 audio (F2), 3 the
 * sprite viewer (F3), which has no rows, 4 debug (F4) */
static og_overlay_tab og_ovls[5] = {
    { NULL, 0, 0 },
    { og_opt_main,  OG_ROWS(og_opt_main),  0 },
    { og_opt_audio, OG_ROWS(og_opt_audio), 0 },
    { NULL, 0, 0 },
    { og_opt_debug, OG_ROWS(og_opt_debug), 0 }
};

/* F1's rows as shown: og_opt_main less the ones that do not apply --
 * "arcade roms" with no arcade set found in gamedata/trackpak, "arcade
 * mode" with that off too.  og_opt_main keeps every row, so sorp.cfg
 * keeps a hidden row's setting.  Made again whenever one of those
 * changes; the cursor stays on the row it was on. */
static og_option og_opt_shown[OG_ROWS(og_opt_main)];
static int og_tp_found = 0;        /* an arcade set: og_tp_detect says  */
static int og_tp_have[TP_KINDS];   /* ...and which: found at start-up   */

/* A value F1 offers for a row: "arcade mode" only the modes whose sets
 * are there -- "original" and "all 16" want the arcade's own chips,
 * "track-pak" and "all 16" the Track-Pak's. */
static int og_opt_offered(const og_option *o, int v)
{
    if (o->value == &og_arcade_mode)
        return v == OG_ARCADE_ORIGINAL ? og_tp_have[TP_ARCADE]
             : v == OG_ARCADE_TRACKPAK ? og_tp_have[TP_TRACKPAK]
             : og_tp_have[TP_ARCADE] && og_tp_have[TP_TRACKPAK];
    return 1;
}
static void og_opt_refresh(void)
{
    og_overlay_tab *t = &og_ovls[1];
    const og_option *was = (t->rows && t->row < t->n) ? &t->rows[t->row] : NULL;
    const char *was_label = was ? was->label : NULL;
    int r, n = 0;
    t->row = 0;
    for (r = 0; r < OG_ROWS(og_opt_main); ++r) {
        const og_option *o = &og_opt_main[r];
        if (o->value == &og_arcade_roms && !og_tp_found) continue;
        if (o->value == &og_arcade_mode && (!og_tp_found || !og_arcade_roms)) continue;
        if (was_label && o->label == was_label) t->row = n;
        og_opt_shown[n++] = *o;
    }
    t->rows = og_opt_shown;
    t->n    = n;
}

/* Limit F1's shader rows to what the screen can show: the screen shader
 * to its first `screen` states (none, scanlines, then the 2x scalers),
 * and the sprite and background shaders to `passes` of MMPX -- 0 none,
 * 1 up to MMPX 2x.  A row left with one state reads "n/a". */
static void og_shader_limit(int screen, int passes)
{
    int r;
    for (r = 0; r < OG_ROWS(og_opt_main); ++r) {
        og_option *o = &og_opt_main[r];
        if (o->value == &og_screen_shader) o->modulo = screen;
        else if (o->value == &og_sprite_shader || o->value == &og_back_shader) {
            if (passes + 1 < o->modulo) o->modulo = passes + 1;
        } else continue;
        if (*o->value >= o->modulo) *o->value = 0;
    }
    og_opt_refresh();                  /* F1 shows the new limits */
    OG_INFO("[OG_Video] Shaders on this screen: %s%s%s", screen > 1 ? "scanlines " : "",
            screen > 2 ? "eagle MMPX-screen " : "",
            passes >= 2 ? "MMPX 4x" : passes == 1 ? "MMPX 2x" :
            screen > 1 ? "" : "none");
}

/* ------------------------------------------------------------------ */
/* sorp.cfg: F1's settings, kept beside the executable                  */
/* ------------------------------------------------------------------ */
/*
 * Every F1 row with a value -- sound, volume, full screen and the three
 * shaders -- one line each, named as the overlay names them:
 *
 *   screen shader = scanlines
 *   volume = 75
 *
 * Read at start-up, before the window is made, so a full screen comes up
 * full screen; the screen's own limits (og_shader_limit) apply after, so
 * a shader this screen cannot show falls back to none.  Written whenever
 * one of them changes -- by way of a temporary file and a rename, since
 * a handheld's hotkey ends SORP with a kill, not a quit.  Unknown lines
 * and values are ignored; F2's and F4's rows are test and debug aids and
 * are not kept.
 */
#define OG_CFG_FILE "sorp.cfg"
static char og_cfg_path[1024];
static int  og_cfg_read = -1;            /* settings read, or -1: no file */

/* A desktop window's place, kept with the settings:
 *
 *   window size = 960 x 600
 *   window position = 120, 80
 *
 * Only a real desktop window's -- not KMSDRM's or the framebuffer's,
 * which are the whole screen -- and only while it is neither full screen
 * nor maximised, so the size it returns to is the one kept.  A move or a
 * resize marks it changed (og_win_track) and it is written once things
 * have been still for a moment (og_win_settle) and at exit.  A position
 * no display shows any more is not used (og_win_place).  Under Wayland,
 * which keeps a window's place from it, only the size is kept. */
static int    og_win_w, og_win_h;          /* 0: none kept               */
static int    og_win_x, og_win_y, og_win_have_xy;
static int    og_win_no_xy;                /* Wayland: the size only     */
static Uint64 og_win_dirty;                /* when it last changed, or 0 */
#define OG_WIN_SETTLE_MS 750

/* the text a row's value is kept as */
static void og_cfg_value(const og_option *o, char *out, size_t n)
{
    const int v = *o->value;
    if (o->value == &og_volume)
        SDL_snprintf(out, n, "%d", v * OG_VOLUME_STEP);
    else if (o->names)
        SDL_strlcpy(out, o->names[v], n);
    else
        SDL_strlcpy(out, og_on_off[v ? 1 : 0], n);
}

static int og_cfg_kept(const int *value)
{
    int r;
    for (r = 0; r < OG_ROWS(og_opt_main); ++r)
        if (og_opt_main[r].value == value && og_opt_main[r].modulo >= 2)
            return 1;
    return 0;
}

static void og_cfg_save(void)
{
    char tmp[1040], line[160], val[64];
    SDL_IOStream *f;
    int r, ok = 1;
    if (!og_cfg_path[0]) return;
    SDL_snprintf(tmp, sizeof tmp, "%s.tmp", og_cfg_path);
    f = SDL_IOFromFile(tmp, "wb");
    if (!f) {
        OG_ERR("[OG_Settings] Cannot write %s: %s", og_rel(tmp), SDL_GetError());
        return;
    }
    SDL_strlcpy(line, "# SORP's settings: the F1 options\n", sizeof line);
    ok &= SDL_WriteIO(f, line, SDL_strlen(line)) == SDL_strlen(line);
    for (r = 0; r < OG_ROWS(og_opt_main); ++r) {
        const og_option *o = &og_opt_main[r];
        if (!o->value || o->modulo < 2) continue;
        og_cfg_value(o, val, sizeof val);
        SDL_snprintf(line, sizeof line, "%s = %s\n", o->label, val);
        ok &= SDL_WriteIO(f, line, SDL_strlen(line)) == SDL_strlen(line);
    }
    if (og_win_w > 0) {
        SDL_snprintf(line, sizeof line, "window size = %d x %d\n", og_win_w, og_win_h);
        ok &= SDL_WriteIO(f, line, SDL_strlen(line)) == SDL_strlen(line);
    }
    if (og_win_have_xy) {
        SDL_snprintf(line, sizeof line, "window position = %d, %d\n", og_win_x, og_win_y);
        ok &= SDL_WriteIO(f, line, SDL_strlen(line)) == SDL_strlen(line);
    }
    og_win_dirty = 0;
    ok &= SDL_CloseIO(f);
    if (!ok || !SDL_RenamePath(tmp, og_cfg_path)) {
        OG_ERR("[OG_Settings] Cannot write %s: %s", og_rel(og_cfg_path), SDL_GetError());
        SDL_RemovePath(tmp);
    } else {
        OG_DEBUG("[OG_Settings] Saved to %s", og_rel(og_cfg_path));
    }
}

/* blanks off both ends, in place */
static char *og_cfg_trim(char *t)
{
    char *e;
    while (*t == ' ' || *t == '\t') ++t;
    for (e = t + SDL_strlen(t);
         e > t && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'); --e)
        e[-1] = '\0';
    return t;
}

static void og_cfg_load(void)
{
    const char *base = SDL_GetBasePath();
    char *text, *line, *next;
    size_t len = 0;
    int n = 0;
    SDL_snprintf(og_cfg_path, sizeof og_cfg_path, "%s%s",
                 base ? base : "", OG_CFG_FILE);
    text = (char *)SDL_LoadFile(og_cfg_path, &len);
    og_cfg_read = -1;                  /* none yet: og_cfg_report says so */
    if (!text) return;
    for (line = text; line && *line; line = next) {
        char *eq, *key, *val;
        int r, v;
        next = SDL_strchr(line, '\n');
        if (next) *next++ = '\0';
        if (*line == '#' || !(eq = SDL_strchr(line, '='))) continue;
        *eq = '\0';
        key = og_cfg_trim(line);
        val = og_cfg_trim(eq + 1);
        if (SDL_strcasecmp(key, "sound") == 0) {     /* the old names */
            if (SDL_strcasecmp(val, "effects + music") == 0 ||
                SDL_strcasecmp(val, "effects and music") == 0) val = "music & effects";
            else if (SDL_strcasecmp(val, "AdLib effects") == 0) val = "effects";
            else if (SDL_strcasecmp(val, "AdLib music") == 0) val = "music";
        } else if (SDL_strcasecmp(key, "arcade mode") == 0 &&
                   SDL_strcasecmp(val, "All 16 Tracks") == 0) {
            val = "all 16";                     /* "Original" and "Track-Pak" match */
        }
        if (SDL_strcasecmp(key, "window size") == 0) {
            int w, h;
            if (SDL_sscanf(val, "%d x %d", &w, &h) == 2 &&
                w >= OG_SCREEN_W && h >= OG_SCREEN_H && w <= 16384 && h <= 16384) {
                og_win_w = w; og_win_h = h; ++n;
            }
            continue;
        }
        if (SDL_strcasecmp(key, "window position") == 0) {
            int x, y;
            if (SDL_sscanf(val, "%d , %d", &x, &y) == 2) {
                og_win_x = x; og_win_y = y; og_win_have_xy = 1; ++n;
            }
            continue;
        }
        for (r = 0; r < OG_ROWS(og_opt_main); ++r) {
            og_option *o = &og_opt_main[r];
            char have[64];
            if (!o->value || o->modulo < 2 || SDL_strcasecmp(o->label, key) != 0)
                continue;
            if (o->value == &og_volume) {
                v = SDL_atoi(val) / OG_VOLUME_STEP;
                if (v >= 0 && v < o->modulo) { *o->value = v; ++n; }
                break;
            }
            for (v = 0; v < o->modulo; ++v) {
                int keep = *o->value;
                *o->value = v;
                og_cfg_value(o, have, sizeof have);
                *o->value = keep;
                if (SDL_strcasecmp(have, val) == 0) { *o->value = v; ++n; break; }
            }
            break;
        }
    }
    SDL_free(text);
    og_cfg_read = n;
}

/* what og_cfg_load found -- it runs before the log's first line, so it
 * says nothing itself */
static void og_cfg_report(void)
{
    if (og_cfg_read < 0) OG_INFO("[OG_Settings] None yet (%s)", og_rel(og_cfg_path));
    else OG_INFO("[OG_Settings] %d from %s", og_cfg_read, og_rel(og_cfg_path));
}

/* the window moved or was resized: keep its place, unless it is full
 * screen or maximised (then the place it returns to stays kept) */
static void og_win_track(SDL_Window *w)
{
    const SDL_WindowFlags fl = w ? SDL_GetWindowFlags(w) : 0;
    int x = og_win_x, y = og_win_y, ww, wh;
    if (!w || (fl & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED |
                     SDL_WINDOW_MINIMIZED)))
        return;
    if (!SDL_GetWindowSize(w, &ww, &wh) ||
        (!og_win_no_xy && !SDL_GetWindowPosition(w, &x, &y)))
        return;
    if (ww == og_win_w && wh == og_win_h &&
        (og_win_no_xy || (og_win_have_xy && x == og_win_x && y == og_win_y)))
        return;
    og_win_w = ww; og_win_h = wh;
    if (!og_win_no_xy) { og_win_x = x; og_win_y = y; og_win_have_xy = 1; }
    og_win_dirty = SDL_GetTicks();
    if (!og_win_dirty) og_win_dirty = 1;
}

/* once a frame: write a changed place when it has been still a while,
 * or now (at exit) */
static void og_win_settle(int now)
{
    if (og_win_dirty && (now || SDL_GetTicks() - og_win_dirty >= OG_WIN_SETTLE_MS)) {
        if (og_win_no_xy) OG_DEBUG("[OG_Video] The window is %dx%d", og_win_w, og_win_h);
        else OG_DEBUG("[OG_Video] The window is %dx%d at (%d, %d)",
                      og_win_w, og_win_h, og_win_x, og_win_y);
        og_cfg_save();
    }
}

/* the kept position, if any display still shows a corner of the title
 * bar's worth of it */
static void og_win_place(SDL_Window *w)
{
    SDL_DisplayID *ds;
    int i, n = 0, seen = 0;
    if (!w || !og_win_have_xy) return;
    ds = SDL_GetDisplays(&n);
    for (i = 0; ds && i < n && !seen; ++i) {
        SDL_Rect b, r, out;
        if (!SDL_GetDisplayUsableBounds(ds[i], &b)) continue;
        r.x = og_win_x; r.y = og_win_y;
        r.w = og_win_w > 0 ? og_win_w : OG_SCREEN_W; r.h = 32;
        seen = SDL_GetRectIntersection(&b, &r, &out) && out.w >= 64;
    }
    SDL_free(ds);
    if (seen) SDL_SetWindowPosition(w, og_win_x, og_win_y);
    else {
        og_win_have_xy = 0;
        OG_WARN("[OG_Settings] The kept window position is off every display");
    }
}

/* move the selection, skipping the headings */
static void og_opt_move(int dir)
{
    og_overlay_tab *t = &og_ovls[og_options];
    int n = 0;
    if (!og_options || t->n <= 0) return;
    do {
        t->row = (t->row + dir + t->n) % t->n;
    } while (!t->rows[t->row].value && ++n < t->n);
}

static void og_opt_nudge(int dir)
{
    og_option *o;
    if (!og_options) return;
    o = &og_ovls[og_options].rows[og_ovls[og_options].row];
    if (!o->value || o->modulo < 2) return;
    {
        int v = *o->value, n;
        for (n = 0; n < o->modulo; ++n) {
            v = (v + dir + o->modulo) % o->modulo;
            if (og_opt_offered(o, v)) break;
        }
        if (v == *o->value) return;            /* the only one offered */
        *o->value = v;
    }
    if (o->changed) o->changed();
    {
        char val[64];
        if (o->names || o->modulo == 2 || o->value == &og_volume)
            og_cfg_value(o, val, sizeof val);
        else
            SDL_snprintf(val, sizeof val, "%d", *o->value);
        if (og_cfg_kept(o->value)) OG_INFO("[OG_Settings] %s: %s", o->label, val);
        else                       OG_DEBUG("[OG_Overlay] %s: %s", o->label, val);
    }
    if (og_cfg_kept(o->value)) og_cfg_save();
}

/* the button: the row's action, or a nudge to the right */
static void og_opt_fire(void)
{
    og_option *o;
    if (!og_options) return;
    o = &og_ovls[og_options].rows[og_ovls[og_options].row];
    if (o->act) o->act();
    else        og_opt_nudge(1);
}

/* open overlay n, or close it if it is the one showing */
static void og_ovl_toggle(int n)
{
    og_options = (og_options == n) ? 0 : n;
    og_opt_held = 1;              /* this press is not a nudge */
    if (og_options && og_ovls[og_options].rows &&
        !og_ovls[og_options].rows[og_ovls[og_options].row].value)
        og_opt_move(1);
}

static const char *og_audio_mode_name(int m)
{
    switch (m) {
    case 1:  return "effects";
    case 2:  return "music";
    case 3:  return "music & effects";
    case 4:  return "PC speaker effects";
    default: return "silent";
    }
}
/* ------------------------------------------------------------------ */
/* THE OPTIONS OVERLAY (F1)                                            */
/* ------------------------------------------------------------------ */
/*
 * One place for everything that was a scattered key.  F1 opens and
 * closes it; while it is open the players' controls drive the overlay
 * and NOTHING reaches the game behind it, so a truck does not wander
 * off while its driver is changing a setting.
 *
 * up / down    move between rows
 * left / right change the setting on that row
 * the button   the same as right (so one button is enough)
 *
 * The first section is the settings a player might actually want; the
 * second is the debugging aids.
 */



/* ------------------------------------------------------------------ */
/* THE ARCADE'S SOUND                                                  */
/* ------------------------------------------------------------------ */
/*
 * With "arcade roms" on and the set's six sound chips in gamedata/
 * trackpak, the arcade's own sound board plays in place of the OPL:
 * tp_sound.c runs its 80186 program, and SORP sends it the
 * commands the arcade's master CPU sends, at the moments the master
 * sends them, as MAME showed them sent.  The
 * base game's board for its tracks, the Track-Pak's for the Track-Pak's.
 *
 * The protocol is the master's (0x12b4): the parameter and the command
 * into the latch, INT0 pulsed, bit 7 of the command toggled each send
 * (the first is 1), and the next command only once the 80186 has
 * echoed this one -- sent too close together, commands are lost.  So
 * the game queues them and the audio thread hands them over between
 * slices of rendering, each when the last is answered.
 *
 * The board takes commands only once its program has started; the
 * master's first, the title's song, comes a third of a second after it
 * lets the board out of reset.  Every command but the engines' ignores
 * its parameter (each was played with 00, 40, 80 and ff: identical
 * output), so only the engines' carries anything.
 *
 * F1's "sound" still applies: music off sends no songs, effects off no
 * effects or engines; the PC speaker is the DOS game's, board or not.
 */
/* The board's mix (DC taken out) runs at 0.05-0.09 RMS in its songs,
 * the OPL's at 0.2-0.29; 2.5 brings the two close, and a soft knee above
 * 0.8 keeps a song with effects over it (peaks 4-15 times the RMS) from
 * clipping hard. */
#define OG_ARC_GAIN  2.5f
#define OG_ARC_SLICE 32          /* samples between looks at the queue */

static int og_arc_on(const og_audio *a)
{
    return a && a->arc && audio_mode != OG_AUDIO_SPK &&
           (audio_mode & (OG_AUDIO_SFX | OG_AUDIO_MUSIC));
}

/* queue a command (the stream locked, or not threaded) */
static void og_arc_queue(og_audio *a, int cmd, int param)
{
    int next;
    if (!a || !a->arc) return;
    og_snd_lock(a);
    next = (a->arc_qt + 1) % (int)SDL_arraysize(a->arc_q);
    if (next != a->arc_qh) {
        a->arc_q[a->arc_qt] = (unsigned short)(((param & 0xff) << 8) | (cmd & 0x7f));
        a->arc_qt = next;
        OG_TRACE("[OG_Audio] Arcade command %02x %02x", cmd & 0x7f, param & 0xff);
    }
    og_snd_unlock(a);
}

/* ...only while the board is what plays.  og_arc_queue itself is for
 * the sound mode changing, which must reach the board in any mode: a
 * song or the engine left running there would carry on under "effects",
 * or come back where it was left after the PC speaker. */
static void og_arc_send(og_audio *a, int cmd, int param)
{
    if (og_arc_on(a)) og_arc_queue(a, cmd, param);
}

/* an effect: only while effects are on */
static void og_arc_fx(og_audio *a, int cmd)
{
    if (audio_mode & OG_AUDIO_SFX) og_arc_send(a, cmd, 0);
}

/* Each player's own sound (master 0x3ffd and 0x49ed, by truck: red,
 * yellow, blue): their pickup, their joining, their START NEXT RACE. */
static const unsigned char og_arc_player_snd[3] = { 0x0a, 0x2d, 0x2e };

/* The board's meters (F2): one bar for each of its six 8-bit DACs and
 * one for the 10-bit, fed by tp_sound's hook on every DAC write.  An
 * 8-bit DAC's level is how far its sample is from the middle (0x80),
 * scaled by its volume DAC (the write's high byte); the 10-bit DAC's,
 * how far it is from 0x200.  Each holds its peak and falls back once a
 * frame with the OPL's (og_arc_meters_decay). */
#define OG_ARC_METERS 7
static float         og_arc_level[OG_ARC_METERS];
static unsigned char og_arc_dac_s[6] = { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 };
static unsigned char og_arc_dac_v[6];
static int           og_arc_metering;    /* the bars show the board's   */

static void og_arc_meter_hook(void *user, unsigned long long tick, int dac,
                              unsigned value, unsigned mask)
{
    float lvl;
    int m;
    (void)user; (void)tick;
    if (dac >= 0 && dac < 6) {
        if (mask & 0x00ff) og_arc_dac_s[dac] = (unsigned char)value;
        if (mask & 0xff00) og_arc_dac_v[dac] = (unsigned char)(value >> 8);
        lvl = (float)SDL_abs((int)og_arc_dac_s[dac] - 0x80) / 128.0f *
              (float)og_arc_dac_v[dac] / 255.0f;
        m = dac;
    } else if (dac == 9) {
        lvl = (float)SDL_abs((int)(value & 0x3ff) - 0x200) / 512.0f;
        m = 6;
    } else return;
    lvl = SDL_sqrtf(lvl);                /* a quiet voice still shows */
    if (lvl > og_arc_level[m]) og_arc_level[m] = lvl > 1.0f ? 1.0f : lvl;
}

static void og_arc_meters_decay(void)
{
    int i;
    for (i = 0; i < OG_ARC_METERS; ++i) {
        og_arc_level[i] *= 0.82f;
        if (og_arc_level[i] < 0.004f) og_arc_level[i] = 0.0f;
    }
}

/* hand the next command over if the last has been answered */
static void og_arc_service(og_audio *a)
{
    tp_sound *s = a->arc;
    const unsigned long long now = tp_sound_now(s);
    if (a->arc_busy) {
        if (!((tp_sound_response(s) ^ a->arc_sent) & 0x80)) a->arc_busy = 0;
        else if (now >= a->arc_due) {
            a->arc_busy = 0;
            OG_WARN("[OG_Audio] The sound board did not answer command %02x", a->arc_sent & 0x7f);
        }
    }
    if (!a->arc_busy && a->arc_qh != a->arc_qt && now >= a->arc_ready) {
        const unsigned short q = a->arc_q[a->arc_qh];
        a->arc_qh = (a->arc_qh + 1) % (int)SDL_arraysize(a->arc_q);
        a->arc_sent = (unsigned char)((q & 0x7f) | (a->arc_toggle ? 0x80 : 0));
        a->arc_toggle ^= 1;
        tp_sound_command(s, a->arc_sent, (unsigned char)(q >> 8));
        a->arc_busy = 1;
        a->arc_due = now + TP_SOUND_CLOCK / 20;          /* 50 ms */
    }
}

/* n samples of the board, its DC taken out */
static void og_arc_render(og_audio *a, float *out, int n)
{
    int i;
    for (i = 0; i < n; ) {
        const int k = n - i < OG_ARC_SLICE ? n - i : OG_ARC_SLICE;
        og_arc_service(a);
        tp_sound_render(a->arc, out + i, k, OG_AUDIO_RATE);
        i += k;
    }
    for (i = 0; i < n; ++i) {
        const float x = out[i], y = x - a->arc_x1 + 0.995f * a->arc_y1;
        float v = y * OG_ARC_GAIN;
        a->arc_x1 = x;
        a->arc_y1 = y;
        if (v > 0.8f)       v = 0.8f + 0.2f * tanhf((v - 0.8f) / 0.2f);
        else if (v < -0.8f) v = -0.8f - 0.2f * tanhf((-v - 0.8f) / 0.2f);
        out[i] = v;
    }
}

/* A board running `rom` (NULL: none), started as the master starts it
 * (MAME's log): the latch FFFF while in reset, out of reset (F0: 78,
 * then F8), the latch 0000 15 ms later. */
static void og_arc_board(og_audio *a, const unsigned char *rom, int kind)
{
    og_snd_lock(a);
    if (a->arc) tp_sound_free(a->arc);
    a->arc = NULL;
    a->arc_kind = -1;
    a->arc_qh = a->arc_qt = 0;
    a->arc_busy = 0;
    a->arc_song = 0;
    a->arc_race = 0;
    a->arc_live = 0;
    a->arc_x1 = a->arc_y1 = 0.0f;
    if (rom) {
        tp_sound *s = tp_sound_new(rom);
        if (s) {
            tp_sound_control(s, 0x00);
            tp_sound_latch(s, 0xffff);
            tp_sound_control(s, 0x78);
            tp_sound_control(s, 0xf8);
            tp_sound_run_to(s, tp_sound_now(s) + TP_SOUND_CLOCK * 15 / 1000);
            tp_sound_latch(s, 0x0000);
            a->arc_ready = tp_sound_now(s) + TP_SOUND_CLOCK / 3;
            a->arc_toggle = 1;
            a->arc = s;
            a->arc_kind = kind;
            tp_sound_set_hook(s, og_arc_meter_hook, a);
        }
    }
    og_snd_unlock(a);
}

/* The race's sound, as the master starts it (0x4edf): 0x02, 0x51 (no
 * song), and the three engine voices (0x4f20: 0x14..0x16, each with 0
 * for a human, 1 for the computer -- though the board has ONE engine,
 * 0x15/0x1d's; the other two commands do nothing). */
static void og_arc_race_on(og_audio *a, const og_racer *R)
{
    int t;
    if (!og_arc_on(a) || !(audio_mode & OG_AUDIO_SFX)) return;
    og_arc_send(a, 0x02, 0x40);
    for (t = 0; t < 3; ++t) {
        og_arc_send(a, 0x14 + t, R ? (R[t].computer ? 1 : 0) : 1);
        a->arc_eng[t] = t + 1;                 /* staggered, as (ix+0x30) */
    }
    a->arc_race = 1;
}

/* a race starts: its sound, and remember that one is under way */
static void og_arc_race_start(og_audio *a, const og_racer *R)
{
    if (a) a->arc_live = 1;
    og_arc_race_on(a, R);
}

/* ...and its end (0x4dc6): 0x03, and the chequered fanfare 0x12 if the
 * race was won; the song's 0x51 comes from og_music_end */
static void og_arc_race_off(og_audio *a, int fanfare)
{
    if (a) a->arc_live = 0;
    if (!og_arc_on(a) || !a->arc_race) return;
    og_arc_send(a, 0x03, 0x00);
    if (fanfare) og_arc_fx(a, 0x12);
    a->arc_race = 0;
}

/* The engines (0xc5fd): each truck's every 6th update of 30 a second,
 * 0.2 s, so every 4th of SORP's 20 ticks: 0x1c + truck, with the high
 * byte of its speed along its nose, 0 going backwards.  The arcade moves
 * a truck 2 x that byte (bank 4's table at 0x2000, a * b / 128) 30 times
 * a second where DOS moves it [di+0x85] 20 times, so the byte is DOS's
 * speed / 3. */
static void og_arc_engines(og_audio *a, const og_racer *R, int ticks)
{
    int k, t;
    if (!og_arc_on(a) || !a->arc_race || !(audio_mode & OG_AUDIO_SFX)) return;
    for (k = 0; k < ticks; ++k)
        for (t = 0; t < 3; ++t) {
            float slip = R[t].head - R[t].trav;
            int v;
            if (--a->arc_eng[t] > 0) continue;
            a->arc_eng[t] = 4;
            while (slip < 0.0f)    slip += 256.0f;
            while (slip >= 256.0f) slip -= 256.0f;
            v = (slip >= 64.0f && slip < 192.0f) ? 0 : (int)(R[t].speed / 3.0f);
            og_arc_send(a, 0x1c + t, v > 255 ? 255 : v < 0 ? 0 : v);
        }
}

/* the race paused (THE PAUSE): the engines stop and start again */
static void og_arc_pause(og_audio *a, const og_racer *R, int paused)
{
    if (paused) {
        if (og_arc_on(a) && a->arc_race) og_arc_send(a, 0x03, 0x00);
    } else if (og_arc_on(a) && a->arc_race) {
        og_arc_race_on(a, R);
    }
}

static int og_arc_tune(int track);     /* with the arcade sets, below */
static int og_arc_anthem(int country);
/* The winner's nation as the podium found it: DOS's songs stop at 10 (the
 * race tunes follow), so SORP's 9-16 ask for song 10, the computer
 * winner's (the user's choice), and the arcade's board looks the nation
 * up here */
static int og_anthem_nation = OG_COUNTRY_NONE;

static og_music og_music_state;
static int      og_music_song = 0;   /* what should be playing          */

/* Start (or restart) the song for the current track: image 0x306 plays
 * track + 11 while racing. */
static void og_music_select(og_audio *a, int song)
{
    if (!a->ready) return;
    if (song != og_music_song) {
        char what[32];
        if (song == OG_MUSIC_SHOP)
            SDL_strlcpy(what, "the menus' tune", sizeof what);
        else if (song >= OG_MUSIC_TRACK1 && song < OG_MUSIC_TRACK1 + 8)
            SDL_snprintf(what, sizeof what, "track %d's tune", song - OG_MUSIC_TRACK1);
        else if (song >= OG_MUSIC_ANTHEM && song < OG_MUSIC_TRACK1)
            SDL_snprintf(what, sizeof what, "country %d's anthem", song - OG_MUSIC_ANTHEM);
        else
            SDL_strlcpy(what, "the last song, with no id", sizeof what);
        OG_DEBUG("[OG_Audio] Song %d, %s%s", song, what,
                 (audio_mode & OG_AUDIO_MUSIC) ? "" : " (music is off)");
    }
    og_music_song = song;
    og_snd_lock(a);
    if (og_arc_on(a)) {
        /* The arcade's: 0x5e the menu song (title, registration, shop,
         * and the club and attract shop, where DOS is silent -- the
         * user's choice), a race tune from the master's table, and on
         * the podium the winner's anthem -- a player's nation's, from
         * the master's table, or for a computer truck, which has none,
         * the jingle 0x56.  A song starts only with none playing, so
         * 0x51 stops the last first; the same song asked for again plays
         * on, as the arcade's does. */
        const int cmd = song == OG_MUSIC_SHOP ? 0x5e
                      : song >= OG_MUSIC_TRACK1 && song < OG_MUSIC_TRACK1 + 8
                            ? og_arc_tune(song - OG_MUSIC_TRACK1)
                      : song >= OG_MUSIC_ANTHEM && song < OG_MUSIC_TRACK1
                            ? og_arc_anthem(song - OG_MUSIC_ANTHEM == OG_COUNTRY_NONE
                                            ? og_anthem_nation : song - OG_MUSIC_ANTHEM) : -1;
        og_music_stop(&a->sfx, &og_music_state);
        if (!(audio_mode & OG_AUDIO_MUSIC)) {
            if (a->arc_song) og_arc_send(a, 0x51, 0x00);
            a->arc_song = 0;
        } else if (cmd > 0 && cmd != a->arc_song) {
            og_arc_send(a, 0x51, 0x00);
            og_arc_send(a, cmd, 0x00);
            a->arc_song = cmd;
        }
    } else if (audio_mode & OG_AUDIO_MUSIC) og_music_start(&a->sfx, &og_music_state, song);
    else                             og_music_stop (&a->sfx, &og_music_state);
    og_snd_unlock(a);
}

/* end the song, and forget it */
static void og_music_end(og_audio *a)
{
    og_snd_lock(a);
    og_music_stop(&a->sfx, &og_music_state);
    if (og_arc_on(a) && a->arc_song) og_arc_send(a, 0x51, 0x00);
    a->arc_song = 0;
    og_snd_unlock(a);
    og_music_song = 0;
}

/* Cut an effect short.  No effect in ADLIB.SFX holds -- each record's
 * duration is finite -- so this is never needed to end a sound. */
static void og_audio_stop(og_audio *a, int effect)
{
    int v;
    if (!a->ready || og_arc_on(a)) return;
    og_snd_lock(a);
    for (v = 0; v < OG_VOICES; ++v)
        if (a->voice[v].effect == effect) {
            og_sound_effect_stop(&a->sfx, v % 3);
            a->voice[v].active = 0;
        }
    og_snd_unlock(a);
}

static void og_audio_trigger_locked(og_audio *a, int effect);

static void og_audio_trigger(og_audio *a, int effect)
{
    /* the board plays the arcade's effects instead, sent where they
     * happen (og_arc_fx) */
    if (!a->ready || og_arc_on(a)) return;
    og_snd_lock(a);
    og_audio_trigger_locked(a, effect);
    og_snd_unlock(a);
}

static void og_audio_trigger_locked(og_audio *a, int effect)
{
    static char warned[OG_NUM_EFFECTS];
    int v;
    if (!a->ready || effect < 0 || effect >= a->sfx.neffects) return;
    if (audio_mode == OG_AUDIO_SPK) { og_pcspk_trigger(&pcspk, effect); return; }
    if (!(audio_mode & OG_AUDIO_SFX)) return;

    /* round-robin the voices so overlapping effects do not cut */
    v = a->next_voice;
    a->next_voice = (a->next_voice + 1) % OG_VOICES;

    /* The AdLib sound: record 0x20 + n of ADLIB.SFX, with the pitch and
     * the two voices the record itself carries.  The driver's own enable
     * table silences effects 0, 7 and 8, so those make no sound here
     * either -- they appear to exist only for the PC speaker. */
    if (og_sound_effect_enabled(&a->sfx, effect) || og_force_effects) {
        /* An effect occupies channel and channel+3, so it must start in
         * 0..2 -- 3..5 would pair into the rhythm channels.  Music and
         * effects therefore SHARE 0..2 by design, and the priority
         * compare at 0x4220 is what arbitrates between them; rhythm mode
         * is permanently on and owns 6..8. */
        int ch = v % 3;
        /* An effect takes channels the MUSIC also uses (0..2 and their
         * pair at 3..5), so release whatever is sounding there FIRST:
         * both paths keep their own countdown for a channel, and leaving
         * the music's behind is what hangs a note when effects and music
         * play together. */
        og_sound_effect_stop(&a->sfx, ch);
        og_sound_effect(&a->sfx, effect, ch);
        a->voice[v].active = 0;          /* not the speaker sequencer */
        OG_TRACE("[OG_Audio] Effect %d on channel %d", effect, ch);
    } else if (!warned[effect]) {
        /* The driver's own table at DS:0xb2b7 is zero for effects 0, 7
         * and 8, and nothing in the executable ever writes it, so the
         * AdLib driver really does drop them -- the PC speaker driver,
         * which uses its own table, still plays them.  "forced effects" in the
         * F2 overlay plays them anyway. */
        warned[effect] = 1;
        OG_TRACE("[OG_Audio] Effect %d is silent on AdLib (F2: forced effects plays it)", effect);
    }
}

static void og_audio_silence(og_audio *a)
{
    int v;
    if (!a->ready) return;
    og_snd_lock(a);
    og_sound_effect_silence(&a->sfx);
    for (v = 0; v < OG_VOICES; ++v) a->voice[v].active = 0;
    og_sound_reset(&a->sfx);
    og_snd_unlock(a);
}

/* Keep roughly a tenth of a second queued, stepping the sequencer once
 * per chunk so effect timing follows the audio clock rather than the
 * frame rate. */
/* The driver ticks at the video rate; a voice keyed on with a duration
 * is released when it runs out (0x40f5), which is what stops one-shot
 * effects.  Every effect's duration is finite. */
static double og_snd_hz = OG_SND_HZ_DEFAULT;

/* one tick of the driver: the effects, the speaker, the song */
static void og_audio_step(og_audio *a)
{
    og_sound_effect_tick(&a->sfx);
    if (audio_mode == OG_AUDIO_SPK)  og_pcspk_tick(&pcspk);
    if (audio_mode & OG_AUDIO_MUSIC) og_music_tick(&a->sfx, &og_music_state);
}

/* on the main thread, ticked by the frame's time (SORP_AUDIO_THREAD=0);
 * the audio thread ticks by the samples it renders instead */
static void og_audio_tick(og_audio *a, float dt)
{
    static double acc = 0.0;
    if (!a->ready || a->threaded) return;
    acc += dt * og_snd_hz;
    while (acc >= 1.0) {
        og_audio_step(a);
        acc -= 1.0;
    }
}

/* The level meters: one bar for each of the nine OPL channels -- 0..2
 * the music's three streams (cyan), 3..5 the second voice of a note or
 * an effect (yellow), 6..8 the rhythm voices (red) -- and one for the
 * PC speaker (green).  A muted channel is grey with a red frame.
 * `labels` writes the channel numbers under the bars.
 *
 * While the arcade's sound board plays, its DACs take the OPL's place
 * (og_arc_meter_hook): 0 and 1, fed by DMA (cyan); 2 to 5, which its
 * program polls (yellow); and the 10-bit DAC (red, "10"). */
static float og_spk_level;
static void og_draw_arc_meters(SDL_Renderer *ren, int x0, int y0, int h, int labels)
{
    int d;
    for (d = 0; d < OG_ARC_METERS; ++d) {
        SDL_FRect bg, lv;
        const float lvl = og_arc_level[d];
        bg.x = (float)(x0 + d * 14); bg.y = (float)y0;
        bg.w = 10.0f; bg.h = (float)h;
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_RenderFillRect(ren, &bg);
        if (lvl > 0.0f) {
            lv.x = bg.x + 1.0f; lv.w = 8.0f;
            lv.h = lvl * (float)(h - 2);
            lv.y = bg.y + (float)(h - 1) - lv.h;
            if (d < 2)       SDL_SetRenderDrawColor(ren, 80, 220, 255, 255);
            else if (d < 6)  SDL_SetRenderDrawColor(ren, 255, 210, 60, 255);
            else             SDL_SetRenderDrawColor(ren, 255, 110, 110, 255);
            SDL_RenderFillRect(ren, &lv);
        }
        SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
        SDL_RenderRect(ren, &bg);
        if (labels) {
            char n[4];
            SDL_snprintf(n, sizeof n, d < 6 ? "%d" : "10", d);
            SDL_SetRenderDrawColor(ren, 170, 170, 170, 255);
            SDL_RenderDebugText(ren, bg.x + (d < 6 ? 1.0f : -3.0f),
                                bg.y + (float)h + 3.0f, n);
        }
    }
    if (labels) {
        SDL_SetRenderDrawColor(ren, 170, 170, 170, 255);
        SDL_RenderDebugText(ren, (float)(x0 + OG_ARC_METERS * 14 + 4),
                            (float)(y0 + h + 3), "board DACs");
    }
}

static void og_draw_meters(SDL_Renderer *ren, int x0, int y0, int h, int labels)
{
    int ch;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (og_arc_metering) {
        og_draw_arc_meters(ren, x0, y0, h, labels);
        return;
    }
    for (ch = 0; ch < 10; ++ch) {
        SDL_FRect bg, lv;
        int spk = (ch == 9);
        float lvl = spk ? og_spk_level : og_ch_level[ch];
        int muted = !spk && og_ch_muted[ch];
        bg.x = (float)(x0 + ch * 14 + (spk ? 10 : 0)); bg.y = (float)y0;
        bg.w = 10.0f; bg.h = (float)h;
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_RenderFillRect(ren, &bg);
        if (lvl > 0.0f) {
            lv.x = bg.x + 1.0f; lv.w = 8.0f;
            lv.h = lvl * (float)(h - 2);
            lv.y = bg.y + (float)(h - 1) - lv.h;
            if (muted)        SDL_SetRenderDrawColor(ren, 90, 90, 90, 255);
            else if (spk)     SDL_SetRenderDrawColor(ren, 110, 230, 110, 255);
            else if (ch < 3)  SDL_SetRenderDrawColor(ren, 80, 220, 255, 255);
            else if (ch < 6)  SDL_SetRenderDrawColor(ren, 255, 210, 60, 255);
            else              SDL_SetRenderDrawColor(ren, 255, 110, 110, 255);
            SDL_RenderFillRect(ren, &lv);
        }
        SDL_SetRenderDrawColor(ren, muted ? 255 : 110, muted ? 60 : 110,
                               muted ? 60 : 110, 255);
        SDL_RenderRect(ren, &bg);
        if (labels) {
            char n[4];
            SDL_snprintf(n, sizeof n, spk ? "PC" : "%d", ch);
            SDL_SetRenderDrawColor(ren, 170, 170, 170, 255);
            SDL_RenderDebugText(ren, bg.x + (spk ? -3.0f : 1.0f),
                                bg.y + (float)h + 3.0f, n);
        }
    }
}

/* ------------------------------------------------------------------ */
/* The audio overlay's actions (F2)                                     */
/* ------------------------------------------------------------------ */

static og_audio *og_audio_p;          /* main's, set once it is open */

/* The sound mode changed: stop what the new mode does not play, and
 * restart the song if music is back on. */
static void og_ovl_audio_mode(void)
{
    og_snd_lock(og_audio_p);
    og_pcspk_silence(&pcspk);
    if (og_audio_p) {
        og_audio *a = og_audio_p;
        const int arc = og_arc_on(a);
        if (!(audio_mode & OG_AUDIO_SFX)) og_audio_silence(a);
        /* The arcade's board: what the mode does not play stops there --
         * in any mode, since the PC speaker and silent do not render it
         * and would leave it to carry on later from where it was.  The
         * engine starts again with effects back on during a race. */
        if (a->arc) {
            if ((!arc || !(audio_mode & OG_AUDIO_SFX)) && a->arc_race) {
                og_arc_queue(a, 0x03, 0x00);
                a->arc_race = 0;
            } else if (arc && (audio_mode & OG_AUDIO_SFX) && a->arc_live && !a->arc_race)
                og_arc_race_on(a, NULL);
            if ((!arc || !(audio_mode & OG_AUDIO_MUSIC)) && a->arc_song) {
                og_arc_queue(a, 0x51, 0x00);
                a->arc_song = 0;
            }
        }
        if (audio_mode & OG_AUDIO_MUSIC) {
            if (og_music_song) og_music_select(a, og_music_song);
        } else
            og_music_stop(&a->sfx, &og_music_state);
    }
    og_snd_unlock(og_audio_p);
    if (audio_mode) og_sound_sel = audio_mode - 1;
    OG_INFO("[OG_Audio] %s", og_audio_mode_name(audio_mode));
}

/* F1's "sound" changed */
static void og_ovl_sound_sel(void)
{
    audio_mode = og_sound_sel + 1;
    og_ovl_audio_mode();
}

/* the driver's tick rate, for comparison.  The original's is the
 * screen's refresh: its INT 8 handler (0x4d3a) is re-armed each tick
 * and waits for the retrace, the divisor measured over one frame at
 * start-up (0x4c32: 70 Hz under ~65, 60 under ~56, else 50), and the
 * sound driver is chained on it (0x39d6 -> 0x3a08).  On VGA, 70 Hz. */
static void og_ovl_tick_rate(void)
{
    og_snd_lock(og_audio_p);
    og_snd_hz = (double)og_tick_rate[og_tick_i];
    og_snd_unlock(og_audio_p);
    OG_DEBUG("[OG_Audio] Sound tick %.0f Hz -- a 16-tick effect lasts %.0f ms",
            og_snd_hz, 16000.0 / og_snd_hz);
}

static void og_ovl_silence(void)
{
    og_snd_lock(og_audio_p);
    og_pcspk_silence(&pcspk);
    if (og_audio_p) og_audio_silence(og_audio_p);
    og_snd_unlock(og_audio_p);
}

static void og_ovl_play_fx(void)
{
    if (og_audio_p) og_audio_trigger(og_audio_p, og_test_fx);
}

static void og_ovl_play_song(void)
{
    if (og_audio_p) og_music_select(og_audio_p, og_test_song + 1);
}

static void og_ovl_stop_song(void)
{
    if (og_audio_p) og_music_end(og_audio_p);
}

/* the note on channel 0 in the chosen instrument -- the music's own
 * note-on, so it carries the instrument's duration and stops itself */
static void og_ovl_play_note(void)
{
    if (!og_audio_p || !og_audio_p->ready) return;
    og_snd_lock(og_audio_p);
    og_sound_note(&og_audio_p->sfx, 0, og_test_ins, og_test_note);
    og_snd_unlock(og_audio_p);
}

/* a key-on dropped while muted leaves its voice sounding, and one
 * dropped while unmuting never releases -- clear everything either way */
static void og_ovl_mute(void)
{
    og_ch_muted[og_test_ch] = !og_ch_muted[og_test_ch];
    og_ovl_silence();
}

static float og_volume_gain(void)
{
    float g = (float)(og_volume * OG_VOLUME_STEP) / 100.0f;
    return g * g;
}

static void og_ovl_track(void)
{
    og_req_track   = og_dbg_track;
    og_req_reverse = og_dbg_reverse;
    og_req_tp      = -1;
    og_race_req    = 1;
}

static void og_ovl_shader(void)
{
    ++og_shade_gen;                 /* everything made under the old one */
}

static void og_ovl_volume(void)
{
    if (og_audio_p && og_audio_p->stream)
        SDL_SetAudioStreamGain(og_audio_p->stream, og_volume_gain());
}

/* nothing as text: the overlay draws the bar and the figure */
static const char *og_ovl_show_volume(int v)
{
    (void)v;
    return "";
}

static const char *og_ovl_show_rate(int v)
{
    static char b[16];
    SDL_snprintf(b, sizeof b, "%d Hz", og_tick_rate[v]);
    return b;
}

static const char *og_ovl_show_fx(int v)
{
    static char b[40];
    int on = og_audio_p && og_audio_p->ready
           ? og_sound_effect_enabled(&og_audio_p->sfx, v) : 1;
    SDL_snprintf(b, sizeof b, "%2d%s", v, on ? "" : "  (AdLib drops it)");
    return b;
}

static const char *og_ovl_show_song(int v)
{
    static char b[32];
    int n = v + 1;
    SDL_snprintf(b, sizeof b, "%2d  %s", n,
                 n == OG_MUSIC_SHOP ? "menu" :
                 n < OG_MUSIC_TRACK1 ? "anthem" :
                 n < OG_MUSIC_TRACK1 + 8 ? "track" : "");
    return b;
}

static const char *og_ovl_show_ins(int v)
{
    static char b[24];
    int used = og_audio_p && og_audio_p->ready && og_audio_p->sfx.patch[v].used;
    SDL_snprintf(b, sizeof b, "%2d%s", v, used ? "" : "  (empty)");
    return b;
}

static const char *og_ovl_show_note(int v)
{
    static const char *nm[12] = { "C", "C#", "D", "D#", "E", "F",
                                  "F#", "G", "G#", "A", "A#", "B" };
    static char b[24];
    SDL_snprintf(b, sizeof b, "%2d  %s%d", v, nm[v % 12], v / 12);
    return b;
}

static const char *og_ovl_show_ch(int v)
{
    static char b[24];
    SDL_snprintf(b, sizeof b, "%d  %s", v, og_ch_muted[v] ? "muted" : "playing");
    return b;
}

static const char *og_ovl_show_act(int v)
{
    (void)v;
    return "(button)";
}

/* Full screen, from F1's row or F11.  The window says what it became --
 * a handheld's KMSDRM window is full screen whatever is asked -- and its
 * events keep the row in step afterwards.  The framebuffer has no window
 * to change: it is always the whole screen. */
static void og_ovl_fullscreen(void)
{
    if (!og_window) { og_fullscreen = 1; return; }
    SDL_SetWindowFullscreen(og_window, og_fullscreen ? true : false);
    SDL_SyncWindow(og_window);
    og_fullscreen = (SDL_GetWindowFlags(og_window) & SDL_WINDOW_FULLSCREEN) ? 1 : 0;
    OG_INFO("[OG_Video] Full screen %s", og_fullscreen ? "on" : "off");
}

static const char *og_ovl_show_full(int v)
{
    return !og_window ? "always" : v ? "on" : "off";
}

/* F1's build stamp: "About" comes up over the overlay */
static void og_ovl_about(void)
{
    og_about = og_about_held = 1;
}

/* when this executable was built -- sorp.c's compile time, __DATE__
 * "Mmm dd yyyy" made yyyy-mm-dd.  It is the compiler's local time, which
 * every build makes UTC: the Docker containers run on it, and the Windows
 * task gives cl.exe TZ=UTC0 (.vscode/tasks.json). */
static const char *og_build_stamp(void)
{
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    static const char d[] = __DATE__, tm[] = __TIME__;
    static char stamp[32];
    int m = 0;
    while (m < 12 && SDL_strncmp(mon + 3 * m, d, 3)) ++m;
    SDL_snprintf(stamp, sizeof stamp, "build %.4s-%02d-%c%c %s",
                 d + 7, m + 1, d[4] == ' ' ? '0' : d[4], d[5], tm);
    return stamp;
}

/* text centred in lines of at most `cols` characters, broken at spaces;
 * the y below it */
static float og_draw_wrapped(SDL_Renderer *ren, const char *s, float y, int cols)
{
    char line[64];
    if (cols > (int)sizeof line - 1) cols = (int)sizeof line - 1;
    while (*s) {
        int n = (int)SDL_strlen(s), cut = n;
        if (n > cols) {
            cut = cols;
            while (cut > 0 && s[cut] != ' ') --cut;
            if (cut == 0) cut = cols;          /* one word too long: split it */
        }
        SDL_memcpy(line, s, (size_t)cut);
        line[cut] = '\0';
        SDL_RenderDebugText(ren, (float)((OG_SCREEN_W - cut * 8) / 2), y, line);
        y += 10.0f;
        s += cut;
        while (*s == ' ') ++s;
    }
    return y;
}

/* F1's "About", a panel over the overlay */
static void og_draw_about(SDL_Renderer *ren)
{
    static const char *const title = "SORP: Super Off Road Port";
    static const char *const what  = "A 2026 C port of the 1990 DOS port of the 1989 "
                                     "arcade game Ivan Ironman Stewart's Super Off Road.";
    static const char *const who   = "Created by dopaz & Claude, mostly by saying "
                                     "to Claude: 'Continue'";
    static const char *const visit = "Press Start or Enter to visit:";
    const int cols = 36;
    SDL_FRect bg;
    float y;
    bg.x = 8.0f;
    bg.y = 28.0f;
    bg.w = (float)(OG_SCREEN_W - 16);
    bg.h = (float)(OG_SCREEN_H - 56);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 240);
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
    SDL_RenderRect(ren, &bg);
    y = bg.y + 12.0f;
    SDL_SetRenderDrawColor(ren, 255, 210, 60, 255);
    y = og_draw_wrapped(ren, title, y, cols) + 8.0f;
    SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
    y = og_draw_wrapped(ren, what, y, cols) + 8.0f;
    y = og_draw_wrapped(ren, who, y, cols) + 8.0f;
    SDL_SetRenderDrawColor(ren, 160, 160, 160, 255);
    y = og_draw_wrapped(ren, visit, y, cols);
    SDL_SetRenderDrawColor(ren, 90, 200, 255, 255);
    y = og_draw_wrapped(ren, OG_ABOUT_URL, y, cols);
    SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
    og_draw_wrapped(ren, og_build_stamp(), bg.y + bg.h - 16.0f, cols);
    (void)y;
}

/* F1's "Exit SORP": the overlay goes and "Exit SORP?" asks, as ESC does */
static void og_ovl_exit(void)
{
    og_options   = 0;
    og_ovls[1].row = 0;           /* F1 opens on its first row next time */
    og_quit_ask  = 1;
    og_quit_held = 1;
}

static const char *og_ovl_show_none(int v)
{
    (void)v;
    return "";
}

static void og_ovl_next_stage(void)
{
    og_next_req = 1;
}

/* the stage the button would end */
static const char *og_ovl_show_stage(int v)
{
    (void)v;
    return og_next_nm;
}

/* n samples (at most OG_AUDIO_CHUNK_MAX) into a->chunk */
static void og_audio_render(og_audio *a, int n)
{
    int i;
    if (og_arc_on(a)) {
        og_arc_render(a, a->chunk, n);
    } else if (audio_mode == OG_AUDIO_SPK) {
        /* the speaker instead of the chip: one square wave, the PIT
         * divisor from the effect's current note */
        static short spk[OG_AUDIO_CHUNK_MAX];
        pcspk.rate = OG_AUDIO_RATE;
        og_pcspk_render(&pcspk, spk, n);
        for (i = 0; i < n; ++i) a->chunk[i] = spk[i] / 32768.0f;
    } else {
        /* Nuked renders interleaved stereo 16-bit; the port wants
         * mono floats, so take the left channel and scale. */
        static short nk[OG_AUDIO_CHUNK_MAX * 2];
        OPL3_GenerateStream(&a->chip, nk, (unsigned)n);
        for (i = 0; i < n; ++i) a->chunk[i] = nk[i * 2] / 32768.0f;
    }
}

/* SDL's audio thread, the stream locked: render what the device asks for,
 * ticking the driver every OG_AUDIO_RATE / og_snd_hz samples -- so the
 * audio clock paces the sound, not the frame rate, and the main thread
 * renders nothing. */
static void SDLCALL og_audio_feed(void *user, SDL_AudioStream *st,
                                  int additional, int total)
{
    og_audio *a = (og_audio *)user;
    int want = (additional + (int)sizeof(float) - 1) / (int)sizeof(float);
    (void)total;
    if (!a->ready) return;
    while (want > 0) {
        int n;
        if (a->tick_due < 1.0) {
            og_audio_step(a);
            a->tick_due += (double)OG_AUDIO_RATE / og_snd_hz;
        }
        n = (int)a->tick_due;
        if (n > want) n = want;
        if (n > OG_AUDIO_CHUNK_MAX) n = OG_AUDIO_CHUNK_MAX;
        og_audio_render(a, n);
        SDL_PutAudioStreamData(st, a->chunk, n * (int)sizeof(float));
        a->tick_due -= n;
        want -= n;
    }
}

/* on the main thread (SORP_AUDIO_THREAD=0): keep roughly a tenth of a
 * second queued */
static void og_audio_pump(og_audio *a)
{
    const int target = (int)(OG_AUDIO_RATE * sizeof(float)) / 10;
    int guard = 0;

    if (!a->ready || a->threaded) return;

    while (SDL_GetAudioStreamQueued(a->stream) < target && guard++ < 64) {
        int v;
        for (v = 0; v < OG_VOICES; ++v)
            if (a->voice[v].active)
                og_sound_step(&a->sfx, &a->voice[v]);
        og_audio_render(a, a->chunk_len);
        SDL_PutAudioStreamData(a->stream, a->chunk,
                               a->chunk_len * (int)sizeof(float));
    }
}

/* ------------------------------------------------------------------ */
/* Per-track state                                                     */
/* ------------------------------------------------------------------ */

static og_surface g_surface;
static og_slopes  slopes;      /* the per-layout climbable-slope cap */
static og_fontfile roadfont;   /* the scoreboard's lettering          */
static og_hud      hud;        /* its 19 cells and the per-track origin */
static og_pages    pages;      /* GAME.EXE, for the title's text pages  */
static og_fontfile ironfont;   /* what those pages are written in       */
static og_bank     titlebank;  /* ETITLE: the logos and headings        */
static og_styles   styles;     /* the driver's colour rows              */
static og_hiscore  club;       /* IRONHI.BIN, the Ironman Club          */
static char        og_club_path[1024];   /* SORP's copy, beside sorp.exe */
static int         og_club_ask = 0;      /* "Save HiScore table (Y/N)"   */

/* ------------------------------------------------------------------ */
/* The arcade sets                                                     */
/* ------------------------------------------------------------------ */

/* F1's "arcade roms" races on the arcade game's tracks, read from the
 * player's own ROMs in gamedata/trackpak (tp.h) -- its art, and
 * with it its terrain, routes, gates, starts, pickup spots, flagman,
 * pennants and scoreboard -- and "arcade mode" chooses which:
 *
 *   Original        the eight tracks the DOS game has, as the arcade has
 *                   them (offroad), in GAME.EXE's race order
 *   Track-Pak       the Track-Pak's eight new ones (offroadt), in its own
 *                   race order
 *   All 16 Tracks   both: the two orders taken a race from each in turn
 *
 * Both rows are shown only when gamedata/trackpak holds a set, and a mode
 * whose set is not there falls back to one that is.  Everything else --
 * the trucks, their physics and AI, the shop, the podium, the sound -- is
 * the DOS game's as ever.
 *
 * A change takes effect at the next race that loads: og_tp_sync, called
 * before each, opens the sets the mode needs or puts the DOS game's own
 * tables back, and og_tp_use picks the set of the race about to load.
 * While a set is in use the race's POLY and QUAD are its track's own and
 * the slope caps are open (the arcade has none).  The Track-Pak's tracks
 * keep their own Ironman Club, IRONHITP.BIN, so their records are not
 * mixed with the original tracks': in "Track-Pak" it is THE club, and in
 * "All 16 Tracks" its races' records go there while the players and the
 * original tracks stay in IRONHI.BIN. */

static const char *og_dir;                 /* the gamedata folder           */
static tp_set     *og_tp_sets[TP_KINDS];   /* the sets open                 */
static tp_set     *og_tp;                  /* the race's set                */
static int         og_tp_kind = -1;        /* ...TP_TRACKPAK or TP_ARCADE   */
static int         og_tp_mode = -1;        /* the mode the sets are open for */
static tp_art      og_tp_art;              /* the loaded track's art        */
static og_surface  og_tp_surface;          /* ...and its POLY and QUAD      */
static og_surface  og_dos_surface;         /* GAME's, put back after        */
static og_slopes   og_dos_slopes;
static unsigned short og_dos_hud_ox[8], og_dos_hud_oy[8];
static unsigned char og_tp_order[TP_SCHEDULE_MAX];
static int           og_tp_norder;
static og_hiscore    club_tp;              /* "All 16": the Track-Pak's club */
static char          og_club_tp_path[1024];
static int           og_club_tp_ok;

/* A layout in the race order, with the set it is from: bit 4 marks the
 * Track-Pak's (only "All 16 Tracks" needs it, but every mode carries it). */
#define OG_LAY_TP 0x10

static const char *const og_tp_kind_nm[TP_KINDS] = { "Track-Pak", "arcade" };

/* The Track-Pak's tracks by number, as the arcade names them: each
 * track's name is a sprite in the video CPU's ROM (descriptor 0x40A4 +
 * 16 * track, bank 4; drawn at 0x4C3A from the table at 0x4ED0). */
static const char *const og_tp_track_nm[] = {
    "Cuttoff Pass", "Boulder Hill", "Redoubt About", "Rio Trio",
    "Leapin' Lizards", "Shortcut", "Pig Bog", "Volcano Valley"
};

/* The arcade set's numbering for SORP's track and layout, which are
 * GAME.EXE's: the base game numbers some the other way (tp.h). */
static int og_tp_t(int track)
{
    return og_tp_kind == TP_ARCADE ? TP_DOS_TRACK(track) : track;
}

static int og_tp_l(int layout)
{
    return og_tp_kind == TP_ARCADE ? tp_layout_from_dos[layout & 15] : layout;
}

/* The DOS scoreboard is painted into each DOS track's art; the arcade
 * draws its own as a sprite, so its art has none.  The board is taken
 * from the player's DOS tracks (the pixels two of them agree on, inside
 * its 71x31 frame) and painted in where the arcade puts its own
 * (tp_board_at). */
#define OG_BOARD_W 71
#define OG_BOARD_H 31
static unsigned char og_tp_board[OG_BOARD_W * OG_BOARD_H];   /* index+1, 0 = none */

static void og_tp_board_build(const char *dir)
{
    og_track_art a, b;
    int x, y, n = 0;
    memset(og_tp_board, 0, sizeof og_tp_board);
    if (og_track_load(dir, 3, &a) != OG_OK) return;
    if (og_track_load(dir, 5, &b) != OG_OK) { og_track_free(&a); return; }
    for (y = 0; y < OG_BOARD_H; ++y)
        for (x = 0; x < OG_BOARD_W; ++x) {
            int ax = og_dos_hud_ox[3] + x, ay = og_dos_hud_oy[3] + y;
            int bx = og_dos_hud_ox[5] + x, by = og_dos_hud_oy[5] + y;
            unsigned va, vb;
            if (ax >= OG_SCREEN_W || ay >= OG_SCREEN_H || bx >= OG_SCREEN_W || by >= OG_SCREEN_H)
                continue;
            va = a.pic.px[ay * OG_SCREEN_W + ax] & 15;
            vb = b.pic.px[by * OG_SCREEN_W + bx] & 15;
            if (va == vb) { og_tp_board[y * OG_BOARD_W + x] = (unsigned char)(va + 1); ++n; }
        }
    og_track_free(&a);
    og_track_free(&b);
    OG_DEBUG("[OG_TrackPak] The scoreboard taken from TRACK3.XX and TRACK5.XX: %d pixels", n);
}

/* a palette's nearest colour */
static int og_nearest(const unsigned char *pal, int n, const unsigned char *c)
{
    int i, best = 0;
    long bd = -1;
    for (i = 0; i < n; ++i) {
        long dr = c[0] - pal[i * 3], dg = c[1] - pal[i * 3 + 1], db = c[2] - pal[i * 3 + 2];
        long d = dr * dr + dg * dg + db * db;
        if (bd < 0 || d < bd) { bd = d; best = i; }
    }
    return best;
}

/* START NEXT RACE's picture for a Track-Pak track.  The DOS ones (ESHOP
 * 37..52) are maps of the course alone -- the rails and what is inside
 * them, filling 41x20 of a 42x22 picture on grey, the box's teal edge to
 * its right -- so this is the course's box on the screen (tp_course_box)
 * scaled into the same 41x20, by area, in the shop's palette, drawn over
 * the DOS picture so the box's own edge stays. */
#define OG_THUMB_W 42
#define OG_THUMB_H 22
static void og_tp_thumbs_build(void)
{
    static tp_art a;
    static const unsigned char grey[3] = { 81, 81, 81 };
    const int MW = 41, MH = 20;
    const int bg = og_nearest(og_pal_shop, 16, grey);
    int t, x, y, box[4];
    for (t = 0; t < TP_TRACKS; ++t) {
        og_image_free(&og_tp_thumb[t]);
        if (tp_art_load(og_tp, t, &a) != OG_OK) continue;
        if (tp_course_box(og_tp, t, box) != OG_OK) {
            box[0] = 0; box[1] = 0; box[2] = TP_ART_W - 1; box[3] = TP_ART_H - 1;
        }
        if (og_image_init(&og_tp_thumb[t], OG_THUMB_W, OG_THUMB_H) != OG_OK) continue;
        memset(og_tp_thumb[t].px, bg, OG_THUMB_W * OG_THUMB_H);
        for (y = 0; y < MH; ++y)
            for (x = 0; x < MW; ++x) {
                const int bw = box[2] - box[0] + 1, bh = box[3] - box[1] + 1;
                int x0 = box[0] + x * bw / MW, x1 = box[0] + (x + 1) * bw / MW;
                int y0 = box[1] + y * bh / MH, y1 = box[1] + (y + 1) * bh / MH, sx, sy, k;
                long acc[3] = { 0, 0, 0 }, n = 0;
                unsigned char c[3];
                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;
                for (sy = y0; sy < y1; ++sy)
                    for (sx = x0; sx < x1; ++sx, ++n)
                        for (k = 0; k < 3; ++k) acc[k] += a.rgb[(sy * TP_ART_W + sx) * 3 + k];
                for (k = 0; k < 3; ++k) c[k] = (unsigned char)(n ? acc[k] / n : 0);
                og_tp_thumb[t].px[(y + 1) * OG_THUMB_W + x] =
                    (unsigned char)og_nearest(og_pal_shop, 16, c);
            }
    }
}

/* The club file for the tracks in use: IRONHI.BIN beside sorp.exe, or
 * IRONHITP.BIN for the Track-Pak.  SORP's own copy if there is one; the
 * game's from gamedata if not; and failing both, the table GAME.EXE
 * carries in its data segment, as the original falls back on.  With no
 * copy of its own yet, SORP makes one now, so the game's end saves the
 * table (0x134 writes only a file that is already there) without anyone
 * pressing Ctrl+S on the club screen: gamedata's IRONHI.BIN copied byte
 * for byte, or, without one, the default table written out as 0x3669
 * writes it.  An existing copy is never touched -- it holds this
 * machine's scores. */
#define OG_HISCORE_TP_FILE "IRONHITP.BIN"
static int og_club_open(const char *dir)
{
    const char *base = SDL_GetBasePath();
    int rc;
    SDL_snprintf(og_club_path, sizeof og_club_path, "%s%s", base ? base : "",
                 og_tp_live && og_tp_mode == OG_ARCADE_TRACKPAK ? OG_HISCORE_TP_FILE
                                                                : OG_HISCORE_FILE);
    rc = og_hiscore_open(og_club_path, dir, &club);
    if (rc != OG_OK) return rc;
    OG_INFO("[OG_Club] %d entries, top $%ld, from %s", club.n,
            club.money[0],
            club.source == OG_CLUB_FROM_SORP ? og_rel(og_club_path) :
            club.source == OG_CLUB_FROM_GAME ? "gamedata" :
            "GAME.EXE's own default table");
    if (club.source == OG_CLUB_FROM_GAME) {
        char gp[1024];
        unsigned char *buf = NULL;
        size_t len = 0;
        int rc2 = og_resolve(dir, OG_HISCORE_FILE, gp, sizeof gp);
        if (rc2 == OG_OK) rc2 = og_read_file(gp, &buf, &len);
        if (rc2 == OG_OK) {
            FILE *f = fopen(og_club_path, "wb");
            rc2 = (f && fwrite(buf, 1, len, f) == len) ? OG_OK : OG_E_IO;
            if (f && fclose(f) != 0) rc2 = OG_E_IO;
        }
        free(buf);
        OG_INFO("[OG_Club] Gamedata's %s copied to %s: %s", OG_HISCORE_FILE,
                og_rel(og_club_path), og_strerror(rc2));
    } else if (club.source == OG_CLUB_FROM_DEFAULT) {
        int rc2 = og_hiscore_save(&club, og_club_path);
        OG_INFO("[OG_Club] The default Ironman Club written to %s: %s",
                og_rel(og_club_path), og_strerror(rc2));
    }
    return OG_OK;
}

/* what was found of a set, in the log */
static void og_tp_report(const tp_set *t, int kind, int rc)
{
    int i;
    if (!t) {
        OG_DEBUG("[OG_TrackPak] No %s/trackpak folder", og_rel(og_dir));
        return;
    }
    for (i = 0; i < tp_chip_count(kind); ++i) {
        const char *f = tp_chip_found(t, i);
        if (f) OG_DEBUG("[OG_TrackPak] %s from %s", tp_chip_name(kind, i), f);
        else if (tp_chip_optional(kind, i) == 1) {
            /* the sound board's: all six or none are expected */
            if (tp_sound_missing(t) < 6)
                OG_WARN("[OG_TrackPak] %s (sound) is missing from %s",
                        tp_chip_name(kind, i), og_rel(t->dir));
        } else if (tp_chip_optional(kind, i) == 2) {
            /* F3's sprites: likewise all or none */
            if (tp_view_missing(t) < tp_view_chips(kind))
                OG_WARN("[OG_TrackPak] %s (F3's sprites) is missing from %s",
                        tp_chip_name(kind, i), og_rel(t->dir));
        } else if (rc == OG_E_SHORT && tp_missing(t) < tp_chip_count(kind))
            OG_WARN("[OG_TrackPak] %s is missing from %s", tp_chip_name(kind, i), og_rel(t->dir));
    }
    if (rc == OG_OK) {
        const int vm = tp_view_missing(t);
        OG_INFO("[OG_TrackPak] The %s set: all %d ROM chips found in %s; %s; %s",
                og_tp_kind_nm[kind],
                tp_chip_count(kind) - 6 - tp_view_chips(kind) +
                    (tp_sound_missing(t) ? 0 : 6) + tp_view_chips(kind) - vm,
                og_rel(t->dir), tp_sound_missing(t)
                    ? "no sound board chips, so the DOS game's sound"
                    : "the sound board's too, so the arcade's sound",
                !vm ? "F3 shows all its sprites"
                    : vm < tp_view_chips(kind) ? "F3 shows some of its sprites"
                    : "F3 shows the sprites of the chips the races use");
    }
}

static void og_tp_detect(void)
{
    int k;
    for (k = 0; k < TP_KINDS; ++k) {
        tp_set *t = NULL;
        int rc = tp_open(og_dir, k, &t);
        og_tp_report(t, k, rc);
        og_tp_have[k] = rc == OG_OK;
        tp_close(t);
    }
    og_tp_found = og_tp_have[TP_TRACKPAK] || og_tp_have[TP_ARCADE];
    if (!og_tp_found) og_arcade_roms = 0;
    else og_arcade_mode = og_arcade_mode_usable();  /* F1 shows what is had */
    og_opt_refresh();
}

/* the mode that can be had: the one asked for, or one whose set is there */
static int og_arcade_mode_usable(void)
{
    const int want = og_arcade_mode;
    if ((want == OG_ARCADE_ORIGINAL && og_tp_have[TP_ARCADE]) ||
        (want == OG_ARCADE_TRACKPAK && og_tp_have[TP_TRACKPAK]) ||
        (want == OG_ARCADE_ALL16 && og_tp_have[TP_ARCADE] && og_tp_have[TP_TRACKPAK]))
        return want;
    return og_tp_have[TP_ARCADE] ? OG_ARCADE_ORIGINAL : OG_ARCADE_TRACKPAK;
}

static void og_ovl_arcade_roms(void)
{
    og_opt_refresh();                  /* "arcade mode" goes with it */
    OG_INFO("[OG_TrackPak] %s from the next race", og_arcade_roms
            ? "The arcade's tracks" : "The DOS game's tracks");
}

static void og_ovl_arcade_mode(void)
{
    OG_INFO("[OG_TrackPak] Arcade mode: %s, from the next race",
            og_arcade_mode_nm[og_arcade_mode]);
}

/* The race tune's command for SORP's track, from the set in use */
/* The podium's song for a nation (GAME.EXE's 0x872d: USA, Italy,
 * Germany, Britain, France, Japan, Canada, Australia; 8 none, every
 * computer driver's; SORP's 9-16 from the second page): the arcade's
 * anthem for that nation, or its jingle 0x56, as it plays for a computer
 * winner (tp.h). */
static int og_arc_anthem(int country)
{
    const int cmd = country >= 0 && country < OG_NATIONS && country != OG_COUNTRY_NONE && og_tp
                  ? tp_anthem(og_tp, og_nation_tp[country]) : -1;
    return cmd > 0 ? cmd : 0x56;
}
/* The set the nations' names and flags come from: either, both have them
 * (the same pictures), when "arcade roms" has one open and its sprite
 * chip u5t was found. */
static const tp_set *og_nation_set(void)
{
    static const int pref[2] = { TP_TRACKPAK, TP_ARCADE };
    int i;
    for (i = 0; i < 2; ++i) {
        const tp_set *t = og_tp_sets[pref[i]];
        unsigned char p[TP_FLAG_W * TP_FLAG_H];
        char nm[TP_NATION_NAME];
        if (t && tp_flag_pens(t, TP_NATIONS, p, NULL, NULL, NULL) == OG_OK &&
            tp_nation_name(t, TP_BELGIUM, nm) == OG_OK)
            return t;
    }
    return NULL;
}

static int og_nations_more(void)
{
    return og_arcade_roms && og_nation_set() != NULL;
}

/* A second-page nation's flag in the DOS game's colours.  GAME.EXE's
 * flags ARE the arcade's, pixel for pixel: the shop's take
 * the arcade's pens as they stand, its palette being the select screen's
 * list 0x1A75 index for index; the podium's go through this map, its
 * palette the podium list's 0x1A53 colours rearranged, and drop the last
 * row (32 x 19).  The second page's flags use two pens none of the eight
 * does: 12, yellow, which both palettes hold (12 in each), and 13,
 * Norway's dark blue -- the podium's 8, but the shop has none (its 13 is
 * olive), so there it takes the flags' blue, 14 (the user's choice). */
static int og_nation_flag(int code, int where, og_image *out)
{
    static const signed char shop[16] = {
        0, 1, -1, -1, 4, 5, -1, -1, -1, 9, 10, 11, 12, 14, 14, -1
    };
    static const signed char podium[16] = {
        0, 1, -1, -1, 5, 2, -1, -1, -1, 11, 3, 6, 12, 8, 9, -1
    };
    const signed char *map = where == OG_FLAG_PODIUM ? podium : shop;
    const tp_set *t = og_nation_set();
    unsigned char p[TP_FLAG_W * TP_FLAG_H];
    int w, h, i;
    if (!out || code < 0 || code >= OG_NATIONS || !t ||
        tp_flag_pens(t, og_nation_tp[code], p, &w, &h, NULL) != OG_OK)
        return OG_E_ARG;
    if (where == OG_FLAG_PODIUM && h > 19) h = 19;
    for (i = 0; i < w * h; ++i)
        if (map[p[i] & 15] < 0) return OG_E_RANGE;     /* a pen no flag uses */
    if (og_image_init(out, w, h) != OG_OK) return OG_E_NOMEM;
    for (i = 0; i < w * h; ++i) out->px[i] = (unsigned char)map[p[i] & 15];
    return OG_OK;
}

/* The second page of the nation list: a page of SORP's own in the format
 * of GAME.EXE's (DS:0x872a), drawn by the same engine after the same
 * prologue -- 05 00 x for the first name's indent, then for each other
 * 08 (a new line) and 07 00 x 0c, the indent and 12 rows down: 21 a line.
 * The names are the arcade's.  GAME.EXE placed its eight by hand: each
 * new name takes the indent GAME.EXE gives a name of its length (7 -> 8,
 * 5 -> 13; 6 letters had both 11 and 10, FRANCE and CANADA: 11). */
static unsigned og_nation_page(og_pages *pg)
{
    static unsigned at;                 /* where it was added to GAME.EXE */
    static const tp_set *built;         /* whose names it holds           */
    const tp_set *t = og_nation_set();
    unsigned char b[160];
    size_t n = 0;
    int i;
    if (!t || !pg || !pg->loaded) return 0;
    if (!at && og_page_add(pg, sizeof b, &at) != OG_OK) { at = 0; return 0; }
    if (built == t) return at;
    for (i = 0; i < 8; ++i) {
        char nm[TP_NATION_NAME];
        int len, x;
        if (tp_nation_name(t, og_nation_tp[OG_NATION_MORE + i], nm) != OG_OK) return 0;
        len = (int)SDL_strlen(nm);
        x = len == 3 ? 20 : len == 5 ? 13 : len == 6 ? 11 : len == 7 ? 8
          : len == 9 ? 2 : SDL_max(0, 29 - 3 * len);
        if (i == 0) { b[n++] = 0x05; b[n++] = 0; b[n++] = (unsigned char)x; }
        else {
            b[n++] = 0x08;
            b[n++] = 0x07; b[n++] = 0; b[n++] = (unsigned char)x; b[n++] = 0x0c;
        }
        SDL_memcpy(b + n, nm, (size_t)len); n += (size_t)len;
    }
    b[n++] = 0;
    if (og_page_poke(pg, at, 0, (const char *)b, n) != OG_OK) return 0;
    built = t;
    return at;
}

static int og_arc_tune(int track)
{
    return og_tp ? tp_race_tune(og_tp, og_tp_t(track)) : -1;
}

/* The sound board follows the set in use: the base game's for its
 * tracks, the Track-Pak's for its own, none (the DOS game's sound) when
 * no set is, or the set has no sound chips.  The song playing carries
 * over to whichever plays it now. */
static void og_arc_follow(void)
{
    og_audio *a = og_audio_p;
    const unsigned char *rom = og_tp_live && og_tp ? tp_sound_rom(og_tp) : NULL;
    const int kind = rom ? og_tp_kind : -1;
    if (!a || !a->ready || kind == a->arc_kind) return;
    og_snd_lock(a);
    if (kind >= 0 && a->arc_kind < 0) {          /* the DOS sound gives way */
        og_music_stop(&a->sfx, &og_music_state);
        og_audio_silence(a);
    }
    og_arc_board(a, rom, kind);
    if (og_music_song) og_music_select(a, og_music_song);
    og_snd_unlock(a);
    if (kind >= 0)
        OG_INFO("[OG_Audio] The %s set's sound board: its music, effects and engine",
                og_tp_kind_nm[kind]);
    else
        OG_INFO("[OG_Audio] The DOS game's sound again");
}

/* Use set `kind` for the race about to load: its numbering, its board's
 * place, and F4's names for its tracks. */
static void og_tp_use(int kind)
{
    int t, r;
    if (!og_tp_live || kind < 0 || kind >= TP_KINDS || !og_tp_sets[kind]) return;
    og_tp = og_tp_sets[kind];
    og_tp_kind = kind;
    for (t = 0; t < 8; ++t) {
        int bx, by;
        if (tp_board_at(og_tp, og_tp_t(t), &bx, &by) == OG_OK) {
            hud.ox[t] = (unsigned short)bx;
            hud.oy[t] = (unsigned short)by;
        }
    }
    for (r = 0; r < OG_ROWS(og_opt_debug); ++r)
        if (og_opt_debug[r].value == &og_dbg_track)
            og_opt_debug[r].names = kind == TP_TRACKPAK ? og_tp_track_nm : og_track_file_nm;
    og_arc_follow();
}

/* the set of a race-order entry */
static void og_tp_use_lay(unsigned lay)
{
    og_tp_use((lay & OG_LAY_TP) ? TP_TRACKPAK : TP_ARCADE);
}

/* the club the race's records go to, and whether it holds the players */
static og_hiscore *og_race_club(int *own)
{
    const int apart = og_tp_live && og_tp_mode == OG_ARCADE_ALL16 &&
                      og_tp_kind == TP_TRACKPAK && og_club_tp_ok;
    if (own) *own = !apart;
    return apart ? &club_tp : &club;
}

/* save the clubs open, where the game saves its own */
static void og_club_tp_save(void)
{
    if (og_club_tp_ok && SDL_GetPathInfo(og_club_tp_path, NULL)) {
        int rc = og_hiscore_save(&club_tp, og_club_tp_path);
        OG_INFO("[OG_Club] The Track-Pak's records saved to %s: %s",
                og_rel(og_club_tp_path), og_strerror(rc));
    }
}

/* Before a race loads: open the sets F1 asks for, or put the DOS game's
 * own tables back. */
static void og_tp_sync(const char *dir)
{
    const int mode = og_arcade_roms ? og_arcade_mode_usable() : -1;
    int t, k;
    if (mode == og_tp_mode) return;

    if (og_tp_mode >= 0) {                         /* put the DOS game back */
        og_tp_live = 0;
        og_arc_follow();                           /* before the sets close */
        og_tp_kind = -1;
        og_tp_bg = NULL;
        og_surface_free(&og_tp_surface);
        g_surface = og_dos_surface;
        slopes = og_dos_slopes;
        for (t = 0; t < 8; ++t) { hud.ox[t] = og_dos_hud_ox[t]; hud.oy[t] = og_dos_hud_oy[t]; }
        for (t = 0; t < TP_TRACKS; ++t) og_image_free(&og_tp_thumb[t]);
        og_tp_norder = 0;
        for (k = 0; k < TP_KINDS; ++k) { tp_close(og_tp_sets[k]); og_tp_sets[k] = NULL; }
        og_tp = NULL;
        og_club_tp_save();
        og_club_tp_ok = 0;
        og_tp_mode = -1;
        for (k = 0; k < OG_ROWS(og_opt_debug); ++k)
            if (og_opt_debug[k].value == &og_dbg_track) og_opt_debug[k].names = og_track_file_nm;
        og_club_open(dir);
        OG_INFO("[OG_TrackPak] The DOS game's tracks again");
    }
    if (mode < 0) return;

    for (k = 0; k < TP_KINDS; ++k) {
        const int need = (k == TP_ARCADE   && mode != OG_ARCADE_TRACKPAK) ||
                         (k == TP_TRACKPAK && mode != OG_ARCADE_ORIGINAL);
        if (need) {
            int rc = tp_open(dir, k, &og_tp_sets[k]);
            if (rc != OG_OK) {
                og_tp_report(og_tp_sets[k], k, rc);
                for (t = 0; t < TP_KINDS; ++t) { tp_close(og_tp_sets[t]); og_tp_sets[t] = NULL; }
                og_arcade_roms = 0;
                return;
            }
        }
    }
    og_tp_mode = mode;
    og_tp_live = 1;
    og_dos_surface = g_surface;
    memset(&g_surface, 0, sizeof g_surface);
    og_dos_slopes = slopes;
    tp_slopes(&slopes);
    for (t = 0; t < 8; ++t) { og_dos_hud_ox[t] = hud.ox[t]; og_dos_hud_oy[t] = hud.oy[t]; }
    og_tp_board_build(dir);
    if (og_tp_sets[TP_TRACKPAK]) {
        og_tp = og_tp_sets[TP_TRACKPAK];
        og_tp_norder = tp_schedule(og_tp, og_tp_order, TP_SCHEDULE_MAX);
        og_tp_thumbs_build();
    }
    og_tp_use(mode == OG_ARCADE_TRACKPAK ? TP_TRACKPAK : TP_ARCADE);
    og_club_open(dir);                 /* IRONHITP.BIN in "Track-Pak" */
    if (mode == OG_ARCADE_ALL16) {     /* ...and beside IRONHI.BIN in "All 16" */
        const char *base = SDL_GetBasePath();
        SDL_snprintf(og_club_tp_path, sizeof og_club_tp_path, "%s%s",
                     base ? base : "", OG_HISCORE_TP_FILE);
        og_club_tp_ok = og_hiscore_open(og_club_tp_path, dir, &club_tp) == OG_OK;
        if (og_club_tp_ok && club_tp.source != OG_CLUB_FROM_SORP) {
            int rc = og_hiscore_save(&club_tp, og_club_tp_path);
            OG_INFO("[OG_Club] The Track-Pak's records start in %s: %s",
                    og_rel(og_club_tp_path), og_strerror(rc));
        }
    }
    OG_INFO("[OG_TrackPak] In use: %s", og_arcade_mode_nm[mode]);
}

/* A track's art and terrain from the set in use, in place of TRACKn.XX
 * and CONTOURn.XX. */
static int og_tp_track_load(int index, og_track_art *art, og_contour *contour)
{
    const int at = og_tp_t(index);
    int rc, x, y, i;
    memset(art, 0, sizeof *art);
    rc = tp_art_load(og_tp, at, &og_tp_art);
    if (rc != OG_OK) return rc;
    /* the scoreboard, painted in as DOS paints it, with its own dark
     * twins (index | 1) under a shadow */
    for (y = 0; y < OG_BOARD_H; ++y)
        for (x = 0; x < OG_BOARD_W; ++x) {
            int v = og_tp_board[y * OG_BOARD_W + x] - 1;
            int sx = hud.ox[index] + x, sy = hud.oy[index] + y, p;
            if (v < 0 || sx >= OG_SCREEN_W || sy >= OG_SCREEN_H) continue;
            p = sy * OG_SCREEN_W + sx;
            memcpy(og_tp_art.rgb + p * 3, og_pal_race + v * 3, 3);
            memcpy(og_tp_art.shadow + p * 3, og_pal_race + (v | 1) * 3, 3);
            og_tp_art.fg[p] = 0;
            og_tp_art.over[p] = 0;
        }
    /* the indexed art the course trace and the debug aids read: the
     * nearest race colours */
    rc = og_image_init(&art->pic, OG_SCREEN_W, OG_SCREEN_H);
    art->fg = (unsigned char *)malloc(OG_SCREEN_W * OG_SCREEN_H);
    if (rc != OG_OK || !art->fg) { og_track_free(art); return OG_E_NOMEM; }
    for (i = 0; i < OG_SCREEN_W * OG_SCREEN_H; ++i)
        art->pic.px[i] = (unsigned char)og_nearest(og_pal_race, 16, og_tp_art.rgb + i * 3);
    memcpy(art->fg, og_tp_art.fg, OG_SCREEN_W * OG_SCREEN_H);
    art->has_fg = 1;

    og_surface_free(&og_tp_surface);
    memset(&g_surface, 0, sizeof g_surface);
    rc = tp_contour_load(og_tp, at, contour, &og_tp_surface, NULL);
    if (rc != OG_OK) return rc;
    g_surface = og_tp_surface;          /* the race's POLY and QUAD */
    og_tp_bg = &og_tp_art;
    OG_DEBUG("[OG_TrackPak] Track %d from the %s set's track %d", index,
             og_tp_kind_nm[og_tp_kind], at);
    return OG_OK;
}

static int og_load_route(const char *dir, int layout, og_route *r)
{
    return og_tp_live ? tp_route_load(og_tp, og_tp_l(layout), r) : og_route_load(dir, layout, r);
}

static int og_load_gates(const char *dir, int layout, og_checkpoints *c)
{
    return og_tp_live ? tp_checkpoints_load(og_tp, og_tp_l(layout), c)
                      : og_checkpoints_load(dir, layout, c);
}

static int og_load_grid(const char *dir, int layout, og_race_start *g)
{
    return og_tp_live ? tp_race_start_load(og_tp, og_tp_l(layout), g)
                      : og_race_start_load(dir, layout, g);
}

static void og_load_origin(const char *dir, int track, int *ox, int *oy)
{
    if (og_tp_live) {
        tp_track_origin(og_tp, og_tp_t(track), ox, oy);
        OG_DEBUG("[OG_TrackPak] Track %d's screen origin: (%d, %d)", track, *ox, *oy);
    } else {
        og_track_origin(dir, track, ox, oy);
    }
}

/* the pickup spots: per track in GAME.EXE, per layout in the arcade */
static int og_load_spawns(const char *dir, int track, int reverse, og_spawns *s)
{
    return og_tp_live ? tp_spawns_load(og_tp, og_tp_l(OG_LAYOUT(track, reverse)), s)
                      : og_spawns_load(dir, track, s);
}

/* An arcade track's scenery is the arcade's: its flagman (whose wave
 * starts the race) and its pennant poles and banners, where its tables
 * put them (tp_flagman, tp_scenery).  The sprites, scripts and timing
 * are GAME.EXE's own, as the DOS game drew the same pieces; the DOS
 * barrels stand in DOS art only, so there are none. */
static int og_load_scenery(const char *dir, int layout, og_scenery *s)
{
    tp_deco d[OG_SCENERY_MAX];
    int rc, i, n;
    if (!og_tp_live) return og_scenery_load(dir, layout, s);
    rc = og_scenery_load(dir, og_tp_kind == TP_ARCADE ? layout : (layout & 7), s);
    if (rc != OG_OK) return rc;
    n = tp_scenery(og_tp, og_tp_l(layout), d, OG_SCENERY_MAX);
    s->n = n < 0 ? 0 : n;
    for (i = 0; i < s->n; ++i) {
        s->type[i] = (unsigned char)d[i].type;
        s->sx[i]   = (short)d[i].sx;
        s->sy[i]   = (short)d[i].sy;
        s->sort[i] = (short)(d[i].wy >> 5);
        s->wx[i]   = (short)(d[i].wx >> 5);
    }
    rc = tp_flagman(og_tp, og_tp_l(layout), &s->fm_x, &s->fm_y);
    if (rc == OG_OK)
        OG_DEBUG("[OG_TrackPak] Layout %d: the flagman at (%d, %d), %d pennant%s",
                 layout, s->fm_x, s->fm_y, s->n, s->n == 1 ? "" : "s");
    return rc;
}

/* The championship's layout for race n, OG_LAY_TP marking a Track-Pak
 * one: DS:0x1a3, or the Track-Pak's order, or in "All 16 Tracks" the two
 * a race from each in turn.  0x1d5 carries the DOS order on from entry
 * 27 after the 43rd; the arcade's 0xFF sends its own back to the start. */
static unsigned og_dos_sched(int i)
{
    if (i >= OG_SCHEDULE_N) i = 0x1b + (i - OG_SCHEDULE_N) % (OG_SCHEDULE_N - 0x1b);
    return og_schedule[i];
}

static unsigned og_sched_layout(int n)
{
    if (og_tp_live && og_tp_mode == OG_ARCADE_TRACKPAK && og_tp_norder > 0)
        return og_tp_order[n % og_tp_norder] | OG_LAY_TP;
    if (og_tp_live && og_tp_mode == OG_ARCADE_ALL16 && og_tp_norder > 0)
        return (n & 1) ? og_tp_order[(n / 2) % og_tp_norder] | OG_LAY_TP
                       : og_dos_sched(n / 2);
    return og_schedule[n % OG_SCHEDULE_N];
}



/* The title cycle, as the "TI" object runs it: four views, the fourth
 * rotating through three pages each time round (image 0x366c). */
/* A view's artwork: the child objects the TI script spawns before its
 * page, each a run of consecutive sprites (their field 8) at its own
 * position (fields 0x2e and 0x2a). */
/* Fields 0x2a and 0x2e ARE the position, with no adjustment: the shop
 * dump shows its panels holding 8, 112 and 216 in 0x2a and drawn at
 * exactly those x.  Measured in a capture of the publisher view, the
 * Virgin box occupies x 62..103, y 42..79 -- which is the descriptor's
 * (62,42) unchanged.  The +16 I had came from a routine that builds a
 * different kind of object and does not apply here. */
typedef struct { int sprite, pieces, x, y; } og_title_art;
static const og_title_art og_title_lo = { 582, 5,  7, 30 };   /* the logo  */
static const og_title_art og_title_vi = { 575, 2, 62, 42 };   /* Virgin    */
static const og_title_art og_title_gr = { 570, 5, 24, 94 };   /* Graftgold */
static const og_title_art og_title_rh = { 560, 5,  2, 13 };   /* hints     */
/* The bank falls into groups of five 32-wide pieces: 560 is the "Racing
 * Hints" heading and 565 the "Awesome Wins" one, drawn in the same place
 * on their own view.  577..581 are 16 wide and sit between the Virgin
 * logo and the Off Road logo: the Virgin Mastertronic wordmark, five
 * 16-wide pieces, which the game never shows -- and which is why they
 * read as a different palette in a raw dump of the bank. */
static const og_title_art og_title_aw = { 565, 5,  2, 13 };   /* wins      */

typedef struct { unsigned page; int ticks; } og_title_view;
static const og_title_view og_title_seq[3][4] = {
    { { 0x8c34, OG_TITLE_V1_TICKS }, { 0x88f1, OG_TITLE_V2_TICKS },
      { 0x8b0d, OG_TITLE_V3_TICKS }, { 0x8c55, OG_TITLE_V4_TICKS } },
    { { 0x8c34, OG_TITLE_V1_TICKS }, { 0x88f1, OG_TITLE_V2_TICKS },
      { 0x8b0d, OG_TITLE_V3_TICKS }, { 0x8e0f, 528 } },
    { { 0x8c34, OG_TITLE_V1_TICKS }, { 0x88f1, OG_TITLE_V2_TICKS },
      { 0x8b0d, OG_TITLE_V3_TICKS }, { 0x904f, 528 } }
};
static og_ladder  ladder;      /* race -> difficulty, by the player's pace */
static int  cpu_base   = OG_CPU_BASE;  /* what this race starts the field at */
static long player_ticks = 0;          /* [di+0x44]: laps timed, in ticks    */
static int  player_laps  = 0;
static int  race_number  = 0;          /* [0x19e]                            */
/* 0x1d5: the next race -- after the 43rd entry the schedule carries on
 * from entry 27, not from 0.  The Track-Pak's order (0x5710) ends in
 * 0xFF and a pointer back to its start. */
static int og_race_next(int n)
{
    ++n;
    if (og_tp_live && og_tp_mode == OG_ARCADE_TRACKPAK && og_tp_norder > 0)
        return n < og_tp_norder ? n : 0;
    if (og_tp_live && og_tp_mode == OG_ARCADE_ALL16)
        return n < 2 * OG_SCHEDULE_N + 2 * TP_SCHEDULE_MAX ? n : 0;
    return n < OG_SCHEDULE_N ? n : 0x1b;
}
/* 0x2c4d: the START NEXT RACE thumbnail is the track of the race AFTER
 * the one just run -- [0x19e] + 1, wrapped as 0x1d5 wraps it, looked up
 * in DS:0x1a3 and masked to the track.  SORP moves race_number on as a
 * race STARTS, so in the shop it already names that next race.  (The
 * shop was given the track just raced.) */
static int og_shop_next_track(void)
{
    const unsigned lay = og_sched_layout(race_number);
    return (int)(lay & 7) + ((lay & OG_LAY_TP) ? OG_NUM_TRACKS : 0);
}

typedef struct {
    og_track_art  art;
    og_course    *course;
    og_contour    contour;
    unsigned char heights[OG_CONTOUR_W * OG_CONTOUR_H];
    int           has_heights;
    unsigned char *water;       /* puddles: navy regions touching no barrier */
    SDL_Texture  *bg;
    SDL_Texture  *fg;
    int           index;
} track_view;

static void track_view_free(track_view *tv)
{
    if (tv->bg)   SDL_DestroyTexture(tv->bg);
    if (tv->fg)   SDL_DestroyTexture(tv->fg);
    tv->bg = tv->fg = NULL;
    og_track_free(&tv->art);
    free(tv->course);
    free(tv->water);
    tv->course = NULL;
    tv->water = NULL;
}

static int track_view_load(track_view *tv, SDL_Renderer *ren,
                           const char *dir, int index)
{
    const Uint64 t0 = SDL_GetTicksNS();
    int rc;

    track_view_free(tv);
    memset(tv, 0, sizeof *tv);
    tv->index = index;

    og_tp_bg = NULL;
    if (og_tp_live) {
        rc = og_tp_track_load(index, &tv->art, &tv->contour);
        if (rc != OG_OK) {
            OG_ERR("[OG_TrackPak] Track %d failed: %s", index, og_strerror(rc));
            return rc;
        }
    } else {
        rc = og_track_load(dir, index, &tv->art);
        if (rc != OG_OK) {
            OG_ERR("[OG_DataLoad: TRACK%d.XX] Failed: %s", index, og_strerror(rc));
            return rc;
        }
    }

    tv->course = (og_course *)calloc(1, sizeof *tv->course);
    if (!tv->course) return OG_E_NOMEM;

    /* Surfaces come from the artwork; the second argument is vestigial
     * now that classification runs off og_race_class. */
    og_course_build(tv->course, &tv->art, 8);

    /* POLY and QUAD are decoded now -- surface, normal and the two
     * gradients -- and the surface byte drives water and barriers.  The
     * height field integrated from the gradients is what is still
     * unused: the course geometry comes from the artwork instead. */
    if (!og_tp_live) og_contour_load(dir, index, &tv->contour);

    tv->bg = texture_from(ren, &tv->art.pic, og_pal_race, -1, NULL);
    if (tv->art.has_fg)
        tv->fg = texture_from(ren, &tv->art.pic, og_pal_race, -1,
                              tv->art.fg);
    tv->water = og_find_water(&tv->art);

    /* Integrated elevation from CONTOURn + POLY.BIN + QUAD.BIN.  Drawn
     * by og_draw_contour through the game's own projection; H[y][x] is
     * the height at grid VERTEX (x,y). */
    if (tv->contour.loaded && g_surface.loaded &&
        og_contour_heights(&tv->contour, &g_surface, tv->heights) == OG_OK)
        tv->has_heights = 1;

    OG_DEBUG("[OG_Race] Track %d: %s foreground plane, %d centreline nodes, "
            "lap %.0f px, contour %s",
            index, tv->art.has_fg ? "has" : "no",
            tv->course->nnodes, (double)tv->course->length,
            tv->contour.loaded ? "loaded" : "missing");
    OG_INFO("[OG_Race] Track %d loaded in %.1f ms", index,
            (double)(SDL_GetTicksNS() - t0) / 1e6);
    {   /* a leak of files shows here first: the RG DS's KMSDRM flips once
         * spent all 1024 a process may hold, and then a track would not
         * open at all */
        const int nf = og_open_files();
        if (nf >= 0) OG_DEBUG("[SORP] %d files open", nf);
    }
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* F3's fonts, pictures and tracks                                     */
/* ------------------------------------------------------------------ */

/* The pictures, decoded the first time they are wanted.  LOADA.XX is
 * shipped but never shown (nothing asks for index 10), and SORP does not
 * need it: without it there is no page for it, and nothing is said. */
static const char *const og_spr_pic_name[] = { "LOADB.XX", "TITLES.XX", "LOADA.XX" };
#define OG_SPR_PICS ((int)SDL_arraysize(og_spr_pic_name))
static og_image og_spr_pic[OG_SPR_PICS];
static int      og_spr_pic_tried = 0;

/* The arcade sets, opened for the viewer alone the first time its
 * arcade view is wanted (the race's own are open only with "arcade
 * roms" on), and the track on show, decoded. */
static tp_set *og_spr_tp[TP_KINDS];
static int     og_spr_tp_tried = 0;
static tp_art  og_spr_tp_art;
/* ...and every sprite of each set's video ROM (tp_sprite_list) */
static tp_sprite *og_spr_asp[TP_KINDS];
static int        og_spr_asp_n[TP_KINDS];

static void og_spr_extra_free(void)
{
    int k;
    for (k = 0; k < OG_SPR_PICS; ++k) og_image_free(&og_spr_pic[k]);
    for (k = 0; k < TP_KINDS; ++k) {
        tp_close(og_spr_tp[k]); og_spr_tp[k] = NULL;
        free(og_spr_asp[k]); og_spr_asp[k] = NULL; og_spr_asp_n[k] = 0;
    }
    og_spr_tp_tried = 0;
}

/* The arcade views' sets, opened the first time one is wanted.  Returns
 * how many there are, their kinds in `kinds`: the base game's first. */
static int og_spr_tp_open(const char *dir, int kinds[TP_KINDS])
{
    static const int order[TP_KINDS] = { TP_ARCADE, TP_TRACKPAK };
    int k, nk = 0;
    if (!og_spr_tp_tried) {
        og_spr_tp_tried = 1;
        for (k = 0; k < TP_KINDS; ++k) {
            tp_set *ts = NULL;
            if (tp_open(dir, order[k], &ts) == OG_OK) og_spr_tp[order[k]] = ts;
            else tp_close(ts);
        }
    }
    for (k = 0; k < TP_KINDS; ++k)
        if (og_spr_tp[order[k]]) kinds[nk++] = order[k];
    return nk;
}

/* THE ARCADE'S SPRITES, as sheets: a set's sprites in address order,
 * bank by bank, packed in rows into the screen between F3's two lines
 * of text, a bank starting a page.  Walks every page; for page `want`
 * it calls `put` with each sprite's place.  Returns the page count. */
#define OG_ASP_TOP  11
#define OG_ASP_BOT  (OG_SCREEN_H - 10)
#define OG_ASP_GAP  2
static int og_spr_asp_pages(int kind, int want,
                            void (*put)(void *, const tp_sprite *, int, int), void *u,
                            int *first, int *last)
{
    const tp_sprite *l = og_spr_asp[kind];
    int i, pages = 0, x = 0, y = 0, rh = 0, bank = -1;
    for (i = 0; i < og_spr_asp_n[kind]; ++i) {
        const tp_sprite *s = &l[i];
        if (bank != (s->addr & 15)) {
            ++pages; x = y = rh = 0; bank = s->addr & 15;
        }
        if (x + s->w > OG_SCREEN_W) { x = 0; y += rh + OG_ASP_GAP; rh = 0; }
        if (y + s->h > OG_ASP_BOT - OG_ASP_TOP) { ++pages; x = y = rh = 0; }
        if (pages - 1 == want) {
            if (put) put(u, s, x, OG_ASP_TOP + y);
            if (first && *first < 0) *first = i;
            if (last) *last = i;
        }
        x += s->w + OG_ASP_GAP;
        if (s->h > rh) rh = s->h;
    }
    return pages;
}

typedef struct {
    const tp_set *tp;
    int track;                 /* the race palette, for a race's sprites */
    unsigned char *rgb;
} og_asp_put_ctx;

/* one sprite on a sheet: its box a shade lighter than the page, and its
 * even picture (background pen 0) in the palette the arcade shows it in
 * -- a screen's own, or the chosen track's (tp_sprite_palette) */
static void og_spr_asp_put(void *u, const tp_sprite *s, int x, int y)
{
    static unsigned char pal[1024 * 3];
    const og_asp_put_ctx *c = (const og_asp_put_ctx *)u;
    int r, k;
    for (r = 0; r < s->h && y + r < OG_SCREEN_H; ++r)
        for (k = 0; k < s->w && x + k < OG_SCREEN_W; ++k)
            memset(c->rgb + ((size_t)(y + r) * OG_SCREEN_W + (size_t)(x + k)) * 3, 56, 3);
    tp_sprite_palette(c->tp, s->addr, c->track, pal);
    tp_sprite_draw(c->tp, s->addr, 0, pal, 0, c->rgb, OG_SCREEN_W, OG_SCREEN_H, x, y);
}

/* a 320x200 RGB picture as a texture */
static SDL_Texture *og_spr_rgb_texture(SDL_Renderer *ren, const unsigned char *rgb)
{
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
                                       TP_ART_W, TP_ART_H);
    if (!t) return NULL;
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(t, NULL, rgb, TP_ART_W * 3);
    return t;
}

/* How far `s` moves the pen.  og_font_draw's own walk, with ROADFONT's
 * capitals standing in for its lowercase: drawn far above the image,
 * nothing lands, and the pen is where it would be. */
static int og_spr_text_w(const og_font *f, og_image *im, const char *s)
{
    return og_font_draw(f, im, 0, -1000, s, 0, 0, -1);
}

/* `s` from (x,y), wrapped at a space -- or anywhere, for a string with
 * none -- to the image's width.  Returns the y under the last line. */
static int og_spr_text(const og_font *f, og_image *im, int y, const char *s)
{
    const int x = 4, lh = f->height + 3;
    char ln[256];
    while (*s && y < im->h) {
        int n = 0, cut = 0;
        while (s[n] && n < (int)sizeof ln - 1) {
            ln[n] = s[n];
            ln[n + 1] = '\0';
            if (x + og_spr_text_w(f, im, ln) > im->w - 4) break;
            if (s[n] == ' ') cut = n;
            ++n;
        }
        if (s[n] && cut) n = cut;          /* break at the last space */
        if (n == 0) n = 1;
        ln[n] = '\0';
        og_font_draw(f, im, x, y, ln, 0, 14, 13);
        y += lh;
        s += n;
        while (*s == ' ') ++s;
    }
    return y;
}

/* One page of F3's fonts, pictures or tracks: composed, made a texture
 * at *tex, and described in info.  Returns 1 if another page follows. */
static int og_spr_extra(SDL_Renderer *ren, const char *dir, int view_fonts,
                        int view, int page, og_image *sheet,
                        SDL_Texture **tex, char *info, size_t cap)
{
    switch (view - view_fonts) {
    case 0: {                              /* fonts: IRONFONT, ROADFONT */
        static const char *const sample[] = {
            "The quick brown fox jumps over the lazy dog.",
            "I can eat glass, it does not hurt me.",
            "Ivan Ironman Stewart's Super Off Road"
        };
        const og_fontfile *ff = page ? &roadfont : &ironfont;
        const char *fn = page ? "ROADFONT.XX" : "IRONFONT.XX";
        char set[256];
        int k, n = 0, y = 14;
        if (!ff->loaded) {
            SDL_snprintf(info, cap, "%s not loaded", fn);
            return page == 0;
        }
        memset(sheet->px, 0, (size_t)sheet->w * sheet->h);   /* 0 = clear */
        for (k = 0; k < (int)SDL_arraysize(sample); ++k)
            y = og_spr_text(&ff->f, sheet, y, sample[k]) + 2;
        /* and every character it has, as og_font_draw takes them: not
         * ROADFONT's empty lowercase slots */
        for (k = ff->f.first; k <= ff->f.last && n < (int)sizeof set - 1; ++k) {
            og_glyph g;
            if (k > ' ' && og_font_glyph(&ff->f, k, &g) == OG_OK &&
                g.flags <= ff->f.height && !(g.advance == 0 && g.height == 0))
                set[n++] = (char)k;
        }
        set[n] = '\0';
        og_spr_text(&ff->f, sheet, y + 6, set);
        *tex = texture_from(ren, sheet, og_pal_title, 0, NULL);
        SDL_snprintf(info, cap, "%s, %d-%d, p%d/2", fn,
                     ff->f.first, ff->f.last, page + 1);
        return page == 0;
    }
    case 1: {                              /* pictures */
        int k, have[OG_SPR_PICS], n = 0;
        if (!og_spr_pic_tried) {
            og_spr_pic_tried = 1;
            for (k = 0; k < OG_SPR_PICS; ++k) {
                char path[1024];
                unsigned char *raw = NULL;
                size_t len = 0;
                if (og_resolve(dir, og_spr_pic_name[k], path, sizeof path) == OG_OK &&
                    og_read_file(path, &raw, &len) == OG_OK) {
                    if (og_xx_decode(raw, len, OG_F_CHECK_LENGTH, &og_spr_pic[k]) != OG_OK)
                        og_image_free(&og_spr_pic[k]);
                    free(raw);
                }
            }
        }
        /* LOADB and TITLES have a page loaded or not; LOADA only loaded */
        for (k = 0; k < OG_SPR_PICS; ++k)
            if (k < 2 || og_spr_pic[k].px) have[n++] = k;
        if (page >= n) page = n - 1;
        k = have[page];
        if (og_spr_pic[k].px) {
            *tex = texture_from(ren, &og_spr_pic[k], og_pal_title, -1, NULL);
            SDL_snprintf(info, cap, "%s, p%d/%d", og_spr_pic_name[k], page + 1, n);
        } else {
            SDL_snprintf(info, cap, "%s not loaded, p%d/%d",
                         og_spr_pic_name[k], page + 1, n);
        }
        return page + 1 < n;
    }
    case 3: {                              /* the arcade's tracks */
        /* The art as tp_art_load makes it, before SORP paints the DOS
         * scoreboard in: its colours, its mask -- what is drawn over the
         * trucks (the palettes' priority patch), white on black -- and
         * its shadow, the darker colours a shadow shows (palette bank
         * 15).  The base game's tracks first, by the arcade's numbers,
         * then the Track-Pak's. */
        static const unsigned char mask_pal[16 * 3] = { 0, 0, 0, 255, 255, 255 };
        static const char *const what[4] = { "picture", "mask", "shadow", "overlays" };
        int kinds[TP_KINDS], nk, n, kind, t, sub, rc;
        nk = og_spr_tp_open(dir, kinds);
        if (!nk) {
            SDL_snprintf(info, cap, "no arcade set in gamedata/trackpak");
            return 0;
        }
        n = nk * TP_TRACKS * 4;
        if (page >= n) page = n - 1;
        kind = kinds[page / (TP_TRACKS * 4)];
        t    = page / 4 % TP_TRACKS;
        sub  = page % 4;
        rc = tp_art_load(og_spr_tp[kind], t, &og_spr_tp_art);
        if (rc != OG_OK) {
            SDL_snprintf(info, cap, "arcade track %d: %s, p%d/%d", t, og_strerror(rc),
                         page + 1, n);
            return page + 1 < n;
        }
        if (sub == 3) {
            /* the overlays (tp_data.c): each one's depth as a grey over
             * the picture darkened -- the brighter, the further down the
             * course a truck must be to pass in front of it */
            static unsigned char rgb[TP_ART_W * TP_ART_H * 3];
            int i, m = 0;
            for (i = 0; i < TP_ART_W * TP_ART_H; ++i) {
                const int d = og_spr_tp_art.over[i];
                int c;
                for (c = 0; c < 3; ++c)
                    rgb[i * 3 + c] = d ? (unsigned char)d
                                       : (unsigned char)(og_spr_tp_art.rgb[i * 3 + c] / 4);
                m += d ? 1 : 0;
            }
            *tex = og_spr_rgb_texture(ren, rgb);
            if (kind == TP_TRACKPAK)
                SDL_snprintf(info, cap, "Track-Pak %d, %s: overlays, %d px, p%d/%d", t,
                             og_tp_track_nm[t], m, page + 1, n);
            else
                SDL_snprintf(info, cap, "arcade %d, %s: overlays, %d px, p%d/%d", t,
                             og_track_file_nm[TP_DOS_TRACK(t)], m, page + 1, n);
        } else if (sub == 1) {
            int i, m = 0;
            for (i = 0; i < TP_ART_W * TP_ART_H; ++i) {
                sheet->px[i] = (unsigned char)(og_spr_tp_art.fg[i] ? 1 : 0);
                m += og_spr_tp_art.fg[i] ? 1 : 0;
            }
            *tex = texture_from(ren, sheet, mask_pal, -1, NULL);
            if (kind == TP_TRACKPAK)
                SDL_snprintf(info, cap, "Track-Pak %d, %s: mask, %d px, p%d/%d", t,
                             og_tp_track_nm[t], m, page + 1, n);
            else
                SDL_snprintf(info, cap, "arcade %d, %s: mask, %d px, p%d/%d", t,
                             og_track_file_nm[TP_DOS_TRACK(t)], m, page + 1, n);
        } else {
            *tex = og_spr_rgb_texture(ren, sub ? og_spr_tp_art.shadow : og_spr_tp_art.rgb);
            if (kind == TP_TRACKPAK)
                SDL_snprintf(info, cap, "Track-Pak %d, %s: %s, p%d/%d", t,
                             og_tp_track_nm[t], what[sub], page + 1, n);
            else
                SDL_snprintf(info, cap, "arcade %d, %s: %s, p%d/%d", t,
                             og_track_file_nm[TP_DOS_TRACK(t)], what[sub], page + 1, n);
        }
        return page + 1 < n;
    }
    case 4: {                              /* the arcade's sprites */
        /* Every sprite in each set's video ROM (tp_sprite_list): the
         * base game's first, then the Track-Pak's, a sheet a page, each
         * on a box its size.  A sprite drawn on a screen of its own (the
         * podium, the flags...) is in that screen's palette; the rest in
         * one track's -- the button steps through the eight, as each
         * track has its own. */
        static unsigned char rgb[OG_SCREEN_W * OG_SCREEN_H * 3];
        int kinds[TP_KINDS], nk = og_spr_tp_open(dir, kinds), k, n = 0, base, first = -1, last = -1;
        og_asp_put_ctx c;
        if (!nk) {
            SDL_snprintf(info, cap, "no arcade set in gamedata/trackpak");
            return 0;
        }
        for (k = 0; k < nk; ++k) {
            const int kk = kinds[k];
            if (!og_spr_asp[kk]) {
                const int cnt = tp_sprite_list(og_spr_tp[kk], NULL, 0);
                og_spr_asp[kk] = (tp_sprite *)calloc((size_t)(cnt ? cnt : 1), sizeof *og_spr_asp[kk]);
                if (og_spr_asp[kk])
                    og_spr_asp_n[kk] = tp_sprite_list(og_spr_tp[kk], og_spr_asp[kk], cnt);
            }
            n += og_spr_asp_pages(kk, -1, NULL, NULL, NULL, NULL);
        }
        if (n <= 0) {
            SDL_snprintf(info, cap, "no sprites found");
            return 0;
        }
        if (page >= n) page = n - 1;
        for (k = 0, base = 0; k < nk; ++k) {
            const int pk = og_spr_asp_pages(kinds[k], -1, NULL, NULL, NULL, NULL);
            if (page < base + pk) break;
            base += pk;
        }
        if (k >= nk) k = nk - 1;
        memset(rgb, 32, sizeof rgb);
        c.tp = og_spr_tp[kinds[k]]; c.track = og_spr_apal; c.rgb = rgb;
        og_spr_asp_pages(kinds[k], page - base, og_spr_asp_put, &c, &first, &last);
        *tex = og_spr_rgb_texture(ren, rgb);
        if (first >= 0) {
            const tp_sprite *l = og_spr_asp[kinds[k]];
            SDL_snprintf(info, cap, "%s b%d %04x-%04x p%d/%d",
                         kinds[k] == TP_TRACKPAK ? "Track-Pak" : "arcade",
                         l[first].addr & 15, l[first].addr, l[last].addr, page + 1, n);
        }
        return page + 1 < n;
    }
    default: {                             /* tracks: each picture, mask */
        /* the mask alone: what is drawn over the trucks, white on black */
        static const unsigned char mask_pal[16 * 3] = { 0, 0, 0, 255, 255, 255 };
        const int t = page / 2, mask = page & 1;
        og_track_art art;
        int rc;
        memset(&art, 0, sizeof art);
        rc = og_track_load(dir, t, &art);
        if (rc != OG_OK) {
            SDL_snprintf(info, cap, "TRACK%d.XX: %s, p%d/%d", t, og_strerror(rc),
                         page + 1, 2 * OG_NUM_TRACKS);
        } else if (!mask) {
            *tex = texture_from(ren, &art.pic, og_pal_race, -1, NULL);
            SDL_snprintf(info, cap, "TRACK%d.XX picture, p%d/%d", t,
                         page + 1, 2 * OG_NUM_TRACKS);
        } else {
            int i, n = sheet->w * sheet->h;
            for (i = 0; i < n; ++i)
                sheet->px[i] = (unsigned char)(art.has_fg && art.fg[i] ? 1 : 0);
            *tex = texture_from(ren, sheet, mask_pal, -1, NULL);
            SDL_snprintf(info, cap, "TRACK%d.XX mask%s, p%d/%d", t,
                         art.has_fg ? "" : " (none)", page + 1, 2 * OG_NUM_TRACKS);
        }
        og_track_free(&art);
        return page + 1 < 2 * OG_NUM_TRACKS;
    }
    }
}


/* ------------------------------------------------------------------ */
/* The wheels against the walls                                        */
/* ------------------------------------------------------------------ */

/* The order 0x1552 installs the wheels -- 225, 135, 315 and 45 degrees
 * from the heading -- as our indices, whose wheels sit at 45, 135, 225
 * and 315.  Each tick takes them in this order, and a wall response on
 * one turns the truck before the next is placed. */
static const int og_wheel_order[4] = { 2, 1, 3, 0 };

/* 0x1871: the step's direction, the travel's whole part rounded UP to
 * one of 32 (add 7, and 0xf8) */
static float og_step_angle(float trav)
{
    int qd = (((int)trav + 7) >> 3) & 31;
    return (float)qd * 8.0f * (2.0f * (float)M_PI / 256.0f);
}

/* One wheel, placed as 0x18df places it: (rel + heading) >> 11 -- a
 * quarter turn and a half either side, cut DOWN to 32 directions -- one
 * cell out from (px, py), the truck plus this tick's step.  Its ground
 * goes in *g.  Returns -1 when it stands on drivable ground; otherwise
 * the wall it is in, as the POLY offset whose byte +1 is the normal, or
 * -2 off the 44 x 44 grid, which is as impassable as a wall. */
static long og_wheel_place(const unsigned char *Hh, const og_contour *c,
                           int layout, float px, float py, float head,
                           int w, float *wx, float *wy, float *g)
{
    int a = (int)floorf((head + 32.0f + 64.0f * (float)w) / 8.0f) & 31;
    float ang = (float)a * (2.0f * (float)M_PI / 32.0f);
    int cx, cy;
    long po;
    *wx = px + cosf(ang);
    *wy = py + sinf(ang);
    /* per-cell plane, as the original samples it */
    *g = og_ground_plane(Hh, &g_surface, c, *wx, *wy);       /* [+8] */
    if (!c->loaded || !g_surface.loaded) return -1;
    cx = (int)floorf(*wx);
    cy = (int)floorf(*wy);
    if (cx < 0 || cx >= OG_CONTOUR_W || cy < 0 || cy >= OG_CONTOUR_H)
        return -2;
    /* ONE resolution, by the wheel's own fraction, as the original does:
     * a QUAD cell's corners are meant to differ, and a drivable corner of
     * a mostly-solid cell is drivable. */
    po = og_surface_resolve_at(&g_surface, c->cell[cy * OG_CONTOUR_W + cx],
                               (int)((*wx - cx) * 256.0f),
                               (int)((*wy - cy) * 256.0f));
    if (po < 0) return -1;
    {
        /* IMPASSABLE: flagged, or a gradient past the layout's cap */
        int solid = (g_surface.poly[po] & 0x80) != 0;
        if (!solid && slopes.loaded) {
            int b = slopes.bias[layout & 7], l = slopes.limit[layout & 7];
            int gx = (signed char)g_surface.poly[po + 2];
            int gy = (signed char)g_surface.poly[po + 3];
            if (((b + gx) & 0xff) >= l || ((b + gy) & 0xff) >= l) solid = 1;
        }
        return solid ? po : -1;
    }
}

static float og_wrap256(float v)
{
    while (v < 0.0f)    v += 256.0f;
    while (v >= 256.0f) v -= 256.0f;
    return v;
}

/* One wheel in a wall, 0x1a68.  *state is [0x4f7a], cleared at the start
 * of each tick (0x154f).  The FIRST wheel to find it clear gets the full
 * response (0x1a86); a later one, with it set, only turns the travel
 * (0x1b7c).  A reflect leaves it clear, so the next wheel in a wall
 * responds in full again.  Returns 1 when effect 8 is due (0x1ae0). */
static int og_wall_hit(og_racer *R, int top, long po, int *state,
                       int ri, int w)
{
    /* 0x1b28: the normal's high nibble through DS:0x5248 gives the state,
     * whose LOW BITS say which axes may not move (0x1580: bit 0 keeps the
     * old x, bit 1 the old y) */
    static const unsigned char state5248[16] = {
        0x81,0x81,0x80,0x82,0x82,0x82,0x80,0x81,
        0x81,0x81,0x80,0x82,0x82,0x82,0x80,0x81
    };
    float nrm, slip_a, flip, a;
    int crash = 0;
    /* the POLY normal, byte +1, subtracted whole; off the grid there is
     * no POLY, so it points back to the map's middle */
    nrm = (po >= 0)
        ? (float)g_surface.poly[po + 1]
        : og_wrap256(atan2f(OG_CONTOUR_H * 0.5f - R->wy,
                            OG_CONTOUR_W * 0.5f - R->wx)
                     * (256.0f / (2.0f * (float)M_PI)));
    if (*state) {
        /* 0x1b7c: travel into the wall (64..191 from the normal) turns by
         * half that angle, and both axes are held */
        a = og_wrap256(R->trav - nrm);
        if (a >= 64.0f && a < 192.0f) {
            R->trav = og_wrap256(R->trav + (float)((int)a >> 1));
            *state = 0x83;
        }
        return 0;
    }
    /* 0x1a86: the angle is the HEADING exclusive-ored with [di+0x89], the
     * travelling-backwards flag (0x1cad: the slip plus 64, bit 7), so the
     * impact is judged by the way the truck is going */
    slip_a = og_wrap256(R->head - R->trav);
    flip = (slip_a >= 64.0f && slip_a < 192.0f) ? 128.0f : 0.0f;
    a = og_wrap256(R->head + flip - nrm);
    /* 0x1a92, a byte angle tested by sign:
     *
     *   192..255, 0..63  reflect (0x1aed) -- going away from it
     *   64..119          glancing (0x1afa)
     *   120..135         head-on (0x1a9e)
     *   136..191         glancing (0x1b0e)
     *
     * The 192..255 quarter is a reflect, not a glance: taking it as one
     * turned the heading 16 every tick a wheel touched, a full turn in
     * sixteen ticks. */
    if (a < 64.0f || a >= 192.0f) {
        /* 0x1aed: the travel from the heading, the flag applied; the
         * state stays clear */
        R->trav = og_wrap256(R->head + flip);
    } else if (a >= 120.0f && a < 136.0f) {
        /* 0x1a9e: the speed becomes 5/8 of top, the throttle and the
         * boost stop, the heading swings 32 either way at random
         * (0x42f:0x644, bit 5), the flag flips and the travel goes from
         * the NEW heading the other way: the truck bounces back off the
         * wall.  0x1d24 then brakes it, flagged backwards, to a stop. */
        R->speed   = (float)((0xa0 * top) >> 8);
        R->thr     = 0.0f;
        R->nitro_t = 0;
        R->head    = og_wrap256(R->head + (SDL_rand(2) ? 32.0f : -32.0f));
        flip       = 128.0f - flip;
        R->trav    = og_wrap256(R->head + flip);
        *state = 3;                                         /* 0x1adb */
        crash = 1;
    } else {
        /* 0x1afa / 0x1b0e: the travel a quarter turn round the normal,
         * the flag applied, and the heading nudged 16 the OTHER way */
        int side = (a < 120.0f) ? 1 : -1;
        float t = (float)top;
        R->head = og_wrap256(R->head - side * 16.0f);
        R->trav = og_wrap256(nrm + side * 64.0f + flip);
        *state = (po >= 0) ? state5248[(g_surface.poly[po + 1] >> 4) & 15]
                           : 0x83;
        /* 0x1b3b: a throttle over 0x40 loses a quarter, not below 0x40,
         * and the speed is SET from it -- top (half again under boost,
         * capped) times throttle */
        if (R->thr > 64.0f) {
            R->thr -= (float)((int)R->thr >> 2);
            if (R->thr < 64.0f) R->thr = 64.0f;
        }
        if (R->nitro_t > 0) {
            t *= 1.5f;
            if (t > (float)OG_NITRO_CAP) t = (float)OG_NITRO_CAP;
        }
        R->speed = t * R->thr / 256.0f;
        crash = 1;
    }
    if (og_wall_debug)
        OG_TRACE("[OG_Physics] Truck %d wheel %d in a wall at (%.2f,%.2f): normal %.0f, "
                "heading %.0f%s, impact angle %.0f, travel now %.0f, "
                "speed %.0f, state %02x",
                ri, w, R->wx, R->wy, nrm, R->head,
                flip > 0.0f ? " (backwards)" : "", a, R->trav, R->speed,
                *state);
    return crash;
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* THE LOG BUFFER                                                      */
/* ------------------------------------------------------------------ */
/*
 * Every line SDL_Log writes is kept here whether or not a console is
 * open, so nothing said before the console appears is lost -- opening
 * it replays the whole buffer, and so does starting sorp.log.  It is a
 * ring: once full, the oldest line goes to make room.
 *
 * Each line is stamped with the local time to the millisecond (the date
 * is in the first lines, once), gives its severity (see OG_ERR and the
 * rest, at the top), and names what it comes from in brackets:
 *
 *   [09:13:45.001] [INFO]  [OG_Video] Driver windows, renderer direct3d11, ...
 *   [09:13:45.050] [DEBUG] [OG_DataLoad: GAME.EXE] [Offset 0x9A7C Bytecount 0xC] Loaded ...
 *
 * SORP's own lines carry an [OG_...] tag; anything SDL itself says
 * arrives without one and is tagged [SDL].  Lines below the log level
 * never reach here.  Files beside the program are named alone, not by
 * their whole path.
 *
 * F1's "write sorp.log" puts the log in sorp.log beside the program too,
 * written afresh each run from its first line; sorp.cfg keeps the
 * choice, so it starts with the run.  Where the launcher keeps a log of
 * its own the row says "disabled" (og_log_prepare): Knulli's sends stdout
 * to sorp.log itself, which SORP opening again would write over, and
 * PortMaster's pipes it through tee into log.txt, which a second copy
 * would only repeat.
 */
#define OG_LOG_LINES  512
#define OG_LOG_CHARS  256
static char og_log_buf[OG_LOG_LINES][OG_LOG_CHARS];
static int  og_log_head = 0;      /* where the next line goes          */
static int  og_log_count = 0;     /* how many are held, up to the cap  */
#ifdef _WIN32
static int  og_log_echo = 0;      /* a console is open: print as well  */
#else
static int  og_log_echo = 1;      /* stdout is the terminal, or the log */
#endif
static FILE *og_log_file = NULL;  /* sorp.log                          */
static int   og_log_on_stdout = 0;/* the launcher keeps a log already:  */
static char  og_log_by[256];      /* ... this file, beside the program  */
static char  og_log_path[1024];

static const char *og_log_label(SDL_LogPriority p)
{
    switch (p) {
    case SDL_LOG_PRIORITY_TRACE:
    case SDL_LOG_PRIORITY_VERBOSE:  return "[TRACE] ";
    case SDL_LOG_PRIORITY_DEBUG:    return "[DEBUG] ";
    case SDL_LOG_PRIORITY_INFO:     return "[INFO]  ";
    case SDL_LOG_PRIORITY_WARN:     return "[WARN]  ";
    default:                        return "[ERROR] ";
    }
}

static void SDLCALL og_log_sink(void *user, int category,
                                SDL_LogPriority priority,
                                const char *message)
{
    char *line = og_log_buf[og_log_head];
    SDL_Time now = 0;
    SDL_DateTime dt;
    (void)user; (void)category;
    if (!message) message = "";
    SDL_zero(dt);
    if (SDL_GetCurrentTime(&now)) SDL_TimeToDateTime(now, &dt, true);
    SDL_snprintf(line, OG_LOG_CHARS, "[%02d:%02d:%02d.%03d] %s%s%s",
                 dt.hour, dt.minute, dt.second, dt.nanosecond / 1000000,
                 og_log_label(priority), message[0] == '[' ? "" : "[SDL] ",
                 message);
    og_log_head = (og_log_head + 1) % OG_LOG_LINES;
    if (og_log_count < OG_LOG_LINES) ++og_log_count;
    if (og_log_echo) {
        fputs(line, stdout); fputc('\n', stdout);
        fflush(stdout);
    }
    if (og_log_file) {
        fputs(line, og_log_file); fputc('\n', og_log_file);
        fflush(og_log_file);
    }
}

#if defined(__linux__)
/* stdout a pipe: the process reading it -- the one whose stdin it is --
 * and a file that process holds open beside the program, if it has one.
 * PortMaster's launcher runs its port as
 *     exec > >(tee "$GAMEDIR/log.txt") 2>&1
 * so this finds tee, writing log.txt.  Other processes share the pipe's
 * writing end (gptokeyb2, the shell), but only tee reads it. */
static int og_log_tee_file(const char *base, char *out, size_t cap)
{
    struct stat st;
    char want[64], path[64], link[1024];
    const size_t blen = SDL_strlen(base);
    const pid_t self = getpid();
    DIR *proc;
    struct dirent *e;
    int found = 0;
    if (!blen || fstat(1, &st) != 0 || !S_ISFIFO(st.st_mode)) return 0;
    SDL_snprintf(want, sizeof want, "pipe:[%lu]", (unsigned long)st.st_ino);
    if (!(proc = opendir("/proc"))) return 0;
    while (!found && (e = readdir(proc)) != NULL) {
        const int pid = SDL_atoi(e->d_name);
        DIR *fds;
        struct dirent *f;
        ssize_t n;
        if (pid <= 0 || pid == (int)self) continue;
        SDL_snprintf(path, sizeof path, "/proc/%d/fd/0", pid);
        n = readlink(path, link, sizeof link - 1);
        if (n <= 0) continue;
        link[n] = '\0';
        if (SDL_strcmp(link, want) != 0) continue;       /* not the reader */
        SDL_snprintf(path, sizeof path, "/proc/%d/fd", pid);
        if (!(fds = opendir(path))) continue;
        while ((f = readdir(fds)) != NULL) {
            char fp[96];
            if (f->d_name[0] == '.') continue;
            SDL_snprintf(fp, sizeof fp, "/proc/%d/fd/%s", pid, f->d_name);
            n = readlink(fp, link, sizeof link - 1);
            if (n <= 0) continue;
            link[n] = '\0';
            if (SDL_strncmp(link, base, blen) == 0 && !SDL_strchr(link + blen, '/')) {
                SDL_strlcpy(out, link + blen, cap);
                found = 1;
                break;
            }
        }
        closedir(fds);
    }
    closedir(proc);
    return found;
}
#endif

/* Where sorp.log goes, and whether the launcher keeps a log of its own
 * already -- then F1's "write sorp.log" is "disabled", as a second copy
 * would only repeat it (and Knulli's, which IS sorp.log, would be
 * written over).  Knulli's launcher sends stdout straight to sorp.log;
 * PortMaster's pipes it through tee into log.txt.  Before anything is
 * logged. */
static void og_log_prepare(void)
{
    const char *base = SDL_GetBasePath();
    SDL_snprintf(og_log_path, sizeof og_log_path, "%s%s", base ? base : "", "sorp.log");
#ifndef _WIN32
    {
        struct stat a, b;
        if (fstat(1, &a) == 0 && stat(og_log_path, &b) == 0 &&
            a.st_dev == b.st_dev && a.st_ino == b.st_ino) {
            og_log_on_stdout = 1;
            SDL_strlcpy(og_log_by, "sorp.log", sizeof og_log_by);
        }
    }
#endif
#if defined(__linux__)
    if (!og_log_on_stdout && base &&
        og_log_tee_file(base, og_log_by, sizeof og_log_by))
        og_log_on_stdout = 1;
#endif
}

/* sorp.log started: everything held so far first, then each line as it
 * comes */
static void og_log_file_start(void)
{
    int i, start;
    if (og_log_file || og_log_on_stdout) return;
    og_log_file = fopen(og_log_path, "w");
    if (!og_log_file) {
        OG_ERR("[SORP] Cannot write %s", og_rel(og_log_path));
        return;
    }
    start = (og_log_head - og_log_count + OG_LOG_LINES) % OG_LOG_LINES;
    for (i = 0; i < og_log_count; ++i) {
        fputs(og_log_buf[(start + i) % OG_LOG_LINES], og_log_file);
        fputc('\n', og_log_file);
    }
    fflush(og_log_file);
}

static void og_log_close(void)
{
    if (og_log_file) fclose(og_log_file);
    og_log_file = NULL;
}

/* the level in force: SORP_LOG's for this run, until F1 changes it, or
 * F1's (sorp.cfg's) */
static void og_log_apply(void)
{
    const int lv = og_log_env >= 0 ? og_log_env : og_log_level;
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, og_log_level_p[lv]);
}

/* F1's "log level" */
static void og_ovl_log_level(void)
{
    og_log_env = -1;                   /* F1 has the say now, not SORP_LOG */
    og_log_apply();
}

/* F1's "write sorp.log": "disabled", and unmoved, where the launcher
 * has the file */
static void og_ovl_log_write(void)
{
    if (og_log_on_stdout) { og_log_write = !og_log_write; return; }
    if (og_log_write) {
        og_log_file_start();
        if (og_log_file)
            OG_INFO("[SORP] Writing %s, from the start of this run", og_rel(og_log_path));
    } else if (og_log_file) {
        OG_INFO("[SORP] No longer writing %s", og_rel(og_log_path));
        og_log_close();
    }
}

static const char *og_ovl_show_log_write(int v)
{
    return og_log_on_stdout ? "disabled" : v ? "on" : "off";
}

/* the first lines after the build: when, where, on what, and how the
 * log itself is set */
static void og_log_start(const char *argv0, const char *dir)
{
    const int v = SDL_GetVersion();
    const char *base = SDL_GetBasePath();
    const char *exe = argv0 ? argv0 : "sorp";
    const char *e;
    SDL_Time now = 0;
    SDL_DateTime dt;
    SDL_zero(dt);
    if (SDL_GetCurrentTime(&now)) SDL_TimeToDateTime(now, &dt, true);
    og_crash_utc = dt.utc_offset;          /* the crash line's clock */
    OG_INFO("[SORP] Started %04d-%02d-%02d %02d:%02d:%02d, UTC%+03d:%02d",
            dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
            dt.utc_offset / 3600, SDL_abs(dt.utc_offset) / 60 % 60);
    for (e = exe; *e; ++e)                    /* the program's own name */
        if (*e == '/' || *e == '\\') exe = e + 1;
    OG_INFO("[SORP] Program %s%s", base ? base : "", exe);
    OG_INFO("[SORP] %s, SDL %d.%d.%d (%s; built against %d.%d.%d), "
            "%d CPU cores, %d MB RAM", SDL_GetPlatform(),
            SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v),
            SDL_GetRevision(), SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
            SDL_MICRO_VERSION, SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    OG_INFO("[SORP] Game files in %s", dir);
    og_cfg_report();
    if (SDL_getenv("SORP_LOG") && og_log_env < 0)
        OG_WARN("[SORP] SORP_LOG=%s is not one of error, warn, info, debug or trace",
                SDL_getenv("SORP_LOG"));
    OG_INFO("[SORP] Log level %s (%s); sorp.log %s%s",
            og_log_level_nm[og_log_env >= 0 ? og_log_env : og_log_level],
            og_log_env >= 0 ? "SORP_LOG" : "F1",
            og_log_on_stdout ? "is disabled: the launcher writes the log to " :
            og_log_file ? "is being written" : "is off (F1)",
            og_log_on_stdout ? og_log_by : "");
}

/* og_data's report of each table it takes from the original's files:
 * the file, where in it and how much, and what it is.  A whole file is
 * reported from offset 0. */
static void og_log_load(const char *file, long offset, long bytes,
                        const char *what)
{
    OG_DEBUG("[OG_DataLoad: %s] [Offset 0x%lX Bytecount 0x%lX] %s", file,
            (unsigned long)(offset < 0 ? 0 : offset), (unsigned long)bytes, what);
}

/* ... and a table that could not be had, with what SORP does without it */
static void og_load_failed(const char *file, int rc, const char *without)
{
    OG_ERR("[OG_DataLoad: %s] Failed (%s): %s", file, og_strerror(rc), without);
}

/* print everything held, oldest first */
static void og_log_replay(void)
{
    int i, start = (og_log_head - og_log_count + OG_LOG_LINES) % OG_LOG_LINES;
    printf("---- %d buffered line%s ----\n", og_log_count,
           og_log_count == 1 ? "" : "s");
    for (i = 0; i < og_log_count; ++i)
        puts(og_log_buf[(start + i) % OG_LOG_LINES]);
    printf("---- live ----\n");
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* THE LINUX FRAMEBUFFER                                               */
/* ------------------------------------------------------------------ */
/*
 * Handhelds such as Knulli's H700 devices have no DRM device, only
 * /dev/fb0 driven by the Mali blob -- and SDL3 has no video driver for
 * that.  So SORP renders in software into an offscreen window of the
 * game's own 320x200 and scales each finished frame into the framebuffer
 * itself: the largest 4:3 picture the screen holds, as the DOS game was
 * shown (640x480 on 640x480 and on 720x480, 720x540 on 720x720), drawn
 * on the hidden page and panned to when the screen has two.  Scaling
 * there rather than in SDL's software renderer is what makes it quick
 * enough -- a frame is a fifth of the pixels to draw.
 * The joysticks come from evdev and the sound from ALSA, PulseAudio or
 * PipeWire as ever; none of them needs a window.
 *
 * It is used when SDL's own video drivers all fail -- no X or Wayland
 * display, and KMSDRM absent or without the GBM it needs; SORP_FBDEV=1
 * goes to it at once, SORP_FBDEV=0 rules it out.
 */
/* The name of the DRM connector Knulli has EmulationStation on ("DSI-2"),
 * into name; 0 if there is none to read.  Knulli keeps the output it chose
 * -- Main Menu > System Settings > Video Output -- in /var/run/drmConn as
 * an index into the card's whole connector list, connected or not (on the
 * RG DS: HDMI-A-1, DSI-1, DSI-2, Writeback-1), and before ES has started
 * in the setting global.videooutput ("2 - DSI").  The index is turned into
 * the connector's name as libdrm's drmModeGetResources and
 * drmModeGetConnector would, by their two ioctls, and named as SDL names
 * its KMSDRM displays.  (The vkbd project's es_current_screen.) */
#if defined(__linux__)
/* libdrm's copies: Debian's kernel headers leave out drm/ (only the
 * arm64 cross headers have it), and libdrm-dev is there for SDL anyway */
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <fcntl.h>
#include <sys/ioctl.h>
static int og_es_connector(char *name, size_t size)
{
    static const char *const types[] = {
        "Unknown", "VGA", "DVI-I", "DVI-D", "DVI-A", "Composite", "SVIDEO", "LVDS",
        "Component", "DIN", "DP", "HDMI-A", "HDMI-B", "TV", "eDP", "Virtual", "DSI",
        "DPI", "Writeback", "SPI", "USB",
    };
    struct drm_mode_card_res res;
    struct drm_mode_get_connector c;
    uint32_t ids[64], n;
    int index = -1, fd, ok = 0;
    FILE *f = fopen("/var/run/drmConn", "r");
    if (f) {
        if (fscanf(f, "%d", &index) != 1) index = -1;
        fclose(f);
    }
    if (index < 0 && (f = popen("knulli-settings-get global.videooutput 2>/dev/null", "r"))) {
        if (fscanf(f, "%d", &index) != 1) index = -1;
        pclose(f);
    }
    if (index < 0) return 0;
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) return 0;
    memset(&res, 0, sizeof res);
    if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == 0 && (unsigned)index < res.count_connectors) {
        n = res.count_connectors < 64 ? res.count_connectors : 64;
        memset(&res, 0, sizeof res);
        res.count_connectors = n;
        res.connector_id_ptr = (uint64_t)(uintptr_t)ids;
        memset(&c, 0, sizeof c);
        if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == 0 && (unsigned)index < n) {
            c.connector_id = ids[index];
            /* no modes asked for: the kernel then reports what is known */
            if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) == 0) {
                SDL_snprintf(name, size, "%s-%u",
                             c.connector_type < SDL_arraysize(types) ? types[c.connector_type] : "Unknown",
                             c.connector_type_id);
                ok = 1;
            }
        }
    }
    close(fd);
    return ok;
}
#else
static int og_es_connector(char *name, size_t size)
{
    (void)name; (void)size;
    return 0;
}
#endif

/* The screen to draw on under KMSDRM, which opens on SDL's first display
 * unless the window is placed on another: SORP_SCREEN (0, 1, ... in SDL's
 * list) if set, or the one EmulationStation is on (og_es_connector), found
 * by name.  0 (SDL's first) when there is no second screen to choose. */
static SDL_DisplayID og_kms_display(void)
{
    const char *e = SDL_getenv("SORP_SCREEN");
    SDL_DisplayID *ds, id = 0;
    int n = 0, want = -1, i;
    char es[32] = "";
    if (e && *e >= '0' && *e <= '9') want = SDL_atoi(e);
    else if (og_es_connector(es, sizeof es))
        OG_INFO("[OG_Video] EmulationStation is on %s", es);
    ds = SDL_GetDisplays(&n);
    for (i = 0; ds && i < n; ++i) {
        const char *dn = SDL_GetDisplayName(ds[i]);
        if (want < 0 && es[0] && dn && SDL_strcmp(dn, es) == 0) want = i;
        OG_INFO("[OG_Video] Screen %d: %s%s", i, dn ? dn : "?",
                i == want ? " -- this one" : "");
    }
    if (ds && n > 1 && want >= 0 && want < n) id = ds[want];
    SDL_free(ds);
    return id;
}

/* How the panel is mounted, as fbcon numbers it (FB_ROTATE_*): 0 upright,
 * 1 a quarter turn clockwise, 2 upside down, 3 a quarter turn counter-
 * clockwise -- the turn the picture is given to show upright.
 * SORP_ROTATE (0..3) if set, or the kernel console's own: Knulli sets
 * fbcon=rotate for a panel mounted on its side, as the Miniloong Pocket's
 * 720x960 portrait panel is (3); else none. */
static int og_panel_rotation(void)
{
    const char *e = SDL_getenv("SORP_ROTATE");
    int r = 0;
    if (e && *e >= '0' && *e <= '3') return *e - '0';
    {
        FILE *f = fopen("/sys/class/graphics/fbcon/rotate", "r");
        if (f) {
            if (fscanf(f, "%d", &r) != 1 || r < 0 || r > 3) r = 0;
            fclose(f);
        }
    }
    return r;
}

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <pthread.h>
#include <semaphore.h>
#include <dlfcn.h>
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void *)0)   /* glibc's, without _GNU_SOURCE */
#endif

static int  og_gl_open(void);    /* the GPU presenter, further down */
static void og_gl_close(void);

static int                      og_fb_fd = -1;
static unsigned char           *og_fb_mem;
static size_t                   og_fb_len;
static size_t                   og_fb_pitch;   /* bytes a row, checked (og_fb_open) */
/* How the panel is mounted (og_panel_rotation).  The picture (og_fb_x ..
 * og_fb_h) is laid out in the turned space -- landscape on a portrait
 * panel -- and each frame is turned into og_fb_rotbuf, in cached memory,
 * then copied out a row at a time. */
static int                      og_fb_rot;
static unsigned char           *og_fb_rotbuf;
static int                      og_fb_lw, og_fb_lh;   /* the turned screen */
static struct fb_var_screeninfo og_fb_var, og_fb_var0;   /* now, and at start */
static struct fb_fix_screeninfo og_fb_fix;
static SDL_PixelFormat          og_fb_format;
static int                      og_fb_pages, og_fb_page;
static int                      og_fb_x, og_fb_y, og_fb_w, og_fb_h; /* the picture */
static int                     *og_fb_xmap;    /* picture column -> source column */
static Uint64                   og_fb_ns_wait;  /* time spent waiting, for the log */
/* The pan waits for the screen's next refresh, so it has a thread of its
 * own: the game draws the next frame meanwhile.  og_fb_done says the last
 * pan has happened -- the hidden page is hidden again and may be drawn
 * on; og_fb_req hands the thread the next one. */
static pthread_t                og_fb_thread;
static int                      og_fb_threaded, og_fb_quit;
static sem_t                    og_fb_req, og_fb_done;
static struct fb_var_screeninfo og_fb_pan;      /* the thread's copy */

static void *og_fb_panner(void *arg)
{
    (void)arg;
    for (;;) {
        sem_wait(&og_fb_req);
        if (og_fb_quit) break;
        ioctl(og_fb_fd, FBIOPAN_DISPLAY, &og_fb_pan);
        sem_post(&og_fb_done);
    }
    return NULL;
}

/* SORP_FBDEV: 1 the framebuffer at once, 0 never; otherwise (0 here)
 * only when SDL's own video drivers have all failed */
static int og_fb_mode(void)
{
    const char *e = SDL_getenv("SORP_FBDEV");
    if (e && *e == '1') return 1;
    if (e && *e == '0') return -1;
    return 0;
}

/* the framebuffer's pixels as an SDL pixel format, or UNKNOWN.  32-bit
 * pixels are written WITH alpha, opaque: some screens use the fourth byte
 * (the A133's, 8/24), and the software renderer leaves whatever blending
 * made there -- the options overlay came out black.  Where the byte is
 * unused, 255 in it is harmless. */
static SDL_PixelFormat og_fb_sdl_format(const struct fb_var_screeninfo *v)
{
    if (v->bits_per_pixel == 32 && v->green.offset == 8) {
        if (v->red.offset == 16 && v->blue.offset == 0) return SDL_PIXELFORMAT_ARGB8888;
        if (v->red.offset == 0 && v->blue.offset == 16) return SDL_PIXELFORMAT_ABGR8888;
    }
    if (v->bits_per_pixel == 16 && v->red.offset == 11 && v->green.offset == 5)
        return SDL_PIXELFORMAT_RGB565;
    return SDL_PIXELFORMAT_UNKNOWN;
}

/* open /dev/fb0; its visible size comes back in *w and *h */
static int og_fb_open(int *w, int *h)
{
    og_fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (og_fb_fd < 0) return 0;
    if (ioctl(og_fb_fd, FBIOGET_VSCREENINFO, &og_fb_var) < 0 ||
        ioctl(og_fb_fd, FBIOGET_FSCREENINFO, &og_fb_fix) < 0) goto fail;
    og_fb_var0 = og_fb_var;
    og_fb_format = og_fb_sdl_format(&og_fb_var);
    if (og_fb_format == SDL_PIXELFORMAT_UNKNOWN) {
        fprintf(stderr, "fb0: %u bpp, red at %u -- not a format SORP can draw\n",
                og_fb_var.bits_per_pixel, og_fb_var.red.offset);
        goto fail;
    }
    og_fb_len = og_fb_fix.smem_len;
    /* The row pitch.  line_length can be stale: the H700's display
     * driver, switched to HDMI, reports xres 1280 and a buffer of
     * 1280 x 1440 x 4 bytes but keeps the internal panel's 2880 (720 x
     * 4), and the screen is scanned 5120 bytes a row -- written 2880
     * apart, the picture came out as diagonal noise.  A pitch too short
     * for xres_virtual is not believed when the buffer holds the full
     * one. */
    og_fb_pitch = og_fb_fix.line_length;
    {
        const size_t full = (size_t)og_fb_var.xres_virtual * (og_fb_var.bits_per_pixel / 8);
        if (og_fb_pitch < full && og_fb_len >= full * og_fb_var.yres_virtual) {
            OG_WARN("[OG_Video] Framebuffer: line length %zu is too short for %u pixels; using %zu",
                    og_fb_pitch, og_fb_var.xres_virtual, full);
            og_fb_pitch = full;
        }
    }
    og_fb_mem = (unsigned char *)mmap(NULL, og_fb_len, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, og_fb_fd, 0);
    if (og_fb_mem == MAP_FAILED) { og_fb_mem = NULL; goto fail; }
    og_fb_pages = (og_fb_var.yres_virtual >= 2 * og_fb_var.yres &&
                   og_fb_len >= 2u * og_fb_pitch * og_fb_var.yres) ? 2 : 1;
    og_fb_page = (og_fb_var.yoffset >= og_fb_var.yres) ? 0 : 1;  /* the hidden one */
    if (og_fb_pages == 1) og_fb_page = 0;
    og_fb_rot = og_panel_rotation();
    og_fb_lw = (og_fb_rot & 1) ? (int)og_fb_var.yres : (int)og_fb_var.xres;
    og_fb_lh = (og_fb_rot & 1) ? (int)og_fb_var.xres : (int)og_fb_var.yres;
    if (og_fb_rot) {
        og_fb_rotbuf = (unsigned char *)calloc((size_t)og_fb_var.xres * og_fb_var.yres,
                                               og_fb_var.bits_per_pixel / 8);
        if (!og_fb_rotbuf) goto fail;
    }
    {   /* the largest 4:3 picture, centred; black around it on both pages */
        int sw = og_fb_lw, sh = og_fb_lh, i;
        og_fb_w = sw;
        if (og_fb_w * 3 > sh * 4) og_fb_w = sh * 4 / 3;
        og_fb_h = og_fb_w * 3 / 4;
        og_fb_x = (sw - og_fb_w) / 2;
        og_fb_y = (sh - og_fb_h) / 2;
        og_fb_xmap = (int *)malloc(sizeof *og_fb_xmap * (size_t)og_fb_w);
        if (!og_fb_xmap) goto fail;
        for (i = 0; i < og_fb_w; ++i) og_fb_xmap[i] = i * OG_SCREEN_W / og_fb_w;
        memset(og_fb_mem, 0, og_fb_len);
    }
    SDL_strlcpy(og_render_path, "CPU fb copy", sizeof og_render_path);
    og_render_gpu = 0;
    og_gl_open();
    /* SDL draws the game at its own size; SORP does the scaling */
    *w = OG_SCREEN_W;
    *h = OG_SCREEN_H;
    OG_INFO("[OG_Video] Framebuffer: %ux%u, %u bpp, %d page%s; the picture %dx%d at (%d,%d)%s",
            og_fb_var.xres, og_fb_var.yres, og_fb_var.bits_per_pixel,
            og_fb_pages, og_fb_pages > 1 ? "s" : "",
            og_fb_w, og_fb_h, og_fb_x, og_fb_y,
            og_fb_rot == 1 ? ", turned a quarter clockwise" :
            og_fb_rot == 2 ? ", turned upside down" :
            og_fb_rot == 3 ? ", turned a quarter counter-clockwise" : "");
    return 1;
fail:
    close(og_fb_fd);
    og_fb_fd = -1;
    return 0;
}

/* A picture row at turned-space row ly, put where the panel shows it:
 * turned (lx, ly) is the panel's (X, Y) as fbcon places its console --
 *   1  X = xres - 1 - ly,  Y = lx
 *   2  X = xres - 1 - lx,  Y = yres - 1 - ly
 *   3  X = ly,             Y = yres - 1 - lx
 * written into og_fb_rotbuf (xres x yres, cached). */
static void og_fb_turn_row(const unsigned char *src, int ly, int bpp)
{
    const int xres = (int)og_fb_var.xres, yres = (int)og_fb_var.yres;
    int x, X, Y, dX, dY;
    switch (og_fb_rot) {
    case 1:  X = xres - 1 - ly;      Y = og_fb_x;            dX = 0;  dY = 1;  break;
    case 2:  X = xres - 1 - og_fb_x; Y = yres - 1 - ly;      dX = -1; dY = 0;  break;
    default: X = ly;                 Y = yres - 1 - og_fb_x; dX = 0;  dY = -1; break;
    }
    if (bpp == 4) {
        const Uint32 *s = (const Uint32 *)src;
        Uint32 *b = (Uint32 *)og_fb_rotbuf;
        for (x = 0; x < og_fb_w; ++x, X += dX, Y += dY) b[Y * xres + X] = s[x];
    } else {
        const Uint16 *s = (const Uint16 *)src;
        Uint16 *b = (Uint16 *)og_fb_rotbuf;
        for (x = 0; x < og_fb_w; ++x, X += dX, Y += dY) b[Y * xres + X] = s[x];
    }
}

/* og_fb_rotbuf out to the page: the panel rows and columns the picture
 * covers, a whole row at a time -- the framebuffer is uncached. */
static void og_fb_turn_out(unsigned char *page, size_t pitch, int bpp)
{
    const int xres = (int)og_fb_var.xres, yres = (int)og_fb_var.yres;
    int x0, y0, w, h, y;
    if (og_fb_rot & 1) {
        w = og_fb_h; h = og_fb_w;
        x0 = og_fb_rot == 1 ? xres - og_fb_y - og_fb_h : og_fb_y;
        y0 = og_fb_rot == 1 ? og_fb_x : yres - og_fb_x - og_fb_w;
    } else {
        w = og_fb_w; h = og_fb_h;
        x0 = xres - og_fb_x - og_fb_w;
        y0 = yres - og_fb_y - og_fb_h;
    }
    for (y = y0; y < y0 + h; ++y)
        memcpy(page + (size_t)y * pitch + (size_t)x0 * bpp,
               og_fb_rotbuf + ((size_t)y * xres + x0) * bpp, (size_t)w * bpp);
}

/* ------------------------------------------------------------------ */
/* THE GPU PRESENTER, for the framebuffer path                          */
/* ------------------------------------------------------------------ */
/* SDL 3 has no driver for a GPU that draws straight to the framebuffer
 * -- the H700's Mali (ARM's fbdev libmali) and the A133P's PowerVR
 * (IMG's NULL window system) -- so SDL renders in software and SORP
 * copies the frame.  Where OpenGL ES 2 can be had on the framebuffer
 * the copy goes to the GPU instead: SDL's frame, 320x200 or 640x400, is
 * uploaded as a texture and one quad scales it to the picture, turns it
 * for a panel on its side and draws the scanlines.
 *
 * EGL and GLES are loaded at run time (dlopen), so SORP runs where they
 * are not; the EGL native window is ARM's fbdev_window -- the size --
 * or, for PowerVR's NULL window system, nothing.  Any step that fails
 * leaves the CPU copy in charge.  SORP_GPU=0 does without it; a GBM
 * build of the driver (KMSDRM's) is passed over unless SORP_GPU=1.
 *
 * Measured on the demo race with MMPX 2x and scanlines: the CubeXX
 * (Mali-G31, 720x720) 51 frames a second, the TrimUI Brick (PowerVR
 * GE8300, 1024x768) 47; on the title screens both keep the panel's 60. */
typedef void *og_egl_ptr;
typedef int   og_egl_int;
#define OG_EGL_NONE            0x3038
#define OG_EGL_RED_SIZE        0x3024
#define OG_EGL_GREEN_SIZE      0x3023
#define OG_EGL_BLUE_SIZE       0x3022
#define OG_EGL_SURFACE_TYPE    0x3033
#define OG_EGL_WINDOW_BIT      0x0004
#define OG_EGL_RENDERABLE_TYPE 0x3040
#define OG_EGL_OPENGL_ES2_BIT  0x0004
#define OG_EGL_CONTEXT_VERSION 0x3098
#define OG_EGL_OPENGL_ES_API   0x30A0
#define OG_EGL_VENDOR          0x3053
#define OG_EGL_VERSION         0x3054
#define OG_GL_TEXTURE_2D       0x0DE1
#define OG_GL_TEXTURE_MAG      0x2800
#define OG_GL_TEXTURE_MIN      0x2801
#define OG_GL_WRAP_S           0x2802
#define OG_GL_WRAP_T           0x2803
#define OG_GL_NEAREST          0x2600
#define OG_GL_CLAMP_TO_EDGE    0x812F
#define OG_GL_RGBA             0x1908
#define OG_GL_UNSIGNED_BYTE    0x1401
#define OG_GL_FLOAT            0x1406
#define OG_GL_TRIANGLE_STRIP   0x0005
#define OG_GL_COLOR_BUFFER_BIT 0x4000
#define OG_GL_FRAGMENT_SHADER  0x8B30
#define OG_GL_VERTEX_SHADER    0x8B31
#define OG_GL_COMPILE_STATUS   0x8B81
#define OG_GL_LINK_STATUS      0x8B82
#define OG_GL_UNPACK_ALIGNMENT 0x0CF5
#define OG_GL_RENDERER         0x1F01
#define OG_GL_VERSION          0x1F02

static struct {
    void *egl, *gles;
    og_egl_ptr (*GetDisplay)(og_egl_ptr);
    unsigned (*Initialize)(og_egl_ptr, og_egl_int *, og_egl_int *);
    unsigned (*BindAPI)(unsigned);
    unsigned (*ChooseConfig)(og_egl_ptr, const og_egl_int *, og_egl_ptr *, og_egl_int, og_egl_int *);
    og_egl_ptr (*CreateWindowSurface)(og_egl_ptr, og_egl_ptr, og_egl_ptr, const og_egl_int *);
    og_egl_ptr (*CreateContext)(og_egl_ptr, og_egl_ptr, og_egl_ptr, const og_egl_int *);
    unsigned (*MakeCurrent)(og_egl_ptr, og_egl_ptr, og_egl_ptr, og_egl_ptr);
    unsigned (*SwapBuffers)(og_egl_ptr, og_egl_ptr);
    unsigned (*SwapInterval)(og_egl_ptr, og_egl_int);
    unsigned (*DestroySurface)(og_egl_ptr, og_egl_ptr);
    unsigned (*DestroyContext)(og_egl_ptr, og_egl_ptr);
    unsigned (*Terminate)(og_egl_ptr);
    og_egl_int (*GetError)(void);
    const char *(*QueryString)(og_egl_ptr, og_egl_int);
    unsigned (*glCreateShader)(unsigned);
    void (*glShaderSource)(unsigned, int, const char *const *, const int *);
    void (*glCompileShader)(unsigned);
    void (*glGetShaderiv)(unsigned, unsigned, int *);
    void (*glGetShaderInfoLog)(unsigned, int, int *, char *);
    unsigned (*glCreateProgram)(void);
    void (*glAttachShader)(unsigned, unsigned);
    void (*glBindAttribLocation)(unsigned, unsigned, const char *);
    void (*glLinkProgram)(unsigned);
    void (*glGetProgramiv)(unsigned, unsigned, int *);
    void (*glUseProgram)(unsigned);
    int  (*glGetUniformLocation)(unsigned, const char *);
    void (*glUniform1i)(int, int);
    void (*glUniform1f)(int, float);
    void (*glGenTextures)(int, unsigned *);
    void (*glBindTexture)(unsigned, unsigned);
    void (*glTexParameteri)(unsigned, unsigned, int);
    void (*glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
    void (*glTexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
    void (*glPixelStorei)(unsigned, int);
    void (*glVertexAttribPointer)(unsigned, int, unsigned, unsigned char, int, const void *);
    void (*glEnableVertexAttribArray)(unsigned);
    void (*glDrawArrays)(unsigned, int, int);
    void (*glViewport)(int, int, int, int);
    void (*glClearColor)(float, float, float, float);
    void (*glClear)(unsigned);
    const unsigned char *(*glGetString)(unsigned);
    og_egl_ptr dpy, surf, ctx;
    unsigned prog, tex;
    int tex_w, tex_h, u_bgr, u_scan;
    struct { unsigned short width, height; } fbwin;   /* ARM's fbdev_window */
    int on;
} og_gl;

/* a function from the library, or -- for eglSwapBuffers above all --
 * whatever the process already has by that name, so a preloaded hook
 * (Knulli's overlay) sees the frames */
static void *og_gl_sym(void *lib, const char *name, int global_first)
{
    void *f = global_first ? dlsym(RTLD_DEFAULT, name) : NULL;
    return f ? f : dlsym(lib, name);
}

static unsigned og_gl_shader(unsigned type, const char *src)
{
    unsigned sh = og_gl.glCreateShader(type);
    int ok = 0;
    og_gl.glShaderSource(sh, 1, &src, NULL);
    og_gl.glCompileShader(sh);
    og_gl.glGetShaderiv(sh, OG_GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = "";
        og_gl.glGetShaderInfoLog(sh, (int)sizeof log, NULL, log);
        OG_ERR("[OG_Video] GPU shader: %s", log);
        return 0;
    }
    return sh;
}

static void og_gl_close(void)
{
    if (og_gl.dpy) {
        if (og_gl.MakeCurrent) og_gl.MakeCurrent(og_gl.dpy, NULL, NULL, NULL);
        if (og_gl.ctx)  og_gl.DestroyContext(og_gl.dpy, og_gl.ctx);
        if (og_gl.surf) og_gl.DestroySurface(og_gl.dpy, og_gl.surf);
        og_gl.Terminate(og_gl.dpy);
    }
    if (og_gl.gles) dlclose(og_gl.gles);
    if (og_gl.egl)  dlclose(og_gl.egl);
    memset(&og_gl, 0, sizeof og_gl);
}

/* EGL on the framebuffer, a context, and the one program; 1 if it is
 * all there */
static int og_gl_open(void)
{
    static const char *vs =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() { v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    /* the scanlines darken the lower half of every GAME row (200 of
     * them, whatever the texture's size) to 145/256, as the CPU copy
     * does; highp where there is one, for fract(v * 200) */
    static const char *fs =
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
        "precision highp float;\n"
        "#else\n"
        "precision mediump float;\n"
        "#endif\n"
        "uniform sampler2D u_tex;\n"
        "uniform float u_bgr;\n"
        "uniform float u_scan;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    vec4 c = texture2D(u_tex, v_uv);\n"
        "    if (u_bgr > 0.5) c = c.bgra;\n"
        "    if (u_scan > 0.5 && fract(v_uv.y * 200.0) >= 0.5) c.rgb *= 145.0 / 256.0;\n"
        "    gl_FragColor = vec4(c.rgb, 1.0);\n"
        "}\n";
    static const og_egl_int cfg_attr[] = {
        OG_EGL_RED_SIZE, 5, OG_EGL_GREEN_SIZE, 6, OG_EGL_BLUE_SIZE, 5,
        OG_EGL_SURFACE_TYPE, OG_EGL_WINDOW_BIT,
        OG_EGL_RENDERABLE_TYPE, OG_EGL_OPENGL_ES2_BIT, OG_EGL_NONE
    };
    static const og_egl_int ctx_attr[] = { OG_EGL_CONTEXT_VERSION, 2, OG_EGL_NONE };
    const char *e = SDL_getenv("SORP_GPU");
    og_egl_ptr cfg = NULL;
    og_egl_int n = 0, major = 0, minor = 0;
    unsigned v, f;
    int ok = 0;
    const char *why = "";

    if (e && *e == '0') return 0;
    memset(&og_gl, 0, sizeof og_gl);
    og_gl.egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!og_gl.egl) og_gl.egl = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    og_gl.gles = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
    if (!og_gl.gles) og_gl.gles = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
    if (!og_gl.egl || !og_gl.gles) { why = "no libEGL / libGLESv2"; goto fail; }
    /* A GBM build of the driver -- Rockchip's libmali, the RK3566's -- is
     * KMSDRM's, not the framebuffer's: handed a framebuffer window it may
     * fall over rather than fail.  SORP_GPU=1 tries it all the same. */
    if (dlsym(og_gl.egl, "gbm_create_device") && !(e && *e == '1')) {
        why = "a GBM driver, for KMSDRM"; goto fail;
    }

#define OG_EGL(f)  if (!(*(void **)&og_gl.f = og_gl_sym(og_gl.egl, "egl" #f, 0))) { why = "egl" #f; goto fail; }
#define OG_GLES(f) if (!(*(void **)&og_gl.f = og_gl_sym(og_gl.gles, #f, 0))) { why = #f; goto fail; }
    OG_EGL(GetDisplay) OG_EGL(Initialize) OG_EGL(BindAPI) OG_EGL(ChooseConfig)
    OG_EGL(CreateWindowSurface) OG_EGL(CreateContext) OG_EGL(MakeCurrent)
    OG_EGL(SwapInterval) OG_EGL(DestroySurface) OG_EGL(DestroyContext)
    OG_EGL(Terminate) OG_EGL(GetError) OG_EGL(QueryString)
    OG_GLES(glCreateShader) OG_GLES(glShaderSource) OG_GLES(glCompileShader)
    OG_GLES(glGetShaderiv) OG_GLES(glGetShaderInfoLog) OG_GLES(glCreateProgram)
    OG_GLES(glAttachShader) OG_GLES(glBindAttribLocation) OG_GLES(glLinkProgram)
    OG_GLES(glGetProgramiv) OG_GLES(glUseProgram) OG_GLES(glGetUniformLocation)
    OG_GLES(glUniform1i) OG_GLES(glUniform1f) OG_GLES(glGenTextures)
    OG_GLES(glBindTexture) OG_GLES(glTexParameteri) OG_GLES(glTexImage2D)
    OG_GLES(glTexSubImage2D) OG_GLES(glPixelStorei) OG_GLES(glVertexAttribPointer)
    OG_GLES(glEnableVertexAttribArray) OG_GLES(glDrawArrays) OG_GLES(glViewport)
    OG_GLES(glClearColor) OG_GLES(glClear) OG_GLES(glGetString)
#undef OG_EGL
#undef OG_GLES
    *(void **)&og_gl.SwapBuffers = og_gl_sym(og_gl.egl, "eglSwapBuffers", 1);
    if (!og_gl.SwapBuffers) { why = "eglSwapBuffers"; goto fail; }

    og_gl.dpy = og_gl.GetDisplay(NULL);                  /* EGL_DEFAULT_DISPLAY */
    if (!og_gl.dpy || !og_gl.Initialize(og_gl.dpy, &major, &minor)) {
        og_gl.dpy = NULL; why = "eglInitialize"; goto fail;
    }
    og_gl.BindAPI(OG_EGL_OPENGL_ES_API);
    if (!og_gl.ChooseConfig(og_gl.dpy, cfg_attr, &cfg, 1, &n) || n < 1) {
        why = "eglChooseConfig"; goto fail;
    }
    /* ARM's fbdev: the native window is its size; PowerVR's NULL window
     * system takes none */
    og_gl.fbwin.width  = (unsigned short)og_fb_var.xres;
    og_gl.fbwin.height = (unsigned short)og_fb_var.yres;
    og_gl.surf = og_gl.CreateWindowSurface(og_gl.dpy, cfg, &og_gl.fbwin, NULL);
    if (!og_gl.surf) og_gl.surf = og_gl.CreateWindowSurface(og_gl.dpy, cfg, NULL, NULL);
    if (!og_gl.surf) { why = "eglCreateWindowSurface"; goto fail; }
    og_gl.ctx = og_gl.CreateContext(og_gl.dpy, cfg, NULL, ctx_attr);
    if (!og_gl.ctx) { why = "eglCreateContext"; goto fail; }
    if (!og_gl.MakeCurrent(og_gl.dpy, og_gl.surf, og_gl.surf, og_gl.ctx)) {
        why = "eglMakeCurrent"; goto fail;
    }
    og_gl.SwapInterval(og_gl.dpy, 1);                   /* the screen's refresh */

    v = og_gl_shader(OG_GL_VERTEX_SHADER, vs);
    f = og_gl_shader(OG_GL_FRAGMENT_SHADER, fs);
    if (!v || !f) { why = "shader"; goto fail; }
    og_gl.prog = og_gl.glCreateProgram();
    og_gl.glAttachShader(og_gl.prog, v);
    og_gl.glAttachShader(og_gl.prog, f);
    og_gl.glBindAttribLocation(og_gl.prog, 0, "a_pos");
    og_gl.glBindAttribLocation(og_gl.prog, 1, "a_uv");
    og_gl.glLinkProgram(og_gl.prog);
    og_gl.glGetProgramiv(og_gl.prog, OG_GL_LINK_STATUS, &ok);
    if (!ok) { why = "link"; goto fail; }
    og_gl.glUseProgram(og_gl.prog);
    og_gl.glUniform1i(og_gl.glGetUniformLocation(og_gl.prog, "u_tex"), 0);
    og_gl.u_bgr  = og_gl.glGetUniformLocation(og_gl.prog, "u_bgr");
    og_gl.u_scan = og_gl.glGetUniformLocation(og_gl.prog, "u_scan");
    og_gl.glGenTextures(1, &og_gl.tex);
    og_gl.glBindTexture(OG_GL_TEXTURE_2D, og_gl.tex);
    og_gl.glTexParameteri(OG_GL_TEXTURE_2D, OG_GL_TEXTURE_MIN, OG_GL_NEAREST);
    og_gl.glTexParameteri(OG_GL_TEXTURE_2D, OG_GL_TEXTURE_MAG, OG_GL_NEAREST);
    og_gl.glTexParameteri(OG_GL_TEXTURE_2D, OG_GL_WRAP_S, OG_GL_CLAMP_TO_EDGE);
    og_gl.glTexParameteri(OG_GL_TEXTURE_2D, OG_GL_WRAP_T, OG_GL_CLAMP_TO_EDGE);
    og_gl.glPixelStorei(OG_GL_UNPACK_ALIGNMENT, 4);
    og_gl.on = 1;
    SDL_strlcpy(og_render_path, "GPU gles fb", sizeof og_render_path);
    og_render_gpu = 1;
    OG_INFO("[OG_Video] GPU: EGL %d.%d (%s), %s, %s -- the frame goes to the GPU",
            major, minor, og_gl.QueryString(og_gl.dpy, OG_EGL_VENDOR),
            (const char *)og_gl.glGetString(OG_GL_RENDERER),
            (const char *)og_gl.glGetString(OG_GL_VERSION));
    return 1;
fail:
    OG_WARN("[OG_Video] GPU: not used (%s%s) -- the CPU copies the frame", why,
            og_gl.GetError ? "" : "");
    og_gl_close();
    return 0;
}

/* One frame through the GPU: SDL's surface up as the texture, the quad
 * over the picture's place on the panel, its corners' texture
 * coordinates turned as og_fb_turn_row turns pixels. */
static void og_gl_present(SDL_Surface *s, int scan)
{
    static Uint32 *packed;
    const int xres = (int)og_fb_var.xres, yres = (int)og_fb_var.yres;
    const void *px = s->pixels;
    float q[16];
    int x0, y0, w, h, i;
    const int bgr = (s->format == SDL_PIXELFORMAT_XRGB8888 ||
                     s->format == SDL_PIXELFORMAT_ARGB8888);
    if (!bgr && s->format != SDL_PIXELFORMAT_XBGR8888 &&
        s->format != SDL_PIXELFORMAT_ABGR8888) {
        /* some other format: made RGBA in memory */
        if (!packed) packed = (Uint32 *)malloc((size_t)OG_FB_MAXK * OG_SCREEN_W *
                                               OG_FB_MAXK * OG_SCREEN_H * 4);
        if (!packed) return;
        SDL_ConvertPixels(s->w, s->h, s->format, s->pixels, s->pitch,
                          SDL_PIXELFORMAT_ABGR8888, packed, s->w * 4);
        px = packed;
    } else if (s->pitch != s->w * 4) {                 /* rows padded */
        int y;
        if (!packed) packed = (Uint32 *)malloc((size_t)OG_FB_MAXK * OG_SCREEN_W *
                                               OG_FB_MAXK * OG_SCREEN_H * 4);
        if (!packed) return;
        for (y = 0; y < s->h; ++y)
            memcpy(packed + (size_t)y * s->w,
                   (const unsigned char *)s->pixels + (size_t)y * s->pitch,
                   (size_t)s->w * 4);
        px = packed;
    }
    if (og_gl.tex_w != s->w || og_gl.tex_h != s->h) {
        og_gl.glTexImage2D(OG_GL_TEXTURE_2D, 0, OG_GL_RGBA, s->w, s->h, 0,
                           OG_GL_RGBA, OG_GL_UNSIGNED_BYTE, px);
        og_gl.tex_w = s->w; og_gl.tex_h = s->h;
    } else
        og_gl.glTexSubImage2D(OG_GL_TEXTURE_2D, 0, 0, 0, s->w, s->h,
                              OG_GL_RGBA, OG_GL_UNSIGNED_BYTE, px);

    /* the picture's box on the panel, as og_fb_turn_out finds it */
    if (og_fb_rot & 1) {
        w = og_fb_h; h = og_fb_w;
        x0 = og_fb_rot == 1 ? xres - og_fb_y - og_fb_h : og_fb_y;
        y0 = og_fb_rot == 1 ? og_fb_x : yres - og_fb_x - og_fb_w;
    } else if (og_fb_rot == 2) {
        w = og_fb_w; h = og_fb_h;
        x0 = xres - og_fb_x - og_fb_w; y0 = yres - og_fb_y - og_fb_h;
    } else {
        w = og_fb_w; h = og_fb_h; x0 = og_fb_x; y0 = og_fb_y;
    }
    /* the corners top-left, top-right, bottom-left, bottom-right on the
     * panel: their place in the turned picture gives the texture
     * coordinate (the inverse of og_fb_turn_row's mapping) */
    for (i = 0; i < 4; ++i) {
        const float X = (float)(x0 + ((i & 1) ? w : 0));
        const float Y = (float)(y0 + ((i & 2) ? h : 0));
        float lx, ly;
        switch (og_fb_rot) {
        case 1:  lx = Y;                ly = (float)xres - X; break;
        case 2:  lx = (float)xres - X;  ly = (float)yres - Y; break;
        case 3:  lx = (float)yres - Y;  ly = X;               break;
        default: lx = X;                ly = Y;               break;
        }
        q[i * 4 + 0] = (i & 1) ? 1.0f : -1.0f;          /* the viewport's corner */
        q[i * 4 + 1] = (i & 2) ? -1.0f : 1.0f;
        q[i * 4 + 2] = (lx - (float)og_fb_x) / (float)og_fb_w;
        q[i * 4 + 3] = (ly - (float)og_fb_y) / (float)og_fb_h;
    }
    og_gl.glViewport(0, 0, xres, yres);
    og_gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    og_gl.glClear(OG_GL_COLOR_BUFFER_BIT);
    og_gl.glViewport(x0, yres - y0 - h, w, h);          /* GL counts from the bottom */
    og_gl.glUniform1f(og_gl.u_bgr, bgr ? 1.0f : 0.0f);
    og_gl.glUniform1f(og_gl.u_scan, scan ? 1.0f : 0.0f);
    og_gl.glVertexAttribPointer(0, 2, OG_GL_FLOAT, 0, 16, q);
    og_gl.glVertexAttribPointer(1, 2, OG_GL_FLOAT, 0, 16, q + 2);
    og_gl.glEnableVertexAttribArray(0);
    og_gl.glEnableVertexAttribArray(1);
    og_gl.glDrawArrays(OG_GL_TRIANGLE_STRIP, 0, 4);
    og_gl.SwapBuffers(og_gl.dpy, og_gl.surf);
}

/* After SDL_RenderPresent: the software renderer has finished the frame
 * in the window's surface, 320x200.  Scale it into the hidden page and
 * show that.  Each source row is put into the framebuffer's format and
 * widened in ordinary memory, then copied out: the framebuffer is
 * uncached, so it is only ever written, a whole row at a time. */
static void og_fb_flip(SDL_Renderer *ren)
{
    static unsigned char row[OG_FB_MAXK * OG_SCREEN_W * 4];
    static unsigned char *wide, *dark;       /* one picture row, and darkened */
    static int xmap_w;                       /* the source width og_fb_xmap is for */
    SDL_Surface *s;
    unsigned char *page;
    const size_t pitch = og_fb_pitch;
    const int bpp = (int)og_fb_var.bits_per_pixel / 8;
    const size_t wbytes = (size_t)og_fb_w * (size_t)bpp;
    /* The screen shader's scanlines are drawn here, at the screen's own
     * resolution: the window is 320x200 or 640x400, where half a game
     * row is no row or a single one.  Not over an overlay, which the
     * shader leaves untouched elsewhere. */
    const int scan = og_screen_shader == OG_SHADER_SCANLINES && !og_options &&
                     og_fb_h >= 2 * OG_SCREEN_H;
    int y, x, last = -1, dark_ok = 0;
    if (!og_fb_mem) return;
    s = SDL_GetWindowSurface(SDL_GetRenderWindow(ren));
    if (!s || s->w < OG_SCREEN_W || s->h < OG_SCREEN_H ||
        s->w > OG_FB_MAXK * OG_SCREEN_W) return;
    if (og_gl.on) {                          /* the GPU scales, turns, shades */
        og_gl_present(s, scan);
        goto stats;
    }
    if (!wide && !(wide = (unsigned char *)malloc(wbytes))) return;
    if (!dark && !(dark = (unsigned char *)malloc(wbytes))) return;
    if (xmap_w != s->w) {                    /* 320 or 640 columns across */
        for (x = 0; x < og_fb_w; ++x) og_fb_xmap[x] = x * s->w / og_fb_w;
        xmap_w = s->w;
    }
    if (og_fb_pages > 1 && !og_fb_threaded) {
        sem_init(&og_fb_req, 0, 0);
        sem_init(&og_fb_done, 0, 1);
        og_fb_threaded = pthread_create(&og_fb_thread, NULL, og_fb_panner, NULL) == 0;
        if (!og_fb_threaded) { sem_destroy(&og_fb_req); sem_destroy(&og_fb_done); }
    }
    {   /* the page about to be drawn on must be off the screen */
        Uint64 w0 = SDL_GetTicksNS();
        if (og_fb_threaded) sem_wait(&og_fb_done);
        og_fb_ns_wait += SDL_GetTicksNS() - w0;
    }
    page = og_fb_mem + (size_t)og_fb_page * og_fb_var.yres * pitch;
    for (y = 0; y < og_fb_h; ++y) {
        int sy = y * s->h / og_fb_h;
        /* the lower half of a GAME row, whatever the window's size */
        int lower = scan && ((y * 2 * OG_SCREEN_H / og_fb_h) & 1);
        if (sy != last) {
            SDL_ConvertPixels(s->w, 1, s->format,
                              (const unsigned char *)s->pixels + (size_t)sy * s->pitch,
                              s->pitch, og_fb_format, row, (int)sizeof row);
            if (bpp == 4) {
                const Uint32 *r = (const Uint32 *)row;
                Uint32 *o = (Uint32 *)wide;
                for (x = 0; x < og_fb_w; ++x)
                    o[x] = r[og_fb_xmap[x]] | 0xff000000u;   /* opaque */
            } else {
                const Uint16 *r = (const Uint16 *)row;
                Uint16 *o = (Uint16 *)wide;
                for (x = 0; x < og_fb_w; ++x) o[x] = r[og_fb_xmap[x]];
            }
            last = sy;
            dark_ok = 0;
        }
        if (lower && !dark_ok) {
            /* the desktop's black at 110/255 over the row: each channel
             * kept at 145/256 */
            if (bpp == 4) {
                const Uint32 *i = (const Uint32 *)wide;
                Uint32 *o = (Uint32 *)dark;
                for (x = 0; x < og_fb_w; ++x)
                    o[x] = (((i[x] & 0x00ff00ffu) * 145 >> 8) & 0x00ff00ffu) |
                           (((i[x] & 0x0000ff00u) * 145 >> 8) & 0x0000ff00u) |
                           0xff000000u;
            } else {
                const Uint16 *i = (const Uint16 *)wide;
                Uint16 *o = (Uint16 *)dark;
                for (x = 0; x < og_fb_w; ++x)
                    o[x] = (Uint16)((((i[x] >> 11) * 145 >> 8) << 11) |
                                    ((((i[x] >> 5) & 63) * 145 >> 8) << 5) |
                                    ((i[x] & 31) * 145 >> 8));
            }
            dark_ok = 1;
        }
        if (!og_fb_rot)
            memcpy(page + (size_t)(og_fb_y + y) * pitch + (size_t)og_fb_x * bpp,
                   lower ? dark : wide, wbytes);
        else
            og_fb_turn_row(lower ? dark : wide, og_fb_y + y, bpp);
    }
    if (og_fb_rot) og_fb_turn_out(page, pitch, bpp);
    if (og_fb_pages > 1) {
        og_fb_var.xoffset = 0;
        og_fb_var.yoffset = (unsigned)og_fb_page * og_fb_var.yres;
        if (og_fb_threaded) {
            og_fb_pan = og_fb_var;
            sem_post(&og_fb_req);
        } else {
            ioctl(og_fb_fd, FBIOPAN_DISPLAY, &og_fb_var);
        }
        og_fb_page ^= 1;
    }
stats:
    {   /* the frame rate, every ten seconds */
        static Uint64 t0;
        static int n;
        Uint64 t = SDL_GetTicks();
        if (!t0) t0 = t;
        ++n;
        if (t - t0 >= 10000) {
            OG_DEBUG("[OG_Video] Framebuffer%s: %.1f frames/s, %.1f ms a frame waiting for the screen",
                    og_gl.on ? " (gpu)" : "",
                    n * 1000.0 / (double)(t - t0), og_fb_ns_wait / 1e6 / n);
            t0 = t; n = 0; og_fb_ns_wait = 0;
        }
    }
}

/* The window SDL draws in, on the framebuffer: 320x200, or 640x400 while
 * a shader needs the detail -- Eagle's or MMPX's 2x, the race composed
 * at 2x.  At
 * 320x200 that detail is thrown away before the picture is scaled up. */
static int og_fb_k = 1;
static void og_fb_window_size(SDL_Window *win, int k)
{
    if (!og_fb_mem || k == og_fb_k) return;
    if (SDL_SetWindowSize(win, OG_SCREEN_W * k, OG_SCREEN_H * k)) {
        og_fb_k = k;
        OG_DEBUG("[OG_Video] Framebuffer: drawing at %dx%d", OG_SCREEN_W * k, OG_SCREEN_H * k);
    }
}

/* What the framebuffer's picture can show: 2x detail needs it 640 wide,
 * scanlines two screen rows a game row.  A shader it cannot show is not
 * offered -- its F1 row reads "n/a" -- rather than costing time for
 * nothing. */
static void og_fb_shader_limits(void)
{
    const int two_x = og_fb_w >= OG_FB_MAXK * OG_SCREEN_W;
    const int scan  = og_fb_h >= 2 * OG_SCREEN_H;
    if (!og_fb_mem) return;
    /* the window is at most OG_FB_MAXK times 320x200: MMPX 4x's detail
     * would be thrown away there */
    og_shader_limit(two_x ? OG_SCREEN_SHADERS : scan ? OG_SHADER_EAGLE : 1,
                    two_x ? (OG_FB_MAXK >= 4 ? 2 : 1) : 0);
}

/* black, and the page the system was showing, as SORP found it */
static void og_fb_close(void)
{
    og_gl_close();
    if (og_fb_threaded) {
        sem_wait(&og_fb_done);
        og_fb_quit = 1;
        sem_post(&og_fb_req);
        pthread_join(og_fb_thread, NULL);
        sem_destroy(&og_fb_req);
        sem_destroy(&og_fb_done);
        og_fb_threaded = 0;
    }
    if (og_fb_mem) {
        memset(og_fb_mem, 0, og_fb_len);
        ioctl(og_fb_fd, FBIOPAN_DISPLAY, &og_fb_var0);
        munmap(og_fb_mem, og_fb_len);
        og_fb_mem = NULL;
    }
    free(og_fb_xmap);
    og_fb_xmap = NULL;
    free(og_fb_rotbuf);
    og_fb_rotbuf = NULL;
    if (og_fb_fd >= 0) close(og_fb_fd);
    og_fb_fd = -1;
}
#else
static int  og_fb_mode(void) { return -1; }
static int  og_fb_open(int *w, int *h) { (void)w; (void)h; return 0; }
static void og_fb_flip(SDL_Renderer *ren) { (void)ren; }
static void og_fb_window_size(SDL_Window *win, int k) { (void)win; (void)k; }
static void og_fb_shader_limits(void) { }
static void og_fb_close(void) { }
#endif

/* Under SDL's own KMSDRM driver -- a handheld's whole screen, as on the
 * Miniloong Pocket with its GBM libmali -- everything is drawn into
 * og_screen_rt instead of the window: the largest 4:3 picture the screen
 * holds, as the framebuffer path shows it and the DOS game was seen, with
 * the 320x200 stretched over it.  og_present puts it on the screen
 * centred, and turned og_turn quarters clockwise for a panel mounted on
 * its side (og_panel_rotation), which SDL does not do: 1:1, by the GPU. */
static SDL_Texture *og_screen_rt;
static int          og_turn;

static void og_screen_open(SDL_Renderer *ren)
{
    int w = 0, h = 0, pw, ph;
    const char *vd = SDL_GetCurrentVideoDriver();
    if (!vd || SDL_strcmp(vd, "kmsdrm") != 0) return;
    if (!SDL_GetCurrentRenderOutputSize(ren, &w, &h) || w <= 0 || h <= 0) return;
    og_turn = og_panel_rotation();
    if (og_turn & 1) { const int t = w; w = h; h = t; }
    pw = w;
    if (pw * 3 > h * 4) pw = h * 4 / 3;
    ph = pw * 3 / 4;
    og_screen_rt = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                     SDL_TEXTUREACCESS_TARGET, pw, ph);
    if (!og_screen_rt) { og_turn = 0; return; }
    SDL_SetTextureScaleMode(og_screen_rt, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(og_screen_rt, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(ren, og_screen_rt);
    OG_INFO("[OG_Video] The picture %dx%d on a %dx%d screen%s", pw, ph, w, h,
            og_turn == 1 ? ", turned a quarter clockwise" :
            og_turn == 2 ? ", turned upside down" :
            og_turn == 3 ? ", turned a quarter counter-clockwise" : "");
}

/* the window, or the picture standing in for it */
static void og_screen_target(SDL_Renderer *ren)
{
    SDL_SetRenderTarget(ren, og_screen_rt);
}

/* 320x200 letterboxed in a window; stretched to 4:3 on og_screen_rt */
static void og_screen_logical(SDL_Renderer *ren)
{
    SDL_SetRenderLogicalPresentation(ren, OG_SCREEN_W, OG_SCREEN_H,
                                     og_screen_rt ? SDL_LOGICAL_PRESENTATION_STRETCH
                                                  : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

static void og_present(SDL_Renderer *ren)
{
    if (og_screen_rt) {
        int ow = 0, oh = 0;
        float tw = 0.0f, th = 0.0f;
        SDL_FRect d;
        SDL_SetRenderTarget(ren, NULL);
        SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
        SDL_GetTextureSize(og_screen_rt, &tw, &th);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        /* centred, and turned about its centre */
        d.w = tw; d.h = th;
        d.x = ((float)ow - tw) / 2.0f;
        d.y = ((float)oh - th) / 2.0f;
        SDL_RenderTextureRotated(ren, og_screen_rt, NULL, &d, 90.0 * og_turn,
                                 NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(ren);
        SDL_SetRenderTarget(ren, og_screen_rt);
    } else
        SDL_RenderPresent(ren);
    og_fb_flip(ren);
}

/* The debug console.  SORP is linked as a WINDOWS application so no
 * console appears on its own -- closing one used to take the game with
 * it, since the process owned it.  This opens one on request and
 * reattaches the C streams to it; closing it again detaches cleanly. */
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifdef _MSC_VER
/* GetConsoleWindow is kernel32, but ShowWindow, GetSystemMenu and
 * DeleteMenu are user32 -- which SDL does not pull in for us. */
#pragma comment(lib, "user32.lib")
#endif

/* The console is allocated ONCE and then shown or hidden.  It is never
 * freed, and its close button is removed, because closing a console the
 * process owns sends CTRL_CLOSE_EVENT and Windows then terminates us
 * whatever the handler returns -- which is why the X used to take SORP
 * with it.  With no X to press, the overlay's switch is the way to put
 * it away, and the ctrl handler below catches the other routes (the
 * task bar's close, Ctrl+C) and hides it instead. */
static int og_console_ready = 0;

static BOOL WINAPI og_console_ctrl(DWORD type)
{
    if (type == CTRL_CLOSE_EVENT || type == CTRL_C_EVENT ||
        type == CTRL_BREAK_EVENT) {
        HWND w = GetConsoleWindow();
        if (w) ShowWindow(w, SW_HIDE);
        og_console = 0;          /* the overlay follows suit */
        return TRUE;             /* handled: do NOT end the process */
    }
    return FALSE;
}

static void og_console_set(int on)
{
    static int shown = 0;
    HWND w;

    if (!og_console_ready) {
        /* A new console window takes the focus as it opens; SORP's had
         * it (F1 was pressed there), so hand it straight back and leave
         * the console behind the game. */
        HWND game = GetForegroundWindow();
        if (!on) return;                 /* nothing to do yet */
        if (!AllocConsole()) return;
        {
            FILE *f;
            freopen_s(&f, "CONOUT$", "w", stdout);
            freopen_s(&f, "CONOUT$", "w", stderr);
        }
        SetConsoleTitleA("SORP -- debug console");
        SetConsoleCtrlHandler(og_console_ctrl, TRUE);
        /* take the close box away so the window cannot be destroyed */
        w = GetConsoleWindow();
        if (w) {
            HMENU m = GetSystemMenu(w, FALSE);
            if (m) DeleteMenu(m, SC_CLOSE, MF_BYCOMMAND);
        }
        if (w) ShowWindow(w, SW_SHOWNOACTIVATE);
        if (game) SetForegroundWindow(game);
        og_console_ready = 1;
        shown = 1;
        og_log_echo = 1;
        og_log_replay();
        return;
    }

    if (on == shown) return;
    w = GetConsoleWindow();
    if (w) ShowWindow(w, on ? SW_SHOWNOACTIVATE : SW_HIDE);   /* not focused */
    shown = on;
    og_log_echo = on;
    if (on) og_log_replay();             /* catch up on what was missed */
}
#else
static void og_console_set(int on) { (void)on; }
#endif

/* Each game file as it is checked: a good one goes in the log; one
 * missing or not the DOS release's is named on stdout as well. */
static void og_check_gamedata(const char *name, long bytes, const char *why,
                              void *user)
{
    if (!why) {
        if (user) ++*(int *)user;          /* one more good one */
        OG_DEBUG("[OG_Gamedata: %s] [Offset 0x0 Bytecount 0x%lX] "
                "SHA-256 matches the DOS release", name, (unsigned long)bytes);
        return;
    }
#ifdef _WIN32
    {   /* SORP links as a WINDOWS application, so started from a command
         * prompt it has no stdout and the lines would go nowhere.  Unless
         * stdout was redirected, attach to the prompt's console and write
         * there -- the game is about to exit, so the debug console (which
         * would need a console of its own) is not wanted. */
        static int tried = 0;
        if (!tried) {
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            tried = 1;
            if ((h == NULL || h == INVALID_HANDLE_VALUE ||
                 GetFileType(h) == FILE_TYPE_UNKNOWN) &&
                AttachConsole(ATTACH_PARENT_PROCESS)) {
                FILE *f;
                freopen_s(&f, "CONOUT$", "w", stdout);
                printf("\n");            /* clear of the prompt cmd printed */
            }
        }
    }
#endif
    printf("%s: %s\n", name, why);
    fflush(stdout);
    OG_ERR("[OG_Gamedata: %s] %s", name, why);
}

/* EAGLE, 2x: each source pixel C becomes four, and a corner takes the
 * colour of its three outer neighbours where they agree --
 *
 *     S T U        1 2      1 = S if V == S == T, else C
 *     V C W   ->   3 4      2 = U if T == U == W
 *     X Y Z                 3 = X if V == X == Y
 *                           4 = Z if W == Z == Y
 *
 * with the edges clamped.  `src` is w x h, `dst` 2w x 2h. */
static void og_eagle2x(const Uint32 *src, int w, int h, Uint32 *dst)
{
    int x, y;
#define OG_PX(xx, yy) (src[(((yy) < 0 ? 0 : (yy) >= h ? h - 1 : (yy)) * w) + \
                           ((xx) < 0 ? 0 : (xx) >= w ? w - 1 : (xx))] & 0xffffffu)
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x) {
            Uint32 c = OG_PX(x, y);
            Uint32 sS = OG_PX(x - 1, y - 1), t = OG_PX(x, y - 1), u = OG_PX(x + 1, y - 1);
            Uint32 v = OG_PX(x - 1, y),                           wW = OG_PX(x + 1, y);
            Uint32 xX = OG_PX(x - 1, y + 1), yY = OG_PX(x, y + 1), z = OG_PX(x + 1, y + 1);
            Uint32 *o = dst + (size_t)(2 * y) * (size_t)(2 * w) + 2 * x;
            o[0]         = (v == sS && sS == t)  ? sS : c;
            o[1]         = (t == u && u == wW)   ? u  : c;
            o[2 * w]     = (v == xX && xX == yY) ? xX : c;
            o[2 * w + 1] = (wW == z && z == yY)  ? z  : c;
        }
#undef OG_PX
}

/* Put the finished game frame -- drawn into `rt`, a 320x200 target -- on
 * the window through the screen shader, and leave the window as the
 * target for whatever is drawn over it. */
static void og_shade_present(SDL_Renderer *ren, SDL_Texture *rt,
                             SDL_Texture **big, int mode)
{
    static Uint32 out[OG_SCREEN_W * 2 * OG_SCREEN_H * 2];
    SDL_Surface *grab = NULL;
    SDL_FRect d;
    d.x = 0.0f; d.y = 0.0f;
    d.w = (float)OG_SCREEN_W; d.h = (float)OG_SCREEN_H;

    if (OG_SHADER_2X(mode)) grab = SDL_RenderReadPixels(ren, NULL);
    SDL_SetRenderScale(ren, 1.0f, 1.0f);   /* the target's 4x, if it had it */
    og_screen_target(ren);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    if (grab) {
        SDL_Surface *px = SDL_ConvertSurface(grab, SDL_PIXELFORMAT_XRGB8888);
        SDL_DestroySurface(grab);
        if (px && px->w == OG_SCREEN_W && px->h == OG_SCREEN_H &&
            px->pitch == OG_SCREEN_W * 4) {
            if (mode == OG_SHADER_MMPX)
                og_mmpx2x_px((const Uint32 *)px->pixels, OG_SCREEN_W, OG_SCREEN_H,
                             out, NULL);
            else
                og_eagle2x((const Uint32 *)px->pixels, OG_SCREEN_W, OG_SCREEN_H, out);
            if (!*big) {
                *big = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         OG_SCREEN_W * 2, OG_SCREEN_H * 2);
                if (*big) SDL_SetTextureScaleMode(*big, SDL_SCALEMODE_NEAREST);
            }
            if (*big) {
                SDL_UpdateTexture(*big, NULL, out, OG_SCREEN_W * 2 * 4);
                SDL_RenderTexture(ren, *big, NULL, &d);
                SDL_DestroySurface(px);
                return;
            }
        }
        if (px) SDL_DestroySurface(px);
    }

    SDL_RenderTexture(ren, rt, NULL, &d);          /* as it is */
    /* (the framebuffer draws its own, at the screen's resolution) */
    if (mode == OG_SHADER_SCANLINES && og_window) {
        /* the lower half of each of the 200 rows, darkened */
        static SDL_FRect line[OG_SCREEN_H];
        int y;
        for (y = 0; y < OG_SCREEN_H; ++y) {
            line[y].x = 0.0f;          line[y].y = (float)y + 0.5f;
            line[y].w = (float)OG_SCREEN_W; line[y].h = 0.5f;
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 110);
        SDL_RenderFillRects(ren, line, OG_SCREEN_H);
    }
}

/* The truck as a desktop window's icon.  Windows takes it from the .ico
 * linked in by sorp.rc; X11 and Wayland have no such thing, so the
 * same icon's 256x256 image is built in (truck_icon_png.h) and handed to
 * SDL.  A Wayland desktop shows it if it has the toplevel-icon protocol
 * (KDE Plasma does; GNOME takes icons only from .desktop files). */
static void og_window_icon(SDL_Window *w);

/* ------------------------------------------------------------------ */
/* A CRASH, IN THE LOG                                                 */
/* ------------------------------------------------------------------ */
/*
 * Every line reaches stdout and sorp.log as it is written, so a crash
 * loses nothing -- but the log just stops.  These write one last line,
 * the way the log writes its own, saying what happened and on which
 * screen, then let the crash go on as it would have.  On Linux this runs
 * in a signal handler, so the line is put together by hand -- no
 * formatting, no allocation -- and written with write().
 */

static size_t og_crash_cat(char *d, size_t at, size_t cap, const char *s)
{
    while (s && *s && at + 1 < cap) d[at++] = *s++;
    d[at] = '\0';
    return at;
}

static size_t og_crash_line(char *b, size_t cap, const char *what)
{
    char t[16];
    long s = 0, ms = 0;
    size_t n;
#ifndef _WIN32
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        s = (long)((ts.tv_sec + og_crash_utc) % 86400);
        ms = ts.tv_nsec / 1000000;
    }
#else
    SYSTEMTIME st;
    GetLocalTime(&st);
    s = st.wHour * 3600L + st.wMinute * 60L + st.wSecond;
    ms = st.wMilliseconds;
#endif
    if (s < 0) s += 86400;
    t[0] = '[';
    t[1] = (char)('0' + s / 36000);      t[2] = (char)('0' + s / 3600 % 10);
    t[3] = ':';
    t[4] = (char)('0' + s % 3600 / 600); t[5] = (char)('0' + s % 600 / 60);
    t[6] = ':';
    t[7] = (char)('0' + s % 60 / 10);    t[8] = (char)('0' + s % 10);
    t[9] = '.';
    t[10] = (char)('0' + ms / 100); t[11] = (char)('0' + ms / 10 % 10);
    t[12] = (char)('0' + ms % 10);
    t[13] = ']'; t[14] = ' '; t[15] = '\0';
    n = og_crash_cat(b, 0, cap, t);
    n = og_crash_cat(b, n, cap, "[ERROR] [SORP] Crashed: ");
    n = og_crash_cat(b, n, cap, what);
    n = og_crash_cat(b, n, cap, ", on ");
    n = og_crash_cat(b, n, cap, og_crash_where);
    return og_crash_cat(b, n, cap, "\n");
}

#ifndef _WIN32
static void og_on_crash(int sig)
{
    char b[256];
    const char *what = sig == SIGSEGV ? "SIGSEGV, a bad memory access" :
                       sig == SIGBUS  ? "SIGBUS, a bad memory access" :
                       sig == SIGFPE  ? "SIGFPE, an arithmetic fault" :
                       sig == SIGILL  ? "SIGILL, an illegal instruction" :
                                        "SIGABRT, an abort";
    const size_t n = og_crash_line(b, sizeof b, what);
    if (og_log_echo && write(1, b, n) < 0) { /* nowhere left to say so */ }
    if (og_log_file && write(fileno(og_log_file), b, n) < 0) { }
    signal(sig, SIG_DFL);
    raise(sig);
}
#else
static LONG WINAPI og_on_crash_win(EXCEPTION_POINTERS *x)
{
    char b[256], what[64];
    const DWORD c = x && x->ExceptionRecord ? x->ExceptionRecord->ExceptionCode : 0;
    SDL_snprintf(what, sizeof what, "%s (0x%08lX)",
                 c == EXCEPTION_ACCESS_VIOLATION ? "an access violation" :
                 c == EXCEPTION_STACK_OVERFLOW ? "a stack overflow" :
                 c == EXCEPTION_INT_DIVIDE_BY_ZERO ? "a division by zero" :
                 c == EXCEPTION_ILLEGAL_INSTRUCTION ? "an illegal instruction" :
                 "an exception", (unsigned long)c);
    og_crash_line(b, sizeof b, what);
    if (og_log_echo) { fputs(b, stdout); fflush(stdout); }
    if (og_log_file) { fputs(b, og_log_file); fflush(og_log_file); }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static void og_crash_install(void)
{
#ifndef _WIN32
    signal(SIGSEGV, og_on_crash);
    signal(SIGBUS,  og_on_crash);
    signal(SIGFPE,  og_on_crash);
    signal(SIGILL,  og_on_crash);
    signal(SIGABRT, og_on_crash);
#else
    SetUnhandledExceptionFilter(og_on_crash_win);
#endif
}

static void og_window_icon(SDL_Window *w)
{
#ifndef _WIN32
    const char *vd = SDL_GetCurrentVideoDriver();
    SDL_Surface *icon;
    if (!w || !vd || (SDL_strcmp(vd, "x11") != 0 && SDL_strcmp(vd, "wayland") != 0))
        return;
    icon = SDL_LoadPNG_IO(SDL_IOFromConstMem(og_icon_png, sizeof og_icon_png), true);
    if (!icon) {
        OG_WARN("[OG_Video] The window icon: %s", SDL_GetError());
        return;
    }
    if (SDL_SetWindowIcon(w, icon))
        OG_DEBUG("[OG_Video] The window icon set, %dx%d", icon->w, icon->h);
    else
        OG_DEBUG("[OG_Video] No window icon here: %s", SDL_GetError());
    SDL_DestroySurface(icon);
#else
    (void)w;
#endif
}

/* No game files: say so on the screen and wait for any key, mouse or
 * controller button (or the window's close box).  Drawn on the game's
 * own 320x200 screen, so SDL's 8-pixel debug font is 1/40 of the width,
 * with the heading at twice that; the long line is split to fit. */
static void og_missing_data(SDL_Renderer *ren)
{
    static const struct {
        const char *text; int y, scale; Uint8 r, g, b;
    } line[] = {
        { "Gamedata Not Found!",            56, 2, 255, 210,  60 },
        { "SORP requires the original DOS", 92, 1, 255, 255, 255 },
        { "game files for Super Off Road.", 104, 1, 255, 255, 255 },
        { "Press any button to exit.",     136, 1, 150, 150, 150 }
    };
    SDL_Event ev;
    og_screen_logical(ren);
    for (;;) {
        size_t i;
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        for (i = 0; i < SDL_arraysize(line); ++i) {
            float k = (float)line[i].scale;
            SDL_SetRenderScale(ren, k, k);
            SDL_SetRenderDrawColor(ren, line[i].r, line[i].g, line[i].b, 255);
            SDL_RenderDebugText(ren,
                ((float)OG_SCREEN_W / k - (float)SDL_strlen(line[i].text) * 8.0f) / 2.0f,
                (float)line[i].y / k, line[i].text);
        }
        SDL_SetRenderScale(ren, 1.0f, 1.0f);
        og_present(ren);
        if (!SDL_WaitEvent(&ev)) return;
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            return;
        case SDL_EVENT_KEY_DOWN:
            if (!ev.key.repeat) return;
            break;
        default:
            break;
        }
    }
}

int main(int argc, char **argv)
{
    const char   *dir = (argc > 1) ? argv[1] : "gamedata";
    SDL_Window   *win = NULL;
    int           win_w, win_h;       /* the window, or the framebuffer */
    SDL_Renderer *ren = NULL;
    SDL_Texture  *title_tex = NULL;
    og_image      title;
    og_bank       trucks;
    og_sprite_table sprtab;
    og_route       route;
    og_ai          ai;                /* the game's route-walk state   */
    og_checkpoints cps;               /* this track's checkpoint boxes */
    int            ai_from = 0, ai_to = 0;
    int            reverse = 0;       /* race the track backwards      */
    float          ai_t = 0.0f;
    float          tx = 0.0f, ty = 0.0f;
    /* Cell -> pixel mapping.
     *
     * X IS DERIVED FROM THE CODE.  GAME.EXE at image 0x13ef takes the
     * world X it just stored at [di+0x79] and shifts it right five
     * times into the object's screen record at [si+0x22]:
     *
     *     mov [di+0x79], cx ; mov dx,cx ; shr dx,1 (x5) ; mov [si+0x22],dx
     *
     * So 32 world units per screen pixel.  The contour sampler takes a
     * cell index from the HIGH BYTE (>> 8, i.e. 256 world units), so a
     * cell is 256/32 = 8 pixels.  The grid is therefore 44*8 = 352 px
     * across -- WIDER than the 320 screen, so the playfield is a window
     * onto a larger world, not a grid scaled to fit.
     *
     * Y uses the same 8 units per cell with a two-thirds foreshortening
     * applied for the oblique view -- see OG_Y_FORESHORTEN.  That part
     * is inferred rather than read from the code, so it stays
     * adjustable with - and =. */
    /* The route is placed with the game's own projection (og_project),
     * using each track's origin read from GAME.EXE. */
    int            track_ox = -70, track_oy = 0;
    int            map_dirty = 1;
    /* ` cycles the overlay: waypoints, gates, wheel dots, all three, off */


    track_view    tv;
    unsigned char *raw = NULL;
    size_t         raw_len = 0;
    char           path[1024];
    int            showing_title = 1;
    int            showing_club = 0;   /* the Ironman Club, in attract mode */
    int            shop_dirty = 0;     /* a purchase: redraw the panels      */
    int            race_pending = 0;   /* load the race once the title goes  */
    int            open_shop  = 0;     /* cut to the shop this frame         */
    SDL_Texture   *club_tex = NULL;

    int            track = 0, running = 1, have_truck = 0;
    /* Row-count periodicity puts clean 16-heading rotation sets in
     * roughly idx 250..405 (lag-16 correlation 0.92-0.98 there, weak or
     * negative below 250).  That is ~10 sets of 16; the lower ~250
     * sprites look like other objects -- flags, the flagman, and so on.
     * Start in the strong region rather than at frame 0. */
    og_racer       racers[OG_RACERS];
    og_race_start  grid;
    int            race_phase = 0;     /* 0 on the grid, 1 racing, 2 over */
    og_bank        shopbank, endbank;
    SDL_Texture   *shop_tex = NULL, *end_tex = NULL;
    int            showing_shop = 0, showing_end = 0;
    int            race_winner = -1, race_places = 0;
    int            fm_mode = 0;        /* 0 idle, 1 white flag, 2 chequered */
    int            fm_white = 0;       /* [0x19c] bit 0: a white flag is due */
    int            ri = 0, phys_ticks = 0, tick_i = 0;
    int            truck_colour = OG_TRUCK_COLOUR_RED;
    /* The truck's attitude, from its wheels, as GAME.EXE 0x1de5.  With
     * the heading byte it indexes the game's own frame table. */
    og_truck_attitudes att_tab;
    og_shadows     shadows;            /* ESHADOW.PCG: 32 silhouettes    */
    og_bank        egame;              /* EGAME.PCG: in-game effects     */
    og_effects     fxdef;              /* effect scripts from GAME.EXE   */
    og_fx_obj      fxs[OG_FX_POOL];
    int            fx_key = 0;         /* the truck's own [si+0x18]      */
    SDL_Texture   *screen_tex = NULL;  /* the race, composed (og_screen_compose) */
    SDL_Texture   *screen_tex4 = NULL; /* ... and at 2x or 4x, for the MMPX shaders */
    int            screen_tex4_k = 0;  /* ... which of them                 */
    /* The pickup, running the "BO" script's own timing (image 0x3404). */
    og_spawns      spawns;
    og_scenery     scenery;            /* poles, bunting, barrels        */
    og_sc_state    sc_state[OG_SCENERY_MAX];
    og_shape       truck_box[16];      /* per-heading footprint, 0x52f8  */
    og_shape       pickup_box;         /* the BO shape, DS:0x3260        */
    int            fm_timer = OG_FLAGMAN_WAIT, fm_i = -1, fm_hold = 0;
    int            pk_live = 0, pk_type = 0, pk_timer = OG_PICKUP_WAIT;
    int            pk_px = 0, pk_py = 0, pk_sort = 0;
    long           pk_wx = 0, pk_wy = 0;
    unsigned       pk_seed = 0x1234;
    int            award_nitro = 0, award_cash = 0;   /* [0x48b], [0x48a] */
    /* [0x48a] and [0x48b]: the award climbs through a race and is
     * reset when the next starts, so the first pickup of a race is
     * always worth one nitro or $10,000. */
    SDL_Texture   *egame_tex[OG_EGAME_COUNT];
    int            pitch_i = 2, roll_i = 2, head_b = 0;
    SDL_FPoint     wheel_scr[4];
    float          body_h = 128.0f;    /* [di+0x7d]: mean wheel height (flies)    */
    float          shadow_h = 128.0f;  /* [di+0x7f]: mean ground under the wheels */
    SDL_FPoint     shadow_pt = { 0.0f, 0.0f };  /* where 0x15d0 projects the truck */
    /* per-wheel vertical state: [+4] height, [+6] velocity, 8.8 -> float */
    float          wh[4] = { 128.0f, 128.0f, 128.0f, 128.0f };
    float          wv[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float          phys_acc = 0.0f;
    int            wheels_live = 0, flying = 0;
    float          fly_air = 0.0f;
    og_physics     phys;
    og_perf        perf;
    /* Demo only: the game's speed comes from engine and upgrades, which
     * are not modelled.  A grounded wheel launches only if the ground
     * climbs about 8.5 units in one step at stock shocks, so a slow truck
     * never leaves the ground: below about 100 px/s the wheels stay down,
     * around 120 they lift off crests, and past about 140 the truck crosses
     * the game's own airborne threshold on Blaster. */
    /* [di+0x85]: speed in WORLD units per physics tick, as GAME.EXE 0x1871
     * moves the truck (its sine table peaks at 16384, so the step is exactly
     * speed).  190 is close to the ~186 per tick measured on a live truck;
     * the same in every direction, unlike a speed in screen pixels. */
    int            have_wheels = 0;
    og_audio       audio;
    og_image       sheet, gsheet;      /* F3: attitudes, and a grid page */
    SDL_Texture   *sheet_tex = NULL;
    og_bank        shadowbank;         /* F3: ESHADOW's masks as sprites */
    SDL_Texture   *game_rt = NULL;     /* the frame, for a screen shader  */
    int            game_rt_k = 0;      /* ... drawn at 1x, 2x or 4x         */
    SDL_Texture   *eagle_tex = NULL;   /* Eagle's 640x400 result          */
    int            shading = 0;        /* this frame goes through a shader */
    int            rc;                 /* a loader's result, for the log  */
    int            good = 0;           /* game files that pass their check */
    const Uint64   t_launch = SDL_GetTicksNS();
    /* the race in the log: its frames, from the green light to the flag,
     * the game ticks it ran, and what drew it */
    Uint64         rs_t0 = 0, rs_t1 = 0, rs_prev = 0, rs_worst = 0, rs_late_ns = 0;
    int            rs_frames = 0, rs_late = 0;
    long           rs_ticks = 0;
    char           rs_how[160] = "";
    Uint64         last;

    memset(&tv, 0, sizeof tv);
    memset(&title, 0, sizeof title);
    memset(&trucks, 0, sizeof trucks);
    memset(&sprtab, 0, sizeof sprtab);

    /* Capture the log from the very first line, before anything else
     * happens: the console can be opened later and will replay it.
     * sorp.cfg is read first, quietly, for the log's level and whether
     * sorp.log is written -- so the file starts with the run. */
    SDL_SetLogOutputFunction(og_log_sink, NULL);
    og_set_load_note(og_log_load);
    og_log_prepare();
    og_cfg_load();                         /* sorp.cfg, F1's settings */
    audio_mode = og_sound_sel + 1;
    og_dir = dir;                          /* F1's arcade rows look there */
    og_tp_detect();                        /* ...and are shown if it has them */
    og_log_env = og_log_level_named(SDL_getenv("SORP_LOG"));
    og_log_apply();
    if (og_log_write) og_log_file_start();
    /* the first line: which SORP this is, the same stamp as F1's */
    OG_INFO("[SORP] Super Off Road Port - Build %s - 2026 dopaz",
            og_build_stamp() + 6);           /* past "build " */
    og_log_start(argc > 0 ? argv[0] : NULL, dir);
    og_crash_install();

    /* The display: SDL's own drivers first -- Windows, X11, Wayland,
     * KMSDRM -- and failing those the Linux framebuffer (see og_fb_open),
     * where SDL renders in software, offscreen, at 320x200, and SORP scales
     * each frame onto the screen.  Whether a device has a /dev/dri says
     * little: the H700's has none, the A133's has one but no GBM for SDL. */
    /* KMSDRM's atomic page flips, as SDL 3.4.16 does them, leak whenever
     * a commit fails: KMSDRM_GLES_SwapWindowFenced returns with the
     * GPU's fence fd still open and the screen buffer it locked never
     * released, and the GPU driver makes another.  On the RG DS one in
     * forty frames or so fails, and within minutes the 1024 files a
     * process may hold are spent -- a track then cannot be opened, and
     * the race runs on black.  The legacy flips SDL2 always used wait
     * for each flip and release the buffer either way.  Knulli starts
     * EmulationStation, and so every port, with SDL_KMSDRM_ATOMIC=1,
     * which outranks an ordinary hint: this one overrides it.
     * SORP_KMSDRM_ATOMIC=1 leaves SDL's choice alone, for testing. */
    if (!SDL_GetHintBoolean("SORP_KMSDRM_ATOMIC", false))
        SDL_SetHintWithPriority(SDL_HINT_KMSDRM_ATOMIC, "0", SDL_HINT_OVERRIDE);
    {
        int fb = og_fb_mode();
        for (;;) {
            win_w = OG_SCREEN_W * SCALE;
            win_h = OG_SCREEN_H * SCALE;
            if (fb > 0) {
                if (!og_fb_open(&win_w, &win_h)) {
                    OG_ERR("[OG_Video] No framebuffer SORP can use either");
                    return 1;
                }
                SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
                SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
            }
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
                /* KMSDRM makes the window's EGL surface at once, for the
                 * GL it is configured for -- desktop GL in a build that
                 * has it, unless SDL was built for a Pi or Rockchip.  A
                 * handheld's driver (the RK3566's libmali) has only GLES,
                 * so ask for GLES 2, which the renderer then uses. */
                const char *vd = SDL_GetCurrentVideoDriver();
                /* a desktop window opens at its kept size, hidden until
                 * it is moved to its kept place */
                const int desk = fb <= 0 && !(vd && SDL_strcmp(vd, "kmsdrm") == 0);
                if (desk && og_win_w > 0) { win_w = og_win_w; win_h = og_win_h; }
                /* Wayland neither tells a window where it is (SDL says
                 * 0, 0) nor lets it choose: keep only the size there, and
                 * leave a kept position (from X11, say) as it is */
                og_win_no_xy = desk && vd && SDL_strcmp(vd, "wayland") == 0;
                const int place = desk && og_win_have_xy && !og_win_no_xy;
                if (vd && SDL_strcmp(vd, "kmsdrm") == 0) {
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                        SDL_GL_CONTEXT_PROFILE_ES);
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
                }
                /* KMSDRM puts the window on the display its position is
                 * on, when it is made: on EmulationStation's screen */
                const SDL_DisplayID kms_on =
                    (vd && SDL_strcmp(vd, "kmsdrm") == 0) ? og_kms_display() : 0;
                const SDL_WindowFlags wflags =
                    SDL_WINDOW_RESIZABLE |
                    (og_fullscreen && fb <= 0 ? SDL_WINDOW_FULLSCREEN : 0) |
                    (place ? SDL_WINDOW_HIDDEN : 0);
                if (kms_on) {
                    SDL_PropertiesID pr = SDL_CreateProperties();
                    SDL_SetStringProperty(pr, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                                          "SORP - Super Off Road Port");
                    SDL_SetNumberProperty(pr, SDL_PROP_WINDOW_CREATE_X_NUMBER,
                                          SDL_WINDOWPOS_CENTERED_DISPLAY(kms_on));
                    SDL_SetNumberProperty(pr, SDL_PROP_WINDOW_CREATE_Y_NUMBER,
                                          SDL_WINDOWPOS_CENTERED_DISPLAY(kms_on));
                    SDL_SetNumberProperty(pr, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, win_w);
                    SDL_SetNumberProperty(pr, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, win_h);
                    SDL_SetNumberProperty(pr, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                                          (Sint64)wflags);
                    win = SDL_CreateWindowWithProperties(pr);
                    SDL_DestroyProperties(pr);
                    ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
                    if (win && !ren) { SDL_DestroyWindow(win); win = NULL; }
                }
                if (kms_on ? win != NULL
                           : SDL_CreateWindowAndRenderer("SORP - Super Off Road Port",
                                                         win_w, win_h, wflags,
                                                         &win, &ren)) {
                    if (place) {
                        og_win_place(win);
                        SDL_ShowWindow(win);
                    }
                    if (!desk) og_win_w = og_win_have_xy = 0;  /* not ours to keep */
                    break;
                }
            }
            /* a warning while the framebuffer is still to be tried */
            SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION,
                           fb != 0 ? SDL_LOG_PRIORITY_ERROR : SDL_LOG_PRIORITY_WARN,
                           "[OG_Video] Failed%s: %s", fb > 0 ? " (framebuffer)" : "",
                           SDL_GetError());
            SDL_Quit();
            if (fb != 0) {
                og_fb_close();
                return 1;
            }
            fb = 1;                        /* SDL found nothing: fb0 */
        }
        /* F11 and F1's row work on a real window; the framebuffer's
         * offscreen one is not shown, and the picture fills the screen */
        og_window = (fb > 0) ? NULL : win;
        if (og_window) {
            const char *rn = SDL_GetRendererName(ren);
            og_render_gpu = rn && SDL_strcmp(rn, "software") != 0;
            SDL_snprintf(og_render_path, sizeof og_render_path, "%s %s",
                         og_render_gpu ? "GPU" : "CPU", rn ? rn : "?");
            og_window_icon(og_window);
        }
        og_fullscreen = !og_window ||
            (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
        og_fb_shader_limits();
    }
    og_screen_open(ren);
    og_ev_advance = SDL_RegisterEvents(1);
    OG_INFO("[OG_Video] Driver %s, renderer %s, %dx%d, on %s", SDL_GetCurrentVideoDriver(),
            SDL_GetRendererName(ren), win_w, win_h,
            SDL_GetDisplayName(SDL_GetDisplayForWindow(win)));

    /* The original is 320x200 shown on a 4:3 screen, so its pixels are
     * taller than they are wide.  A window letterboxes it with square
     * pixels; a handheld's whole screen -- the framebuffer, KMSDRM
     * (og_screen_open) -- shows it 4:3. */
    SDL_InitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK);
    og_input_open();

    /* Every file SORP reads must be there and be the DOS release's own,
     * byte for byte: each is checked against its SHA-256, and any that
     * fails is named on stdout. */
    if (og_gamedata_verify(dir, og_check_gamedata, &good) != 0) {
        OG_ERR("[OG_Gamedata] The Super Off Road data in \"%s\" is missing or wrong.  "
                "Put the original files there, or pass the directory as "
                "the first argument.", dir);
        og_missing_data(ren);
        og_input_close();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        og_fb_close();
        SDL_Quit();
        og_log_close();
        return 1;
    }
    OG_INFO("[OG_Gamedata] All %d files match the DOS release (SHA-256)", good);

    og_screen_logical(ren);

    /* ---- title screen ---- */
    if (og_resolve(dir, "TITLES.XX", path, sizeof path) == OG_OK &&
        og_read_file(path, &raw, &raw_len) == OG_OK) {
        if (og_xx_decode(raw, raw_len, OG_F_CHECK_LENGTH, &title) == OG_OK) {
            og_load_note("TITLES.XX", 0, (long)raw_len,
                         "Loaded the title screen, %dx%d", title.w, title.h);
            title_tex = texture_from_kept(ren, &title, og_pal_title, -1, OG_LAYER_BACK);
        }
        free(raw);
    }
    if (!title_tex) og_load_failed("TITLES.XX", OG_E_MAGIC, "no title screen");
    rc = og_pcspk_load(dir, &pcspk);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no PC speaker effects");
    /* SORP's own IRONHI.BIN beside sorp.exe (og_club_open) */
    rc = og_club_open(dir);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no Ironman Club");
    rc = og_styles_load(dir, &styles);
    if (rc != OG_OK) og_load_failed("EGA.EXE", rc, "no text styles");
    rc = og_title_bank_load(dir, &titlebank);
    if (rc != OG_OK) og_load_failed("ETITLE.PCG", rc, "no title pieces");
    rc = og_pages_load(dir, &pages);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no screen pages");
    rc = og_fontfile_load(dir, "IRONFONT.XX", &ironfont);
    if (rc != OG_OK) og_load_failed("IRONFONT.XX", rc, "no text font");

    /* ---- truck sprites ---- */
    /* ETRUCK is a table-driven bank of VARIABLE-height sprites, not a
     * uniform grid.  EGA.EXE holds 412 records giving each sprite's
     * height and hotspot; a sprite spans 2*height rows of 9 bytes
     * (3 planes x 3 bytes = 24 px).  See og_data.h for how this was
     * established.  Complement is ON: background decodes to 15. */
    rc = og_sprite_table_load(dir, "EGA.EXE", OG_SPRTAB_ETRUCK, &sprtab);
    if (rc != OG_OK) og_load_failed("EGA.EXE", rc, "no sprite table, no trucks");
    else {
        rc = og_bank_load_varh(dir, "ETRUCK.PCG", &sprtab, OG_ETRUCK_CELL_BYTES,
                               OG_ETRUCK_PLANES, OG_ETRUCK_KEY, 0, &trucks);
        if (rc == OG_OK && trucks.nframes > 0) have_truck = 1;
        else og_load_failed("ETRUCK.PCG", rc != OG_OK ? rc : OG_E_SHORT, "no trucks");
    }

    og_image_init(&sheet, OG_SCREEN_W, OG_SCREEN_H);
    /* a grid page leaves a line free above and below for the captions */
    og_image_init(&gsheet, OG_SCREEN_W, OG_SCREEN_H - 2 * OG_SPR_CAPTION);
    memset(&shadowbank, 0, sizeof shadowbank);

    rc = og_flagmap_load(dir, og_flagmap);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no country flags");
    rc = og_fontfile_load(dir, "ROADFONT.XX", &roadfont);
    if (rc != OG_OK) og_load_failed("ROADFONT.XX", rc, "no scoreboard font");
    rc = og_hud_load(dir, &hud);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no scoreboard layout");
    rc = og_ladder_load(dir, &ladder);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no difficulty ladders");
    rc = og_slopes_load(dir, &slopes);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no slope limits");
    rc = og_surface_load(dir, &g_surface);
    if (rc != OG_OK) og_load_failed("POLY.BIN", rc, "POLY.BIN or QUAD.BIN: no elevation");

    og_audio_open(&audio, dir);
    og_audio_p = &audio;               /* for the F2 overlay's tests */
    og_ovl_volume();                   /* the master volume's default */

    /* The title screen plays the Speed Shop's tune, song 1 -- the one
     * the S1 object starts with op 55.  This has to come AFTER the device
     * is open: og_music_select returns at once while a->ready is false,
     * so asking earlier dropped the request silently. */
    og_music_select(&audio, OG_MUSIC_SHOP);

    /* The TRACK is not loaded here either: the title comes up first, and
     * the track and its race load together when the title makes way. */
    /* The race is NOT started here.  The title screen comes up first and
     * the race is loaded when it makes way for one -- the original loads
     * titles.xx and starts song 1 (0x2468), and a track only later. */
    race_pending = 1;
    og_tp_sync(dir);                   /* F1's arcade rows, from sorp.cfg */
    if (og_load_grid(dir, OG_LAYOUT(track, reverse), &grid) != OG_OK)
        memset(&grid, 0, sizeof grid);
    {   /* race 0's layout, from the schedule (0x01ed) */
        unsigned lay = og_sched_layout(race_number);
        track   = lay & 7;
        reverse = (lay & 8) ? 1 : 0;
        og_tp_use_lay(lay);
    }
    {   /* 0x12ae, once: every truck starts with no nation, country 8 --
         * the signature banner for a flag and song 10 for an anthem */
        int z;
        for (z = 0; z < OG_RACERS; ++z) racers[z].country = OG_COUNTRY_NONE;
    }
    racers_reset_ex(racers, &route, 1);   /* a new season: full tanks */
    og_ai_seed_levels(racers, OG_RACERS, og_attract, cpu_base,
                      track, race_number);   /* 0x2105 */
    award_nitro = award_cash = 0;   /* [0x48a], [0x48b] */
    /* 0x2132 re-bases the field from the player's average lap
     * time before each race; with none timed yet it stays at 6. */
    cpu_base = og_ladder_level(&ladder, race_number,
                               og_ladder_avg(player_ticks, player_laps));
    /* NOT moved on: this field only stands behind the title, and the
     * first demo race (or game) is race 0 */
    wheels_live = 0;

    rc = og_physics_load(dir, &phys);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no shock physics");
    memset(&shadows, 0, sizeof shadows);
    memset(&egame, 0, sizeof egame);
    rc = og_shadows_load(dir, &shadows);
    if (rc != OG_OK) og_load_failed("ESHADOW.PCG", rc, "no shadows");
    rc = og_egame_load(dir, &egame);
    if (rc != OG_OK) og_load_failed("EGAME.PCG", rc, "no effects sprites");
    memset(fxs, 0, sizeof fxs);
    rc = og_perf_load(dir, &perf);
    if (rc != OG_OK) {
        og_load_failed("GAME.EXE", rc, "no truck performance tables");
        memset(&perf, 0, sizeof perf);
    }
    memset(&shopbank, 0, sizeof shopbank);
    rc = og_shop_load(dir, &shopbank);
    if (rc != OG_OK) og_load_failed("ESHOP.PCG", rc, "no Speed Shop sprites");
    memset(&endbank, 0, sizeof endbank);
    rc = og_end_load(dir, &endbank);
    if (rc != OG_OK) og_load_failed("EEND.PCG", rc, "no results screen sprites");
    rc = og_truck_shapes_load(dir, truck_box);
    if (rc != OG_OK) {
        og_load_failed("GAME.EXE", rc, "no truck collision boxes");
        memset(truck_box, 0, sizeof truck_box);
    }
    pickup_box.x0 = -4; pickup_box.xn = 8;    /* DS:0x3260 */
    pickup_box.y0 = -4; pickup_box.yn = 8;
    memset(egame_tex, 0, sizeof egame_tex);
    rc = og_effects_load(dir, &fxdef);
    if (rc != OG_OK) {
        og_load_failed("GAME.EXE", rc, "no effect scripts");
        memset(&fxdef, 0, sizeof fxdef);
    }

    rc = og_truck_attitudes_load(dir, &att_tab);
    if (rc != OG_OK) og_load_failed("GAME.EXE", rc, "no attitude table, trucks use frame 0");
    OG_INFO("[OG_Init] Ready in %.0f ms: the title screen comes up",
            (double)(SDL_GetTicksNS() - t_launch) / 1e6);

    last = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        Uint64 now = SDL_GetTicks();
        float dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = 0;
                og_exit_why = "the window or the system closed it";
            }
            og_input_hotplug(&ev);             /* pads plugged in or out */
            /* ADVANCE: move the screen on -- pushed by the Speed Shop when
             * every player is ready, by a player joining in attract mode,
             * by the attract cycle, and by F4's "next stage".  No key
             * does this; ENTER is not a key of the game. */
            if (ev.type == og_ev_advance && og_ev_advance) {
                if (showing_end) {           /* on to the Speed Shop */
                    og_end_to_shop = 1;      /* the timer's effect   */
                } else if (race_phase == 1) {
                    /* End the race here and now: the leader takes it,
                     * and the flagman does what he does at a real
                     * finish -- silence the effects, sound the
                     * fanfare (effect 3) and wave the chequered
                     * flag, which brings up the results screen. */
                    int q, lead;
                    og_race_positions(racers, OG_RACERS);
                    lead = og_standing[0];
                    /* the prizes are paid with the podium, from the
                     * final sort, as 0x371a pays them */
                    for (q = 0; q < OG_RACERS; ++q) racers[q].finished = 1;
                    race_places = OG_RACERS;
                    race_winner = lead;
                    race_phase  = 2;
                    fm_mode = 2; fm_i = 0; fm_hold = 0;
                    og_music_end(&audio);
                    og_audio_silence(&audio);
                    og_audio_trigger(&audio, 3);
                    og_arc_race_off(&audio, 1);           /* 0x4dc6 */
                    OG_INFO("[OG_Race] Race ended early -- truck %d takes it", lead);
                } else if (showing_shop) {   /* START NEXT RACE */
                    unsigned lay;
                    showing_shop = 0;
                    /* the first race is under way: next visit, shop */
                    memset(og_get_ready, 0, sizeof og_get_ready);
                    if (shop_tex) { SDL_DestroyTexture(shop_tex); shop_tex = NULL; }
                    /* 0x01ed: the race number indexes DS:0x1a3 and the
                     * byte IS the layout -- the track in its low three
                     * bits and the REVERSE flag in bit 3.  SORP kept
                     * racing whatever track it had, so a championship
                     * never moved on. */
                    og_tp_sync(dir);
                    lay     = og_sched_layout(race_number);
                    track   = lay & 7;
                    reverse = (lay & 8) ? 1 : 0;
                    og_tp_use_lay(lay);
                    race_pending = 0;          /* this IS the race it waited for */
                    if (track_view_load(&tv, ren, dir, track) != OG_OK)
                        OG_ERR("[OG_Race] Could not load track %d", track);
                    og_load_origin(dir, track, &track_ox, &track_oy);
                    if (og_load_spawns(dir, track, reverse, &spawns) != OG_OK)
                        spawns.loaded = 0;
                    if (og_load_scenery(dir, OG_LAYOUT(track, reverse),
                                        &scenery) != OG_OK)
                        scenery.loaded = 0;
                    OG_INFO("[OG_Race] Race %d: track %d %s", race_number, track,
                            reverse ? "reversed" : "forwards");
                    race_start(dir, track, reverse, &route, &cps, &ai,
                               &ai_from, &ai_to, &ai_t);
                    if (!showing_title) { /* the title keeps song 1 */
                        og_arc_race_start(&audio, racers);
                        og_music_select(&audio, OG_MUSIC_TRACK1 + track);
                    }
                    if (og_load_grid(dir, OG_LAYOUT(track, reverse), &grid) != OG_OK)
                        memset(&grid, 0, sizeof grid);
                    racers_reset(racers, &route);
                    og_ai_seed_levels(racers, OG_RACERS, og_attract, cpu_base,
                                      track, race_number);  /* 0x2105 */
                    award_nitro = award_cash = 0;   /* [0x48a], [0x48b] */
                    /* 0x2132 re-bases the field from the player's average lap
                     * time before each race; with none timed yet it stays at 6. */
                    cpu_base = og_ladder_level(&ladder, race_number,
                                               og_ladder_avg(player_ticks, player_laps));
                    race_number = og_race_next(race_number);
                    memset(sc_state, 0, sizeof sc_state);
                    race_phase = 0; race_winner = -1; race_places = 0;
                    fm_mode = 0; fm_white = 0; fm_timer = OG_FLAGMAN_WAIT; fm_i = -1;
                    wheels_live = 0;
                } else
                    showing_title = 0;
                continue;
            }
            /* Guide and Start together -- Hotkey + Start on a handheld,
             * the way its other games are left -- quit at once.  So do
             * Back and Start, Select + Start on a handheld: some have no
             * Hotkey (Menu) button.  Checked before Back's own press, so
             * Start then Select quits without the options opening;
             * Select first opens them, and Start then quits. */
            if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                (ev.gbutton.button == SDL_GAMEPAD_BUTTON_START ||
                 ev.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE ||
                 ev.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)) {
                SDL_Gamepad *g = SDL_GetGamepadFromID(ev.gbutton.which);
                if (g && SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_START)) {
                    const char *why =
                        SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_GUIDE) ? "Guide + Start" :
                        SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_BACK)  ? "Back + Start"  : NULL;
                    if (why) {
                        running = 0;
                        og_exit_why = why;
                        continue;
                    }
                }
            }
            /* Start answers "Exit SORP?" as ESC does: a pad has no ESC */
            if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                ev.gbutton.button == SDL_GAMEPAD_BUTTON_START && og_quit_ask) {
                if (og_quit_ask == 2) {
                    og_quit_ask = 0;
                    og_title_req = 1;
                    continue;
                }
                running = 0;
                og_exit_why = "\"Exit SORP?\" answered with Start";
                continue;
            }
            /* the desktop window moved or was resized: keep its place */
            if ((ev.type == SDL_EVENT_WINDOW_MOVED ||
                 ev.type == SDL_EVENT_WINDOW_RESIZED) && og_window && !og_screen_rt) {
                og_win_track(og_window);
                continue;
            }
            /* the window's full screen changed, by F11 or by the desktop */
            if (ev.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
                ev.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
                const int fs = ev.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
                if (fs != og_fullscreen) {
                    og_fullscreen = fs;
                    OG_INFO("[OG_Video] Full screen %s", fs ? "on" : "off");
                    og_cfg_save();
                }
                continue;
            }
            if (ev.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED && og_window)
                OG_DEBUG("[OG_Video] The window is on %s now",
                         SDL_GetDisplayName(SDL_GetDisplayForWindow(og_window)));
            if (ev.type == SDL_EVENT_WINDOW_MINIMIZED || ev.type == SDL_EVENT_WINDOW_RESTORED)
                OG_DEBUG("[OG_Video] The window is %s",
                         ev.type == SDL_EVENT_WINDOW_MINIMIZED ? "minimised" : "restored");
            /* A pad's Back button -- Select on a handheld, View on a PC
             * pad -- is F1: the options overlay opens and closes. */
            /* "About": Start or Enter opens its link in the system's
             * browser, if it has one; any other key or button puts it
             * away, Back and ESC too.  Guide is left alone, being half of
             * Hotkey + Start.  The players' controls are seen to in
             * og_input_read. */
            if (og_about && ((ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat &&
                              (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER)) ||
                             (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                              ev.gbutton.button == SDL_GAMEPAD_BUTTON_START))) {
                if (SDL_OpenURL(OG_ABOUT_URL)) OG_INFO("[OG_Overlay] About: opened %s", OG_ABOUT_URL);
                else OG_WARN("[OG_Overlay] About: cannot open %s: %s", OG_ABOUT_URL, SDL_GetError());
                continue;
            }
            if (og_about && ((ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) ||
                             (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                              ev.gbutton.button != SDL_GAMEPAD_BUTTON_GUIDE))) {
                og_about = 0;
                continue;
            }
            if (og_about && ev.type == SDL_EVENT_KEY_DOWN) continue;
            if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                ev.gbutton.button == SDL_GAMEPAD_BUTTON_BACK &&
                !og_quit_ask && !og_club_ask) {
                og_ovl_toggle(1);
                OG_INFO("[OG_Overlay] Options %s (Back)", og_options == 1 ? "open" : "closed");
                continue;
            }
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                /* With "Exit SORP?" showing, ESC leaves and ANY other
                 * key puts it away without doing anything else -- a new
                 * press, not the repeat of ENTER held on F1's row. */
                if (og_quit_ask && ev.key.key != SDLK_ESCAPE) {
                    if (!ev.key.repeat) og_quit_ask = 0;
                    continue;
                }
                /* "Save HiScore table (Y/N)" (0x34c4): it waits for Y or
                 * N and nothing else.  Y saves whether or not the file
                 * exists (al = 1); either answer puts the club screen up
                 * again from the start (0x3487). */
                if (og_club_ask && ev.key.key != SDLK_ESCAPE) {
                    if (ev.key.key == SDLK_Y) {
                        int rc = og_hiscore_save(&club, og_club_path);
                        OG_INFO("[OG_Club] The Ironman Club saved to %s: %s",
                                og_rel(og_club_path), og_strerror(rc));
                        og_club_tp_save();
                    }
                    if (ev.key.key == SDLK_Y || ev.key.key == SDLK_N) {
                        og_club_ask = 0;
                        og_club_changed = 1;      /* redraw, timer restarts */
                        og_stage_t = 0.0f;
                    }
                    continue;
                }
                /* 0x34ac: Ctrl+S on the club screen asks to save */
                if (showing_club && ev.key.key == SDLK_S &&
                    (SDL_GetModState() & SDL_KMOD_CTRL)) {
                    og_club_ask = 1;
                    og_club_changed = 1;
                    continue;
                }
                /* With an overlay up only F1 to F4, F10, F11 and ESC get
                 * through: every other key belongs to the overlay or to
                 * nothing. */
                if (og_options && ev.key.key != SDLK_F1 &&
                                  ev.key.key != SDLK_F2 &&
                                  ev.key.key != SDLK_F3 &&
                                  ev.key.key != SDLK_F4 &&
                                  ev.key.key != SDLK_F10 &&
                                  ev.key.key != SDLK_F11 &&
                                  ev.key.key != SDLK_ESCAPE) continue;
                switch (ev.key.key) {
                case SDLK_ESCAPE:
                    /* ESC asks first.  A second ESC leaves; anything
                     * else carries on. */
                    if (ev.key.repeat) break;
                    if (og_quit_ask == 2) {    /* F10's: back to the title */
                        og_quit_ask = 0;
                        og_title_req = 1;
                    } else if (og_quit_ask) {
                        running = 0;
                        og_exit_why = "\"Exit SORP?\" answered with ESC";
                    } else og_quit_ask = og_quit_held = 1;
                    break;
                case SDLK_F10:
                    /* 0x032c: F10 counts only while F1 has the race
                     * paused; main sees whether a race with players is
                     * what is paused */
                    /* SORP asks first, as ESC does before it exits */
                    if (!ev.key.repeat && og_options == 1 && !og_attract &&
                        !showing_title && !showing_shop && !showing_end &&
                        !showing_club) {
                        og_options = 0;
                        og_quit_ask = 2;
                        og_quit_held = 1;
                    }
                    break;
                case SDLK_F11:
                    if (ev.key.repeat) break;
                    og_fullscreen = !og_fullscreen;
                    og_ovl_fullscreen();
                    break;
                case SDLK_F1: og_ovl_toggle(1);
                    OG_INFO("[OG_Overlay] Options %s", og_options == 1 ? "open" : "closed");
                    break;
                case SDLK_F2: og_ovl_toggle(2);
                    OG_INFO("[OG_Overlay] Audio overlay %s", og_options == 2 ? "open" : "closed");
                    break;
                case SDLK_F3: og_ovl_toggle(3);
                    og_spr_dirty = 1;
                    OG_INFO("[OG_Overlay] Sprite viewer %s", og_options == 3 ? "open" : "closed");
                    break;
                case SDLK_F4: og_ovl_toggle(4);
                    OG_INFO("[OG_Overlay] Debug overlay %s", og_options == 4 ? "open" : "closed");
                    break;
                default: break;
                }
            }
        }

        /* F10 with the race paused (0x032c), and "Back to the title?"
         * answered: the game ends where it stands and the title comes up
         * (0x01d0 -> 0x00d9 -> 0x2468) */
        if (og_title_req) {
            og_title_req = 0;
            if (!og_attract && !showing_title && !showing_shop &&
                !showing_end && !showing_club) {
                int p;
                og_music_end(&audio);                  /* 0x3ac8 */
                og_arc_race_off(&audio, 0);
                for (p = 0; p < OG_PLAYERS; ++p) {     /* no op 85: no club */
                    og_joined[p] = 0;
                    racers[p].computer   = 1;
                    racers[p].game_over  = 0;
                    racers[p].continue_t = 0;
                    racers[p].club_ok    = 0;
                    og_regs[p].step = OG_REG_NONE;
                    og_regs[p].initials[0] = '\0';
                    og_shop_over[p] = 0;
                }
                og_options = 0;                        /* the pause is over */
                race_phase = 2;
                og_end_to_shop = 0;
                og_join_pending = 0;
                og_attract  = 1;                       /* 0x00d9 */
                race_number = 0;                       /* 0x1e4 */
                og_standings_reset(racers);            /* ...0x12f9 */
                showing_title = 1;                     /* 0x2468 */
                og_stage   = OG_ATTRACT_TITLE;
                og_stage_t = 0.0f;
                og_music_select(&audio, OG_MUSIC_SHOP);
                OG_INFO("[OG_Game] \"Back to the title?\" answered: the game ends, the title comes up");
            }
        }

        /* LOAD A RACE: F4's track and direction rows, and the attract
         * cycle leaving the title, ask for a track and a direction here.
         * The field is reset and the race starts from the grid.  Not
         * while the title is up -- the title keeps its own screen. */
        if (og_race_req) {
            og_race_req = 0;
            if (!showing_title) {
                track   = og_req_track % OG_NUM_TRACKS;
                reverse = og_req_reverse ? 1 : 0;
                race_pending = 0;              /* this IS the race it waited for */
                og_tp_sync(dir);
                if (og_req_tp >= 0) og_tp_use(og_req_tp ? TP_TRACKPAK : TP_ARCADE);
                track_view_load(&tv, ren, dir, track);
                og_load_origin(dir, track, &track_ox, &track_oy);
                if (og_load_spawns(dir, track, reverse, &spawns) != OG_OK) spawns.loaded = 0;
                if (og_load_scenery(dir, OG_LAYOUT(track, reverse), &scenery) != OG_OK)
                    scenery.loaded = 0;
                memset(sc_state, 0, sizeof sc_state);
                fm_timer = OG_FLAGMAN_WAIT; fm_i = -1;
                pk_live = 0; pk_timer = OG_PICKUP_WAIT;
                map_dirty = 1;
                race_start(dir, track, reverse, &route, &cps, &ai,
                           &ai_from, &ai_to, &ai_t);
                og_arc_race_start(&audio, racers);          /* 0x4edf */
                og_music_select(&audio, OG_MUSIC_TRACK1 + track);
                if (og_load_grid(dir, OG_LAYOUT(track, reverse), &grid) != OG_OK)
                    memset(&grid, 0, sizeof grid);
                racers_reset(racers, &route);
                og_ai_seed_levels(racers, OG_RACERS, og_attract, cpu_base,
                                  track, race_number);  /* 0x2105 */
                award_nitro = award_cash = 0;   /* [0x48a], [0x48b] */
                /* 0x2132 re-bases the field from the player's average lap
                 * time before each race; with none timed yet it stays at 6. */
                cpu_base = og_ladder_level(&ladder, race_number,
                                           og_ladder_avg(player_ticks, player_laps));
                race_number = og_race_next(race_number);
                race_phase = 0; race_winner = -1; race_places = 0;
                fm_mode = 0; fm_white = 0; fm_timer = OG_FLAGMAN_WAIT; fm_i = -1;
                wheels_live = 0;
            }
        }
        /* F4's rows show the race that is loaded */
        og_dbg_track = track;
        og_dbg_reverse = reverse;

        if (map_dirty && tv.course && route.n > 0) {
            int off = 0;
            int bad = og_route_in_walls(&tv.art, tv.water, &route,
                                        (tv.has_heights ? tv.heights : NULL), track_ox, track_oy, &off);
            OG_DEBUG("[OG_Debug] Projection: origin %d,%d  ->  "
                    "%d/%d waypoints in walls, %d off screen",
                    track_ox, track_oy, bad, route.n, off);
            map_dirty = 0;
        }

        /* Poll the controls ONCE a frame, for every screen -- reading them
         * only inside the race loop left the shop, the title and
         * registration deaf. */
        og_input_read();

        og_audio_tick(&audio, dt);  /* the driver's own tick rate */

        /* The "press to continue" countdown of a player beaten by the
         * machine runs while the Speed Shop is up.  The credit is already
         * spent (op 84, og_award_prize); pressing carries on, letting it
         * reach zero ends their game even with credits left. */
        if (showing_shop) {
            static float cont_acc = 0.0f;
            int p;
            cont_acc += dt;
            for (p = 0; p < OG_PLAYERS; ++p) {
                og_racer *R = &racers[p];
                if (R->continue_t <= 0) continue;
                if (og_player[p].fire) {          /* they press on */
                    /* the credit went before the prompt (op 84) */
                    R->continue_t = 0;
                    /* The button is still DOWN.  Arm the shop's debounce
                     * so the same press is not read again as a purchase
                     * the moment the panel becomes the upgrade boxes. */
                    og_shop_held[p] = 1;
                    shop_dirty = 1;
                    OG_INFO("[OG_Game] Player %d continues, %d credits left",
                            p + 1, R->credits);
                } else if (cont_acc >= 1.0f) {
                    /* The arcade ticks its count down with 0x2a as each
                     * digit goes, and 0x2f as it runs out (bank 2
                     * 0x3cf2: 0x2f + the digit, 0x20 for none left);
                     * DOS's is silent. */
                    og_arc_fx(&audio, R->continue_t > 1 ? 0x2a : 0x2f);
                    if (--R->continue_t <= 0) {
                        /* op 85; the credit was spent before the prompt,
                         * so the last one ends on Game Over */
                        R->game_over = 1;
                        OG_INFO("[OG_Game] Player %d let the count run out -- game over",
                                p + 1);
                    }
                    shop_dirty = 1;
                }
            }
            if (cont_acc >= 1.0f) cont_acc = 0.0f;
            for (p = 0; p < OG_PLAYERS; ++p)
                if (racers[p].game_over) {
                    /* op 85, then 0x4654: no credits left shows
                     * "Game Over"; any left, the invite again */
                    og_shop_over[p] = racers[p].credits <= 0;
                    og_club_leave(&club, &racers[p]);       /* op 85 */
                    og_joined[p] = 0;
                    racers[p].computer  = 1;
                    racers[p].game_over = 0;
                    racers[p].continue_t = 0;
                    og_regs[p].step = OG_REG_NONE;
                    og_regs[p].initials[0] = '\0';
                    shop_dirty = 1;
                }
            {   /* With nobody left playing, the machine returns to its
                 * attract cycle -- picking up at the shop, which is the
                 * screen already on display. */
                int any = 0;
                for (p = 0; p < OG_PLAYERS; ++p) if (og_joined[p]) any = 1;
                if (!any && !og_attract) {
                    /* 0x134: the game is over -- save the table with
                     * al = 0, which writes only when the file is already
                     * there (0x42f:0xd09 is DOS find-first).  SORP makes
                     * its copy at start-up, so it is; Ctrl+S on the club
                     * screen still saves at any time. */
                    if (SDL_GetPathInfo(og_club_path, NULL)) {
                        int rc = og_hiscore_save(&club, og_club_path);
                        OG_INFO("[OG_Club] The Ironman Club saved to %s: %s",
                                og_rel(og_club_path), og_strerror(rc));
                    }
                    og_club_tp_save();
                    /* 0x300e: the last truck back to the machine ends
                     * the shop at once, before any "Game Over" shows */
                    memset(og_shop_over, 0, sizeof og_shop_over);
                    og_attract  = 1;
                    og_stage    = OG_ATTRACT_SHOP_REAL;
                    og_stage_t  = 0.0f;
                    race_number = 0;
                    og_standings_reset(racers);    /* 0x1e4 -> 0x12f9 */
                    OG_INFO("[OG_Game] Nobody left playing -- the attract cycle resumes");
                }
            }
        }

        /* the podium's timer (or "next stage") has fired: open the Speed Shop */
        if (og_end_to_shop && showing_end) {
            int lv[3][4], nz[3], cs[3] = { 0, 0, 0 };
            int cr[3], ct[3];
            int q;
            og_end_to_shop = 0;
            showing_end = 0;
            og_arc_fx(&audio, 0x23);           /* 0x38ea */
            if (end_tex) { SDL_DestroyTexture(end_tex); end_tex = NULL; }
            /* The podium is over: a player who finished the race out of
             * credits is out now, and the truck returns to the field.
             * (Done at the race's end, it put the computer's name on
             * the podium in place of the player's initials.) */
            for (q = 0; q < OG_PLAYERS; ++q)
                if (racers[q].game_over) {
                    og_shop_over[q] = racers[q].credits <= 0;   /* "Game Over" */
                    og_club_leave(&club, &racers[q]);
                    og_joined[q] = 0;
                    racers[q].computer  = 1;
                    racers[q].game_over = 0;
                    og_regs[q].step = OG_REG_NONE;
                    og_regs[q].initials[0] = '\0';
                    OG_INFO("[OG_Game] Player %d is out -- the truck returns to the field",
                            q + 1);
                }
            {   /* 0x300e: the last player out ends the shop at once, so
                 * no "Game Over" is drawn for them */
                int any = 0;
                for (q = 0; q < OG_PLAYERS; ++q) if (og_joined[q]) any = 1;
                if (!any) memset(og_shop_over, 0, sizeof og_shop_over);
            }
            for (q = 0; q < 3; ++q) {
                lv[q][0] = racers[q].up_shock; lv[q][1] = racers[q].up_tire;
                lv[q][2] = racers[q].up_accel; lv[q][3] = racers[q].up_top;
                nz[q] = racers[q].nitro;
                cs[q] = racers[q].cash;
                cr[q] = racers[q].credits;
                ct[q] = racers[q].continue_t;
            }
            memset(og_shop_ready, 0, sizeof og_shop_ready);   /* a new visit: nobody has chosen yet */
            shop_tex = shop_build(ren, &shopbank, lv, nz, cs, cr, og_shop_next_track(), pstate(racers),
                                  ironfont.loaded ? &ironfont.f : NULL, ct, og_shop_ready,
                                  &pages, &styles);
            showing_shop = (shop_tex != NULL);
            /* only the game's shop has music (op 55); the attract shop,
             * which opens this way after the club, has none */
            if (showing_shop && (!og_attract || og_arc_on(&audio)))
                og_music_select(&audio, OG_MUSIC_SHOP);
        }

        /* The podium runs on a timer (0x30be waits for the objects' own
         * countdown), so hold it for the anthem and then move on.  NOT in
         * attract mode: there 0xe6 runs the club (0x347e) between the
         * podium and the shop, and the attract machine opens the shop when
         * the club is done.  This 8 s timer used to open it behind the 5 s
         * club, and the club's ENTER then skipped the shop demo outright. */
        if (showing_end && !og_end_to_shop && !og_attract) {
            og_end_t += dt;
            if (og_end_t >= OG_END_HOLD / (float)OG_FRAME_HZ)
                og_end_to_shop = 1;
        }

        /* The title has made way: load the race now, not at startup --
         * unless a race was asked for already (the attract cycle leaving
         * the title, F4, the shop's START NEXT RACE), which loaded its
         * track itself.  Both used to run, the first race after launch
         * loading its track twice in the one frame. */
        if (race_pending && !showing_title) {
            race_pending = 0;
            og_tp_sync(dir);
            {   /* the schedule's race, in the mode now in force */
                const unsigned lay = og_sched_layout(race_number);
                track   = lay & 7;
                reverse = (lay & 8) ? 1 : 0;
                og_tp_use_lay(lay);
            }
            if (track_view_load(&tv, ren, dir, track) != OG_OK)
                OG_ERR("[OG_Race] Could not load a track");
            og_load_origin(dir, track, &track_ox, &track_oy);
            if (og_load_spawns(dir, track, reverse, &spawns) != OG_OK) spawns.loaded = 0;
            if (og_load_scenery(dir, OG_LAYOUT(track, reverse), &scenery) != OG_OK)
                scenery.loaded = 0;
            memset(sc_state, 0, sizeof sc_state);
            fm_timer = OG_FLAGMAN_WAIT; fm_i = -1;
            pk_live = 0; pk_timer = OG_PICKUP_WAIT;
            race_start(dir, track, reverse, &route, &cps, &ai,
                       &ai_from, &ai_to, &ai_t);
            og_arc_race_start(&audio, racers);          /* 0x4edf */
            og_music_select(&audio, OG_MUSIC_TRACK1 + track);
            if (og_load_grid(dir, OG_LAYOUT(track, reverse), &grid) != OG_OK)
                memset(&grid, 0, sizeof grid);
            wheels_live = 0;
            map_dirty = 1;
        }

        /* WHERE a player may join.
         *
         *   attract mode   -- any button, at any point in the cycle, cuts
         *                     to registration at once
         *   the Speed Shop -- a panel reading "Player N / Press Button"
         *                     is an invitation, so joining there is how a
         *                     player enters a season already running
         *   a live race    -- NO.  With a human already racing, a button
         *                     must not drop a registration screen over
         *                     the track.
         */
        {
            static const char *who[OG_PLAYERS] = { "red", "yellow", "blue" };
            int p, d, may_join;

            may_join = og_attract || showing_shop;
            if (!may_join) {
                /* no humans racing yet?  then the "race" on screen is
                 * still the attract demo and a button may interrupt it */
                int any = 0;
                for (p = 0; p < OG_PLAYERS; ++p) if (og_joined[p]) any = 1;
                may_join = !any;
            }
            /* A controller not yet driving that presses its button takes
             * the first truck no player has. */
            for (d = 0; may_join && d < OG_DEVICES; ++d)
                if (og_dev[d].fire && og_dev_truck[d] < 0 &&
                    (p = og_dev_join(d)) >= 0) {
                    og_joined[p] = 1;
                    og_reg_start(p);
                    og_arc_fx(&audio, og_arc_player_snd[p]);   /* 0x49b2 */
                    racers[p].computer = 0;
                    /* 0x1383: a truck taken over by a player starts the
                     * season clean -- no upgrades, no money, 25 nitros,
                     * and the field's base level. */
                    racers[p].up_shock = racers[p].up_tire  = 0;
                    racers[p].up_accel = racers[p].up_top   = 0;
                    racers[p].nitro    = OG_JOIN_NITRO;
                    racers[p].cash     = OG_JOIN_CASH;
                    racers[p].total    = 0;
                    racers[p].race_cash = 0;
                    racers[p].credits  = OG_SHOP_CREDITS_NEW;
                    racers[p].level    = OG_JOIN_LEVEL;
                    racers[p].ai_base  = OG_JOIN_LEVEL;
                    /* Do NOT clear the press here: registration starts
                     * with its debounce set and clears it only when the
                     * button is genuinely released, so the press that
                     * joined cannot also pick the first letter.  Zeroing
                     * it made the debounce clear on the same frame. */
                    OG_INFO("[OG_Game] Player %d (%s) joins with %s -- registration first",
                            p + 1, who[p], og_dev_name(d));
                    /* In attract mode the button cuts straight to
                     * registration wherever the cycle happens to be --
                     * a demo race stops there, with no chequered flag
                     * and no results. */
                    if (og_attract) open_shop = 1;
                }
        }

        /* Cutting straight to the shop: the demo race ends where it is. */
        if (open_shop) {
            int lv[3][4], nz[3], cs[3], cr[3], ct[3], q;
            open_shop = 0;
            og_attract = 0;
            og_shop_demo = 0;                  /* the players' shop now */
            og_join_pending = 0;
            race_phase = 2;                    /* the race is over */
            showing_title = 0;
            showing_club  = 0;
            showing_end   = 0;
            if (end_tex) { SDL_DestroyTexture(end_tex); end_tex = NULL; }
            for (q = 0; q < 3; ++q) {
                lv[q][0] = racers[q].up_shock; lv[q][1] = racers[q].up_tire;
                lv[q][2] = racers[q].up_accel; lv[q][3] = racers[q].up_top;
                nz[q] = racers[q].nitro;
                cs[q] = racers[q].cash;
                cr[q] = racers[q].credits;
                ct[q] = racers[q].continue_t;
            }
            memset(og_shop_ready, 0, sizeof og_shop_ready);   /* a new visit: nobody has chosen yet */
            if (shop_tex) SDL_DestroyTexture(shop_tex);
            shop_tex = shop_build(ren, &shopbank, lv, nz, cs, cr, og_shop_next_track(), pstate(racers),
                                   ironfont.loaded ? &ironfont.f : NULL, ct, og_shop_ready,
                                   &pages, &styles);
            showing_shop = (shop_tex != NULL);
            /* the game's shop script starts song 1 (op 55); the attract
             * one (0x48ec) starts nothing */
            if (!og_attract || og_arc_on(&audio)) og_music_select(&audio, OG_MUSIC_SHOP);
        }

        /* The Speed Shop: walk the button loop and press things. */
        if (showing_shop) {
            static const char *bname[OG_BUY_COUNT] = {
                "Nitro", "Acceleration", "Top Speed", "Start Next Race",
                "Shocks", "Tires"
            };
            static float acc = 0.0f;
            static int   frame[OG_PLAYERS];      /* [si+0xf] */
            int p, n;
            /* the panels' handlers run once a 70 Hz frame */
            acc += dt;
            n = (int)(acc * OG_FRAME_HZ);
            acc -= (float)n / OG_FRAME_HZ;
            og_shop_demo = og_attract && og_stage == OG_ATTRACT_SHOP_DEMO;
            if (og_shop_demo) {
                int f;
                for (f = 0; f < n; ++f)
                    for (p = 0; p < 3; ++p) {
                        og_demo_ctr[p] = (og_demo_ctr[p] - 1) & 0xf;
                        if (og_demo_ctr[p]) continue;
                        og_shop_sel[p] = (og_shop_sel[p] + (SDL_rand(2) ? 1 : OG_BUY_COUNT - 1))
                                         % OG_BUY_COUNT;
                        { og_audio_trigger(&audio, OG_SFX_SHOP_MOVE); og_arc_fx(&audio, 0x2b); }   /* 0x2bc0 */
                    }
                n = 0;                    /* nobody's controls drive it */
            }
            for (p = 0; p < OG_PLAYERS; ++p) {
                const og_input *in = &og_player[p];
                int move = in->right ? 1 : in->left ? -1 : 0, f;
                if (!og_joined[p] || og_regs[p].step != OG_REG_NONE) continue;
                /* Once a player has chosen to start, their shopping is
                 * over: the cursor stops where it is and the buttons do
                 * nothing until the race begins. */
                if (og_shop_ready[p]) continue;
                /* 0x2d68: a HELD direction keeps moving -- every 2nd frame
                 * the stick is read as it stands, right (bit 0) first and
                 * stepping +1, clockwise round the loop from Nitro to
                 * Acceleration; left -1 -- with effect 0xb each step.
                 *
                 * SORP DOUBLES that period, to every 4th frame, as it
                 * does registration's: at the authentic rate a held
                 * stick runs the six buttons round in a sixth of a
                 * second. */
                for (f = 0; f < n; ++f) {
                    frame[p] = (frame[p] - 1) & 0xff;
                    if ((frame[p] & 3) || !move) continue;
                    og_shop_sel[p] = (og_shop_sel[p] + move + OG_BUY_COUNT) % OG_BUY_COUNT;
                    { og_audio_trigger(&audio, OG_SFX_SHOP_MOVE); og_arc_fx(&audio, 0x2b); }   /* 0x2bc0 */
                    OG_DEBUG("[OG_Shop] Player %d: %s", p + 1, bname[og_shop_sel[p]]);
                }
                /* the button is taken once a press (bit 0x40) */
                if (og_shop_held[p]) {
                    if (!in->fire) og_shop_held[p] = 0;
                    continue;
                }
                if (in->fire) {
                    int b = og_shop_sel[p], got;
                    og_shop_held[p] = 1;
                    /* 0x2dd1: effect 0xd on EVERY press, before anything
                     * is decided */
                    og_audio_trigger(&audio, OG_SFX_SHOP_PRESS);
                    if (b == OG_BUY_START) {
                        /* EVERY player in the game has to choose "Start
                         * Next Race" -- one player picking it does not
                         * drag the others out of the shop.  A player
                         * still registering, or waiting on the continue
                         * count, has not chosen yet either. */
                        int q, ready = 1;
                        og_shop_ready[p] = 1;
                        /* the arcade: the player's own sound (0x43c7) */
                        og_arc_fx(&audio, og_arc_player_snd[p]);
                        for (q = 0; q < OG_PLAYERS; ++q)
                            if (og_joined[q] && !og_shop_ready[q]) ready = 0;
                        OG_INFO("[OG_Shop] Player %d is ready%s", p + 1,
                                ready ? " -- the race starts" : ", waiting for the others");
                        if (ready) {
                            int q;
                            og_advance();
                            /* the button is still down: do not let it
                             * read as a nitro press on the green light */
                            for (q = 0; q < OG_PLAYERS; ++q)
                                og_shop_held[q] = 1;
                        }
                        shop_dirty = 1;
                    } else if ((got = og_shop_buy(&racers[p], b)) > 0) {
                        /* the arcade's, by item: nitro 0x11, tires 0x0d,
                         * shocks 0x10, top speed 0x0f, acceleration 0x0e
                         * (bank 2 0x41dd..0x43bb, matched by price) */
                        static const unsigned char buy_snd[OG_BUY_COUNT] = {
                            0x11, 0x0e, 0x0f, 0, 0x10, 0x0d
                        };
                        if (buy_snd[b]) og_arc_fx(&audio, buy_snd[b]);
                        OG_INFO("[OG_Shop] Player %d buys %s ($%d left, %d credits)",
                                p + 1, bname[b],
                                racers[p].cash * OG_SHOP_CASH_UNIT,
                                racers[p].credits);
                        shop_dirty = 1;
                    } else if (got < 0) {
                        /* 0x2dfc: already at its top -- effect 0xe */
                        og_audio_trigger(&audio, OG_SFX_SHOP_FULL);
                        og_arc_fx(&audio, 0x2f);           /* 0x41ab */
                        OG_DEBUG("[OG_Shop] Player %d: %s is at its top", p + 1, bname[b]);
                    } else {
                        OG_DEBUG("[OG_Shop] Player %d cannot afford %s ($%d of $%d)",
                                p + 1, bname[b],
                                racers[p].cash * OG_SHOP_CASH_UNIT,
                                og_shop_price[b] * OG_SHOP_CASH_UNIT);
                    }
                }
            }
        }

        /* Registration, for any player part way through it -- only while
         * the Speed Shop is on screen, which is where its panels live. */
        if (showing_shop) {
            char line[40]; int p;
            for (p = 0; p < OG_PLAYERS; ++p) {
                int sfx = 0, live = og_reg_tick(p, dt, &sfx);
                line[0] = 0;
                if (sfx & 1) { og_audio_trigger(&audio, OG_SFX_SHOP_MOVE); og_arc_fx(&audio, 0x2b); }   /* 0x2bc0 */
                if (sfx & 2) { og_audio_trigger(&audio, 12); og_arc_fx(&audio, 0x2c); }   /* 0x2bd1 */
                if (!live) { og_reg_line[p][0] = 0; continue; }
                {
                    og_reg *r = &og_regs[p];
                    static const char *ask[] = {
                        "", "INITIALS", "MONTH", "DAY", "YEAR", "NATION", ""
                    };
                    if (r->step == OG_REG_NATION)
                        SDL_snprintf(line, sizeof line, "%s\n%s",
                                     ask[r->step], og_nation[r->nation]);
                    else if (r->step == OG_REG_INITIALS)
                        SDL_snprintf(line, sizeof line, "%s\n%s",
                                     ask[r->step], r->initials);
                    else
                        SDL_snprintf(line, sizeof line, "%s\n%d",
                                     ask[r->step],
                                     r->step == OG_REG_MONTH ? r->month :
                                     r->step == OG_REG_DAY   ? r->day : r->year);
                    if (r->step == OG_REG_DONE && !r->skipped) racers[p].country = r->nation;
                }
                /* Drawn at the END of the frame, over the shop -- and every
                 * player registers at once, each in their own third. */
                SDL_strlcpy(og_reg_line[p], line, sizeof og_reg_line[p]);
            }
            for (p = 0; p < OG_PLAYERS; ++p)
                if (og_regs[p].step == OG_REG_DONE) {
                    OG_INFO("[OG_Game] Player %d registered: %s, born %02d/%02d/%02d, %s",
                            p + 1, og_regs[p].initials, og_regs[p].month,
                            og_regs[p].day, og_regs[p].year,
                            og_nation[og_regs[p].nation]);
                    if (!og_regs[p].skipped)          /* SKIP leaves [di+0xe] */
                        racers[p].country = og_regs[p].nation;
                    og_club_register(&club, &racers[p], &og_regs[p]);  /* op 86 */
                    og_regs[p].step = OG_REG_NONE;
                    /* "Get Ready!" -- no shopping before the first race,
                     * and the panel's script has ended, so the shop does
                     * not wait on this player (see OG_PANEL_READY) */
                    og_get_ready[p]  = 1;
                    og_shop_ready[p] = 1;
                    shop_dirty = 1;
                    {
                        int q, all = 1;
                        for (q = 0; q < OG_PLAYERS; ++q)
                            if (og_joined[q] && !og_shop_ready[q]) all = 0;
                        if (all) {
                            /* nobody left shopping: the race begins, as
                             * when the last player picks Start Next Race */
                            og_advance();
                            for (q = 0; q < OG_PLAYERS; ++q)
                                og_shop_held[q] = 1;
                        }
                    }
                }
        }

        /* A player who has just joined: once the race has wound up and
         * the results are on screen, move straight on to the Speed Shop,
         * which is where registration happens. */
        if (og_join_pending && showing_end) {
            og_advance();
            og_join_pending = 0;
        }

        /* F4's "next stage" outside attract mode: the screens ADVANCE
         * moves on.  (In attract mode the attract machine takes it.) */
        if (!og_attract) {
            int can = showing_end || showing_shop || race_phase == 1;
            og_next_nm = showing_end  ? "podium" :
                         race_phase == 1 ? "race" :
                         showing_shop ? "shop" : "(nothing)";
            if (og_next_req) {
                og_next_req = 0;
                if (can) og_advance();
            }
        }

        /* ---- attract mode ---- */
        if (og_attract) {
            int p, anyone = 0;
            for (p = 0; p < OG_PLAYERS; ++p) anyone |= og_joined[p];
            if (anyone) {
                /* Somebody pressed a button.  The demo race stops there
                 * and the Speed Shop comes up so the new player can
                 * register -- initials, birthdate and nation, the three
                 * prompts traced at DS:0x8752 and DS:0x8760.  Those entry
                 * screens are not built yet, so the shop appears with the
                 * real state: every slot inviting a player. */
                og_attract = 0;
                og_next_req = 0;
                race_number = 0;            /* 0x107 -> 0x1e4: race 0 */
                og_standings_reset(racers); /* ...and 0x12f9 */
                OG_INFO("[OG_Attract] A player joins -- the demo race ends, "
                        "the Speed Shop opens for registration "
                        "(initials, birthdate, nation; entry not built yet)");
                if (race_phase == 1) {          /* end the demo race now */
                    og_advance();
                }
                showing_title = 0;
                og_join_pending = 1;            /* see below: open the shop */
            } else {
                static const char *stage_name[] = {
                    "title", "demo race", "podium", "Ironman Club",
                    "shop (demo)", "shop (real)"
                };
                og_attract_stage was = og_stage;
                int advance = 0;
                /* RESYNC.  Anything can change the screen out from under
                 * the attract machine -- a player joining, the shop
                 * opening on the podium's timer, attract resuming at the
                 * shop.  If the stage no longer matches what is actually
                 * displayed, move it and restart its clock; otherwise a
                 * stale stage fires its timer later and, say, pushes a
                 * track change in the middle of a running race. */
                {
                    og_attract_stage want = og_stage;
                    if (showing_title)      want = OG_ATTRACT_TITLE;
                    else if (showing_club)  want = OG_ATTRACT_CLUB;
                    else if (showing_end)   want = OG_ATTRACT_PODIUM;
                    else if (showing_shop)  want = (og_stage == OG_ATTRACT_SHOP_DEMO)
                                                 ? OG_ATTRACT_SHOP_DEMO
                                                 : OG_ATTRACT_SHOP_REAL;
                    else                    want = OG_ATTRACT_RACE;
                    if (want != og_stage) {
                        og_stage   = want;
                        og_stage_t = 0.0f;
                    }
                }

                og_stage_t += dt;
                og_next_nm = stage_name[og_stage];
                /* F4's "next stage": run out this stage's clock, so the
                 * cycle carries on exactly as its timer would (on the
                 * title that starts a FRESH demo race).  The demo race
                 * has no clock; it is ended early, and the podium
                 * follows. */
                if (og_next_req) {
                    og_next_req = 0;
                    if (og_stage != OG_ATTRACT_RACE) {
                        og_stage_t = 1e9f;
                    } else if (race_phase == 1) {
                        og_advance();
                    }
                }
                switch (og_stage) {
                case OG_ATTRACT_TITLE:
                    /* one full pass of the title's four views */
                    if (og_stage_t >= OG_ATTRACT_TITLE_HOLD / (float)OG_FRAME_HZ) {
                        /* The race runs on behind the title, so simply
                         * leaving the title reveals one already half
                         * over -- and the podium follows at once.  Start
                         * a fresh one instead: ask for this schedule
                         * entry's track and direction, which resets the
                         * field and calls race_start. */
                        unsigned lay;
                        og_tp_sync(dir);
                        lay = og_sched_layout(race_number);
                        showing_title = 0;
                        og_req_track   = (int)(lay & 7);
                        og_req_reverse = (lay & 8) ? 1 : 0;
                        og_req_tp      = (lay & OG_LAY_TP) ? 1 : 0;
                        og_race_req    = 1;
                        og_stage = OG_ATTRACT_RACE;
                        og_stage_t = 0.0f;
                    }
                    break;
                case OG_ATTRACT_RACE:
                    /* A demo race runs its four laps and is NEVER cut
                     * short by a timer -- only the podium appearing ends
                     * it (or F4's "next stage", for debugging). */
                    if (showing_end) { og_stage = OG_ATTRACT_PODIUM; og_stage_t = 0.0f; }
                    break;
                case OG_ATTRACT_PODIUM:
                    if (og_stage_t >= OG_ATTRACT_HOLD / (float)OG_FRAME_HZ) {
                        og_stage = OG_ATTRACT_CLUB;  /* the high scores next */
                        og_stage_t = 0.0f;
                        showing_club = 1;
                        /* the club and both attract shops are silent: the
                         * anthem ends with the podium.  The arcade's
                         * podium ends with 0x23, and its club has the
                         * menu song (bank 2 0x39d5). */
                        og_arc_fx(&audio, 0x23);
                        if (og_arc_on(&audio)) og_music_select(&audio, OG_MUSIC_SHOP);
                        else                   og_music_end(&audio);
                    }
                    break;
                case OG_ATTRACT_CLUB:
                    if (og_club_ask) og_stage_t = 0.0f;   /* it waits */
                    if (og_stage_t >= OG_ATTRACT_HOLD / (float)OG_FRAME_HZ) {
                        int q;
                        showing_club = 0;
                        advance = 1;                 /* on to the shop */
                        og_stage = OG_ATTRACT_SHOP_DEMO;
                        og_shop_demo = 1;            /* built as the demo */
                        for (q = 0; q < 3; ++q) {    /* init 0c = parent */
                            og_demo_ctr[q] = 5 * (q + 1);
                            og_shop_sel[q] = 0;
                        }
                    }
                    break;
                case OG_ATTRACT_SHOP_DEMO:
                case OG_ATTRACT_SHOP_REAL:
                    if (og_stage_t >= (og_stage == OG_ATTRACT_SHOP_DEMO
                                       ? OG_ATTRACT_SHOP_BOXES
                                       : OG_ATTRACT_SHOP_INVITE) / (float)OG_FRAME_HZ) {
                        if (og_stage == OG_ATTRACT_SHOP_DEMO) {
                            og_stage = OG_ATTRACT_SHOP_REAL;
                            og_stage_t = 0.0f;
                        } else {                     /* round again */
                            showing_shop = 0;
                            showing_end  = 0;
                            showing_title = 1;
                            og_stage = OG_ATTRACT_TITLE;
                            og_stage_t = 0.0f;
                            /* the title plays song 1; without this the
                             * race or anthem music carries on under it */
                            og_music_select(&audio, OG_MUSIC_SHOP);
                        }
                    }
                    break;
                }
                if (og_stage != was)
                    OG_DEBUG("[OG_Attract] %s -> %s (after %.1f s; title %d, "
                            "end %d, shop %d, race phase %d, track %d)",
                            stage_name[was], stage_name[og_stage], (double)og_stage_t,
                            showing_title, showing_end, showing_shop,
                            race_phase, track);
                if (advance) {           /* the same key the player would press */
                    og_stage_t = 0.0f;
                    og_advance();
                }
            }
        }
        og_audio_pump(&audio);

        /* With a screen shader the game is drawn into a 320x200 target
         * and the shader puts it on the window before the overlays. */
        /* The target is 4x when the race is composed at 4x (so its detail is
         * kept), except under Eagle, which scales a 1x frame itself.  The
         * render scale maps the game's 320x200 drawing onto it. */
        /* on the framebuffer, the window doubles while a shader has
         * detail finer than 320x200 to show */
        og_fb_window_size(win, (OG_SHADER_2X(og_screen_shader) ||
                                OG_COMPOSE4()) ? OG_FB_MAXK : 1);
        {
            int k = (OG_COMPOSE4() &&
                     !OG_SHADER_2X(og_screen_shader)) ? og_up_k() : 1;
            if (game_rt && game_rt_k != k) {
                SDL_DestroyTexture(game_rt);
                game_rt = NULL;
            }
            /* The scanlines on the framebuffer are drawn on the way to
             * the screen (og_fb_flip, or the GPU): no target for them,
             * which only cost a full-window copy a frame. */
            const int need_rt = og_screen_shader &&
                                !(og_screen_shader == OG_SHADER_SCANLINES && !og_window);
            if (need_rt && !game_rt) {
                game_rt = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                            SDL_TEXTUREACCESS_TARGET,
                                            OG_SCREEN_W * k, OG_SCREEN_H * k);
                if (game_rt)
                    SDL_SetTextureScaleMode(game_rt, k > 1 ? SDL_SCALEMODE_LINEAR
                                                           : SDL_SCALEMODE_NEAREST);
                game_rt_k = k;
            }
            shading = need_rt && game_rt;
            if (shading) {
                SDL_SetRenderTarget(ren, game_rt);
                SDL_SetRenderScale(ren, (float)k, (float)k);
            }
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        if (showing_club) {
            if (og_club_changed && club_tex) {   /* the table or the prompt */
                SDL_DestroyTexture(club_tex);
                club_tex = NULL;
            }
            og_club_changed = 0;
            if (!club_tex && ironfont.loaded)
                club_tex = club_build(ren, &endbank, &club, &ironfont.f,
                                      &pages, &styles, og_club_ask);
            og_reshade(ren, &club_tex);
            if (club_tex) SDL_RenderTexture(ren, club_tex, NULL, NULL);
        } else if (showing_title && title_tex) {
            /* the page over the artwork, changing on the object's own
             * timings and rotating the fourth view each time round */
            static int view = 0, pass = 0;
            static float held_t = 0.0f;
            static SDL_Texture *page_tex = NULL;
            static int built = -1;
            og_reshade(ren, &title_tex);
            SDL_RenderTexture(ren, title_tex, NULL, NULL);
            if (pages.loaded && ironfont.loaded) {
                const og_title_view *v = &og_title_seq[pass][view];
                if (built != (int)v->page) {
                    og_image pg;
                    if (og_image_init(&pg, OG_SCREEN_W, OG_SCREEN_H) == OG_OK) {
                        const og_title_art *art[2] = { NULL, NULL };
                        int a;
                        memset(pg.px, 0, (size_t)OG_SCREEN_W * OG_SCREEN_H);
                        /* the children this view spawns, before its page */
                        if (view == 0) art[0] = &og_title_lo;
                        else if (view == 2) { art[0] = &og_title_vi;
                                              art[1] = &og_title_gr; }
                        else if (view == 3 && pass == 0) art[0] = &og_title_rh;
                        else if (view == 3 && pass == 1) art[0] = &og_title_aw;
                        for (a = 0; a < 2; ++a) {
                            int q;
                            if (!art[a] || titlebank.nframes <= 0) continue;
                            for (q = 0; q < art[a]->pieces; ++q) {
                                int fi = art[a]->sprite - OG_ETITLE_FIRST + q;
                                int px = art[a]->x;
                                int z;
                                if (fi < 0 || fi >= titlebank.nframes) continue;
                                for (z = 0; z < q; ++z)
                                    px += titlebank.frames[art[a]->sprite
                                          - OG_ETITLE_FIRST + z].w;
                                /* Each piece carries its own top offset -- the
                                 * Off Road logo's five are 6, 1, 0, 0, 0 --
                                 * so blitting them all at one y shears the
                                 * word.  frame_oy holds 16 - top. */
                                {
                                    int dy = titlebank.frame_oy
                                           ? OG_TRUCK_BOX_ROWS / 2
                                             - titlebank.frame_oy[fi] : 0;
                                    og_image_blit(&pg, &titlebank.frames[fi],
                                                  px, art[a]->y + dy,
                                                  titlebank.key);
                                }
                            }
                        }
                        og_page_draw(&pages, v->page, &pg, &ironfont.f,
                                     roadfont.loaded ? &roadfont.f : NULL,
                                     &styles);
                        if (page_tex) SDL_DestroyTexture(page_tex);
                        page_tex = texture_from_kept(ren, &pg, og_pal_title, 0,
                                                     OG_LAYER_SPRITE);
                        og_image_free(&pg);
                        built = (int)v->page;
                    }
                }
                og_reshade(ren, &page_tex);
                if (page_tex) SDL_RenderTexture(ren, page_tex, NULL, NULL);
                /* The durations are in frames, so they
                 * are counted in TIME -- counting rendered frames makes
                 * every view as fast as the machine happens to draw. */
                held_t += dt;
                if (held_t >= (float)v->ticks / (float)OG_FRAME_HZ) {
                    held_t = 0.0f;
                    if (++view >= 4) { view = 0; pass = (pass + 1) % 3; }
                }
            }
        } else if (showing_end && end_tex) {
            og_reshade(ren, &end_tex);
            SDL_RenderTexture(ren, end_tex, NULL, NULL);
        } else if (showing_shop && shop_tex) {
            {   /* a panel changing purpose also needs a rebuild */
                static int last_state[3] = { -1, -1, -1 };
                const int *st = pstate(racers);
                int q;
                for (q = 0; q < 3; ++q)
                    if (st[q] != last_state[q]) { shop_dirty = 1; last_state[q] = st[q]; }
            }
            if (shop_dirty) {           /* a purchase changed the panels */
                int q, lv[3][4], nz[3], cs[3], cr[3], ct[3];
                for (q = 0; q < 3; ++q) {
                    lv[q][0] = racers[q].up_shock; lv[q][1] = racers[q].up_tire;
                    lv[q][2] = racers[q].up_accel; lv[q][3] = racers[q].up_top;
                    nz[q] = racers[q].nitro;
                    cs[q] = racers[q].cash;
                    cr[q] = racers[q].credits;
                    ct[q] = racers[q].continue_t;
                }
                SDL_DestroyTexture(shop_tex);
                shop_tex = shop_build(ren, &shopbank, lv, nz, cs, cr, og_shop_next_track(), pstate(racers),
                                   ironfont.loaded ? &ironfont.f : NULL, ct, og_shop_ready,
                                   &pages, &styles);
                shop_dirty = 0;
            }
            og_reshade(ren, &shop_tex);
            SDL_RenderTexture(ren, shop_tex, NULL, NULL);
            /* the selected button, outlined in each player's panel */
            {
                /* the panel's layout, matching the box table in
                 * shop_build and the button order above */
                static const struct { int x, y; } bxy[OG_BUY_COUNT] = {
                    {  2,  30 },    /* nitro        */
                    { 49,  30 },    /* acceleration */
                    { 49,  76 },    /* top speed    */
                    { 49, 122 },    /* start        */
                    {  2, 122 },    /* shocks       */
                    {  2,  76 }     /* tires        */
                };
                int p;
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                for (p = 0; p < OG_PLAYERS; ++p) {
                    SDL_FRect r;
                    if (!og_shop_demo &&
                        (!og_joined[p] || og_regs[p].step != OG_REG_NONE)) continue;
                    /* a player waiting to continue has the invite panel,
                     * not the upgrade boxes -- no cursor over it; nor has
                     * a player who is READY, whose shopping is done */
                    if (racers[p].continue_t > 0 || og_shop_ready[p]) continue;
                    r.x = (float)(OG_SHOP_PANEL_X0 + p * OG_SHOP_PANEL_DX
                                  + bxy[og_shop_sel[p]].x - 1);
                    r.y = (float)(OG_SHOP_PANEL_Y + bxy[og_shop_sel[p]].y - 1);
                    r.w = 46.0f; r.h = 42.0f;
                    SDL_RenderRect(ren, &r);
                }
            }
        } else if (tv.bg) {
            SDL_RenderTexture(ren, tv.bg, NULL, NULL);
            og_ndraws = 0;               /* this frame's display list */

            /* Drive a truck along the traced centreline. */
            if (have_truck && route.n > 1) {

                /* Drive the game's own route program.  The truck moves
                 * through world CELL space along the segment ai_from ->
                 * ai_to; its checkpoint count is updated each frame from
                 * the cell it is in, as the game does from [0x4f71] and
                 * [0x4f73]; and og_ai_next resolves every branch against
                 * that count -- so a figure-of-eight takes the right half
                 * on each pass through the crossover. */
                /* physics ticks for this frame, shared by all four --
                 * none while the race is paused (THE PAUSE), so it stands
                 * still and carries on with no ticks to catch up */
                {
                    static int was_paused = 0;
                    const int paused = og_race_paused();
                    if (paused != was_paused) {
                        OG_INFO("[OG_Race] %s", paused ? "Paused" : "Carries on");
                        og_arc_pause(&audio, racers, paused);
                    }
                    was_paused = paused;
                    if (!paused) phys_acc += dt;
                }
                if (phys_acc > 0.25f) phys_acc = 0.25f;     /* after a stall */
                phys_ticks = 0;
                while (phys_acc >= 1.0f / OG_PHYS_HZ) {
                    phys_acc -= 1.0f / OG_PHYS_HZ;
                    ++phys_ticks;
                }
                if (race_phase == 1) rs_ticks += phys_ticks;   /* for the log */
                if (race_phase < 2) og_arc_engines(&audio, racers, phys_ticks);

                for (ri = 0; ri < OG_RACERS; ++ri) {
                    const unsigned char *Hh = tv.has_heights ? tv.heights : NULL;
                    int ox = track_ox, oy = track_oy;
                    float ax, ay, bx, by, seg, fx, fy;
                    og_racer *R = &racers[ri];
                    int lw;

                    ai = R->ai; ai_from = R->from; ai_to = R->to; ai_t = R->t;
                    for (lw = 0; lw < 4; ++lw) { wh[lw] = R->wh[lw]; wv[lw] = R->wv[lw]; }
                    wheels_live = R->live; flying = R->flying;
                    truck_colour = R->colour;

                    og_wp_screen(&route, ai_from, Hh, ox, oy, &ax, &ay);
                    og_wp_screen(&route, ai_to,   Hh, ox, oy, &bx, &by);
                    seg = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
                    if (seg < 1.0f) seg = 1.0f;

                    /* The truck drives itself; the route only tells it
                     * where to aim.  See the AI notes in og_data.h. */
                    fx = R->wx; fy = R->wy;
                    if (race_phase == 0) {          /* stopped on the grid */
                        fx = R->wx = (float)(grid.x + grid.dx[ri]) / 256.0f;
                        fy = R->wy = (float)(grid.y + grid.dy[ri]) / 256.0f;
                        R->head = R->trav = (float)((grid.heading >> 8) & 0xff);
                        R->speed = 0.0f; R->thr = 0.0f;
                        og_ai_acquire(R, &route);
                        ai_from = R->from; ai_to = R->to; ai = R->ai;
                    } else if (og_ai_cross(&ai, &cps, (int)fx, (int)fy)) {
                        if (ovl_gates) {
                            /* its place: how many trucks are further round,
                             * ranked by lap then progress as 0x2314 ranks them */
                            static const char *place_name[OG_RACERS] = {
                                "first", "second", "third", "fourth"
                            };
                            int q, ahead = 0;
                            for (q = 0; q < OG_RACERS; ++q) {
                                if (q == ri) continue;
                                if (racers[q].laps > ai.laps ||
                                    (racers[q].laps == ai.laps &&
                                     racers[q].ai.progress > ai.progress))
                                    ++ahead;
                            }
                            OG_TRACE("[OG_Race] Truck %d: gate %d of %d, progress %3d, lap %d, %s place",
                                    ri, ai.checkpoint, cps.n, ai.progress, ai.laps,
                                    place_name[ahead]);
                        }
                        if (ai.laps > R->laps) {       /* image 0x1f9c */
                            /* the player's own laps are what the ladder
                             * averages over, as [di+0x44] / lap count */
                            if (ri == 0) ++player_laps;
                            {   /* this truck's lap time, capped as 0x3843 does */
                                long lap = R->time_ticks - R->lap_mark;
                                if (lap > OG_LAP_TICKS_MAX) lap = OG_LAP_TICKS_MAX;
                                R->lap_mark = R->time_ticks;
                                R->last_lap = (int)lap;
                                if (!R->best_lap || lap < R->best_lap)
                                    R->best_lap = (int)lap;
                                OG_DEBUG("[OG_Race] Truck %d lap %d: %.2f s (best %.2f)",
                                        ri, ai.laps, lap / 20.0,
                                        R->best_lap / 20.0);
                            }
                            R->laps = ai.laps;
                            if (R->laps == OG_RACE_LAPS - 1) {
                                fm_white = 1;          /* [0x19c] bit 0 */
                                /* the arcade sounds it for every truck
                                 * (0xa64d: its laps reaching 3) */
                                og_arc_fx(&audio, 0x13);
                                OG_DEBUG("[OG_Race] Truck %d starts its last lap -- white flag", ri);
                            }
                            /* The place is kept current by
                             * og_race_positions, so "has it finished?"
                             * needs its own flag -- testing !place would
                             * never fire now. */
                            /* Crossing the line gives a truck NO place
                             * of its own: its place is its slot in the
                             * standings, settled by the sort as the race
                             * ends, and the prize is paid from that. */
                            if (R->laps >= OG_RACE_LAPS && !R->finished) {
                                R->finished = 1;
                                R->fin_ticks = R->time_ticks;
                                ++race_places;
                                if (race_winner < 0) {
                                    race_winner = ri;
                                    race_phase = 2;
                                    /* The race music stops the moment the
                                     * winner crosses: the lap counter sets
                                     * bit 1 of [0x19c] at 0x1fc5 and the
                                     * chequered branch takes over the
                                     * sound.  It must not run on under the
                                     * flag and into the podium. */
                                    og_music_end(&audio);
                                    fm_mode = 2;       /* [0x19c] bit 1 */
                                    fm_i = 0; fm_hold = 0;
                                    /* The flagman's chequered branch:
                                     * op 54 (0xffff) SILENCES the effects
                                     * first, then op 54 (3) plays the
                                     * little fanfare -- 32 ticks of
                                     * warble -- while the flag waves. */
                                    og_audio_silence(&audio);
                                    og_audio_trigger(&audio, 3);
                                    og_arc_race_off(&audio, 1);           /* 0x4dc6 */
                                    OG_INFO("[OG_Race] Truck %d WINS after %d laps -- chequered flag",
                                            ri, R->laps);
                                } else
                                    OG_INFO("[OG_Race] Truck %d crosses the line, %d%s over it", ri,
                                            race_places, race_places == 2 ? "nd" :
                                            race_places == 3 ? "rd" : "th");
                            }
                        }
                    }

                    /* The four wheels, placed as GAME.EXE 0x18df places them:
                     * 45, 135, 225 and 315 degrees from the heading, one cell
                     * out (the sine table at DS:0x50dc has radius 256), with
                     * the angle cut to 32 directions.  Each wheel is a point
                     * mass with its own height and velocity, stepped 20 times
                     * a second exactly as the game steps it -- see the
                     * vertical physics notes in og_data.h.  Ground heights
                     * are bilinear between vertices; the game's own sampler
                     * also adds a POLY gradient inside the cell. */
                    {
                        float head = R->head * (2.0f * (float)M_PI / 256.0f);
                        float g[4], wxs[4], wys[4];
                        long  wpo[4] = { -1, -1, -1, -1 };  /* each wheel's wall:
                                                   * its POLY, -2 off the grid */
                        float step_x = 0.0f, step_y = 0.0f;
                        int   wblocked[4] = { 0, 0, 0, 0 };
                        float pitch, ground_h = 0.0f, air;
                        int w, landed = 0;

                        /* THE LOOK-AHEAD.  0x1890 forms the sampling
                         * position as the truck's own plus this tick's
                         * movement step, so the wheels are tested where the
                         * truck is ABOUT TO BE and the direction is corrected
                         * before the move is committed.  Sampling where it
                         * already is only reacts once it has penetrated. */
                        {
                            float a0 = og_step_angle(R->trav);
                            step_x = cosf(a0) * R->speed / 256.0f;
                            step_y = sinf(a0) * R->speed / 256.0f;
                        }
                        /* where the wheels stand this frame, for the
                         * picture and the start; each tick places them
                         * again, one at a time, as it runs */
                        for (w = 0; w < 4; ++w) {
                            wpo[w] = og_wheel_place(Hh, &tv.contour,
                                                    OG_LAYOUT(track, reverse),
                                                    fx + step_x, fy + step_y,
                                                    R->head, w,
                                                    &wxs[w], &wys[w], &g[w]);
                            wblocked[w] = (wpo[w] != -1);
                            /* 0x1a68: a wheel in a wall takes the mean
                             * ground [di+0x7f] (averaged at 0x1dc0) */
                            if (wblocked[w]) g[w] = R->ground_m;
                        }
                        if (!wheels_live) {            /* race start: 0x1413 */
                            for (w = 0; w < 4; ++w) { wh[w] = g[w]; wv[w] = 0.0f; }
                            wheels_live = 1; phys_acc = 0.0f; flying = 0;
                        }

                        for (tick_i = 0; tick_i < phys_ticks; ++tick_i) {
                            og_tperf tp;
                            float k;
                            static int rubber_due = 0;
                            /* 0x1871 forms this tick's step from the speed
                             * and travel as they stand NOW, before the walls,
                             * the steering or the drive touch them; 0x1580
                             * commits that step, the one the wheels tested */
                            const float mv_trav = R->trav, mv_speed = R->speed;
                            float slip0 = 0.0f;     /* 0x1cad: heading - travel */
                            int   back = 0;         /* [di+0x89]: going backwards */
                            int   rear_n = 0;       /* rear wheels down */
                            int   wall_state = 0;   /* [0x4f7a], 0x154f clears it */
                            int   wall_any = 0, wall_crash = 0, kw;
                            racer_perf(&tp, &perf, R);
                            k = perf.loaded ? (float)tp.shock / 65536.0f
                                            : (phys.loaded ? phys.shocks[R->computer ? 0 : R->up_shock]
                                                           : 0.234f);

                            /* [di+0x44] counts PHYSICS steps (20 Hz), not
                             * frames -- every lap time divides by
                             * OG_PHYS_HZ, never OG_FRAME_HZ. */
                            if (race_phase == 1) ++R->time_ticks;
                            if (ri == 0 && race_phase == 1) ++player_ticks;
                            if (ri == 0) {          /* once per tick, not per truck */
                            {   /* has any truck just reached a 0x85 waypoint? */
                                int q;
                                rubber_due = 0;
                                for (q = 0; q < OG_RACERS; ++q)
                                    if (racers[q].from != racers[q].rubber_at &&
                                        route.wp[racers[q].from].rubber) {
                                        racers[q].rubber_at = racers[q].from;
                                        rubber_due = 1;
                                    }
                            }
                            /* The rubber band (0x2314): rank the field,
                             * then move each computer truck's level toward
                             * or away from the leader's pace.  The game
                             * reloads its performance the moment the level
                             * changes, which racer_perf does below.
                             *
                             * It is re-weighed only where the ROUTE asks:
                             * opcode 0x85 carries no argument and its
                             * handler runs over all four truck structures,
                             * and the layouts place it at nine or ten
                             * waypoints rather than everywhere.  Doing it
                             * every tick makes the computers react far more
                             * sharply than the original. */
                            if (ri == 0 && perf.loaded && race_phase == 1 &&
                                rubber_due) {
                                int q, lead = 0;
                                for (q = 1; q < OG_RACERS; ++q)
                                    if (racers[q].laps > racers[lead].laps ||
                                        (racers[q].laps == racers[lead].laps &&
                                         racers[q].ai.progress > racers[lead].ai.progress))
                                        lead = q;
                                /* The authentic band (0x2321) compares
                                 * each computer truck INDIVIDUALLY against
                                 * the human, so one that falls behind
                                 * becomes very quick. */
                                /* 0x231a: demo mode skips the sort too */
                                if (!og_attract) og_race_positions(racers, OG_RACERS);
                                og_ai_rubber_band(racers, OG_RACERS,
                                                  og_attract, cpu_base,
                                                  cps.n ? cps.cp[cps.n - 1].progress
                                                        : 255);
                            }

                            og_fx_tick(fxs);                              /* 0x78b loop */
                            og_scenery_tick(&scenery, sc_state);
                            /* the flagman: 40 ticks, sound 1, then his wave */
                            if (scenery.loaded) {
                                if (fm_i < 0) {
                                    if (--fm_timer <= 0) {
                                        fm_i = 0;
                                        fm_hold = scenery.fm_wave.n ? scenery.fm_wave.dur[0] : 0;
                                        og_audio_trigger(&audio, OG_FLAGMAN_SOUND);
                                        og_arc_fx(&audio, 0x0b);      /* 0x413e */
                                        /* The script clears [0x19c] bit 3 with
                                         * op 50 BEFORE it waves (0x3470), so
                                         * the trucks go as the flag starts. */
                                        if (race_phase == 0) race_phase = 1;
                                    }
                                } else if (fm_i < scenery.fm_wave.n) {
                                    if (--fm_hold < 0) {
                                        if (++fm_i < scenery.fm_wave.n)
                                            fm_hold = scenery.fm_wave.dur[fm_i];
                                        else if (fm_mode == 1)
                                            fm_mode = 0;
                                    }
                                } else if (fm_mode == 0 && fm_white) {
                                    /* His loop (0x3492) tests the chequered
                                     * bit, then the white: op 54 (2) sounds,
                                     * op 50 clears bit 0 of [0x19c], and the
                                     * flag waves.  A truck starting its last
                                     * lap during a wave sets the bit again,
                                     * so another wave follows this one. */
                                    fm_white = 0;
                                    fm_mode = 1;
                                    fm_i = 0;
                                    fm_hold = scenery.fm_wave.n ? scenery.fm_wave.dur[0] : 0;
                                    og_audio_trigger(&audio, OG_FLAGMAN_WHITE);
                                }
                            }
                                /* With the chequered flag waved, the race is
                                 * done: the Speed Shop comes up between
                                 * races, showing each player's panel. */
                                if (ri == 0 && race_phase == 2 && !showing_shop &&
                                    !showing_end && fm_mode == 2 &&
                                    fm_i >= scenery.fm_chequer.n &&
                                    endbank.nframes > 0) {
                                    /* who stands where: the colour groups
                                     * are white 0, blue 1, yellow 2, red 3 */
                                    /* The eend colour groups are red 0, blue 1,
                                     * yellow 2, white 3, and our trucks run
                                     * red, yellow, blue, white.  A human keeps
                                     * his own column -- red left, yellow
                                     * middle, blue right -- and the columns no
                                     * human occupies show the leading
                                     * finishers instead. */
                                    /* A COLUMN IS A PLAYER.  0x30d6 builds
                                     * exactly three, one per player truck and
                                     * always in the same place:
                                     *
                                     *   truck 0 (0x4c68) at x 7
                                     *   truck 1 (0x4d2a) at x 115
                                     *   truck 2 (0x4dec) at x 223
                                     *
                                     * and 0x3111 draws a column only when that
                                     * truck's [di+2] has bit 2 -- the player is
                                     * in the game -- reading his finishing
                                     * place from [di+0x3b] for the artwork.
                                     * Nothing is assigned by finishing order,
                                     * so a player can never appear twice.
                                     *
                                     * One refinement is not implemented: for a
                                     * player who IS in the game, 0x3111 walks
                                     * the ranked list at [0x9d2e] from his
                                     * place onward, skipping trucks of type 3
                                     * and any not in the game, and falls back
                                     * to the white truck at 0x4eae when it
                                     * runs off the end.  White is the filler
                                     * for a place no player holds, so white --
                                     * and only white -- may show twice. */
                                    /* The columns are FIXED to the three
                                     * player trucks -- red, yellow, blue
                                     * left to right (0x30d6 walks 0x4c68,
                                     * 0x4d2a, 0x4dec in that order), each
                                     * plaque stating that truck's own
                                     * place.
                                     *
                                     * The white truck is the exception:
                                     * when it WINS it takes the column of
                                     * the lowest-placed COMPUTER truck.
                                     * It never displaces a human, so with
                                     * three human players it is not shown
                                     * at all. */
                                    int col_group[OG_RACERS];
                                    int ord[3], q;
                                    for (q = 0; q < OG_RACERS; ++q)
                                        col_group[q] = q;
                                    /* 0x3111 opens by asking, for each
                                     * column: is there a COMPUTER truck
                                     * other than white placed BELOW this
                                     * one?  It walks the standings down
                                     * from this truck's place, stepping
                                     * over white and over humans; if it
                                     * reaches the end without finding
                                     * one, the column shows WHITE
                                     * (0x313d: di = 0x4eae) instead.
                                     *
                                     * A human column never substitutes
                                     * (0x3117 leaves at once), so white
                                     * cannot displace a player -- and
                                     * with three human players it is not
                                     * shown at all. */
                                    for (q = 0; q < 3; ++q) {
                                        int a, found = 0;
                                        if (!racers[q].computer) continue;
                                        for (a = 0; a < OG_RACERS; ++a) {
                                            if (a == 3) continue;        /* white */
                                            if (!racers[a].computer) continue;
                                            if (racers[a].place > racers[q].place)
                                                found = 1;               /* below it */
                                        }
                                        if (!found) col_group[q] = 3;
                                    }
                                    for (q = 0; q < 3; ++q) ord[q] = col_group[q];
                                    {
                                        int ctry[3];
                                        /* the flag is the DRIVER's: the
                                         * truck standing in the column,
                                         * which may be the white truck --
                                         * Ivan Stewart -- substituted in
                                         * (0x3183 reads [di+0x0e] of the
                                         * column's own truck) */
                                        for (q = 0; q < 3; ++q) ctry[q] = racers[ord[q]].country;
                                        {
                                            og_endtime et[OG_RACERS];
                                            int q;
                                            const int span = cps.n ? cps.cp[cps.n - 1].progress : 255;
                                            SDL_zeroa(et);
                                            for (q = 0; q < OG_RACERS; ++q) {
                                                et[q].laps  = racers[q].laps;
                                                et[q].total = (int)og_race_ticks(&racers[q], span);
                                                et[q].best  = (int)racers[q].best_lap;
                                                et[q].pickups = racers[q].race_cash;
                                            }
                                            {
                                                int hm[4]; char rg[3][4];
                                                int pl[4]; int z;
                                                /* The race is over: every
                                                 * truck still running takes
                                                 * its live place now, and
                                                 * is paid (or charged a
                                                 * credit) accordingly.
                                                 * Only the trucks that
                                                 * crossed the line were
                                                 * settled before, so a
                                                 * human who was lapped
                                                 * never paid. */
                                                og_race_positions(racers, OG_RACERS);  /* 0x36ec */
                                                for (z = 0; z < OG_RACERS; ++z) {
                                                    racers[z].finished = 1;
                                                    og_award_prize(racers, &racers[z], track);
                                                }
                                                /* 0x36d3 runs 0x371a for
                                                 * every truck before the
                                                 * plaques are built, so
                                                 * they show this race */
                                                int own;
                                                og_hiscore *rc_club = og_race_club(&own);
                                                for (z = 0; z < OG_PLAYERS; ++z) {
                                                    og_racer *H = &racers[z];
                                                    og_racer_club(rc_club, H, track, span, own);
                                                    /* a player's own figures are kept
                                                     * for the club's own tracks only */
                                                    if (!H->club_raced || !own) continue;
                                                    et[z].club      = 1;
                                                    et[z].avg_bcd   = og_club_avg(&H->club, track);
                                                    et[z].best_bcd  = og_club_best(&H->club, track);
                                                    et[z].avg_rank  = og_club_rank(rc_club, OG_CLUB_AVGS,
                                                                                   track, et[z].avg_bcd);
                                                    et[z].best_rank = og_club_rank(rc_club, OG_CLUB_BESTS,
                                                                                   track, et[z].best_bcd);
                                                }
                                                /* A player out of credits
                                                 * (game_over) still stands
                                                 * on the podium under their
                                                 * own initials: the truck
                                                 * goes back to the machine
                                                 * when the podium makes way
                                                 * for the Speed Shop. */
                                                for (z = 0; z < 4; ++z)
                                                    pl[z] = racers[z].place;
                                                for (z = 0; z < 4; ++z)
                                                    hm[z] = (z < OG_PLAYERS)
                                                          && !racers[z].computer;
                                                for (z = 0; z < 3; ++z)
                                                    memcpy(rg[z], og_regs[z].initials, 4);
                                                end_tex = end_build(ren, &endbank, ord, ctry, &pages,
                                                                ironfont.loaded ? &ironfont.f : NULL,
                                                                &styles, et, hm, (const char (*)[4])rg,
                                                                og_club_record(rc_club, track),
                                                                og_club_record_who(rc_club, track),
                                                                pl);
                                            }
                                        }
                                    }
                                    showing_end = (end_tex != NULL);
                                    if (showing_end) {
                                        /* 0x30b2: the country + 2 of the
                                         * truck at the TOP of the standings
                                         * ([0x9d2e]), and it replaces the
                                         * race music */
                                        int wc = racers[og_standing[0]].country;
                                        og_anthem_nation = wc;
                                        og_music_select(&audio, OG_MUSIC_ANTHEM +
                                                        (wc >= 0 && wc < OG_END_COUNTRIES
                                                         ? wc : OG_COUNTRY_NONE));
                                        og_end_t = 0.0f;       /* start the hold */
                                        og_end_to_shop = 0;
                                        {   /* the race, for the log: the
                                             * standings as the podium has them */
                                            int z;
                                            OG_INFO("[OG_Race] Race over: track %d %s%s",
                                                    track,
                                                    reverse ? "backwards" : "forwards",
                                                    og_attract ? " (a demo race)" : "");
                                            for (z = 0; z < OG_RACERS; ++z) {
                                                const og_racer *q = &racers[og_standing[z]];
                                                char fin[24];
                                                if (q->finished && q->fin_ticks > 0)
                                                    SDL_snprintf(fin, sizeof fin, "%.2f s",
                                                                 q->fin_ticks / 20.0);
                                                else
                                                    SDL_snprintf(fin, sizeof fin, "%d of %d laps",
                                                                 q->laps, OG_RACE_LAPS);
                                                OG_INFO("[OG_Race]   %d. truck %d, %s: %s, best lap %.2f s",
                                                        z + 1, og_standing[z],
                                                        q->computer ? "the machine" : "a player",
                                                        fin, q->best_lap / 20.0);
                                            }
                                            if (rs_t0 && rs_t1 > rs_t0 && rs_frames > 0) {
                                                const double sec = (double)(rs_t1 - rs_t0) / 1e9;
                                                OG_INFO("[OG_Race]   %.1f frames/s over %.1f s; the slowest "
                                                        "frame %.0f ms, %d late; %s",
                                                        rs_frames / sec, sec,
                                                        (double)rs_worst / 1e6, rs_late, rs_how);
                                                /* 20 ticks a second, as the game's
                                                 * timer gives them; fewer is the race
                                                 * running slow, frames too long for
                                                 * the time they stand for */
                                                SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION,
                                                    rs_ticks < (long)(sec * OG_PHYS_HZ * 0.98) ?
                                                    SDL_LOG_PRIORITY_WARN : SDL_LOG_PRIORITY_INFO,
                                                    "[OG_Race]   The game clock: %ld ticks in %.1f s, "
                                                    "%.1f%% of its %d a second", rs_ticks, sec,
                                                    100.0 * rs_ticks / (sec * OG_PHYS_HZ), (int)OG_PHYS_HZ);
                                            }
                                        }
                                        OG_INFO("[OG_Race] Results -- the anthem, then the Speed Shop");
                                    }
                                }
                            }  /* ri == 0 */

                            /* EVERY truck knocks the flags, not just the
                             * first: the collision mask is a bit per truck
                             * plus the shared truck class, so this runs for
                             * each racer with its own footprint. */
                            if (scenery.loaded) {
                                int q;
                                const og_shape *tb =
                                    &truck_box[OG_TRUCKBOX_INDEX(head_b)];
                                unsigned cls = (1u << ri) | OG_CLASS_TRUCK;
                                for (q = 0; q < scenery.n; ++q) {
                                    int t = scenery.type[q];
                                    if (sc_state[q].on || scenery.hit[t].n <= 0) continue;
                                    if (!(scenery.mask[t] & cls)) continue;
                                    if (og_collide((int)(fx * 8.0f), (int)(fy * 8.0f), tb,
                                                   scenery.wx[q], scenery.sort[q],
                                                   &scenery.shape[t])) {
                                        sc_state[q].on = 1; sc_state[q].i = 0;
                                        sc_state[q].hold = (signed char)scenery.hit[t].dur[0];
                                    }
                                }
                            }

                            /* Once the winner is home the trucks are done:
                             * no wheels, no driving, no effects thrown up.
                             * The flagman and the live effects carry on --
                             * they were stepped in the block above. */
                            if (race_phase == 2) continue;
                            /* The pickup script: out of sight for 120 ticks,
                             * then op 58 places it -- a random one of this
                             * track's spawn points, cash or nitro on a coin
                             * toss -- op 54 sounds, op 29 shows it. */
                            if (spawns.loaded && ri == 0 && !pk_live) {
                                /* placing it is once per tick, not per truck */
                                {
                                    if (--pk_timer <= 0) {
                                        int idx = spawns.sel[og_rng(&pk_seed) & 15];
                                        if (idx >= spawns.n) idx = 0;
                                        pk_wx = spawns.x[idx];
                                        pk_wy = spawns.y[idx];
                                        pk_type = (int)(og_rng(&pk_seed) & 1u);
                                        og_project(pk_wx, pk_wy,
                                                   (int)(og_ground(Hh, (float)(pk_wx >> 8),
                                                                   (float)(pk_wy >> 8)) + 0.5f),
                                                   ox, oy, &pk_px, &pk_py);
                                        pk_sort = (int)(pk_wy >> 5);
                                        pk_live = 1;
                                        og_audio_trigger(&audio, OG_PICKUP_SOUND);
                                        og_arc_fx(&audio, 0x28);      /* 0x3dbd */
                                        OG_TRACE("[OG_Race] Pickup: %s at (%ld,%ld)",
                                                pk_type ? "nitro" : "cash", pk_wx, pk_wy);
                                    }
                                }
                            }
                            if (spawns.loaded && pk_live) {
                                {
                                    /* Collected when a truck touches it.  The
                                     * game's own test is the object collision
                                     * that sets [si+0x6e]; this stands in for
                                     * it with a one-cell radius. */
                                    /* The object collision: both in coarse
                                     * units, the truck's footprint for its
                                     * heading against the pickup's own box. */
                                    const og_shape *tb =
                                        &truck_box[OG_TRUCKBOX_INDEX(head_b)];
                                    if (og_collide((int)(fx * 8.0f), (int)(fy * 8.0f), tb,
                                                   (int)(pk_wx >> 5), (int)(pk_wy >> 5),
                                                   &pickup_box)) {
                                        /* op 59: the award climbs to 9; nitro
                                         * adds to the truck's count (max 99),
                                         * cash to its money.  Then the digit
                                         * and the word, for 40 ticks. */
                                        int n;
                                        pk_live = 0;
                                        pk_timer = OG_PICKUP_WAIT;
                                        /* image 0x596: the sound is per
                                         * truck, from the table at
                                         * DS:0x48d -- trucks 0, 1, 2 play
                                         * effects 4, 5, 6 and the fourth
                                         * is silent. */
                                        if (ri < 3) og_audio_trigger(&audio, 4 + ri);
                                        /* the arcade's (0x3d30, CALL Z): the
                                         * truck's own, for a human only */
                                        if (ri < 3 && !R->computer)
                                            og_arc_fx(&audio, og_arc_player_snd[ri]);
                                        if (pk_type) {
                                            if (++award_nitro > 9) award_nitro = 9;
                                            n = award_nitro;
                                            R->nitro += n;
                                            if (R->nitro > 99) R->nitro = 99;
                                            og_fx_label(fxs, OG_LABEL_DIGIT + n,
                                                        pk_px - 0x10, pk_py, OG_LABEL_TICKS);
                                            og_fx_label(fxs, n == 1 ? OG_LABEL_NITRO
                                                                    : OG_LABEL_NITROS,
                                                        pk_px, pk_py, OG_LABEL_TICKS);
                                            OG_TRACE("[OG_Race] Truck %d collected %d nitro (now %d)",
                                                    ri, n, R->nitro);
                                        } else {
                                            if (++award_cash > 9) award_cash = 9;
                                            n = award_cash;
                                            /* 0x063b banks a pickup in
                                             * [di+0x62], not the cash --
                                             * the prize adds it later */
                                            R->race_cash += n;
                                            og_fx_label(fxs, OG_LABEL_DIGIT + n,
                                                        pk_px - 0x10, pk_py, OG_LABEL_TICKS);
                                            og_fx_label(fxs, OG_LABEL_THOUS,
                                                        pk_px - 1, pk_py + 1, OG_LABEL_TICKS);
                                            OG_TRACE("[OG_Race] Truck %d collected %d0,000 (now %d)",
                                                    ri, n, R->cash);
                                        }
                                    }
                                }
                            }
                            for (w = 0; w < 4; ++w) wh[w] += wv[w];          /* 0x18be */

                            for (kw = 0; kw < 4; ++kw) {
                                float d, sa = og_step_angle(mv_trav);
                                w = og_wheel_order[kw];
                                /* 0x18df places each wheel as its turn
                                 * comes: from the heading AS IT STANDS -- a
                                 * response on an earlier wheel has already
                                 * turned it -- and the step 0x1871 formed */
                                wpo[w] = og_wheel_place(Hh, &tv.contour,
                                                        OG_LAYOUT(track, reverse),
                                                        R->wx + cosf(sa) * mv_speed / 256.0f,
                                                        R->wy + sinf(sa) * mv_speed / 256.0f,
                                                        R->head, w,
                                                        &wxs[w], &wys[w], &g[w]);
                                wblocked[w] = (wpo[w] != -1);
                                /* A wheel over an impassable cell is not
                                 * given the terrain at all: 0x1a68 sets its
                                 * height from [di+0x7d], the mean wheel
                                 * height, and its ground from [di+0x7f], the
                                 * mean of the four GROUNDS, zeroes its
                                 * velocity, falls into the collision
                                 * response and RETURNS -- the suspension at
                                 * 0x19ae never runs for it.  Running the
                                 * spring anyway makes the gap between the
                                 * mean wheel height and the mean ground look
                                 * like a penetration, and the truck is
                                 * launched. */
                                if (wblocked[w]) {
                                    g[w]  = R->ground_m;
                                    wh[w] = R->body_h;
                                    wv[w] = 0.0f;
                                    wall_any = 1;
                                    wall_crash |= og_wall_hit(R, tp.top, wpo[w],
                                                              &wall_state, ri, w);
                                    continue;
                                }
                                d = g[w] - wh[w];
                                if (d < 0.0f) {
                                    wv[w] -= 1.0f;                 /* airborne: gravity */
                                } else if (d > 0.0f) {             /* at or into ground */
                                    float nv;
                                    if (wv[w] >= 0.0f) {
                                        nv = wv[w] + d * k;
                                        if (og_wall_debug && d > 8.0f)
                                            OG_TRACE("[OG_Physics] Truck %d wheel %d LAUNCH on layout %d: "
                                                    "at (%.2f,%.2f) cell (%d,%d) height %d, "
                                                    "sampled ground %.0f, wheel %.0f, "
                                                    "penetration %.1f -> velocity %.2f "
                                                    "(k %.4f, shock %d), speed %.0f, blocked %d%d%d%d",
                                                    ri, w, OG_LAYOUT(track, reverse),
                                                    wxs[w], wys[w],
                                                    (int)floorf(wxs[w]), (int)floorf(wys[w]),
                                                    (Hh && wxs[w] >= 0 && wys[w] >= 0 &&
                                                     wxs[w] < OG_CONTOUR_W && wys[w] < OG_CONTOUR_H)
                                                      ? Hh[(int)floorf(wys[w]) * OG_CONTOUR_W
                                                           + (int)floorf(wxs[w])] : -1,
                                                    g[w], wh[w], d, nv,
                                                    k, tp.shock, R->speed,
                                                    wblocked[0], wblocked[1],
                                                    wblocked[2], wblocked[3]);
                                    } else {
                                        /* 0x19c3: a falling wheel.  Water
                                         * (POLY flag byte 2) splashes below
                                         * -0.375 (0x1bee); anything else
                                         * throws dust below -2.5 (0x1b96). */
                                        if (fxdef.loaded && tv.contour.loaded) {
                                            int cx = (int)floorf(wxs[w]), cy = (int)floorf(wys[w]);
                                            int flag = -1;
                                            long wwx = (long)(wxs[w] * 256.0f), wwy = (long)(wys[w] * 256.0f);
                                            if (cx >= 0 && cx < OG_CONTOUR_W && cy >= 0 && cy < OG_CONTOUR_H) {
                                                long po = og_surface_resolve_at(&g_surface,
                                                              tv.contour.cell[cy * OG_CONTOUR_W + cx],
                                                              (int)(wwx & 0xff), (int)(wwy & 0xff));
                                                if (po >= 0) flag = g_surface.poly[po];
                                            }
                                            if (flag == 2) {
                                                if (wv[w] < -0.375f) {
                                                    /* variant: wheel side, then
                                                     * the heading's quarter */
                                                    int bl = (w == 2 || w == 3) ? 0 : 1;
                                                    int al = (head_b + 0x20) & 0xc0;
                                                    if (al & 0x40) { if (al & 0x80) bl ^= 1; }
                                                    else { bl |= 2; if (al & 0x80) bl ^= 1; }
                                                    /* 0x1bee plays the sound FIRST and
                                                     * unconditionally -- the only gate is
                                                     * the 16-effect limit -- and spawns the
                                                     * splash afterwards. */
                                                    og_audio_trigger(&audio, 7);
                                                    og_arc_fx(&audio, 0x24);   /* 0xb8ad */
                                                    og_fx_spawn(fxs, &fxdef.splash[bl],
                                                                wwx + fxdef.splash_dx[bl],
                                                                wwy + fxdef.splash_dy[bl],
                                                                (int)(g[w] + 0.5f), ox, oy);
                                                }
                                            } else if (wv[w] < -2.5f) {
                                                /* the arcade sounds a landing
                                                 * as it throws this dust
                                                 * (0xb893), once a truck; DOS
                                                 * has none */
                                                if (!landed) og_arc_fx(&audio, 0x20);
                                                landed = 1;
                                                og_fx_spawn(fxs, &fxdef.dust, wwx - 0x100, wwy - 0x80,
                                                            (int)(g[w] + 0.5f), ox, oy);
                                            }
                                        } else if (wv[w] < -2.5f) landed = 1;
                                        nv = (d > -wv[w] ? d : -wv[w]) * k;
                                    }
                                    if (nv < 2.0f) nv = 0.0f;
                                    if (nv > 8.0f) nv = 8.0f;
                                    wv[w] = nv;
                                    wh[w] = g[w];                  /* onto the ground */
                                }
                            }

                            /* The tick's walls (og_wall_hit, per wheel above):
                             * the state's low bits hold the axes for 0x1580,
                             * and effect 8 sounds on EVERY tick a head-on or
                             * glancing hit happens (0x1ae0) -- the speaker
                             * driver (0x3b5d) restarts an effect whose
                             * priority is not lower, and the AdLib's own
                             * table silences effect 8 altogether. */
                            if (tv.contour.loaded && g_surface.loaded) {
                                R->block_axes = wall_state & 3;
                                if (wall_crash) {
                                    og_audio_trigger(&audio, 8);
                                    og_arc_fx(&audio, 0x21);   /* 0xbc5a, every update too */
                                }
                                R->in_wall = wall_any;
                            }

                            /* The drive model (0x1cf8): a wheel grips while
                             * its clearance is under the tires' figure.  The
                             * code pairs the wheel blocks -- the last two
                             * steer, the first two drive.  With no rear wheel
                             * down there is no drive; with one, the change is
                             * halved.  Target = top * throttle >> 8. */
                            if (perf.loaded) {
                                /* The game installs its wheels at 225, 135,
                                 * 315 and 45 degrees (bx = 0xa000, 0x6000,
                                 * 0xe000, 0x2000 at 0x1552), and pairs the
                                 * last two for steering and the first two for
                                 * drive.  Ours are 45/135/225/315, so the
                                 * front pair is [0] and [3], the rear [1],[2]. */
                                static const int rear_w[2] = { 1, 2 };
                                int gth = tp.grip, rear = 0, wq;
                                float target, step;
                                for (wq = 0; wq < 2; ++wq)
                                    if (wh[rear_w[wq]] - g[rear_w[wq]] < (float)gth) ++rear;
                                rear_n = rear;
                                /* 0x1769: the throttle [di+0x75] is not
                                 * the stick itself -- input bit 3, up, adds
                                 * 0x20 a tick and anything else takes 0x10
                                 * off, kept within 0..0x100.  A wall hit
                                 * cuts it (0x1a9e, 0x1b3b) and it builds
                                 * back up.  A computer truck runs the same
                                 * code on the byte its driver builds: 0x21e2
                                 * sets up only while the segment's figure
                                 * [di+0x52] is ABOVE the throttle -- equal
                                 * sets nothing, so it drops 0x10 and climbs
                                 * back, hovering round the figure. */
                                if (race_phase == 1) {
                                    int up;
                                    if (R->computer) {
                                        float want = 256.0f;
                                        if (route.n > 0) {
                                            want = (float)route.wp[ai_from].width;
                                            if (want <= 0.0f || want > 256.0f) want = 256.0f;
                                        }
                                        up = want > R->thr;
                                    } else
                                        up = og_player[ri].gas;   /* up, or F1's button */
                                    R->thr += up ? 32.0f : -16.0f;
                                    if (R->thr < 0.0f)   R->thr = 0.0f;
                                    if (R->thr > 256.0f) R->thr = 256.0f;
                                }
                                target = (race_phase == 1)
                                       ? (float)tp.top * (R->thr / 256.0f)
                                       : 0.0f;
                                if (R->nitro_t > 0) {      /* half again, capped */
                                    target *= 1.5f;
                                    if (target > (float)OG_NITRO_CAP)
                                        target = (float)OG_NITRO_CAP;
                                }
                                /* 0x1cad: the slip, and [di+0x89] -- set
                                 * when the truck travels more than a quarter
                                 * turn away from its nose */
                                slip0 = R->head - R->trav;
                                while (slip0 < 0.0f)    slip0 += 256.0f;
                                while (slip0 >= 256.0f) slip0 -= 256.0f;
                                back = (slip0 >= 64.0f && slip0 < 192.0f);
                                if (rear == 0) {
                                    /* coasting: 0x1cfb changes nothing */
                                } else if (!back && R->speed < target) {
                                    step = (float)tp.accel;
                                    if (rear < 2) step *= 0.5f;
                                    R->speed += step;
                                    if (R->speed > target) R->speed = target;
                                } else if (back || R->speed > target) {
                                    /* 0x1d24: going backwards -- as after a
                                     * head-on bounce -- the target is 0 and
                                     * the truck brakes.  Braking through zero
                                     * (0x1d5d) stops it, points the travel
                                     * along the heading again and drops the
                                     * throttle and the flag. */
                                    if (back) target = 0.0f;
                                    step = (float)tp.drag;
                                    if (rear < 2) step *= 0.5f;
                                    if (R->speed <= step) {
                                        R->speed = 0.0f;
                                        R->trav  = R->head;
                                        R->thr   = 0.0f;
                                        back = 0; slip0 = 0.0f;
                                    } else {
                                        R->speed -= step;
                                        if (R->speed < target) R->speed = target;
                                    }
                                }
                            }

                            /* The computer driver, image 0x2189.  It keeps
                             * the point it has reached ([di+0x4c]) and the
                             * one after ([di+0x4e]), and STEERS AT THE ONE
                             * AFTER -- the bearing it uses is the delta left
                             * by the last distance call, which is always the
                             * second point.  That lookahead is what stops it
                             * circling a point inside its own turning circle.
                             * Beyond the corner radius it rescans the route
                             * for the nearest point (0x21f3). */
                            if (race_phase == 1 && route.n > 1 && perf.loaded) {
                                float dx1 = (float)route.wp[ai_from].cx + 0.5f - R->wx;
                                float dy1 = (float)route.wp[ai_from].cy + 0.5f - R->wy;
                                float dx2 = (float)route.wp[ai_to].cx + 0.5f - R->wx;
                                float dy2 = (float)route.wp[ai_to].cy + 0.5f - R->wy;
                                float bear, err, slip, a;
                                static const int front_w[2] = { 0, 3 };
                                int front = 0, wq, gth = tp.grip;

                                if (dx2 * dx2 + dy2 * dy2 <= dx1 * dx1 + dy1 * dy1) {
                                    int nx = og_ai_next(&ai, &route);
                                    ai_from = ai_to;
                                    ai_to = (nx < 0) ? ai_from : nx;
                                    dx2 = (float)route.wp[ai_to].cx + 0.5f - R->wx;
                                    dy2 = (float)route.wp[ai_to].cy + 0.5f - R->wy;
                                }

                                /* A wheel over a barrier counts as DOWN: 0x1a68
                                 * puts it exactly on its ground, so the front
                                 * wheels still bite and the truck can be
                                 * steered away from the wall.  Treating it as
                                 * airborne leaves a driver unable to turn while
                                 * touching anything. */
                                for (wq = 0; wq < 2; ++wq)
                                    if (wblocked[front_w[wq]] ||
                                        wh[front_w[wq]] - g[front_w[wq]] < (float)gth) ++front;

                                bear = atan2f(dy2, dx2) * (256.0f / (2.0f * (float)M_PI));
                                err = bear - R->head;
                                while (err < -128.0f) err += 256.0f;
                                while (err >= 128.0f) err -= 256.0f;
                                if (!R->computer) {
                                    /* Player 1 steers: left and right rotate
                                     * the truck at the tyres' rate, and the
                                     * front wheels must be down to bite.
                                     *
                                     * The two steer tables are in DIFFERENT
                                     * units: a player's (DS:0x4fac) holds 2..8
                                     * and is added straight to the heading,
                                     * while a computer's (DS:0x5038) holds
                                     * 1152..1920 in sixteenths.  Dividing the
                                     * player's by 256 as well gives 0.008 a
                                     * tick -- nine minutes for a full turn.
                                     *
                                     * NOTE: og_perf reads the player's steer
                                     * from the OTHER tyre table (1536..2176),
                                     * which is in sixteenths like the
                                     * computer's, so /256 is right here -- 6 to
                                     * 8.5 a tick. */
                                    float rate = tp.steer / 256.0f;
                                    if (front < 2) rate *= 0.5f;
                                    if (front > 0 && og_player[ri].left)  R->head -= rate;
                                    if (front > 0 && og_player[ri].right) R->head += rate;
                                    while (R->head < 0.0f)    R->head += 256.0f;
                                    while (R->head >= 256.0f) R->head -= 256.0f;
                                } else if (front > 0 && (err > 8.0f || err < -8.0f)) {
                                    float rate = tp.steer / 256.0f;
                                    if (front < 2) rate *= 0.5f;
                                    R->head += (err > 0.0f) ? rate : -rate;
                                    while (R->head < 0.0f)    R->head += 256.0f;
                                    while (R->head >= 256.0f) R->head -= 256.0f;
                                }

                                /* travel chases the heading (0x1ce7), by
                                 * the slip taken BEFORE the steering, with the
                                 * backwards flag added -- a truck going
                                 * backwards chases the heading's reverse, not
                                 * swinging its travel round to its nose.
                                 * Only with a rear wheel down; double with
                                 * both. */
                                slip = slip0 + (back ? 128.0f : 0.0f);
                                while (slip < -128.0f) slip += 256.0f;
                                while (slip >= 128.0f) slip -= 256.0f;
                                R->trav += slip * (tp.slip / 256.0f) * (float)rear_n;
                                while (R->trav < 0.0f)    R->trav += 256.0f;
                                while (R->trav >= 256.0f) R->trav -= 256.0f;

                                /* Nitro: the route's 0x83 gate names the
                                 * checkpoints where a computer truck asks
                                 * for it (0x2301); it will not fire while a
                                 * burst runs, and firing spends one from
                                 * the truck's own count. */
                                /* A computer truck asks for nitro through
                                 * route opcode 0x88, which reaches the
                                 * firing handler at 0x2301 -- but walking
                                 * all eight streams, 0x88 NEVER APPEARS.
                                 * The AI's input byte is cleared each tick
                                 * at 0x217b and only the route sets bits in
                                 * it, so in the original a computer truck
                                 * essentially never fires.  SORP gives them
                                 * a little, so the boost is not the
                                 * player's alone: once a lap, and only when
                                 * the rubber band has them well behind. */
                                if (R->nitro_t > 0) {
                                    if (--R->nitro_t == 0)
                                        og_audio_stop(&audio, 10);
                                }
                                else if (!R->computer) {
                                    /* the button: one nitro a press, and
                                     * nothing when the tank is empty */
                                    if (og_player[ri].nitro && R->nitro > 0) {
                                        --R->nitro;
                                        R->nitro_t = OG_NITRO_TICKS;
                                        og_audio_trigger(&audio, 10);
                                        og_arc_fx(&audio, 0x0c);      /* 0xc93b */
                                        if (fxdef.loaded) {
                                            int oct = ((head_b + 0x20) >> 5) & 7;
                                            og_fx_spawn(fxs, &fxdef.nitro,
                                                        (long)(R->wx * 256.0f) + fxdef.burst_dx[oct],
                                                        (long)(R->wy * 256.0f) + fxdef.burst_dy[oct],
                                                        (int)(og_ground(Hh, R->wx, R->wy) + 0.5f),
                                                        ox, oy);
                                        }
                                    }
                                }
                                /* A computer truck does NOT decide to use
                                 * nitro.  The ROUTE tells it where: the
                                 * dispatch table at DS:0x7519 sends
                                 * opcode 0x88 to the firing handler at
                                 * 0x2301, which carries a level range
                                 * tested at 0x238d, and refuses while a
                                 * boost is already running.
                                 *
                                 * NO SHIPPED LAYOUT CONTAINS 0x88 -- all
                                 * sixteen were scanned and none has a
                                 * single gate -- so in the original the
                                 * computer trucks NEVER fire nitro.  The
                                 * machinery is there and the data never
                                 * asks for it.
                                 *
                                 * SORP fired once a lap for any truck at
                                 * cpu_base+3 or better, which is why they
                                 * seemed to be boosting constantly.  The
                                 * condition below is the authentic one,
                                 * so it simply never comes true on the
                                 * shipped tracks. */
                                else if (R->nitro > 0 && R->nitro_t == 0 &&
                                         route.n > 0 &&
                                         R->level >= route.wp[ai_from].nitro_lo &&
                                         R->level <= route.wp[ai_from].nitro_hi &&
                                         (route.wp[ai_from].nitro_lo ||
                                          route.wp[ai_from].nitro_hi)) {
                                    {
                                        R->nitro_lap = R->laps;
                                        --R->nitro;
                                        R->nitro_t = OG_NITRO_TICKS;
                                        og_audio_trigger(&audio, 10);
                                        og_arc_fx(&audio, 0x0c);      /* 0xc93b */
                                        if (fxdef.loaded) {
                                            /* behind the truck, by octant */
                                            int oct = ((head_b + 0x20) >> 5) & 7;
                                            og_fx_spawn(fxs, &fxdef.nitro,
                                                        (long)(R->wx * 256.0f) + fxdef.burst_dx[oct],
                                                        (long)(R->wy * 256.0f) + fxdef.burst_dy[oct],
                                                        (int)(og_ground(Hh, R->wx, R->wy) + 0.5f),
                                                        ox, oy);
                                        }
                                    }
                                }

                                /* the throttle builds in the drive (0x1769),
                                 * a computer's toward the segment's figure */

                                /* it moves along the travel direction, cut to
                                 * 32 steps by the table index at 0x1871 */
                                a = og_step_angle(mv_trav);
                                /* 0x1580: an axis the collision state blocks
                                 * keeps its old value -- the move is simply
                                 * not committed on it, which is what keeps a
                                 * truck out of a wall rather than letting it
                                 * slide about inside one. */
                                /* 0x1871 keeps the step's parts in
                                 * [di+0x55] and [di+0x57] whether or not
                                 * 0x1580 then commits them */
                                R->vx = cosf(a) * mv_speed;
                                R->vy = sinf(a) * mv_speed;
                                if (!(R->block_axes & 1))
                                    R->wx += cosf(a) * mv_speed / 256.0f;
                                if (!(R->block_axes & 2))
                                    R->wy += sinf(a) * mv_speed / 256.0f;
                                R->block_axes = 0;      /* cleared each tick */

                                /* past the corner radius, re-acquire (0x21f3) */
                                dx1 = (float)route.wp[ai_from].cx + 0.5f - R->wx;
                                dy1 = (float)route.wp[ai_from].cy + 0.5f - R->wy;
                                if ((dx1 * 4.0f) * (dx1 * 4.0f) + (dy1 * 4.0f) * (dy1 * 4.0f)
                                        > (float)route.wp[ai_from].radius) {
                                    R->ai = ai;
                                    og_ai_acquire(R, &route);
                                    ai = R->ai; ai_from = R->from; ai_to = R->to;
                                }
                            }

                            /* rigid chassis, 0x1d73: diagonal pairs are
                             * (225,45) = [2],[0] and (135,315) = [1],[3] */
                            {
                                float A = 0.5f * (wh[2] + wh[0]);
                                float B = 0.5f * (wh[1] + wh[3]);
                                float dd = A - B;
                                if (dd >= 1.0f)       { wh[1] += 0.5f * dd; wh[3] += 0.5f * dd; }
                                else if (-dd >= 1.0f) { wh[2] -= 0.5f * dd; wh[0] -= 0.5f * dd; }
                            }
                        }

                        /* [di+0x7d] and [di+0x7f] */
                        body_h   = 0.25f * (wh[0] + wh[1] + wh[2] + wh[3]);
                        R->body_h  = body_h;       /* [di+0x7d] for next tick */
                        ground_h = 0.25f * (g[0] + g[1] + g[2] + g[3]);
                        R->ground_m = ground_h;    /* [di+0x7f], after it is
                                                    * computed -- storing it
                                                    * first saved the zero it
                                                    * starts each tick with */
                        air = body_h - ground_h;
                        shadow_h = ground_h;
                        {
                            float thr = phys.loaded ? phys.air[track & 7] : 8.0f;
                            int now = air >= thr;
                            /* A wheel landing faster than 2.5 is routine --
                             * about 30 a lap off ordinary steps -- so only a
                             * takeoff and the landing that ends a flight are
                             * logged. */
                            if (now && !flying) {
                                if (ovl_wheels)
                                    OG_TRACE("[OG_Physics] Airborne: %.1f above the ground", air);
                                fly_air = air;
                            }
                            if (now && air > fly_air) fly_air = air;
                            if (!now && flying &&
                                ovl_wheels)
                                OG_TRACE("[OG_Physics] Landed%s after reaching %.1f",
                                        landed ? " hard" : "", fly_air);
                            flying = now;
                        }

                        for (w = 0; w < 4; ++w) {       /* wheels where they are */
                            int sx, sy;
                            og_project((long)(wxs[w] * 256.0f), (long)(wys[w] * 256.0f),
                                       (int)(wh[w] + 0.5f), ox, oy, &sx, &sy);
                            wheel_scr[w].x = (float)sx; wheel_scr[w].y = (float)sy;
                        }
                        have_wheels = 1;

                        /* front = [0],[3] (+-45), rear = [1],[2];
                         * right = [0],[1] (+90 side), left = [2],[3] */
                        pitch = 0.5f * (wh[0] + wh[3]) - 0.5f * (wh[1] + wh[2]);
                        if (race_phase != 2) {      /* the attitude freezes too */
                            int np = og_tilt_index(pitch);
                            int nr = og_tilt_index(0.5f * (wh[0] + wh[1])
                                                   - 0.5f * (wh[2] + wh[3]));
                            if ((np != pitch_i || nr != roll_i) &&
                                ovl_wheels)
                                OG_TRACE("[OG_Physics] Attitude: pitch %d roll %d  "
                                        "(front-rear %+.1f)", np, nr, pitch);
                            pitch_i = np; roll_i = nr;
                        }
                        /* heading byte [di+0x82]: world angle, 256 per turn */
                        head_b = (int)floorf(head / (2.0f * (float)M_PI) * 256.0f) & 0xff;
                    }

                    /* Place the truck.  GAME.EXE 0x15d0 projects the
                     * GROUND mean [di+0x7f] -- where the shadow sits --
                     * as the object's point, and EGA.EXE 0x57c4 lifts the
                     * truck sprite above it by the high bytes' difference
                     * [di+0x7e] - [di+0x80]: a pixel a height unit.  The
                     * projection (0x165f) takes the high byte alone, so
                     * each height counts in whole units, cut down, not
                     * rounded.  Projected at its own height the truck
                     * lands on the same row. */
                    {
                        int px, py;
                        og_project((long)(fx * 256.0f), (long)(fy * 256.0f),
                                   (int)floorf(body_h), ox, oy, &px, &py);
                        tx = (float)px;
                        ty = (float)py;
                        og_project((long)(fx * 256.0f), (long)(fy * 256.0f),
                                   (int)floorf(shadow_h), ox, oy, &px, &py);
                        shadow_pt.x = (float)px;
                        shadow_pt.y = (float)py;
                        fx_key = (int)(fy * 256.0f) >> 5;   /* [si+0x18] */
                    }
                    (void)seg;

                /* The truck goes on the display list at its own sort key,
                 * with its shadow (ESHADOW.PCG): the silhouette for this
                 * direction, (-heading)>>3 like the level truck frames,
                 * laid on the GROUND under the truck -- [di+0x7f], so it
                 * stays down when the truck flies -- with the truck's own
                 * box anchor.  og_screen_compose draws the shadow, then
                 * the truck, as 0x57aa does. */
                og_draw_item *di = og_draw_add(OG_DI_TRUCK, fx_key);
                /* frame = base[pitch*5+roll] + table[(-heading)>>3],
                 * exactly as GAME.EXE 0x1e4b; stored at [di+0x87] */
                int idx = og_truck_frame(&att_tab, pitch_i, roll_i, head_b);
                if (idx < 0 || idx >= trucks.nframes) idx = 0;
                if (di) {
                    int s = ((-head_b) & 0xff) >> 3;
                    di->sprite = idx;
                    di->colour = truck_colour < 0 ? 0 : truck_colour;
                    di->high   = flying;     /* 0x15e8: over the foreground */
                    /* the arcade's depth (master 0x58bf), its overlays'
                     * test: from where the truck is on the course */
                    di->depth  = og_tp_bg ? TP_DEPTH(fy * 256.0f) : -1;
                    /* [si+0x2a] = x - 15, [si+0x2e] = y - 14: the box's
                     * top-left.  The sprite's rows start `top` rows into
                     * its 32-row box (table byte +3); frame_oy stores
                     * 16 - top.  x = box x + table +7 (EGA.EXE 0x5893):
                     * sprites are stored at the left of their cell, and
                     * this offset -- 4 side-on, up to 11 end-on -- puts
                     * each in place. */
                    di->x = (int)floorf(tx - OG_TRUCK_ANCHOR_X) + trucks.frame_ox[idx];
                    di->y = (int)floorf(ty - OG_TRUCK_ANCHOR_Y)
                          + (OG_TRUCK_BOX_ROWS / 2 - trucks.frame_oy[idx]);
                    /* EGA.EXE 0x60a0: sprite 0x19c (412) + ((-heading)
                     * & 0xf8)/8; y = box y + top (+5) + 2, x = box x +
                     * x offset (+7) - 2 -- two pixels left and down of
                     * where the truck itself is drawn. */
                    if (shadows.loaded) {
                        di->sh   = s;
                        di->sh_x = (int)floorf(shadow_pt.x - OG_TRUCK_ANCHOR_X) + shadows.xoff[s] - 2;
                        di->sh_y = (int)floorf(shadow_pt.y - OG_TRUCK_ANCHOR_Y) + shadows.top[s] + 2;
                    }
                }

                    R->ai = ai; R->from = ai_from; R->to = ai_to; R->t = ai_t;
                    for (lw = 0; lw < 4; ++lw) { R->wh[lw] = wh[lw]; R->wv[lw] = wv[lw]; }
                    R->live = wheels_live; R->flying = flying;
                }   /* for each racer */

                /* Truck against truck, image 0x169d.  The object pass
                 * (0x0e25) finds contacts from each object's shape in the
                 * column and row bucket tables; og_collide is that test.
                 * A truck's tick (0x153b) settles each contact that is NEW
                 * -- not in [di+0x59], last tick's contacts -- and clears
                 * its own bit from the other's, so a pair is settled once:
                 *
                 *   d     = (B's step - A's step), less a quarter (sar 2)
                 *   A     = A's step + d,  B = B's step - d
                 *   0xf3e turns each back into a travel direction (the
                 *         high byte only) and a speed, the magnitude
                 *   the throttle is floored at 0x20, and effect 0 sounds
                 *
                 * The steps are [di+0x55] and [di+0x57], the parts 0x1871
                 * formed last tick -- cut to 32 directions, and NOT
                 * changed by an earlier bump this tick, so a truck in two
                 * new contacts trades its own old step in both and keeps
                 * the result of the second.  Exchanging the post-tick
                 * travel and speed instead, as SORP did, mixed in the
                 * walls' and the drive's changes and let a first bump
                 * feed the second.  With both parts zero 0xf3e hands back
                 * a direction of 0 and a speed of 0. */
                if (race_phase == 1 && perf.loaded) {
                    const float K = 2.0f * (float)M_PI / 256.0f;
                    int a, b;
                    for (a = 0; a < OG_RACERS; ++a)
                        for (b = a + 1; b < OG_RACERS; ++b) {
                            og_racer *A = &racers[a], *B = &racers[b];
                            const og_shape *sa = &truck_box[OG_TRUCKBOX_INDEX((int)A->head)];
                            const og_shape *sb = &truck_box[OG_TRUCKBOX_INDEX((int)B->head)];
                            float dvx, dvy, nv[2][2];
                            og_racer *T[2];
                            int q;
                            if (!og_collide((int)(A->wx * 8.0f), (int)(A->wy * 8.0f), sa,
                                            (int)(B->wx * 8.0f), (int)(B->wy * 8.0f), sb)) {
                                A->touched &= ~(1u << b);
                                B->touched &= ~(1u << a);
                                continue;
                            }
                            if (A->touched & (1u << b)) continue;   /* already touching */
                            A->touched |= 1u << b;
                            B->touched |= 1u << a;

                            dvx = (B->vx - A->vx) * 0.75f;
                            dvy = (B->vy - A->vy) * 0.75f;
                            nv[0][0] = A->vx + dvx; nv[0][1] = A->vy + dvy;
                            nv[1][0] = B->vx - dvx; nv[1][1] = B->vy - dvy;
                            T[0] = A; T[1] = B;
                            for (q = 0; q < 2; ++q) {
                                og_racer *R2 = T[q];
                                float fr = R2->trav - floorf(R2->trav);
                                int   ang = 0;
                                R2->speed = sqrtf(nv[q][0] * nv[q][0] + nv[q][1] * nv[q][1]);
                                if (nv[q][0] != 0.0f || nv[q][1] != 0.0f)
                                    ang = (int)floorf(atan2f(nv[q][1], nv[q][0]) / K + 0.5f);
                                /* the high byte is replaced, the fraction kept */
                                R2->trav = (float)(ang & 255) + fr;
                                if (R2->thr < 32.0f) R2->thr = 32.0f;
                            }
                            og_audio_trigger(&audio, 0);       /* the bump */
                            og_arc_fx(&audio, 0x22);           /* 0xc7c6 */
                        }
                }
            }

            /* The rest of the display list, then the screen drawn from
             * it in sort order (og_screen_compose).  The track's
             * foreground plane masks the TRUCKS only, so they pass
             * behind bridges and banks: etruck loads with flags 0x87,
             * bit 7, while eshadow and egame load with 0x01 and 0x0f and
             * are painted straight over it -- which is why a flagpole
             * standing against a bank on Huevos Grande or Fandango is
             * still drawn whole. */
            if (have_truck) {
                og_scenery_queue(&scenery, &egame, sc_state);
                if (pk_live)
                    og_draw_sprite_q(pk_sort,
                                     pk_type ? OG_PICKUP_NITRO : OG_PICKUP_CASH,
                                     pk_px, pk_py);
                og_fx_queue(fxs);
                if (scenery.loaded) {      /* the flagman, sort 0x7fff */
                    const og_fx_anim *fa = (fm_mode == 2) ? &scenery.fm_chequer
                                                          : &scenery.fm_wave;
                    int sp = (fm_i >= 0 && fm_i < fa->n)
                             ? fa->sprite + fa->frame[fm_i] : scenery.fm_idle;
                    og_draw_sprite_q(0x7fff, sp, scenery.fm_x, scenery.fm_y);
                }
                /* The race picture is opaque and drawn first: no blending,
                 * and ARGB, the byte order of the software renderer's
                 * window -- blended and converted it cost the framebuffer
                 * path several ms a frame for nothing. */
                if (!screen_tex) {
                    screen_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                                   SDL_TEXTUREACCESS_STREAMING,
                                                   OG_SCREEN_W, OG_SCREEN_H);
                    if (screen_tex) {
                        SDL_SetTextureScaleMode(screen_tex, SDL_SCALEMODE_NEAREST);
                        SDL_SetTextureBlendMode(screen_tex, SDL_BLENDMODE_NONE);
                    }
                }
                if (screen_tex4 && screen_tex4_k != og_up_k()) {
                    SDL_DestroyTexture(screen_tex4);    /* 2x <-> 4x */
                    screen_tex4 = NULL;
                }
                if (OG_COMPOSE4() && !screen_tex4) {
                    screen_tex4_k = og_up_k();
                    screen_tex4 = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    OG_SCREEN_W * screen_tex4_k,
                                                    OG_SCREEN_H * screen_tex4_k);
                    /* drawn at whatever size the window is: smoothed, as
                     * 640x400 and 1280x800 rarely land on whole pixels --
                     * except on the framebuffer, where the window is
                     * this texture's size and smoothing is only slow */
                    if (screen_tex4) {
                        SDL_SetTextureScaleMode(screen_tex4, og_window ? SDL_SCALEMODE_LINEAR
                                                                       : SDL_SCALEMODE_NEAREST);
                        SDL_SetTextureBlendMode(screen_tex4, SDL_BLENDMODE_NONE);
                    }
                }
                if (OG_COMPOSE4() && screen_tex4 && tv.art.pic.px) {
                    SDL_FRect full;
                    full.x = 0.0f; full.y = 0.0f;
                    full.w = (float)OG_SCREEN_W; full.h = (float)OG_SCREEN_H;
                    og_screen_compose4(screen_tex4, tv.art.pic.px,
                                       tv.art.has_fg ? tv.art.fg : NULL,
                                       &egame, &trucks, &shadows);
                    SDL_RenderTexture(ren, screen_tex4, NULL, &full);
                } else if (screen_tex && tv.art.pic.px) {
                    og_screen_compose(screen_tex, tv.art.pic.px,
                                      tv.art.has_fg ? tv.art.fg : NULL,
                                      &egame, &trucks, &shadows);
                    SDL_RenderTexture(ren, screen_tex, NULL, NULL);
                }
            }

            if (mesh_mode)
                og_draw_contour(ren, &tv.contour, &g_surface,
                                (tv.has_heights ? tv.heights : NULL),
                                track_ox, track_oy,
                                mesh_mode);

            if (ovl_wheels && have_wheels) {
                int w;
                /* the ground point under the truck, and the flight above it */
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 160);
                SDL_RenderLine(ren, shadow_pt.x, shadow_pt.y, tx, ty);
                SDL_RenderLine(ren, shadow_pt.x - 3, shadow_pt.y, shadow_pt.x + 3, shadow_pt.y);
                SDL_RenderLine(ren, shadow_pt.x, shadow_pt.y - 2, shadow_pt.x, shadow_pt.y + 2);
                for (w = 0; w < 4; ++w) {
                    SDL_FRect d;
                    /* front pair white, rear pair orange */
                    if (w == 0 || w == 3) SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                    else                  SDL_SetRenderDrawColor(ren, 255, 140,  40, 255);
                    d.x = wheel_scr[w].x - 1.5f; d.y = wheel_scr[w].y - 1.5f;
                    d.w = 3.0f; d.h = 3.0f;
                    SDL_RenderFillRect(ren, &d);
                }
            }
            if (ovl_gates)
                og_draw_gates(ren, &cps, &tv.art, tv.water,
                              (tv.has_heights ? tv.heights : NULL),
                              track_ox, track_oy,
                              ai.next_cp);
            if (ovl_waypoints)
                og_draw_route(ren, &tv.art, tv.water, &route, (tv.has_heights ? tv.heights : NULL),
                              track_ox, track_oy,
                              ai_to);
        }

        /* THE SCOREBOARD, in the place the track's own artwork keeps for
         * it.  0x03f6 walks the 19 cells at DS:0x410, offset by the
         * track's origin from DS:0x3f0, and the driver stamps a digit
         * into each: three for the elapsed time (the artwork supplies
         * the decimal point in the gap between the second and third),
         * four lap digits -- each truck's [di+0x3c] plus one -- and the
         * nitro counts in groups of three. */
        if (!showing_title && !showing_shop && !showing_end && og_options != 3 &&
            hud.loaded && egame.nframes > OG_HUD_DIGIT0 + 9) {
            static char last[32] = "";
            static int last_gen = 0;           /* the shaders it was made under */
            static SDL_Texture *score_tex = NULL;
            static SDL_FRect score_dst;        /* where the digits' box goes */
            static int last_lay = -1;      /* the board moves with the track */
            char digits[OG_HUD_CELLS + 1];
            int lay_now = OG_LAYOUT(track, reverse);
            int q, k = 0, t10 = (int)(racers[0].time_ticks / 200);
            int t1  = (int)((racers[0].time_ticks / 20) % 10);
            int tf  = (int)((racers[0].time_ticks % 20) / 2);
            digits[k++] = (char)('0' + t10 % 10);      /* the time */
            digits[k++] = (char)('0' + t1);
            digits[k++] = (char)('0' + tf);
            for (q = 0; q < OG_RACERS; ++q)            /* the laps, plus one */
                digits[k++] = (char)('0' + (racers[q].laps + 1) % 10);
            for (q = 0; q < OG_RACERS; ++q) {
                /* 0x2ecc uses the group two ways: ten or more puts the
                 * digits in the outer cells, below ten puts the single
                 * digit in the middle one, centred. */
                int n = racers[q].nitro % 100;
                if (n >= 10) {
                    digits[k++] = (char)('0' + n / 10);
                    digits[k++] = ' ';
                    digits[k++] = (char)('0' + n % 10);
                } else {
                    digits[k++] = ' ';
                    digits[k++] = (char)('0' + n);
                    digits[k++] = ' ';
                }
            }
            digits[k] = '\0';
            /* The original sets the board up when a track loads (es:[0x74],
             * which also saves the background behind each cell), so it
             * never carries over from the last track.  Rebuilding only
             * when the digits change is not enough here: the figures are
             * often identical across a change of track while the origin
             * is not, leaving the old overlay in the old corner. */
            if (SDL_strcmp(digits, last) != 0 || lay_now != last_lay ||
                last_gen != og_shade_gen) {
                last_lay = lay_now;
                last_gen = og_shade_gen;
                og_image board;
                SDL_strlcpy(last, digits, sizeof last);
                if (og_image_init(&board, OG_SCREEN_W, OG_SCREEN_H) == OG_OK) {
                    int lay = lay_now, c;
                    memset(board.px, 0, (size_t)OG_SCREEN_W * OG_SCREEN_H);
                    for (c = 0; c < k && c < OG_HUD_CELLS; ++c) {
                        /* the game's own 8x6 digits, EGAME 77 + n */
                        int fr = OG_HUD_DIGIT0 + (digits[c] - '0');
                        if (digits[c] == ' ') continue;
                        if (fr < 0 || fr >= egame.nframes) continue;
                        /* 0x2f7b adds the sprite's own x offset before
                         * splitting the position into byte and bit, so a
                         * digit sits that far right of its cell -- 10 for
                         * most, 11 for the narrow 1. */
                        og_image_blit(&board, &egame.frames[fr],
                                      hud.ox[lay & 7] + hud.cx[c]
                                        + (egame.frame_ox ? egame.frame_ox[fr] : 10),
                                      hud.oy[lay & 7] + hud.cy[c],
                                      egame.key);
                    }
                    if (score_tex) SDL_DestroyTexture(score_tex);
                    score_tex = NULL;
                    {   /* Only the digits' box, with 3 clear pixels round
                         * it -- as far as MMPX looks -- goes through the
                         * shader: the whole 320x200, ten times a second
                         * as the clock runs, cost 35 ms each on an H700. */
                        int x0 = OG_SCREEN_W, y0 = OG_SCREEN_H, x1 = -1, y1 = -1, x, y;
                        og_image part;
                        for (y = 0; y < OG_SCREEN_H; ++y)
                            for (x = 0; x < OG_SCREEN_W; ++x)
                                if (board.px[y * OG_SCREEN_W + x]) {
                                    if (x < x0) x0 = x;
                                    if (x > x1) x1 = x;
                                    if (y < y0) y0 = y;
                                    if (y > y1) y1 = y;
                                }
                        if (x1 >= 0) {
                            x0 = x0 > 3 ? x0 - 3 : 0;  y0 = y0 > 3 ? y0 - 3 : 0;
                            x1 = x1 + 3 < OG_SCREEN_W ? x1 + 3 : OG_SCREEN_W - 1;
                            y1 = y1 + 3 < OG_SCREEN_H ? y1 + 3 : OG_SCREEN_H - 1;
                            if (og_image_init(&part, x1 - x0 + 1, y1 - y0 + 1) == OG_OK) {
                                for (y = 0; y < part.h; ++y)
                                    memcpy(part.px + (size_t)y * part.w,
                                           board.px + (size_t)(y0 + y) * OG_SCREEN_W + x0,
                                           (size_t)part.w);
                                score_tex = texture_from_shaded(ren, &part, og_pal_race, 0,
                                                                OG_LAYER_SPRITE);
                                score_dst.x = (float)x0;      score_dst.y = (float)y0;
                                score_dst.w = (float)part.w;  score_dst.h = (float)part.h;
                                og_image_free(&part);
                            }
                        }
                    }
                    og_image_free(&board);
                }
            }
            if (score_tex) SDL_RenderTexture(ren, score_tex, NULL, &score_dst);
        }

        /* Registration's prompt goes on top of whatever screen is up. */
        /* Registration draws the game's OWN pages into the player's
         * panel -- the letter grid at DS:0x842f -- with the cursor sprite
         * over the chosen cell, flashing between the two halves of the
         * ESHOP cursor set.
         *
         * It belongs to the Speed Shop's panels, so it is drawn ONLY
         * while the shop is up.  Drawing it over a running race put the
         * letter grid across the track. */
        if (showing_shop) {
            static SDL_Texture *reg_tex[OG_PLAYERS];
            static int reg_shown[OG_PLAYERS];
            static int reg_end_x[OG_PLAYERS], reg_end_y[OG_PLAYERS];
            int p;
            for (p = 0; p < OG_PLAYERS; ++p) {
                og_reg *r = &og_regs[p];
                int px0 = OG_PANEL_X0 + p * OG_PANEL_STEP;
                /* the nation list's second page, while the cursor is on it */
                unsigned page2 = 0;
                if (r->step == OG_REG_NONE || r->step == OG_REG_DONE) continue;
                if (!pages.loaded || !roadfont.loaded) continue;
                if (r->step == OG_REG_NATION && r->cell >= 8 && og_nations_more())
                    page2 = og_nation_page(&pages);

                if (!reg_tex[p] ||
                    reg_shown[p] != (int)r->step * 2 + r->ytab + (page2 ? 100 : 0)) {
                    og_image im;
                    if (og_image_init(&im, OG_PANEL_STEP, OG_SCREEN_H - OG_PANEL_Y)
                        == OG_OK) {
                        memset(im.px, 0,
                               (size_t)OG_PANEL_STEP * (OG_SCREEN_H - OG_PANEL_Y));
                        /* the prologue first: it selects ROADFONT, fixed
                         * pitch, the letter spacing and the leading that
                         * the grid is laid out with */
                        /* The shop palette makes index 5 white and 14
                         * blue, so the styles read: "Ye" is white with a
                         * dark border, "B2" blue with none.  The middle
                         * panel is yellow, so its lettering is blue; the
                         * red and blue panels take white. */
                        {
                        unsigned pro = (r->step == OG_REG_INITIALS)
                                     ? OG_REG_PAGE_PROLOGUE : OG_REG_PAGE_PRO_DATE;
                        unsigned pg  = (r->step == OG_REG_INITIALS) ? OG_REG_PAGE_INITIALS
                                     : (r->step == OG_REG_MONTH)    ? OG_REG_PAGE_MONTHS
                                     : (r->step == OG_REG_DAY)      ? OG_REG_PAGE_DAYS
                                     : (r->step == OG_REG_YEAR)
                                       ? (r->ytab == 0
                                            ? OG_REG_PAGE_YEARS : OG_REG_PAGE_YEARS2)
                                     : page2 ? page2 : OG_REG_PAGE_NATIONS;
                        og_page_draw_after(&pages, pro, pg,
                                     &im, &ironfont.f, &roadfont.f, &styles,
                                     &reg_end_x[p], &reg_end_y[p]);
                        }
                        {   /* the page carries no style of its own, so
                             * recolour its ink: white on the red and blue
                             * panels, blue on the yellow one */
                            unsigned char want = (p == 1) ? 14 : 5;
                            size_t q, n = (size_t)im.w * im.h;
                            for (q = 0; q < n; ++q)
                                if (im.px[q]) im.px[q] = want;
                        }
                        if (reg_tex[p]) SDL_DestroyTexture(reg_tex[p]);
                        reg_tex[p] = texture_from_kept(ren, &im, og_pal_shop, 0,
                                                       OG_LAYER_SPRITE);
                        reg_shown[p] = (int)r->step * 2 + r->ytab + (page2 ? 100 : 0);
                        og_image_free(&im);
                    }
                }
                og_reshade(ren, &reg_tex[p]);
                if (reg_tex[p]) {
                    /* The page starts where its wrapper page puts the
                     * pen (og_reg_page_at); the cursor is placed by the
                     * input handler's own arithmetic, so the two meet
                     * exactly as they do in the original. */
                    SDL_FRect dst = { (float)(px0 + og_reg_page_at[r->step][0]),
                                      (float)(OG_PANEL_Y + og_reg_page_at[r->step][1]),
                                      (float)OG_PANEL_STEP,
                                      (float)(OG_SCREEN_H - OG_PANEL_Y) };
                    SDL_RenderTexture(ren, reg_tex[p], NULL, &dst);
                }

                /* What has been entered so far, on the panel's bottom
                 * line (y 186 -- every wrapper page leaves the pen at
                 * (12, 186)).  The input handlers draw it, each its own
                 * field, erasing with the '^' glyph first:
                 *
                 *   initials  letter n at x 0x3c + 8n + panel x (0x27c6)
                 *   month     pen + 16 (page 0x92e9)
                 *   day       pen + 34 (0x9367), '/' at x 40 (page 0x9310)
                 *   year      pen + 52 (0x9435), '/' at x 58 (page 0x93b4)
                 *
                 * and while a field waits its slot alternates between the
                 * blank pair and "_ " (0x279c, 0x28cf) -- a flashing
                 * underscore.  The line is cleared with the panel before
                 * the initials, the month and the nation (rectangle
                 * 0x3218), but NOT before the day or the year (0x3220,
                 * which stops at y 184): so the date builds up as
                 * "11/", "11/27/", and the initials are gone by then. */
                if (roadfont.loaded) {
                    static SDL_Texture *ent_tex[OG_PLAYERS];
                    static char ent_shown[OG_PLAYERS][24];
                    struct { int x; char s[4]; } part[8];
                    int np = 0, q, blink = ((SDL_GetTicks() / 250) & 1) == 0;
                    char key[24];
                    #define OG_ENT(X, S) do { part[np].x = (X);                         SDL_strlcpy(part[np].s, (S), 4); ++np; } while (0)
                    if (r->step == OG_REG_INITIALS) {
                        for (q = 0; q < r->letter && q < 3; ++q) {
                            char c[2] = { r->initials[q], 0 };
                            OG_ENT(60 + 8 * q, c);
                        }
                        if (r->letter < 3 && blink) OG_ENT(60 + 8 * r->letter, "_");
                    } else if (r->step >= OG_REG_MONTH && r->step <= OG_REG_YEAR) {
                        char v[4];
                        if (r->step > OG_REG_MONTH) {
                            SDL_snprintf(v, sizeof v, "%02d", r->month);
                            OG_ENT(20, v);
                            OG_ENT(32, "/");
                        }
                        if (r->step > OG_REG_DAY) {
                            SDL_snprintf(v, sizeof v, "%02d", r->day);
                            OG_ENT(38, v);
                            OG_ENT(50, "/");
                        }
                        if (blink)
                            OG_ENT(r->step == OG_REG_MONTH ? 20
                                 : r->step == OG_REG_DAY   ? 38 : 56, "_");
                    }
                    #undef OG_ENT
                    key[0] = 0;
                    for (q = 0; q < np; ++q) SDL_strlcat(key, part[q].s, sizeof key);
                    SDL_strlcat(key, blink ? "*" : ".", sizeof key);
                    key[sizeof key - 2] = (char)('0' + r->step);
                    static int ent_gen[OG_PLAYERS];
                    if (SDL_strcmp(key, ent_shown[p]) != 0 || !ent_tex[p] ||
                        ent_gen[p] != og_shade_gen) {
                        ent_gen[p] = og_shade_gen;
                        og_image im;
                        SDL_strlcpy(ent_shown[p], key, sizeof ent_shown[p]);
                        if (og_image_init(&im, OG_PANEL_STEP, 12) == OG_OK) {
                            memset(im.px, 0, (size_t)OG_PANEL_STEP * 12);
                            /* "no border": the outline plane is not drawn
                             * at all -- the same ink as the grid above */
                            for (q = 0; q < np; ++q)
                                og_font_draw(&roadfont.f, &im, part[q].x, 0,
                                             part[q].s, -2, p == 1 ? 14 : 5, -1);
                            if (ent_tex[p]) SDL_DestroyTexture(ent_tex[p]);
                            ent_tex[p] = texture_from_shaded(ren, &im, og_pal_shop, 0,
                                                             OG_LAYER_SPRITE);
                            og_image_free(&im);
                        }
                    }
                    if (ent_tex[p]) {
                        SDL_FRect dst = { (float)px0,
                                          (float)(OG_PANEL_Y + OG_REG_ENTRY_DY),
                                          (float)OG_PANEL_STEP, 12.0f };
                        SDL_RenderTexture(ren, ent_tex[p], NULL, &dst);
                    }
                }

                /* The nation list's flags, beside each name; the second
                 * page's are the arcade's in the shop's colours, where
                 * GAME.EXE's frames (top offset 16) sit, at y as given */
                if (r->step == OG_REG_NATION) {
                    int n;
                    for (n = 0; n < 8; ++n) {
                        const float fy = (float)(OG_PANEL_Y + OG_REG_FLAG_Y + n * OG_REG_NATION_DY);
                        og_image fl;
                        if (!page2) {
                            if (og_reg_flag[n] < shopbank.nframes)
                                og_blit_sprite(ren, &shopbank, og_reg_flag[n],
                                               px0 + OG_REG_FLAG_X, (int)fy);
                        } else if (og_nation_flag(OG_NATION_MORE + n, OG_FLAG_SHOP, &fl) == OG_OK) {
                            SDL_Texture *t = texture_from_shaded(ren, &fl, og_pal_shop, 0,
                                                                 OG_LAYER_SPRITE);
                            if (t) {
                                SDL_FRect dst = { (float)(px0 + OG_REG_FLAG_X), fy,
                                                  (float)fl.w, (float)fl.h };
                                SDL_RenderTexture(ren, t, NULL, &dst);
                                SDL_DestroyTexture(t);
                            }
                            og_image_free(&fl);
                        }
                    }
                }

                /* the cursor itself: the handler's shape, in as many
                 * pieces as it has, side by side, flashing between the
                 * two sets of eight */
                if (shopbank.nframes > 16) {
                    int cx, cy, shape, pieces, k;
                    og_reg_cursor(r, &cx, &cy, &shape, &pieces);
                    shape += ((SDL_GetTicks() / 250) & 1) ? OG_CURSOR_FLASH : 0;
                    /* pieces are CONSECUTIVE frames, as the shop's
                     * boxes and the figure are: the nation bar is 0 + 1 */
                    for (k = 0; k < pieces; ++k) {
                        og_blit_sprite(ren, &shopbank, shape + k,
                                       px0 + cx, OG_PANEL_Y + cy);
                        cx += shopbank.frames[shape + k].w;
                    }
                }
            }
        }

        if (shading) og_shade_present(ren, game_rt, &eagle_tex, og_screen_shader);

        /* THE SPRITE VIEWER (F3), over everything else: the bank on a
         * neutral grey, so every palette's darkest and lightest show, with
         * its name above and what is shown below. */
        if (og_options == 3) {
            static const char *const nm[OG_SPR_VIEWS] = {
                "ETRUCK.PCG attitudes", "ETRUCK.PCG", "EGAME.PCG",
                "ESHADOW.PCG", "ESHOP.PCG", "EEND.PCG", "ETITLE.PCG",
                "fonts", "pictures", "tracks", "arcade tracks", "arcade sprites"
            };
            /* each bank's first entry in EGA.EXE's sprite table: ETRUCK
             * is entries 0..411, and the rest follow it */
            static const int table_first[OG_SPR_VIEWS] = {
                0, 0, OG_EGAME_FIRST, OG_SHADOW_FIRST,
                OG_ESHOP_FIRST, OG_EEND_FIRST, OG_ETITLE_FIRST, 0, 0, 0, 0, 0
            };
            static const char *const paint[4] = { "red", "yellow", "blue", "white" };
            static char info[80] = "";
            const int v = og_spr_view;
            const int truck = (v == OG_SPR_ATTITUDES || v == OG_SPR_ETRUCK);
            const og_bank *b = &trucks;
            const unsigned char *pal = og_pal_race;
            /* the shadows are 1-bit masks: black on clear */
            static const unsigned char shadow_pal[16 * 3] = { 0 };
            char line[48];
            SDL_FRect d;
            switch (v) {
            case OG_SPR_EGAME:   b = &egame;                          break;
            case OG_SPR_ESHADOW: b = &shadowbank; pal = shadow_pal;   break;
            case OG_SPR_ESHOP:   b = &shopbank;   pal = og_pal_shop;  break;
            case OG_SPR_EEND:    b = &endbank;    pal = og_pal_end;   break;
            case OG_SPR_ETITLE:  b = &titlebank;  pal = og_pal_title; break;
            default:                                                  break;
            }
            /* ESHADOW is loaded as masks for the race; made into a bank
             * the first time it is wanted here */
            if (v == OG_SPR_ESHADOW && !shadowbank.frames && shadows.loaded) {
                int k;
                shadowbank.frames = (og_image *)calloc(OG_SHADOWS, sizeof *shadowbank.frames);
                shadowbank.frame_oy = (int *)calloc(OG_SHADOWS, sizeof *shadowbank.frame_oy);
                if (shadowbank.frames && shadowbank.frame_oy) {
                    shadowbank.nframes = OG_SHADOWS;
                    for (k = 0; k < OG_SHADOWS; ++k) {
                        shadowbank.frame_oy[k] = OG_TRUCK_BOX_ROWS / 2 - shadows.top[k];
                        if (shadows.rows[k] > 0 &&
                            og_image_init(&shadowbank.frames[k], 24, shadows.rows[k]) == OG_OK)
                            memcpy(shadowbank.frames[k].px, shadows.mask[k],
                                   (size_t)24 * shadows.rows[k]);
                    }
                }
            }
            if (og_spr_dirty) {
                og_spr_dirty = 0;
                og_spr_more = 0;
                if (sheet_tex) { SDL_DestroyTexture(sheet_tex); sheet_tex = NULL; }
                if (v >= OG_SPR_FONTS) {
                    og_spr_more = og_spr_extra(ren, dir, OG_SPR_FONTS, v,
                                               og_spr_page[v], &sheet, &sheet_tex,
                                               info, sizeof info);
                } else if (b->nframes <= 0) {
                    SDL_snprintf(info, sizeof info, "not loaded");
                } else if (v == OG_SPR_ATTITUDES) {
                    int level = -1;
                    bank_compose_attitudes(&sheet, b, &att_tab, og_spr_dir, &level);
                    sheet_tex = texture_from_truck(ren, &sheet, og_pal_race, 0,
                                                   og_spr_colour);
                    /* the level frame's bank index and, ETRUCK starting
                     * the table, its entry -- the same number */
                    SDL_snprintf(info, sizeof info, "direction %d/32, level %d (table %d)",
                                 og_spr_dir, level, level + table_first[v]);
                } else {
                    int pg = og_spr_page[v], first = og_spr_start[v][pg], shown = 0;
                    static unsigned char grp[1024];
                    const unsigned char *gp = NULL;
                    int next, nb = b->nframes < 1024 ? b->nframes : 1024;
                    switch (v) {
                    case OG_SPR_ETITLE:
                        og_spr_groups(grp, nb, og_spr_title_groups,
                                      (int)SDL_arraysize(og_spr_title_groups));
                        gp = grp; break;
                    case OG_SPR_ESHOP:
                        og_spr_groups(grp, nb, og_spr_shop_groups,
                                      (int)SDL_arraysize(og_spr_shop_groups));
                        gp = grp; break;
                    case OG_SPR_EEND:
                        og_spr_groups(grp, nb, og_spr_end_groups,
                                      (int)SDL_arraysize(og_spr_end_groups));
                        gp = grp; break;
                    default: break;
                    }
                    next = bank_compose_grid(&gsheet, b, gp, first, &shown);
                    if (pg + 1 < OG_SPR_PAGES) og_spr_start[v][pg + 1] = next;
                    og_spr_more = next < b->nframes;
                    sheet_tex = (v == OG_SPR_ETRUCK)
                              ? texture_from_truck(ren, &gsheet, og_pal_race, 0,
                                                   og_spr_colour)
                              : texture_from(ren, &gsheet, pal, b->key, NULL);
                    /* the bank's own index, 0 up, and the sprite table's
                     * entry for the same sprites */
                    SDL_snprintf(info, sizeof info, "%d-%d (table %d-%d) of %d p%d",
                                 first, first + shown - 1,
                                 table_first[v] + first,
                                 table_first[v] + first + shown - 1,
                                 b->nframes, pg + 1);
                }
            }
            d.x = 0.0f; d.y = 0.0f;
            d.w = (float)OG_SCREEN_W; d.h = (float)OG_SCREEN_H;
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(ren, 96, 96, 96, 255);
            SDL_RenderFillRect(ren, &d);
            if (sheet_tex) {
                if (v != OG_SPR_ATTITUDES && v < OG_SPR_FONTS) {
                    d.y = (float)OG_SPR_CAPTION;
                    d.h = (float)gsheet.h;
                }
                SDL_RenderTexture(ren, sheet_tex, NULL, &d);
            }
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_snprintf(line, sizeof line, "%d/%d %s", v + 1, OG_SPR_VIEWS, nm[v]);
            SDL_RenderDebugText(ren, 2.0f, 1.0f, line);
            if (truck || v == OG_SPR_ASPRITES) {   /* the paint, and how to change it */
                if (truck)
                    SDL_snprintf(line, sizeof line, "%s [button]", paint[og_spr_colour & 3]);
                else
                    SDL_snprintf(line, sizeof line, "track %d [button]", og_spr_apal);
                SDL_RenderDebugText(ren, (float)(OG_SCREEN_W - 2 - 8 * (int)SDL_strlen(line)),
                                    1.0f, line);
            }
            SDL_RenderDebugText(ren, 2.0f, (float)(OG_SCREEN_H - 9), info);
        } else if (og_options) {
            /* THE OVERLAYS, over everything else: F1's options or F2's
             * audio.  Headings in yellow, the selected row marked with a
             * caret. */
            const og_overlay_tab *t = &og_ovls[og_options];
            /* the whole screen, so nothing of the game shows through */
            SDL_FRect bg;
            /* F2's rows are closer together, to leave room for its
             * meters at the foot of the screen */
            int r, y = OG_OPT_Y0, dy = (og_options == 2) ? 10 : OG_OPT_DY;
            bg.x = 0.0f;
            bg.y = 0.0f;
            bg.w = (float)OG_SCREEN_W;
            bg.h = (float)OG_SCREEN_H;
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 220);
            SDL_RenderFillRect(ren, &bg);
            SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
            bg.x = 2.0f; bg.y = 2.0f;
            bg.w = (float)(OG_SCREEN_W - 4);
            bg.h = (float)(OG_SCREEN_H - 4);
            SDL_RenderRect(ren, &bg);
            if (og_options == 2) y = 6;
            for (r = 0; r < t->n; ++r) {
                const og_option *o = &t->rows[r];
                char line[64];
                if (o->act == og_ovl_exit) {
                    /* F1's "Exit SORP", alone in the bottom left corner,
                     * level with the build stamp; red, bright when chosen */
                    SDL_snprintf(line, sizeof line, "%c %s",
                                 r == t->row ? '>' : ' ', o->label);
                    if (r == t->row) SDL_SetRenderDrawColor(ren, 255, 70, 70, 255);
                    else             SDL_SetRenderDrawColor(ren, 170, 40, 40, 255);
                    SDL_RenderDebugText(ren, (float)OG_OPT_X0,
                                        (float)(OG_SCREEN_H - 14), line);
                    continue;
                }
                if (o->act == og_ovl_about) {
                    /* the build stamp, bottom right: it opens "About" */
                    SDL_snprintf(line, sizeof line, "%c %s",
                                 r == t->row ? '>' : ' ', og_build_stamp());
                    if (r == t->row) SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                    else             SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
                    SDL_RenderDebugText(ren,
                        (float)(OG_SCREEN_W - 6 - (int)SDL_strlen(line) * 8),
                        (float)(OG_SCREEN_H - 14), line);
                    continue;
                }
                if (!o->value) {                       /* a heading */
                    SDL_snprintf(line, sizeof line, "%s", o->label + 2);
                } else {
                    /* a named row cut to one state (og_shader_limit)
                     * has nothing to offer here */
                    const char *v = o->show ? o->show(*o->value)
                                  : (o->names && o->modulo == 1) ? "n/a"
                                  : (o->names && *o->value < o->modulo)
                                  ? o->names[*o->value] : "?";
                    /* F1's labels run to 20 characters; F2 keeps 18, as
                     * its values are the longer ones */
                    SDL_snprintf(line, sizeof line, "%c %-*s %s",
                                 r == t->row ? '>' : ' ',
                                 og_options == 2 ? 18 : 20, o->label, v);
                }
                if (!o->value)
                    SDL_SetRenderDrawColor(ren, 255, 210, 60, 255);
                else if (r == t->row)
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                else
                    SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
                SDL_RenderDebugText(ren, (float)OG_OPT_X0, (float)y, line);
                if (o->value == &og_volume) {
                    /* a level bar, three pixels a step, where the value
                     * column starts, and the figure after it */
                    const int pct = *o->value * OG_VOLUME_STEP;
                    const float bx = (float)(OG_OPT_X0 + 23 * 8);
                    const float bw = (float)(3 * (OG_VOLUME_STEPS - 1));
                    SDL_FRect fr;
                    char num[8];
                    fr.x = bx; fr.y = (float)y; fr.w = bw + 2.0f; fr.h = 8.0f;
                    SDL_RenderRect(ren, &fr);
                    fr.x = bx + 1.0f; fr.y = (float)y + 1.0f;
                    fr.w = (float)(3 * *o->value); fr.h = 6.0f;
                    if (pct > 0) SDL_RenderFillRect(ren, &fr);
                    SDL_snprintf(num, sizeof num, "%3d", pct);
                    SDL_RenderDebugText(ren, bx + bw + 6.0f, (float)y, num);
                }
                y += dy;
            }
            if (og_options == 2)
                og_draw_meters(ren, OG_OPT_X0 + 8, OG_SCREEN_H - 40, 26, 1);
            if (og_options == 1) {
                char stamp[32];
                int len;
                SDL_SetRenderDrawColor(ren, 110, 110, 110, 255);
                /* frames a second, top right, level with the heading:
                 * whole frames shown, the screen copy included */
                SDL_snprintf(stamp, sizeof stamp, "%.1f fps", og_fps);
                len = (int)SDL_strlen(stamp);
                SDL_RenderDebugText(ren, (float)(OG_SCREEN_W - 6 - len * 8),
                                    (float)OG_OPT_Y0, stamp);
                /* the render path, left of it: green when the GPU draws
                 * the frame, amber when the CPU does */
                if (og_render_path[0]) {
                    const int pl = (int)SDL_strlen(og_render_path);
                    if (og_render_gpu) SDL_SetRenderDrawColor(ren, 90, 200, 90, 255);
                    else               SDL_SetRenderDrawColor(ren, 220, 160, 50, 255);
                    SDL_RenderDebugText(ren, (float)(OG_SCREEN_W - 6 - (len + 2 + pl) * 8),
                                        (float)OG_OPT_Y0, og_render_path);
                }
            }
        }

        og_console_set(og_console);     /* opened and closed from the overlay */

        /* F1's "About", over the overlay; it goes with the overlay */
        if (og_about && og_options != 1) og_about = 0;
        if (og_about) og_draw_about(ren);

        /* "Exit SORP?" -- a band across the middle, over everything. */
        if (og_quit_ask) {
            const char *l1 = og_quit_ask == 2 ? "Back to the title?" : "Exit SORP?";
            const char *l2 = og_quit_ask == 2 ? "ESC or Start to end the game,"
                                              : "ESC or Start to exit,";
            static const char *l3 = "any other key or button to resume.";
            SDL_FRect bg;
            bg.x = 0.0f;
            bg.y = (float)(OG_SCREEN_H / 2 - 28);
            bg.w = (float)OG_SCREEN_W;
            bg.h = 56.0f;
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 230);
            SDL_RenderFillRect(ren, &bg);
            SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
            SDL_RenderRect(ren, &bg);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDebugText(ren,
                (float)((OG_SCREEN_W - (int)SDL_strlen(l1) * 8) / 2),
                bg.y + 8.0f, l1);
            SDL_SetRenderDrawColor(ren, 190, 190, 190, 255);
            SDL_RenderDebugText(ren,
                (float)((OG_SCREEN_W - (int)SDL_strlen(l2) * 8) / 2),
                bg.y + 26.0f, l2);
            SDL_RenderDebugText(ren,
                (float)((OG_SCREEN_W - (int)SDL_strlen(l3) * 8) / 2),
                bg.y + 38.0f, l3);
        }

        /* the meters over the game as well, top left */
        if (og_meters && !og_options)
            og_draw_meters(ren, 4, 4, 40, 0);
        /* once a frame, whether or not they are showing: the bars fall
         * back, and the speaker's lights while it has a tone */
        og_sound_meters_decay();
        og_arc_meters_decay();
        og_arc_metering = og_arc_on(&audio);
        og_spk_level *= 0.82f;
        if (og_spk_level < 0.004f) og_spk_level = 0.0f;
        if (audio_mode == OG_AUDIO_SPK && pcspk.divisor) og_spk_level = 1.0f;

        og_present(ren);
        {   /* F1's frame rate: the frames counted over half a second */
            static Uint64 fps_t0;
            static int fps_n;
            const Uint64 t = SDL_GetTicks();
            if (!fps_t0) fps_t0 = t;
            ++fps_n;
            if (t - fps_t0 >= 500) {
                og_fps = (float)fps_n * 1000.0f / (float)(t - fps_t0);
                fps_t0 = t;
                fps_n = 0;
            }
        }
        {   /* the log's: which screen is up, when it changes */
            static const char *const scr_name[] = {
                "the title", "the Ironman Club", "the race", "the results",
                "the Speed Shop"
            };
            static int scr_seen = -1, att_seen = -1;
            const int scr = showing_end ? 3 : showing_shop ? 4 :
                            showing_club ? 1 : showing_title ? 0 : 2;
            if (scr != scr_seen || og_attract != att_seen) {
                OG_INFO("[OG_Game] Screen: %s%s", scr_name[scr],
                        og_attract ? " (attract mode)" : "");
                scr_seen = scr; att_seen = og_attract;
                og_crash_where = scr_name[scr];
            }
        }
        {   /* the log's: the race's frames, green light to chequered flag.
             * A frame is late when it took half as long again as the
             * screen's refresh -- a frame the screen showed twice. */
            const Uint64 t = SDL_GetTicksNS();
            if (race_phase == 1) {
                if (!rs_t0) {
                    const SDL_DisplayMode *m =
                        SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win));
                    const float hz = (m && m->refresh_rate > 0.0f) ? m->refresh_rate : 0.0f;
                    char hzs[24];
                    rs_t0 = rs_prev = t;
                    rs_frames = rs_late = 0;
                    rs_worst = 0;
                    rs_ticks = 0;
                    rs_late_ns = (Uint64)(1.5e9 / (hz > 0.0f ? hz : 60.0f));
                    if (hz > 0.0f) SDL_snprintf(hzs, sizeof hzs, "%.0f Hz", (double)hz);
                    else SDL_strlcpy(hzs, "? Hz (taken as 60)", sizeof hzs);
                    SDL_snprintf(rs_how, sizeof rs_how,
                                 "a %s screen, %s, shaders %s / %s / %s", hzs,
                                 og_window ? og_render_path : "the framebuffer",
                                 og_screen_shader_nm[og_screen_shader],
                                 og_sprite_shader_nm[og_sprite_shader],
                                 og_back_shader_nm[og_back_shader]);
                } else {
                    const Uint64 d = t - rs_prev;
                    ++rs_frames;
                    if (d > rs_worst) rs_worst = d;
                    if (d > rs_late_ns) ++rs_late;
                    rs_prev = t;
                }
                rs_t1 = t;
            } else if (race_phase == 0) {
                rs_t0 = 0;                     /* the next green light */
            }
        }
        {   /* the log's: the frame rate over each minute, and the slowest */
            static Uint64 min_t0, prev;
            static int min_n;
            static Uint64 worst;
            const Uint64 t = SDL_GetTicks();
            if (!min_t0) min_t0 = prev = t;
            ++min_n;
            if (t - prev > worst) worst = t - prev;
            prev = t;
            if (t - min_t0 >= 60000) {
                OG_DEBUG("[OG_Video] %.1f frames/s over the last minute; the "
                         "slowest frame %u ms", (double)min_n * 1000.0 /
                         (double)(t - min_t0), (unsigned)worst);
                min_t0 = t; min_n = 0; worst = 0;
            }
        }
        og_win_settle(0);              /* a moved window, once it is still */
        SDL_Delay(1);
    }

    og_win_settle(1);                  /* and whatever is left, at exit */
    og_crash_where = "the way out";
    {
        const Uint64 s = SDL_GetTicks() / 1000;
        OG_INFO("[SORP] Leaving: %s, after %u:%02u:%02u", og_exit_why,
                (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    }
    if (sheet_tex) SDL_DestroyTexture(sheet_tex);
    if (eagle_tex) SDL_DestroyTexture(eagle_tex);
    if (game_rt) SDL_DestroyTexture(game_rt);
    og_image_free(&sheet);
    og_image_free(&gsheet);
    og_bank_free(&shadowbank);
    og_spr_extra_free();
    og_surface_free(&g_surface);
    og_audio_p = NULL;
    og_audio_close(&audio);
    track_view_free(&tv);
    og_bank_free(&trucks);
    og_bank_free(&egame);
    og_bank_free(&shopbank);
    og_bank_free(&endbank);
    if (shop_tex) SDL_DestroyTexture(shop_tex);
    if (end_tex) SDL_DestroyTexture(end_tex);
    {
        int k;
        for (k = 0; k < OG_EGAME_COUNT; ++k)
            if (egame_tex[k]) SDL_DestroyTexture(egame_tex[k]);
    }
    og_shadows_free(&shadows);
    if (screen_tex) SDL_DestroyTexture(screen_tex);
    if (screen_tex4) SDL_DestroyTexture(screen_tex4);
    og_sprite_table_free(&sprtab);
    og_image_free(&title);
    og_input_close();
    if (club_tex) SDL_DestroyTexture(club_tex);
    if (title_tex) SDL_DestroyTexture(title_tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    og_fb_close();
    SDL_Quit();
    og_log_close();
    return 0;
}
