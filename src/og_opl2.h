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

/* og_opl2.h -- a compact OPL2 (Yamaha YM3812) synthesis core.
 *
 * Written from the published YM3812 register map: nine two-operator
 * melodic channels, four waveform shapes, per-operator ADSR, feedback
 * on the modulator and FM or additive connection.  It is an original
 * implementation, not a port of an existing emulator, so it carries no
 * third-party licence.
 *
 * It is deliberately a *musical* model rather than a cycle-accurate
 * one.  Envelope rates and key-scaling are approximated with smooth
 * exponentials instead of the chip's discrete rate tables, and the
 * operators run in floating point.  For driving game sound effects that
 * is entirely adequate; if you later want bit-exact output, swap this
 * for Nuked-OPL3 (LGPL) or ymfm (BSD) behind the same three calls.
 *
 * The core is rate-agnostic: phase increments are computed against the
 * sample rate you pass to og_opl2_init(), so it can run directly at the
 * audio device rate with no resampling step.
 *
 * Register map implemented:
 *
 *   0x20+op  AM | VIB | EG-type | KSR | multiplier
 *   0x40+op  key-scale level | total level (attenuation)
 *   0x60+op  attack rate | decay rate
 *   0x80+op  sustain level | release rate
 *   0xA0+ch  F-number, low 8 bits
 *   0xB0+ch  key-on | block (octave) | F-number high 2 bits
 *   0xC0+ch  feedback | connection
 *   0xE0+op  waveform select
 *   0xBD     tremolo/vibrato depth (rhythm mode is not modelled)
 */
#ifndef OG_OPL2_H
#define OG_OPL2_H

#ifdef __cplusplus
extern "C" {
#endif

#define OG_OPL2_CHANNELS  9
#define OG_OPL2_OPERATORS 18

typedef struct {
    /* register-derived settings */
    double   mult;
    int      ksl, tl;
    double   ar, dr, sl, rr;
    int      eg_sustain;      /* EG-type: hold at sustain while keyed  */
    int      wave;
    int      trem, vib;

    /* running state */
    double   phase;
    double   env;             /* 0 = silent, 1 = full                  */
    int      stage;           /* 0 off, 1 attack, 2 decay, 3 sustain,
                                 4 release                             */
    double   out;             /* last sample, for feedback             */
    double   prev;
    int    noise_src;         /* rhythm: this drum is noise, not a tone */
} og_opl2_op;

typedef struct {
    int      fnum, block, keyon;
    int      fb, con;         /* feedback amount, connection type      */
    double   fbbuf[2];
} og_opl2_ch;

typedef struct {
    og_opl2_op  op[OG_OPL2_OPERATORS];
    og_opl2_ch  ch[OG_OPL2_CHANNELS];
    double      rate;
    double      lfo_am, lfo_vib;
    int         am_depth, vib_depth;
    unsigned char reg[256];
    unsigned char rhythm;     /* the low bits of 0xbd, for percussion */
    unsigned long noise;      /* 23-bit LFSR, as the chip's noise source */
    int           wave_select;/* register 0x01 bit 5: waveforms enabled  */
} og_opl2;

void   og_opl2_init (og_opl2 *c, int sample_rate);
void   og_opl2_reset(og_opl2 *c);

/* Accepts the same (register, value) pairs the game writes to 0x388/9. */
void   og_opl2_write(og_opl2 *c, unsigned reg, unsigned val);

/* Render `n` mono samples in [-1,1] (summed, then scaled). */
void   og_opl2_render(og_opl2 *c, float *out, int n);

#ifdef __cplusplus
}
#endif
#endif /* OG_OPL2_H */
