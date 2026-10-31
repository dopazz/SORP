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

/* og_palette.c -- see og_palette.h */

#include "og_palette.h"

/* Racing palette, derived by matching a decoded TRACK3 (Sidewinder)
 * against a frame of real gameplay capture: for each of the 64000
 * pixels, the decoded attribute index was tallied against the colour
 * actually on screen.  Most indices come out above 97% pure, so this is
 * measured rather than inferred.  (The impure ones are the low-count
 * indices, where trucks drawn over the background pollute the tally.)
 *
 * Every channel value lands on 0, 48, 81, 113, 146, 178, 211, 243 --
 * i.e. the 6-bit DAC levels 0, 12, 20, 28, 36, 44, 52, 60 scaled by
 * 255/63.  An earlier version of this table was snapped to a ramp that
 * was 3-5 counts low on every step, which made the whole game render
 * one shade too dark.  If you re-derive these, check against that DAC
 * ramp first; it is the giveaway.
 *
 * Index 3 is the one value not witnessed in TRACK3's artwork; it is the
 * only unaccounted colour in the frame and fits the ramp. */
const unsigned char og_pal_race[16 * 3] = {
    178, 113, 113,   /*  0  dusty rose   -- raised plateau surface     */
    113,  48,   0,   /*  1  brown        -- dirt, shaded               */
     48,  48, 178,   /*  2  blue         -- wall face, lit             */
     48,  48, 113,   /*  3  navy         -- wall face, shaded          */
    178, 178, 211,   /*  4  pale blue    -- retaining wall, lit        */
    146, 146, 178,   /*  5  blue grey    -- retaining wall, mid        */
    243, 211,   0,   /*  6  yellow       -- HUD, signage               */
    146, 146,   0,   /*  7  olive        -- HUD, shaded                */
    146,  81,   0,   /*  8  tan brown    -- the dirt trucks race on    */
     81,  48,   0,   /*  9  dark brown   -- dirt shadow / ruts         */
    243,   0,   0,   /* 10  red          -- barrier stripe             */
    146,   0,   0,   /* 11  dark red     -- barrier stripe, shaded     */
    243, 243, 243,   /* 12  white        -- barrier stripe, white      */
    113, 113, 178,   /* 13  slate blue   -- wall edge                  */
      0,   0,   0,   /* 14  black                                      */
      0,   0,   0    /* 15  black                                      */
};


/* Speed Shop / player-select screen, read from the CSPAL row of a
 * register display captured on that screen.  Same method as the racing
 * palette: sixteen cells located by their separator columns, median
 * sampled, snapped to the emulator ramp. */
const unsigned char og_pal_shop[16 * 3] = {
      0,  81,  81,   /*  0  dark teal    -- the screen's surround       */
      0,   0,   0,   /*  1  black                                       */
     48,  48,  48,   /*  2  near black   -- panel outline               */
     81, 113, 146,   /*  3  steel blue   -- panel face                  */
    178, 178, 178,   /*  4  light grey                                  */
    243, 243, 243,   /*  5  white                                       */
    146, 113,   0,   /*  6  bronze                                      */
    211, 146, 113,   /*  7  tan                                         */
    146,   0,   0,   /*  8  dark red     -- player 1 panel, shaded      */
    211,  48,   0,   /*  9  red          -- player 1 panel              */
      0, 178,   0,   /* 10  green        -- the level bars              */
    113,  81,   0,   /* 11  brown                                       */
    211, 211,   0,   /* 12  yellow       -- player 2 panel              */
    146, 146,   0,   /* 13  olive        -- player 2 panel, shaded      */
      0, 113, 243,   /* 14  blue         -- player 3 panel              */
     81,  81,  81    /* 15  grey                                        */
};

/* Title/copyright screen palette, from the register dump the original
 * offroad_gfx transcription was based on.  Kept because TITLES.XX,
 * LOADA.XX and LOADB.XX are drawn under it, not under the race one. */
const unsigned char og_pal_title[16 * 3] = {
    /* Derived by decoding TITLES.XX and reading off what each index
     * shows as in a capture of the title screen -- every entry with
     * enough pixels agreed 96..100%.  The palette does NOT change
     * between the four views. */
    178, 211, 243,   /*  0 */
    243, 243, 243,   /*  1 white                                        */
    243, 211, 113,   /*  2 */
    146, 211, 243,   /*  3 */
    243, 178,   0,   /*  4 */
    243, 211,  79,   /*  5 (few pixels; interpolated)                   */
    211, 113,   0,   /*  6 */
    211,   0,   0,   /*  7 red                                          */
      0, 113,   0,   /*  8 green                                        */
    178,   0,   0,   /*  9 */
    113,  48,   0,   /* 10 */
     81,   0,   0,   /* 11 */
      0, 146, 243,   /* 12 */
      0,   0,   0,   /* 13 black                                        */
    243, 243, 243,   /* 14 white                                        */
      0,  81, 113    /* 15 the teal background                          */
};

/* Which indices stop a truck.
 *
 * The browns (1, 8, 9) are the racing surface and account for most of
 * every track.  Index 0 is the lighter, mottled top of the raised
 * plateaus, which is driven on, so it counts as dirt too.
 *
 * The barriers are the red/white stripe pair (10, 11, 12) and the
 * grey-blue retaining walls (2, 3, 4, 5, 13).  Classifying those as
 * solid gives a consistent 17-27% of the screen across all eight
 * tracks, and the resulting map traces the lane layout cleanly.
 *
 * The golds (6, 7) are the HUD panel, the track-name banner and the
 * chequered start marker.  They sit on top of the playfield rather than
 * in it, so physics ignores them; the HUD area is masked out
 * separately.  Black (14, 15) is outside the playfield.
 */
const unsigned char og_race_class[16] = {
    OG_CLS_DIRT,    /*  0 */
    OG_CLS_DIRT,    /*  1 */
    OG_CLS_SOLID,   /*  2 */
    OG_CLS_SOLID,   /*  3 */
    OG_CLS_SOLID,   /*  4 */
    OG_CLS_SOLID,   /*  5 */
    OG_CLS_TRIM,    /*  6 */
    OG_CLS_TRIM,    /*  7 */
    OG_CLS_DIRT,    /*  8 */
    OG_CLS_DIRT,    /*  9 */
    OG_CLS_SOLID,   /* 10 */
    OG_CLS_SOLID,   /* 11 */
    OG_CLS_SOLID,   /* 12 */
    OG_CLS_SOLID,   /* 13 */
    OG_CLS_TRIM,    /* 14 */
    OG_CLS_TRIM     /* 15 */
};

/* The end-of-race results screen (EEND.PCG).  Read from a captured
 * screen's palette registers, which list the entries in reverse: the
 * plaque sprites confirm indices 2, 4, 5, 13 and 15 exactly, and the
 * checkered backdrop pieces then match the capture to the pixel. */
const unsigned char og_pal_end[16 * 3] = {
    /* Read off the palette debug row in a capture of this screen -- the
     * row is drawn RIGHT TO LEFT, which is what made 14 and 15 look
     * black. */
      0,   0,   0,   /*  0 black */
      0,   0,   0,   /*  1 black */
    243, 243, 243,   /*  2 white */
      0, 211,   0,   /*  3 green */
    113, 113, 146,   /*  4 blue-grey */
    211, 211, 211,   /*  5 light grey */
    113,  81,   0,   /*  6 brown */
    211, 113,   0,   /*  7 orange */
      0,  48, 146,   /*  8 dark blue */
     81, 146, 243,   /*  9 light blue */
    146,   0,   0,   /* 10 dark red */
    243,  81,   0,   /* 11 orange-red */
    211, 178,   0,   /* 12 gold */
    243, 211,   0,   /* 13 yellow */
    211, 146, 113,   /* 14 skin */
    146, 113,   0,   /* 15 dark brown */
};
