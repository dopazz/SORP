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

/* og_palette.h -- the palettes Super Off Road actually programs, and
 * what each attribute index means to the simulation.
 *
 * The game reprograms the Attribute Controller per screen, so there is
 * no single palette.  Two are known:
 *
 *   og_pal_title[]  the copyright/title screen (TITLES.XX).
 *   og_pal_race[]   the racing screens (TRACKn.XX).
 *   og_pal_shop[]   the Speed Shop / player-select screen (ESHOP.PCG).
 *   og_pal_end[]    the end-of-race results screen (EEND.PCG).
 *
 * The racing palette was read off a DOSBox-X register display captured
 * during a race: the sixteen CSPAL swatches were sampled and snapped to
 * the eight-level channel ramp the emulator renders
 * (0, 48, 79, 111, 143, 175, 206, 238).  Rendering TRACK0..7 through it
 * reproduces the screenshot exactly -- brown dirt, red-and-white
 * barriers, grey-blue retaining walls.
 *
 * Getting this right matters for more than looks.  Under the title
 * palette the dominant index reads as green, which invites the
 * conclusion that it is infield grass and that the raceable channel is
 * everything else.  It is the opposite: index 8 is the dirt the trucks
 * drive on, and it covers roughly three quarters of every track.  The
 * barriers are the collision geometry.
 */
#ifndef OG_PALETTE_H
#define OG_PALETTE_H

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned char og_pal_race [16 * 3];
extern const unsigned char og_pal_shop [16 * 3];
extern const unsigned char og_pal_title[16 * 3];
extern const unsigned char og_pal_end  [16 * 3];

/* Surface classes, in the order og_course.h uses them. */
enum {
    OG_CLS_DIRT  = 0,   /* raceable                                   */
    OG_CLS_SOLID = 1,   /* barrier / retaining wall: bounce           */
    OG_CLS_TRIM  = 2    /* HUD, signage, logos: ignored by physics    */
};

/* Class of each of the sixteen attribute indices on a racing screen.
 * Indexed by attribute value; see og_palette.c for the reasoning. */
extern const unsigned char og_race_class[16];

#ifdef __cplusplus
}
#endif
#endif /* OG_PALETTE_H */
