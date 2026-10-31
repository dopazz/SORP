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

/* og_opl2.c -- see og_opl2.h */

#include "og_opl2.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0 * M_PI)

/* Frequency multiplier per the MULT field. Entries 11 and 13 repeat the
 * value below them, and 14/15 both give 15 -- that is how the chip is
 * specified, not a typo. */
static const double og_mult_tab[16] = {
    0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15
};

/* Key-scale level attenuation in dB per octave, indexed by the KSL
 * field. 0 disables it. */
static const double og_ksl_db[4] = { 0.0, 3.0, 1.5, 6.0 };

/* Register operator offsets 0x00..0x15 skip 0x06/0x07 and 0x0e/0x0f.
 * Fold them down to a dense 0..17. Returns -1 for the gaps. */
static int og_op_index(unsigned off)
{
    unsigned low = off & 7u;
    unsigned grp = off >> 3;
    if (low > 5u || grp > 2u) return -1;
    return (int)(grp * 6u + low);
}

/* The two operators belonging to each channel, as dense indices. */
static const int og_ch_op1[9] = { 0, 1, 2,  6,  7,  8, 12, 13, 14 };
static const int og_ch_op2[9] = { 3, 4, 5,  9, 10, 11, 15, 16, 17 };

static double og_db_to_lin(double db) { return pow(10.0, -db / 20.0); }

/* Convert a 4-bit rate to a per-sample approach coefficient. Rate 0 is
 * treated as "never move". */
static double og_rate_coef(double rate4, double sample_rate)
{
    double tau;
    if (rate4 <= 0.0) return 0.0;
    /* ~2 ms at rate 15 up to ~0.36 s at rate 1 */
    tau = 0.002 * pow(2.0, (15.0 - rate4) * 0.5);
    return 1.0 - exp(-1.0 / (tau * sample_rate));
}

/* The chip's noise source: a 23-bit shift register tapped at bits 22 and
 * 8, stepped once a sample.  The snare, hi-hat and cymbal are built from
 * it -- without it they are pitched ticks rather than hisses. */
static void og_noise_step(og_opl2 *c)
{
    unsigned long n = c->noise;
    if (!n) n = 1;
    n = (n >> 1) | ((((n >> 0) ^ (n >> 14)) & 1UL) << 22);
    c->noise = n;
}

void og_opl2_init(og_opl2 *c, int sample_rate)
{
    memset(c, 0, sizeof *c);
    c->rate = (sample_rate > 0) ? (double)sample_rate : 44100.0;
    og_opl2_reset(c);
}

void og_opl2_reset(og_opl2 *c)
{
    int i;
    double rate = c->rate;

    memset(c->op, 0, sizeof c->op);
    memset(c->ch, 0, sizeof c->ch);
    memset(c->reg, 0, sizeof c->reg);
    c->rate = rate;
    c->lfo_am = c->lfo_vib = 0.0;
    c->am_depth = c->vib_depth = 0;

    for (i = 0; i < OG_OPL2_OPERATORS; ++i) {
        c->op[i].mult = 1.0;
        c->op[i].tl   = 63;         /* fully attenuated */
        c->op[i].ar = c->op[i].dr = c->op[i].rr = 0.0;
        c->op[i].sl = 0.0;
        c->op[i].stage = 0;
    }
}

void og_opl2_write(og_opl2 *c, unsigned reg, unsigned val)
{
    unsigned group, off;
    int i;

    reg &= 0xffu;
    val &= 0xffu;
    c->reg[reg] = (unsigned char)val;

    group = reg & 0xe0u;
    off   = reg & 0x1fu;

    if (reg == 0x01) {              /* test / waveform select enable */
        c->wave_select = (val & 0x20u) ? 1 : 0;
        if (!c->wave_select) {
            int q;
            for (q = 0; q < OG_OPL2_OPERATORS; ++q) c->op[q].wave = 0;
        }
        return;
    }

    if (reg == 0xbd) {
        /* Rhythm mode.  The five low bits strike bass drum, snare, tom,
         * cymbal and hi-hat, each on its own fixed operator: the bass
         * drum is a normal pair on channel 6, the others single
         * operators on channels 7 and 8.  A bit going high starts that
         * operator's envelope, a bit going low releases it.  (A real
         * OPL2 runs snare, cymbal and hi-hat off a noise source; this
         * plays them tonally, which is closer than silence.) */
        /* bit 0 hi-hat (op 0x11), 1 cymbal (0x15), 2 tom (0x12),
         * 3 snare (0x14), 4 bass drum (channel 6's pair) */
        static const int perc_op[4] = { 0x11, 0x15, 0x12, 0x14 };
        unsigned was = c->rhythm;
        int k;

        c->am_depth  = (val & 0x80u) ? 1 : 0;
        c->vib_depth = (val & 0x40u) ? 1 : 0;
        c->rhythm    = (unsigned char)(val & 0x3fu);
        if (!(val & 0x20u)) return;          /* rhythm mode off */

        /* which channel each drum sounds on, so the renderer -- which
         * works per channel -- actually processes it */
        static const int perc_ch[4] = { 7, 8, 8, 7 };
        /* hi-hat, cymbal and snare come from the noise source; the tom
         * is a plain tone */
        {
            int nz[3] = { 0x11, 0x15, 0x14 }, q;
            for (q = 0; q < 3; ++q) {
                int ix = og_op_index((unsigned)nz[q]);
                if (ix >= 0) c->op[ix].noise_src = (val & 0x20u) ? 1 : 0;
            }
        }
        for (k = 0; k < 4; ++k) {
            unsigned bit = 1u << k;
            int i2 = og_op_index((unsigned)perc_op[k]);
            if (i2 < 0) continue;
            if ((val & bit) && !(was & bit)) {
                c->op[i2].stage = 1; c->op[i2].phase = 0.0; c->op[i2].env = 0.0;
                c->ch[perc_ch[k]].keyon = 1;
                /* In rhythm mode a percussion operator IS a carrier --
                 * the hi-hat's 0x11 is channel 7's modulator, and a
                 * modulator on its own makes no sound in FM -- so the
                 * channel is put into additive mode while it plays. */
                c->ch[perc_ch[k]].con = 1;
                c->ch[perc_ch[k]].fbbuf[0] = c->ch[perc_ch[k]].fbbuf[1] = 0.0;
            } else if (!(val & bit) && (was & bit)) {
                c->op[i2].stage = 4;
                if (!(c->rhythm & ((perc_ch[k] == 7) ? 0x09u : 0x06u)))
                    c->ch[perc_ch[k]].keyon = 0;
            }
        }
        if ((val & 0x10u) && !(was & 0x10u)) {      /* bass drum */
            int a = og_ch_op1[6], b = og_ch_op2[6];
            c->op[a].stage = 1; c->op[a].phase = 0.0; c->op[a].env = 0.0;
            c->op[b].stage = 1; c->op[b].phase = 0.0; c->op[b].env = 0.0;
            c->ch[6].keyon = 1;    /* the bass drum keeps its own pairing */
        } else if (!(val & 0x10u) && (was & 0x10u)) {
            c->ch[6].keyon = 0;
            c->op[og_ch_op1[6]].stage = 4;
            c->op[og_ch_op2[6]].stage = 4;
        }
        return;
    }

    switch (group) {
    case 0x20:
        if ((i = og_op_index(off)) >= 0) {
            c->op[i].mult = og_mult_tab[val & 0x0fu];
            c->op[i].eg_sustain = (val & 0x20u) ? 1 : 0;
            c->op[i].vib  = (val & 0x40u) ? 1 : 0;
            c->op[i].trem = (val & 0x80u) ? 1 : 0;
        }
        return;

    case 0x40:
        if ((i = og_op_index(off)) >= 0) {
            c->op[i].tl  = (int)(val & 0x3fu);
            c->op[i].ksl = (int)(val >> 6);
        }
        return;

    case 0x60:
        if ((i = og_op_index(off)) >= 0) {
            c->op[i].ar = (double)(val >> 4);
            c->op[i].dr = (double)(val & 0x0fu);
        }
        return;

    case 0x80:
        if ((i = og_op_index(off)) >= 0) {
            c->op[i].sl = (double)(val >> 4);
            c->op[i].rr = (double)(val & 0x0fu);
        }
        return;

    case 0xe0:
        /* An OPL2 ignores the waveform bits until register 0x01 bit 5
         * turns waveform select on -- everything is a plain sine before
         * that, which is how the chip powers up. */
        if (!c->wave_select) return;
        if ((i = og_op_index(off)) >= 0)
            c->op[i].wave = (int)(val & 0x03u);
        return;

    default:
        break;
    }

    if (reg >= 0xa0 && reg <= 0xa8) {
        int ch = (int)(reg - 0xa0);
        c->ch[ch].fnum = (c->ch[ch].fnum & 0x300) | (int)val;
        return;
    }

    if (reg >= 0xb0 && reg <= 0xb8) {
        int ch = (int)(reg - 0xb0);
        int was = c->ch[ch].keyon;
        c->ch[ch].fnum  = (c->ch[ch].fnum & 0xff) | (int)((val & 0x03u) << 8);
        c->ch[ch].block = (int)((val >> 2) & 0x07u);
        c->ch[ch].keyon = (val & 0x20u) ? 1 : 0;

        if (!was && c->ch[ch].keyon) {
            int a = og_ch_op1[ch], b = og_ch_op2[ch];
            c->op[a].stage = 1; c->op[a].phase = 0.0; c->op[a].env = 0.0;
            c->op[b].stage = 1; c->op[b].phase = 0.0; c->op[b].env = 0.0;
            c->ch[ch].fbbuf[0] = c->ch[ch].fbbuf[1] = 0.0;
        } else if (was && !c->ch[ch].keyon) {
            c->op[og_ch_op1[ch]].stage = 4;
            c->op[og_ch_op2[ch]].stage = 4;
        }
        return;
    }

    if (reg >= 0xc0 && reg <= 0xc8) {
        int ch = (int)(reg - 0xc0);
        c->ch[ch].fb  = (int)((val >> 1) & 0x07u);
        c->ch[ch].con = (int)(val & 0x01u);
        return;
    }
}

/* One operator step. `phase_off` is the incoming modulation in radians. */
static double og_op_step(og_opl2 *c, og_opl2_op *o, double inc,
                         double phase_off, double am)
{
    double s, amp, att;

    /* envelope */
    switch (o->stage) {
    case 1: {
        double k = og_rate_coef(o->ar, c->rate);
        if (k <= 0.0) { o->env = 1.0; o->stage = 2; break; }
        o->env += (1.02 - o->env) * k;
        if (o->env >= 1.0) { o->env = 1.0; o->stage = 2; }
        break;
    }
    case 2: {
        double target = og_db_to_lin(o->sl * 3.0);
        double k = og_rate_coef(o->dr, c->rate);
        if (o->sl >= 15.0) target = 0.0;
        o->env += (target - o->env) * k;
        if (fabs(o->env - target) < 1e-4) {
            o->env = target;
            o->stage = o->eg_sustain ? 3 : 4;
        }
        break;
    }
    case 3:
        break;                          /* hold */
    case 4: {
        double k = og_rate_coef(o->rr, c->rate);
        o->env -= o->env * k;
        if (o->env < 1e-5) { o->env = 0.0; o->stage = 0; }
        break;
    }
    default:
        o->env = 0.0;
        break;
    }

    if (o->stage == 0) { o->out = 0.0; return 0.0; }

    o->phase += inc;
    if (o->phase > TWO_PI * 64.0) o->phase -= TWO_PI * 64.0;

    if (o->noise_src) {
        /* snare, hi-hat and cymbal: the chip runs these off the noise
         * source rather than the sine table */
        s = (c->noise & 1UL) ? 1.0 : -1.0;
        amp = og_db_to_lin(o->tl * 0.75 + (o->trem && am > 0.0 ? am * 4.8 : 0.0));
        o->out = s * amp * o->env;
        return o->out;
    }

    {
        double p = o->phase + phase_off;
        double v = sin(p);
        switch (o->wave) {
        case 1: s = (v > 0.0) ? v : 0.0; break;
        case 2: s = fabs(v);             break;
        case 3: {
            double q = fmod(p, M_PI);
            if (q < 0.0) q += M_PI;
            s = (q < M_PI * 0.5) ? fabs(v) : 0.0;
            break;
        }
        default: s = v; break;
        }
    }

    att = (double)o->tl * 0.75;          /* 0.75 dB per TL step */
    amp = og_db_to_lin(att);
    if (o->trem && c->am_depth) amp *= (1.0 - 0.12 * am);

    o->out = s * amp * o->env;
    return o->out;
}

void og_opl2_render(og_opl2 *c, float *out, int n)
{
    int i, ch;

    for (i = 0; i < n; ++i) {
        double acc = 0.0, am, vib;

        /* Slow LFOs: ~3.7 Hz tremolo, ~6.1 Hz vibrato, as specified. */
        og_noise_step(c);
        c->lfo_am  += 3.7 * TWO_PI / c->rate;
        c->lfo_vib += 6.1 * TWO_PI / c->rate;
        if (c->lfo_am  > TWO_PI) c->lfo_am  -= TWO_PI;
        if (c->lfo_vib > TWO_PI) c->lfo_vib -= TWO_PI;
        am  = 0.5 * (1.0 + sin(c->lfo_am));
        vib = sin(c->lfo_vib);

        for (ch = 0; ch < OG_OPL2_CHANNELS; ++ch) {
            og_opl2_ch *k = &c->ch[ch];
            og_opl2_op *m = &c->op[og_ch_op1[ch]];
            og_opl2_op *r = &c->op[og_ch_op2[ch]];
            double base, incm, incr, fb, mo, ro, ksl;

            if (m->stage == 0 && r->stage == 0) continue;

            /* f = fnum * 49716 / 2^(20-block); we want radians/sample. */
            base = (double)k->fnum * 49716.0 /
                   (double)(1UL << (20 - k->block));
            if (m->vib && c->vib_depth) base *= (1.0 + 0.007 * vib);

            incm = TWO_PI * base * m->mult / c->rate;
            incr = TWO_PI * base * r->mult / c->rate;

            /* Key-scale level: attenuate with octave. */
            ksl = og_ksl_db[r->ksl] * (double)k->block * 0.5;

            fb = 0.0;
            if (k->fb)
                fb = (k->fbbuf[0] + k->fbbuf[1]) * 0.5 *
                     (double)(1 << k->fb) * 0.05;

            mo = og_op_step(c, m, incm, fb, am);
            k->fbbuf[1] = k->fbbuf[0];
            k->fbbuf[0] = mo;

            if (k->con) {
                /* additive: both operators reach the output */
                ro = og_op_step(c, r, incr, 0.0, am);
                acc += (mo + ro) * og_db_to_lin(ksl);
            } else {
                /* FM: modulator bends the carrier's phase */
                ro = og_op_step(c, r, incr, mo * 2.6, am);
                acc += ro * og_db_to_lin(ksl);
            }
        }

        acc *= 0.22;
        if (acc >  1.0) acc =  1.0;
        if (acc < -1.0) acc = -1.0;
        out[i] = (float)acc;
    }
}
