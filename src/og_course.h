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

/* og_course.h -- turning a track picture into something raceable.
 *
 * The original keeps its collision data somewhere we have not decoded
 * (see the note on CONTOUR in og_data.h), so the course geometry is
 * reconstructed here instead, in two layers:
 *
 *   1. A per-pixel surface map derived from the background artwork.
 *      The infield/rough is a single flat attribute index across every
 *      shipped track, so "not that index" gives the raceable channel.
 *      The art is heavily dithered, so the mask is despeckled with a
 *      majority filter before use.
 *
 *   2. A centreline: a closed loop of waypoints with a half-width at
 *      each node.  It is traced automatically at load time by a virtual
 *      driver that walks the ridge of the distance transform, and can
 *      be overridden by a per-track waypoint file written by the
 *      built-in editor (F3 in game).
 *
 * The automatic trace is a starting point, not gospel -- it is derived
 * from dithered 1989 artwork and will want a pass in the editor on some
 * tracks.  Everything is computed at run time from the player's own
 * files; no course data ships with this source.
 */
#ifndef OG_COURSE_H
#define OG_COURSE_H

#include "og_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Surface classes the simulation understands. */
enum {
    OG_SURF_ROAD  = 0,   /* the raceable channel                      */
    OG_SURF_ROUGH = 1,   /* infield / verge: slow, poor grip          */
    OG_SURF_WALL  = 2    /* hard edge: bounce                         */
};

#define OG_MAX_NODES 1024

typedef struct {
    float x, y;          /* centre, screen pixels                     */
    float half;          /* half-width of the channel here            */
    float dist;          /* arc length from node 0                    */
} og_node;

typedef struct {
    unsigned char surf[OG_SCREEN_W * OG_SCREEN_H];
    float         open[OG_SCREEN_W * OG_SCREEN_H]; /* dist to non-road */

    og_node       node[OG_MAX_NODES];
    int           nnodes;
    float         length;        /* total lap length in pixels        */
    int           from_file;     /* waypoints came from disk          */

    float         start_x, start_y, start_dir;
} og_course;

/* Build the surface map and open-space field from the artwork. */
int  og_course_build(og_course *c, const og_track_art *art, int rough_index);

/* Trace a centreline automatically.  Called by og_course_build unless
 * waypoints were loaded first. */
int  og_course_autotrace(og_course *c);

/* Per-track waypoint files live beside the config as track<N>.path. */
int  og_course_load_path(og_course *c, const char *dir, int index);
int  og_course_save_path(const og_course *c, const char *dir, int index);

/* Recompute arc lengths, half-widths and the start pose after the node
 * list has been edited. */
void og_course_finalize(og_course *c);

/* Surface at a pixel; out-of-bounds reads as rough. */
int  og_course_surface(const og_course *c, float x, float y);

/* Nearest node index to a point, searching outward from `hint`. */
int  og_course_nearest(const og_course *c, float x, float y, int hint);

/* Signed progress in pixels along the lap for a point near node `n`. */
float og_course_progress(const og_course *c, float x, float y, int n);

/* Point on the centreline `ahead` pixels past node `n`. */
void og_course_lookahead(const og_course *c, int n, float ahead,
                         float *out_x, float *out_y);

/* Editing helpers used by the in-game editor. */
int  og_course_insert_node(og_course *c, int after, float x, float y);
void og_course_delete_node(og_course *c, int idx);

#ifdef __cplusplus
}
#endif
#endif /* OG_COURSE_H */
