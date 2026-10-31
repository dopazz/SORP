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

/* og_course.c -- see og_course.h */

#include "og_course.h"
#include "og_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W OG_SCREEN_W
#define H OG_SCREEN_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Surface map                                                         */
/* ------------------------------------------------------------------ */

/* Two-pass chamfer distance from every road pixel to the nearest
 * non-road pixel. */
static void og_distance_field(const unsigned char *road, float *d)
{
    const float BIG = 1.0e9f;
    int x, y;

    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            d[y * W + x] = road[y * W + x] ? BIG : 0.0f;

    for (y = 0; y < H; ++y) {
        for (x = 0; x < W; ++x) {
            float *v = &d[y * W + x];
            if (*v == 0.0f) continue;
            if (x > 0        && d[y * W + x - 1]       + 1.0f   < *v) *v = d[y * W + x - 1]       + 1.0f;
            if (y > 0        && d[(y-1) * W + x]       + 1.0f   < *v) *v = d[(y-1) * W + x]       + 1.0f;
            if (x > 0 && y > 0   && d[(y-1)*W + x - 1] + 1.414f < *v) *v = d[(y-1)*W + x - 1] + 1.414f;
            if (x < W-1 && y > 0 && d[(y-1)*W + x + 1] + 1.414f < *v) *v = d[(y-1)*W + x + 1] + 1.414f;
        }
    }
    for (y = H - 1; y >= 0; --y) {
        for (x = W - 1; x >= 0; --x) {
            float *v = &d[y * W + x];
            if (*v == 0.0f) continue;
            if (x < W-1      && d[y * W + x + 1]       + 1.0f   < *v) *v = d[y * W + x + 1]       + 1.0f;
            if (y < H-1      && d[(y+1) * W + x]       + 1.0f   < *v) *v = d[(y+1) * W + x]       + 1.0f;
            if (x < W-1 && y < H-1 && d[(y+1)*W + x+1] + 1.414f < *v) *v = d[(y+1)*W + x+1] + 1.414f;
            if (x > 0   && y < H-1 && d[(y+1)*W + x-1] + 1.414f < *v) *v = d[(y+1)*W + x-1] + 1.414f;
        }
    }
}

/* Keep only the largest 4-connected road region; strays like the HUD
 * panel and enclosed scenery drop out. */
static void og_keep_largest(unsigned char *road)
{
    int *lab = (int *)malloc(sizeof(int) * W * H);
    int *stack = (int *)malloc(sizeof(int) * W * H);
    int i, cur = 0, bestlab = -1, bestn = 0;

    if (!lab || !stack) { free(lab); free(stack); return; }
    for (i = 0; i < W * H; ++i) lab[i] = -1;

    for (i = 0; i < W * H; ++i) {
        int sp = 0, n = 0;
        if (!road[i] || lab[i] >= 0) continue;
        stack[sp++] = i;
        lab[i] = cur;
        while (sp) {
            int p = stack[--sp], px = p % W, py = p / W, k;
            static const int dx[4] = { 1, -1, 0, 0 };
            static const int dy[4] = { 0, 0, 1, -1 };
            ++n;
            for (k = 0; k < 4; ++k) {
                int nx = px + dx[k], ny = py + dy[k], q;
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                q = ny * W + nx;
                if (road[q] && lab[q] < 0) { lab[q] = cur; stack[sp++] = q; }
            }
        }
        if (n > bestn) { bestn = n; bestlab = cur; }
        ++cur;
    }

    for (i = 0; i < W * H; ++i)
        if (road[i] && lab[i] != bestlab) road[i] = 0;

    free(lab);
    free(stack);
}

int og_course_build(og_course *c, const og_track_art *art, int rough_index)
{
    unsigned char *road;
    int i;

    if (!c || !art || !art->pic.px) return OG_E_ARG;
    (void)rough_index;   /* superseded by og_race_class */

    road = (unsigned char *)malloc((size_t)W * H);
    if (!road) return OG_E_NOMEM;

    /* The whole playfield is dirt; the barriers are what stops you.
     * So the drivable mask is "not a barrier", taken straight from the
     * attribute index through the race classification table.
     *
     * Trim (HUD panel, banner, start marker) is drawn over the
     * playfield rather than being part of it, so it counts as drivable
     * here -- otherwise the HUD would punch a hole in the course. */
    for (i = 0; i < W * H; ++i) {
        unsigned char cls = og_race_class[art->pic.px[i] & 15];
        road[i] = (unsigned char)(cls != OG_CLS_SOLID);
    }

    /* Do NOT run a majority filter over the barriers.  They are thin
     * dithered lines, and a 3x3 majority pass dilates them: measured
     * wall coverage rose from a true 17-27% to 38-50%, which chokes the
     * corridors and starves the tracer.  Instead drop only pixels with
     * no orthogonal neighbour, which kills stray dither speckle in the
     * dirt without thickening anything. */
    {
        unsigned char *tmp = (unsigned char *)malloc((size_t)W * H);
        if (tmp) {
            int x, y;
            memcpy(tmp, road, (size_t)W * H);
            for (y = 1; y < H - 1; ++y) {
                for (x = 1; x < W - 1; ++x) {
                    int i2 = y * W + x;
                    if (tmp[i2]) continue;               /* barrier */
                    if (tmp[i2 - 1] && tmp[i2 + 1] &&
                        tmp[i2 - W] && tmp[i2 + W])
                        road[i2] = 1;                    /* lone speck */
                }
            }
            free(tmp);
        }
    }

    og_keep_largest(road);

    for (i = 0; i < W * H; ++i)
        c->surf[i] = road[i] ? (unsigned char)OG_SURF_ROAD
                             : (unsigned char)OG_SURF_WALL;

    og_distance_field(road, c->open);
    free(road);

    if (!c->from_file) og_course_autotrace(c);
    return OG_OK;
}

int og_course_surface(const og_course *c, float x, float y)
{
    int ix = (int)x, iy = (int)y;
    if (!c || ix < 0 || ix >= W || iy < 0 || iy >= H) return OG_SURF_ROUGH;
    return c->surf[iy * W + ix];
}

static float og_open_at(const og_course *c, float x, float y)
{
    int ix = (int)x, iy = (int)y;
    if (ix < 0 || ix >= W || iy < 0 || iy >= H) return 0.0f;
    return c->open[iy * W + ix];
}

/* ------------------------------------------------------------------ */
/* Automatic centreline trace                                          */
/* ------------------------------------------------------------------ */

/* Slide a point sideways onto the local ridge of the open field, so the
 * trace stays in the middle of the channel. */
static void og_recentre(const og_course *c, float *x, float *y, float dir)
{
    float nx = -sinf(dir), ny = cosf(dir);
    float best = og_open_at(c, *x, *y), bx = *x, by = *y;
    int k;

    for (k = -6; k <= 6; ++k) {
        float px = *x + nx * (float)k;
        float py = *y + ny * (float)k;
        float v  = og_open_at(c, px, py);
        if (v > best) { best = v; bx = px; by = py; }
    }
    *x = bx; *y = by;
}

int og_course_autotrace(og_course *c)
{
    float x, y, dir, bestv;
    int i, steps, besti = 0;
    const float STEP = 4.0f;

    if (!c) return OG_E_ARG;
    c->nnodes = 0;

    /* Seed at the most open point of the channel. */
    bestv = 0.0f;
    for (i = 0; i < W * H; ++i)
        if (c->open[i] > bestv) { bestv = c->open[i]; besti = i; }
    if (bestv < 2.0f) return OG_E_RANGE;

    x = (float)(besti % W);
    y = (float)(besti / W);

    /* Initial heading: whichever of 32 directions keeps the most open
     * space over a short probe. */
    {
        float bd = 0.0f, bs = -1.0f;
        int a;
        for (a = 0; a < 32; ++a) {
            float th = (float)a * (float)(2.0 * M_PI / 32.0);
            float s = 0.0f;
            int t;
            for (t = 1; t <= 8; ++t)
                s += og_open_at(c, x + cosf(th) * (float)t * 2.0f,
                                   y + sinf(th) * (float)t * 2.0f);
            if (s > bs) { bs = s; bd = th; }
        }
        dir = bd;
    }

    {
        float sx = x, sy = y;
        for (steps = 0; steps < OG_MAX_NODES * 4; ++steps) {
            float bestscore = -1.0e9f, bestdir = dir;
            int k;

            /* Pick the heading that keeps the most room ahead, with a
             * mild penalty for turning so the trace does not double
             * back into side pockets. */
            for (k = -5; k <= 5; ++k) {
                float th = dir + (float)k * 0.13f;
                float s = 0.0f;
                int t;
                for (t = 1; t <= 6; ++t) {
                    float px = x + cosf(th) * (float)t * 3.0f;
                    float py = y + sinf(th) * (float)t * 3.0f;
                    float o  = og_open_at(c, px, py);
                    if (o <= 0.0f) { s -= 40.0f; break; }
                    s += o;
                }
                s -= fabsf((float)k) * 1.5f;
                if (s > bestscore) { bestscore = s; bestdir = th; }
            }

            dir = bestdir;
            x += cosf(dir) * STEP;
            y += sinf(dir) * STEP;
            og_recentre(c, &x, &y, dir);

            if (x < 1 || x >= W - 1 || y < 1 || y >= H - 1) break;
            if (og_open_at(c, x, y) <= 0.0f) break;

            if (c->nnodes < OG_MAX_NODES) {
                c->node[c->nnodes].x = x;
                c->node[c->nnodes].y = y;
                ++c->nnodes;
            } else break;

            /* Closed the loop? */
            if (c->nnodes > 40) {
                float dx = x - sx, dy = y - sy;
                if (dx * dx + dy * dy < (STEP * 1.5f) * (STEP * 1.5f)) break;
            }
        }
    }

    if (c->nnodes < 8) { c->nnodes = 0; return OG_E_RANGE; }
    og_course_finalize(c);
    return OG_OK;
}

/* ------------------------------------------------------------------ */
/* Geometry bookkeeping                                                */
/* ------------------------------------------------------------------ */

void og_course_finalize(og_course *c)
{
    int i;
    float acc = 0.0f;

    if (!c || c->nnodes < 2) return;

    for (i = 0; i < c->nnodes; ++i) {
        int j = (i + 1) % c->nnodes;
        float dx = c->node[j].x - c->node[i].x;
        float dy = c->node[j].y - c->node[i].y;
        c->node[i].dist = acc;
        acc += sqrtf(dx * dx + dy * dy);

        c->node[i].half = og_open_at(c, c->node[i].x, c->node[i].y);
        if (c->node[i].half < 4.0f)  c->node[i].half = 4.0f;
        if (c->node[i].half > 26.0f) c->node[i].half = 26.0f;
    }
    c->length = acc;

    c->start_x = c->node[0].x;
    c->start_y = c->node[0].y;
    c->start_dir = atan2f(c->node[1 % c->nnodes].y - c->node[0].y,
                          c->node[1 % c->nnodes].x - c->node[0].x);
}

int og_course_nearest(const og_course *c, float x, float y, int hint)
{
    int i, best = 0;
    float bd = 1.0e18f;

    if (!c || c->nnodes <= 0) return 0;

    /* Local search around the hint first -- trucks move a few pixels a
     * frame, so this is nearly always enough. */
    if (hint >= 0 && hint < c->nnodes) {
        int k;
        for (k = -12; k <= 12; ++k) {
            int j = ((hint + k) % c->nnodes + c->nnodes) % c->nnodes;
            float dx = c->node[j].x - x, dy = c->node[j].y - y;
            float d = dx * dx + dy * dy;
            if (d < bd) { bd = d; best = j; }
        }
        if (bd < 40.0f * 40.0f) return best;
    }

    bd = 1.0e18f;
    for (i = 0; i < c->nnodes; ++i) {
        float dx = c->node[i].x - x, dy = c->node[i].y - y;
        float d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

float og_course_progress(const og_course *c, float x, float y, int n)
{
    const og_node *a, *b;
    float ax, ay, bx, by, ex, ey, len2, t;

    if (!c || c->nnodes < 2) return 0.0f;
    n = ((n % c->nnodes) + c->nnodes) % c->nnodes;

    a = &c->node[n];
    b = &c->node[(n + 1) % c->nnodes];
    ax = a->x; ay = a->y; bx = b->x; by = b->y;
    ex = bx - ax; ey = by - ay;
    len2 = ex * ex + ey * ey;
    if (len2 < 1e-6f) return a->dist;

    t = ((x - ax) * ex + (y - ay) * ey) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a->dist + t * sqrtf(len2);
}

void og_course_lookahead(const og_course *c, int n, float ahead,
                         float *out_x, float *out_y)
{
    float want;
    int i;

    if (!c || c->nnodes < 2) { *out_x = *out_y = 0.0f; return; }

    want = c->node[((n % c->nnodes) + c->nnodes) % c->nnodes].dist + ahead;
    while (want >= c->length) want -= c->length;
    while (want < 0.0f)       want += c->length;

    for (i = 0; i < c->nnodes; ++i) {
        int j = (i + 1) % c->nnodes;
        float d0 = c->node[i].dist;
        float d1 = (j == 0) ? c->length : c->node[j].dist;
        if (want >= d0 && want <= d1) {
            float t = (d1 - d0) > 1e-6f ? (want - d0) / (d1 - d0) : 0.0f;
            *out_x = c->node[i].x + (c->node[j].x - c->node[i].x) * t;
            *out_y = c->node[i].y + (c->node[j].y - c->node[i].y) * t;
            return;
        }
    }
    *out_x = c->node[0].x;
    *out_y = c->node[0].y;
}

/* ------------------------------------------------------------------ */
/* Editing                                                             */
/* ------------------------------------------------------------------ */

int og_course_insert_node(og_course *c, int after, float x, float y)
{
    int i;
    if (!c || c->nnodes >= OG_MAX_NODES) return OG_E_RANGE;
    if (after < -1 || after >= c->nnodes) after = c->nnodes - 1;

    for (i = c->nnodes; i > after + 1; --i)
        c->node[i] = c->node[i - 1];

    c->node[after + 1].x = x;
    c->node[after + 1].y = y;
    c->node[after + 1].half = 10.0f;
    ++c->nnodes;
    og_course_finalize(c);
    return after + 1;
}

void og_course_delete_node(og_course *c, int idx)
{
    int i;
    if (!c || idx < 0 || idx >= c->nnodes || c->nnodes <= 3) return;
    for (i = idx; i < c->nnodes - 1; ++i)
        c->node[i] = c->node[i + 1];
    --c->nnodes;
    og_course_finalize(c);
}

/* ------------------------------------------------------------------ */
/* Waypoint files                                                      */
/* ------------------------------------------------------------------ */

int og_course_load_path(og_course *c, const char *dir, int index)
{
    char path[1024];
    FILE *fp;
    char line[256];

    if (!c) return OG_E_ARG;
    snprintf(path, sizeof path, "%s/track%d.path", dir ? dir : ".", index);
    fp = fopen(path, "r");
    if (!fp) return OG_E_ARG;

    c->nnodes = 0;
    while (fgets(line, sizeof line, fp)) {
        float x, y;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%f %f", &x, &y) == 2 && c->nnodes < OG_MAX_NODES) {
            c->node[c->nnodes].x = x;
            c->node[c->nnodes].y = y;
            ++c->nnodes;
        }
    }
    fclose(fp);

    if (c->nnodes < 4) { c->nnodes = 0; return OG_E_SHORT; }
    c->from_file = 1;
    return OG_OK;
}

int og_course_save_path(const og_course *c, const char *dir, int index)
{
    char path[1024];
    FILE *fp;
    int i;

    if (!c || c->nnodes < 2) return OG_E_ARG;
    snprintf(path, sizeof path, "%s/track%d.path", dir ? dir : ".", index);
    fp = fopen(path, "w");
    if (!fp) return OG_E_ARG;

    fprintf(fp, "# Super Off Road SDL3 -- centreline for track %d\n", index);
    fprintf(fp, "# One \"x y\" per line, in 320x200 screen pixels, in\n");
    fprintf(fp, "# racing order.  The loop closes from the last back to\n");
    fprintf(fp, "# the first.  Node 0 is the start/finish line.\n");
    for (i = 0; i < c->nnodes; ++i)
        fprintf(fp, "%.2f %.2f\n", c->node[i].x, c->node[i].y);

    fclose(fp);
    return OG_OK;
}
