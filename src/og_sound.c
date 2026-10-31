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

/* og_sound.c -- see og_sound.h */

#include "og_sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OPL2 operator slot offsets for the nine melodic channels. */
static const unsigned char og_op1[9] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12
};
static const unsigned char og_op2[9] = {
    0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15
};

static og_opl_write_fn og_sink;
static void           *og_sink_user;

static unsigned short og_le16(const unsigned char *p)
{
    return (unsigned short)(p[0] | ((unsigned)p[1] << 8));
}

/* Per-channel mute and level, for the on-screen meters.  A muted
 * channel's key-on is dropped and its rhythm bits are masked out, so the
 * sequencer carries on untouched and only the sound stops. */
unsigned char og_ch_muted[9];
float         og_ch_level[9];
/* WHICH VOICES of an instrument to sound.  The two blocks are nearly
 * identical in every record -- the same operators, differing only in
 * their pitch fields -- so sounding both doubles the note and the pair
 * beat against each other.  Which the original uses is not settled:
 *   0 both, 1 the first only, 2 the second only.
 * The F2 overlay cycles it for comparison. */
int           og_second_voice = 1;
int           og_voice_mode   = 0;
extern const unsigned char og_rhythm_channel[5];

static og_sound *og_emit_owner;   /* so og_emit can note the key state */

static void og_emit(unsigned reg, unsigned val)
{
    unsigned r = reg & 0xff, v = val & 0xff;

    if (r >= 0xb0 && r <= 0xb8 && og_emit_owner) {
        og_emit_owner->ch_keyed[r - 0xb0] = (v & 0x20) ? 1 : 0;
        og_emit_owner->ch_breg [r - 0xb0] = v;
    }

    if (r >= 0xb0 && r <= 0xb8) {            /* key-on for a channel */
        int ch = (int)(r - 0xb0);
        if (v & 0x20) {
            if (og_ch_muted[ch]) v &= (unsigned)~0x20;   /* drop the key */
            else og_ch_level[ch] = 1.0f;                 /* it just struck */
        }
    } else if (r == 0xbd) {                  /* the rhythm bits */
        /* bit 0 hi-hat, 1 cymbal, 2 tom, 3 snare, 4 bass -- and their
         * channels come from the game's own table at DS:0xb29f,
         * { 7, 8, 8, 7, 6 }.  Using any other order lights the wrong
         * meter and mutes the wrong voice. */
        int b;
        for (b = 0; b < 5; ++b)
            if (v & (1u << b)) {
                int rc = og_rhythm_channel[b];
                if (og_ch_muted[rc]) v &= ~(1u << b);
                else og_ch_level[rc] = 1.0f;
            }
    }
    if (og_sink) og_sink(og_sink_user, r, v);
}

/* called once a frame: the meters fall back towards zero */
void og_sound_meters_decay(void)
{
    int i;
    for (i = 0; i < 9; ++i) {
        og_ch_level[i] *= 0.82f;
        if (og_ch_level[i] < 0.004f) og_ch_level[i] = 0.0f;
    }
}

void og_sound_set_sink(og_sound *s, og_opl_write_fn fn, void *user)
{
    og_emit_owner = s;
    og_sink = fn;
    og_sink_user = user;
    /* WSE first, before anything else reaches the chip: an OPL2 ignores
     * every 0xE0 waveform write until register 0x01 bit 5 is set, and the
     * original's very first write is 01 <- 20. */
    if (og_sink) og_emit(0x01, 0x20);
}

/* ------------------------------------------------------------------ */
/* Loading                                                             */
/* ------------------------------------------------------------------ */

static int og_load_patches(og_sound *s, const char *dir)
{
    unsigned char *buf = NULL;
    size_t len = 0;
    char path[1024];
    int i, rc;

    rc = og_resolve(dir, "ADLIB.SFX", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &buf, &len);
    if (rc != OG_OK) return rc;

    if (len < (size_t)OG_NUM_PATCHES * OG_PATCH_BYTES) {
        free(buf);
        return OG_E_SHORT;
    }

    for (i = 0; i < OG_NUM_PATCHES; ++i) {
        int k, any = 0;
        memcpy(s->patch[i].raw, buf + (size_t)i * OG_PATCH_BYTES,
               OG_PATCH_BYTES);
        for (k = 0; k < OG_PATCH_BYTES; ++k)
            if (s->patch[i].raw[k]) { any = 1; break; }
        s->patch[i].used = any;
    }
    og_load_note("ADLIB.SFX", 0, (long)OG_NUM_PATCHES * OG_PATCH_BYTES,
                 "Loaded %d AdLib instrument patches, %d bytes each",
                 OG_NUM_PATCHES, OG_PATCH_BYTES);

    free(buf);
    return OG_OK;
}

/* Pull the effect table out of the executable's data segment.  The
 * pointers in it are DS offsets, so image offset = 0x5700 + pointer. */
static int og_load_effects(og_sound *s, const char *dir)
{
    char path[1024];
    unsigned char *raw = NULL;
    size_t len = 0, hdr;
    int i, rc;

    rc = og_resolve(dir, "GAME.EXE", path, sizeof path);
    if (rc != OG_OK) return rc;
    rc = og_read_file(path, &raw, &len);
    if (rc != OG_OK) return rc;

    if (len < 0x20 || raw[0] != 'M' || raw[1] != 'Z') {
        free(raw);
        return OG_E_MAGIC;
    }

    hdr = (size_t)og_le16(raw + 8) * 16;        /* e_cparhdr */
    if (hdr >= len) { free(raw); return OG_E_SHORT; }

    s->exe     = raw;
    s->exe_len = len;

    {
        const unsigned char *img = raw + hdr;
        size_t imglen = len - hdr;
        size_t tbl = OG_EXE_SFX_TABLE;
        size_t first = 0;

        if (tbl + OG_NUM_EFFECTS * 4 > imglen) return OG_E_SHORT;

        for (i = 0; i < OG_NUM_EFFECTS; ++i) {
            unsigned ptr = og_le16(img + tbl + (size_t)i * 4);
            unsigned att = og_le16(img + tbl + (size_t)i * 4 + 2);
            size_t   off = (size_t)OG_EXE_DATA_SEG + ptr;

            if (off >= imglen) break;
            if (!first) first = off;

            s->effect[i].bytes = img + off;
            s->effect[i].attr  = att;
            s->effect[i].len   = 0;      /* filled below */
        }
        s->neffects = i;

        /* Chromatic note table. */
        if ((size_t)OG_EXE_NOTE_TABLE + OG_NUM_NOTES * 4 <= imglen) {
            int k;
            for (k = 0; k < OG_NUM_NOTES; ++k) {
                const unsigned char *e =
                    img + OG_EXE_NOTE_TABLE + (size_t)k * 4;
                s->note_fnum [k] = og_le16(e);
                s->note_block[k] = e[2];
            }
        }

        /* Each sequence runs to the next one; the last runs to its
         * 0xff terminator. */
        for (i = 0; i < s->neffects; ++i) {
            if (i + 1 < s->neffects) {
                s->effect[i].len =
                    (int)(s->effect[i + 1].bytes - s->effect[i].bytes);
            } else {
                const unsigned char *p = s->effect[i].bytes;
                const unsigned char *lim = img + imglen;
                while (p < lim && *p != 0xff) ++p;
                s->effect[i].len = (int)(p - s->effect[i].bytes) + 1;
            }
            if (s->effect[i].len < 0) s->effect[i].len = 0;
        }
        og_load_note("GAME.EXE", (long)(hdr + tbl), (long)s->neffects * 4,
                     "Loaded the sound effect table for AdLib: %d script pointers and attributes",
                     s->neffects);
        if (s->neffects > 0)
            og_load_note("GAME.EXE", (long)(s->effect[0].bytes - raw),
                         (long)(s->effect[s->neffects - 1].bytes - s->effect[0].bytes) +
                         s->effect[s->neffects - 1].len,
                         "Loaded the AdLib sound effect scripts");
        if ((size_t)OG_EXE_NOTE_TABLE + OG_NUM_NOTES * 4 <= imglen)
            og_load_note("GAME.EXE", (long)(hdr + OG_EXE_NOTE_TABLE), OG_NUM_NOTES * 4,
                         "Loaded the AdLib note table: %d F-numbers and octaves",
                         OG_NUM_NOTES);
        og_load_note("GAME.EXE", (long)(hdr + OG_GAME_DS_IMG + OG_MUSIC_TABLE_DS),
                     OG_MUSIC_SONGS * 12,
                     "Loaded the music table: %d songs, a tempo and 3 phrase lists each",
                     OG_MUSIC_SONGS);
        og_load_note("GAME.EXE", (long)(hdr + OG_GAME_DS_IMG + OG_SFX_ENABLE_DS),
                     OG_NUM_EFFECTS,
                     "Loaded the AdLib sound effect switches, one per effect");
    }

    return OG_OK;
}

int og_sound_load(og_sound *s, const char *dir)
{
    int rc;

    if (!s) return OG_E_ARG;
    memset(s, 0, sizeof *s);

    rc = og_load_patches(s, dir);
    if (rc != OG_OK) return rc;
    memcpy(s->rel_op, og_op1, sizeof s->rel_op);    /* DS:0xb296 */

    rc = og_load_effects(s, dir);
    if (rc != OG_OK) { og_sound_free(s); return rc; }

    s->loaded = 1;
    return OG_OK;
}

void og_sound_free(og_sound *s)
{
    if (!s) return;
    free(s->exe);
    s->exe = NULL;
    s->loaded = 0;
}

/* ------------------------------------------------------------------ */
/* Chip control                                                        */
/* ------------------------------------------------------------------ */

/* Mirrors the sweep at GAME.EXE image 0x3c47. */
void og_sound_reset(og_sound *s)
{
    unsigned r;
    int i;

    /* The original's t=0 initialisation, in its order.  WSE first (01 <-
     * 20): an OPL2 ignores every 0xE0 write until bit 5 of register 1 is
     * set.  Then EVERY operator is given a slow envelope and, crucially,
     * 0x90..0x94 -- the rhythm operators -- are released with 0xff.
     * Rhythm mode is on permanently, so operators left at their power-on
     * values sound continuously underneath the music. */
    og_emit(0x01, 0x20);
    for (r = 0x20; r <= 0x35; ++r) og_emit(r, 0x20);   /* multipliers */
    for (r = 0x60; r <= 0x75; ++r) og_emit(r, 0x11);   /* attack/decay */
    for (r = 0x80; r <= 0x95; ++r) og_emit(r, 0xff);   /* all released */
    for (r = 0x40; r <= 0x55; ++r) og_emit(r, 0x3f);   /* and attenuated */
    for (r = 0xe0; r <= 0xf5; ++r) og_emit(r, 0x00);
    for (r = 0xc0; r <= 0xc8; ++r) og_emit(r, 0x00);

    for (r = 0x80; r <= 0x94; ++r) og_emit(r, 0xff);
    for (r = 0xb0; r <= 0xb8; ++r) og_emit(r, 0x00);
    /* rhythm mode stays on (the shadow at [0xb295] starts at 0x20 and is
     * never cleared), but every drum bit goes */
    if (s) s->rhythm = 0x20;
    og_emit(0xbd, 0x20);
    /* and the per-channel bookkeeping, or a muted key-on stays "playing"
     * for ever and the meters never fall */
    if (s) {
        for (i = 0; i < 9; ++i) {
            s->ch_ticks [i] = 0;
            s->ch_tail  [i] = 0;
            s->ch_rhythm[i] = 0;
            s->ch_prio  [i] = 0;
        }
        for (i = 0; i < 5; ++i) s->rhythm_ticks[i] = 0;
    }
    for (i = 0; i < 9; ++i) og_ch_level[i] = 0.0f;
}

/* Push a patch record to a channel.
 *
 * The exact field layout of a 72-byte record is not fully established,
 * so this maps the two halves (+0x00 and +0x1a) onto the standard OPL2
 * operator registers in their conventional order.  It produces sane,
 * audible voices; treat the mapping as the first thing to adjust if a
 * given effect sounds wrong. */
/* The feedback byte of each voice block; its two operator patches follow
 * immediately, four bytes each (registers 0x20, 0x40, 0x60, 0x80). */
const int og_voice_block_off[OG_VOICE_BLOCKS] = { 15, 38, 61 };

/* see og_sound.h: which patch drives the carrier */
int og_sound_swap_ops = 0;

int og_sound_program_block(og_sound *s, int channel, int patch_index, int block)
{
    const unsigned char *m;
    unsigned o1, o2;
    int b;

    if (!s || channel < 0 || channel > 8) return OG_E_ARG;
    if (patch_index < 0 || patch_index >= OG_NUM_PATCHES) return OG_E_RANGE;
    if (block < 0 || block >= OG_VOICE_BLOCKS) return OG_E_RANGE;

    b  = og_voice_block_off[block];
    m  = s->patch[patch_index].raw;
    s->ch_link[channel] = 0;            /* an effect sets it after this */
    o1 = og_sound_swap_ops ? og_op2[channel] : og_op1[channel];
    o2 = og_sound_swap_ops ? og_op1[channel] : og_op2[channel];

    /* 0x3e88 consumes FIVE bytes an operator: it writes 0x20, 0x40,
     * 0x60 and 0x80 stepping the register by 0x20 each time, then adds
     * 0x60 to reach 0xe0 -- the waveform select -- and finally 0x20 more
     * to bring the base back for the next operator.  Reading four left
     * everything past the first operator shifted by a byte. */
    og_emit(0x20 + o1, m[b + 1]);    /* tremolo/vibrato/EG/mult      */
    og_emit(0x40 + o1, m[b + 2]);    /* KSL / output level           */
    og_emit(0x60 + o1, m[b + 3]);    /* attack / decay               */
    og_emit(0x80 + o1, m[b + 4]);    /* sustain / release            */
    og_emit(0xe0 + o1, m[b + 5]);    /* waveform select              */

    og_emit(0x20 + o2, m[b + 6]);
    og_emit(0x40 + o2, m[b + 7]);
    og_emit(0x60 + o2, m[b + 8]);
    og_emit(0x80 + o2, m[b + 9]);
    og_emit(0xe0 + o2, m[b + 10]);

    og_emit(0xc0 + channel, m[b]);   /* feedback / connection, from +15 */
    return OG_OK;
}

int og_sound_program(og_sound *s, int channel, int patch_index)
{
    return og_sound_program_block(s, channel, patch_index, 0);
}

/* ------------------------------------------------------------------ */
/* The AdLib effects                                                   */
/* ------------------------------------------------------------------ */

int og_sound_effect_enabled(const og_sound *s, int effect)
{
    size_t o;
    if (!s || !s->exe || effect < 0 || effect >= OG_NUM_EFFECTS) return 0;
    /* s->exe keeps the whole file, so step over the MZ header first */
    o = (size_t)og_le16(s->exe + 8) * 16
      + (size_t)OG_GAME_DS_IMG + OG_SFX_ENABLE_DS + (size_t)effect;
    if (o >= s->exe_len) return 0;
    return s->exe[o] != 0;
}

/* Release a channel the way the driver does at 0x3d16: force both
 * operators to the fastest release (0x80 + n = 0xff) and THEN clear
 * key-on.  Clearing key-on alone leaves a patch with a slow release
 * ringing on, which sounds like a stuck note. */
static void og_release(og_sound *s, int channel)
{
    if (channel < 0 || channel > 8) return;
    og_emit(0x80 + og_op1[channel], 0xff);
    og_emit(0x80 + og_op2[channel], 0xff);
    og_emit(0xb0 + channel, 0x00);
    /* Clear the channel's countdown as well.  Music and effects share
     * channels 0..5 and each keeps its own counters: when one takes a
     * channel the other was using, a stale ch_ticks either releases the
     * new note early or -- if it had already run out -- leaves the note
     * keyed with nothing scheduled to release it.  That is the hanging
     * note heard only with effects AND music together. */
    if (s) {
        s->ch_ticks [channel] = 0;
        s->ch_tail  [channel] = 0;
        s->ch_delta [channel] = 0;
        s->ch_phase [channel] = 0;
        s->ch_mode  [channel] = 0;
        if (s->ch_rhythm[channel]) {
            s->rhythm &= (unsigned char)~s->ch_rhythm[channel];
            og_emit(0xbd, s->rhythm);
            s->ch_rhythm[channel] = 0;
        }
    }
}

/* The natural release at 0x4111, when a voice's duration and tail are
 * spent: the operator comes from the LIVE table (rel_op), which a drum
 * start has corrupted -- see og_sound.h -- so this can release another
 * channel's operators.  Everything else is og_release. */
static void og_release_4111(og_sound *s, int channel)
{
    const unsigned op = s->rel_op[channel];
    og_emit(0x80 + op, 0xff);
    og_emit(0x83 + op, 0xff);
    og_emit(0xb0 + channel, 0x00);
    s->ch_ticks[channel] = 0;
    s->ch_tail [channel] = 0;
    s->ch_delta[channel] = 0;
    s->ch_phase[channel] = 0;
    s->ch_mode [channel] = 0;
    if (s->ch_rhythm[channel]) {
        s->rhythm &= (unsigned char)~s->ch_rhythm[channel];
        og_emit(0xbd, s->rhythm);
        s->ch_rhythm[channel] = 0;
    }
}

/* 0x3e3b: a drum start's stray write, [0xb292 + ch] = 0 */
static void og_drum_clobber(og_sound *s, int channel)
{
    if (channel >= 4 && channel <= 8) s->rel_op[channel - 4] = 0;
}

/* DS:0xb29f: which OPL channel each rhythm voice sounds on, by the
 * position of its bit -- hi-hat, cymbal, tom, snare, bass drum. */
const unsigned char og_rhythm_channel[5] = { 7, 8, 8, 7, 6 };

/* Start the timer of every drum bit in mask: og_sound_effect_tick()
 * clears each bit from 0xbd when its own time runs out. */
static void og_rhythm_strike(og_sound *s, unsigned mask, int ticks)
{
    int b;
    for (b = 0; b < 5; ++b)
        if (mask & (1u << b)) s->rhythm_ticks[b] = ticks > 0 ? ticks : 1;
}

/* One voice's countdown, as 0x3cf8 loads it from the record at `o`
 * (+3, +0x1a and +0x31 for the three voices): the block, the F-number,
 * the signed slide, the phase word, the mode byte, the duration and the
 * tail.  The phase STARTS at half the word plus one but RELOADS from the
 * word itself (0x40c9 takes [+0xe], which 0x3cf8 stored unhalved), so a
 * slide that reverses swings evenly about its start.  A voice with no
 * duration goes straight to its tail (0x4104) -- still keyed -- and one
 * with neither is never stepped. */
static void og_voice_load(og_sound *s, int ch, const unsigned char *m, int o,
                          int keyed)
{
    unsigned ph = og_le16(m + o + 5);
    int f = (int)(og_le16(m + o + 1) & 0x3ff);
    s->ch_block[ch]     = m[o] & 7;
    s->ch_fnum [ch]     = f;
    s->ch_base [ch]     = f;
    s->ch_delta[ch]     = (int)(short)og_le16(m + o + 3);
    s->ch_phase[ch]     = (int)(ph >> 1) + 1;
    s->ch_phase_rel[ch] = (int)ph;
    s->ch_mode [ch]     = m[o + 7];
    s->ch_ticks[ch]     = (int)og_le16(m + o + 8);
    s->ch_tail [ch]     = (int)og_le16(m + o + 10);
    if (s->ch_ticks[ch] == 0 && s->ch_tail[ch] > 0) s->ch_ticks[ch] = -1;
    s->ch_keyed[ch]     = keyed;
    /* 0x3cf8 writes no pitch: the first tick's step writes 0xa0 and
     * 0xb0, the slide already applied -- so force both then */
    s->ch_breg [ch]     = 0x100;
    s->ch_link [ch]     = 0;
}

/* An effect is record 0x20 + n, started as 0x3cf8 starts it.  Every
 * record is THREE voices: block 0 on `channel`, block 1 on channel + 3,
 * each with its own countdown; and, when byte +2 -- the rhythm mask --
 * is set, block 2 as a drum on the channel rhythm mode gives it (by
 * DS:0xb29f), struck through 0xbd.  Byte +1 LINKS the second voice to
 * the first (0x4018): each tick it takes the first's pitch -- plus its
 * own slide (1), or an octave up (2 and more) -- before its own step.
 * Byte 0 is the priority 0x4220 compares.
 *
 * Playing only the drum of a drum record lost the other two: the white
 * flag (effect 2) is a hi-hat over two voices sweeping down, and nitro
 * (10) a hi-hat over a short sweep down and one up -- a DOSBox capture
 * of the original matches this, register for register. */
int og_sound_effect(og_sound *s, int effect, int channel)
{
    const unsigned char *m;
    int rec, ch2, dch = -1;

    if (!s || !s->loaded) return OG_E_ARG;
    if (effect < 0 || effect >= OG_NUM_EFFECTS) return OG_E_RANGE;
    if (channel < 0 || channel > 2) return OG_E_ARG;
    /* The driver checks DS:0xb2b7 here and drops effects 0, 7 and 8.
     * That check belongs to the CALLER now -- og_sound_effect_enabled()
     * reports it -- so a port can choose to play them anyway. */

    rec = OG_SFX_RECORD + effect;
    if (!s->patch[rec].used) return OG_OK;
    m   = s->patch[rec].raw;
    ch2 = channel + 3;
    if (m[2]) {
        int bit = 0, mask = m[2];
        while (bit < 4 && !(mask & 1)) { mask >>= 1; ++bit; }
        dch = og_rhythm_channel[bit];
    }

    /* whatever these channels were sounding goes first (0x3d0f..0x3d80) */
    og_release(s, channel);
    og_release(s, ch2);
    if (dch >= 0) og_release(s, dch);

    s->ch_prio[channel] = s->ch_prio[ch2] = m[0];
    og_sound_program_block(s, channel, rec, 0);
    og_sound_program_block(s, ch2,     rec, 1);
    og_voice_load(s, channel, m, 0x03, 1);       /* [0xb28c + ch] = 0x20 */
    og_voice_load(s, ch2,     m, 0x1a, 1);       /* [0xb28f + ch] = 0x20 */
    s->ch_link[ch2] = m[1];

    if (dch >= 0) {
        /* 0x3e12: the bits into the 0xbd shadow, the voice programmed,
         * then 0xbd written.  [0xb295] starts at 0x20 and nothing clears
         * it: rhythm mode is on from initialisation.  Clearing the bits
         * first makes a drum still held strike again. */
        s->ch_prio[dch] = m[0];
        og_sound_program_block(s, dch, rec, 2);
        s->rhythm |= 0x20;
        s->rhythm &= (unsigned char)~m[2];
        og_emit(0xbd, s->rhythm);
        s->rhythm |= m[2];
        og_emit(0xbd, s->rhythm);
        og_voice_load(s, dch, m, 0x31, 0);       /* keyed by 0xbd, not 0xb0 */
        og_drum_clobber(s, dch);
        s->ch_rhythm[dch] = m[2];
        /* 0x4052: the bits clear when the drum's duration runs out; a
         * drum with none is held until the next strike, which SORP times
         * instead so a shared drum cannot stick */
        og_rhythm_strike(s, m[2], s->ch_ticks[dch] > 0 ? s->ch_ticks[dch]
                                                       : OG_PERCUSSION_TICKS);
    }
    return OG_OK;
}

/* Stop everything an effect is sounding -- the driver's silence vector. */
void og_sound_effect_silence(og_sound *s)
{
    int ch;
    if (!s || !s->loaded) return;
    for (ch = 0; ch < 9; ++ch) {
        if (s->ch_ticks[ch] == 0) continue;
        s->ch_ticks[ch] = 0;
        og_release(s, ch);
    }
}

/* Cut an effect's channel short.  No effect actually holds: every one of
 * the fifteen records 0x20..0x2e carries a finite duration at +11 (2 to
 * 64 ticks; the drums 1 and 7 carry 0 and take OG_PERCUSSION_TICKS), as
 * GAME.EXE's table has them.  The stuck nitro sound was a drum bit left
 * set in 0xbd -- see rhythm_ticks -- not a missing stop. */
void og_sound_effect_stop(og_sound *s, int channel)
{
    int ch2;
    if (!s || channel < 0 || channel > 8) return;
    ch2 = channel + 3;
    if (s->ch_rhythm[channel]) {
        s->rhythm &= (unsigned char)~s->ch_rhythm[channel];
        og_emit(0xbd, s->rhythm);
        s->ch_rhythm[channel] = 0;
    }
    og_release(s, channel);
    s->ch_ticks[channel] = 0;
    s->ch_tail [channel] = 0;
    s->ch_delta[channel] = 0;
    if (ch2 <= 8) {
        og_release(s, ch2);
        s->ch_ticks[ch2] = 0;
        s->ch_tail [ch2] = 0;
        s->ch_delta[ch2] = 0;
    }
}

void og_sound_effect_tick(og_sound *s)
{
    int ch, b;
    if (!s || !s->loaded) return;
    /* each drum bit stops on its own clock, whoever struck it last */
    for (b = 0; b < 5; ++b)
        if (s->rhythm_ticks[b] > 0 && --s->rhythm_ticks[b] == 0 &&
            (s->rhythm & (1u << b))) {
            s->rhythm &= (unsigned char)~(1u << b);
            og_emit(0xbd, s->rhythm);
        }
    /* 0x4089, once a tick for each channel:
     *
     *   duration [+8] running   count it; at zero clear the key bit
     *                           ([0xb28c + ch]) and, with a tail, go on
     *                           to the step below -- the key-off is the
     *                           step's own 0xb0 write, PITCH KEPT --
     *                           and with none, release (0x4111)
     *   duration done, tail     count the tail [+0xa]; while it runs,
     *                           keep stepping, keyed off; at zero
     *                           release (0x4111)
     *   the step (0x4096)       count the phase [+5]: at zero reload it
     *                           and, by the mode [+7] -- which is never
     *                           decremented -- 0 snap back to the base,
     *                           1 reverse the slide, more both; else
     *                           add the slide.  Then write 0xa0 and 0xb0.
     *
     * Writing 0xb0 = 0 at the end of the duration instead zeroed the
     * block and the top bits of the F-number while the note was still
     * fading, so the release sounded a different, much lower note: the
     * stray note at the end of every anthem, whose last chord is the one
     * note left to run its whole duration. */
    for (ch = 0; ch < 9; ++ch) {
        int step = 0, f0;
        if (s->ch_ticks[ch] > 0) {
            if (--s->ch_ticks[ch] == 0) {     /* 0x40f5 */
                if (s->ch_rhythm[ch]) {       /* stop beating it */
                    s->rhythm &= (unsigned char)~s->ch_rhythm[ch];
                    og_emit(0xbd, s->rhythm);
                }
                s->ch_keyed[ch] = 0;          /* [0xb28c + ch] = 0 */
                if (s->ch_tail[ch] <= 0) {
                    og_release_4111(s, ch);   /* 0x4111 */
                    continue;
                }
                s->ch_ticks[ch] = -1;
                /* 0x40fb..0x4102: straight on to the step, which writes
                 * the key-off with the pitch intact */
                {
                    unsigned b = (unsigned)(((s->ch_fnum[ch] >> 8) & 3)
                                            | ((s->ch_block[ch] & 7) << 2));
                    s->ch_breg[ch] = b;
                    og_emit(0xb0 + ch, b);
                }
            }
            step = 1;
        } else if (s->ch_ticks[ch] < 0) {     /* 0x4104: the tail */
            if (s->ch_tail[ch] > 0 && --s->ch_tail[ch] > 0) step = 1;
            else {
                s->ch_ticks[ch] = 0;
                og_release_4111(s, ch);       /* 0x4111: the full release */
                continue;
            }
        }
        if (!step) continue;

        f0 = s->ch_fnum[ch];
        /* 0x4021..0x4044: a linked second voice takes the first's pitch
         * (stepped already this tick) before its own step */
        if (ch >= 3 && ch <= 5 && s->ch_link[ch]) {
            const int a = ch - 3;
            if (s->ch_link[ch] == 1) {
                s->ch_fnum [ch] = (s->ch_fnum[a] + s->ch_delta[ch]) & 0x3ff;
                s->ch_block[ch] = s->ch_block[a];
            } else {
                s->ch_fnum [ch] = s->ch_fnum[a];
                s->ch_block[ch] = (s->ch_block[a] + 1) & 7;
            }
        }

        /* 0x4096 */
        if (--s->ch_phase[ch] == 0) {         /* 0x40c9 */
            s->ch_phase[ch] = s->ch_phase_rel[ch];
            if (s->ch_mode[ch] == 0)      s->ch_fnum[ch] = s->ch_base[ch];
            else if (s->ch_mode[ch] == 1) s->ch_delta[ch] = -s->ch_delta[ch];
            else {
                s->ch_fnum[ch]  = s->ch_base[ch];
                s->ch_delta[ch] = -s->ch_delta[ch];
            }
        } else
            s->ch_fnum[ch] = (s->ch_fnum[ch] + s->ch_delta[ch]) & 0x3ff;
        {
            /* The original writes both registers every tick; on the chip
             * rewriting an unchanged value does nothing, so only a change
             * is sent.  0xb0 carries the key bit as it stands -- never
             * re-asserted, or a slide would retrigger the envelope. */
            int f = s->ch_fnum[ch];
            unsigned b = (unsigned)(((f >> 8) & 3)
                                    | ((s->ch_block[ch] & 7) << 2)
                                    | (s->ch_keyed[ch] ? 0x20u : 0u));
            /* a fresh effect voice (ch_breg 0x100) writes both */
            if (f != f0 || s->ch_breg[ch] > 0xff)
                og_emit(0xa0 + ch, (unsigned)(f & 0xff));
            if (b != s->ch_breg[ch]) {
                s->ch_breg[ch] = b;
                og_emit(0xb0 + ch, b);
            }
        }
    }
}


/* ------------------------------------------------------------------ */
/* Music (see og_sound.h)                                              */
/* ------------------------------------------------------------------ */

static unsigned og_ds16(const og_sound *s, unsigned off)
{
    size_t o = (size_t)og_le16(s->exe + 8) * 16 + (size_t)OG_GAME_DS_IMG + off;
    if (o + 1 >= s->exe_len) return 0;
    return (unsigned)og_le16(s->exe + o);
}

static unsigned og_ds8(const og_sound *s, unsigned off)
{
    size_t o = (size_t)og_le16(s->exe + 8) * 16 + (size_t)OG_GAME_DS_IMG + off;
    return (o < s->exe_len) ? s->exe[o] : 0xff;
}

int og_music_start(og_sound *s, og_music *mus, int song)
{
    unsigned rec;
    int i;

    if (!s || !s->loaded || !mus || song < 1) return OG_E_ARG;
    memset(mus, 0, sizeof *mus);
    rec = OG_MUSIC_TABLE_DS + (unsigned)(song - 1) * 12;
    mus->tempo = (int)(og_ds16(s, rec) >> 8);      /* [0xb2c7] */
    if (mus->tempo <= 0) mus->tempo = 1;
    mus->song  = (int)(og_ds16(s, rec + 2) & 0xff);
    for (i = 0; i < OG_MUSIC_CHANNELS; ++i) {
        og_music_ch *m = &mus->ch[i];
        m->stream = m->stream_start =
            OG_MUSIC_TABLE_DS + og_ds16(s, rec + 4 + (unsigned)i * 2);
        m->phrase    = 0;
        m->tempo_ctr = 1;
        m->note_ctr  = 1;
        m->len       = 1;
        m->voice     = 0;
        m->playing   = 1;
    }
    mus->playing = 1;
    mus->song_index = song;
    return OG_OK;
}

void og_music_stop(og_sound *s, og_music *mus)
{
    int i;
    if (!s || !mus) return;
    for (i = 0; i < OG_MUSIC_CHANNELS; ++i) mus->ch[i].playing = 0;
    mus->playing = 0;
    for (i = 0; i < OG_MUSIC_CHANNELS; ++i) og_release(s, i);
}

/* Pull the next phrase for a channel (0x42d3): a stream byte is a phrase
 * id, 0xfe loops the list and 0xff ends it. */
static int og_music_next_phrase(og_sound *s, og_music_ch *m)
{
    int guard;
    for (guard = 0; guard < 4; ++guard) {
        unsigned id = og_ds8(s, m->stream++);
        if (id == 0xfe) { m->stream = m->stream_start; continue; }
        if (id == 0xff) { m->playing = 0; return 0; }
        m->phrase = OG_MUSIC_PHRASE_DS
                  + og_ds16(s, OG_MUSIC_PHRASE_DS + id * 2);
        return 1;
    }
    m->playing = 0;
    return 0;
}

/* Start an ADLIB.SFX record on a channel with the pitch the record
 * itself carries -- what 0x4293 does for a music hit and what 0x3c8e
 * does for an effect. */
/* A music hit (0x4293) starts a record exactly as an effect does, so a
 * record carrying a rhythm mask must be struck as PERCUSSION here too --
 * playing it as a melodic voice is what makes a drum part sound like the
 * wrong instrument. */
/* The rhythm path that used to serve the music's 0xb0..0xcf bytes has
 * been removed: 0x4277 plays those records as ordinary notes on the
 * stream's own channel, and routing them through the percussion (0xbd,
 * the rhythm channels) is what made the music buzz.  The sound effects
 * do their own rhythm handling in og_sound_start. */

/* Load a channel's countdown from an instrument record the way 0x3d99
 * does: slide +6, phase +8, mode +10, duration +11, tail +13 -- all
 * words but the mode.  A duration of 0 starts in the tail (0x408e
 * jumps straight to 0x4104); with no tail either the driver never
 * releases the voice, and nor do we (ch_ticks 0 is never counted). */
static void og_music_time(og_sound *s, int ch, const unsigned char *r,
                          unsigned f, unsigned blk)
{
    int dur = (int)og_le16(r + 11), tail = (int)og_le16(r + 13);
    s->ch_fnum [ch] = (int)(f & 0x3ff);
    s->ch_base [ch] = (int)(f & 0x3ff);
    s->ch_block[ch] = (int)(blk & 7);
    s->ch_delta[ch] = (int)(short)og_le16(r + 6);
    s->ch_phase_rel[ch] = (int)((og_le16(r + 8) >> 1) + 1);
    s->ch_phase[ch] = s->ch_phase_rel[ch];
    s->ch_mode [ch] = r[10];
    s->ch_tail [ch] = tail;
    s->ch_ticks[ch] = dur ? dur : (tail ? -1 : 0);
}

/* A music note, 0x3cf8: note b of the pitch table DS:0xb2c9 on stream
 * channel i (0..2) in instrument `voice`, its second voice on i + 3.
 * The music's note bytes come here, and so does og_sound_note. */
static void og_music_note(og_sound *s, og_music_ch *m, int i, unsigned b)
{
    unsigned e = (unsigned)OG_GAME_DS_IMG + 0xb2c9 + b * 4;
    size_t o = (size_t)og_le16(s->exe + 8) * 16 + e;
    unsigned f = (o + 2 < s->exe_len) ? og_le16(s->exe + o) : 0;
    unsigned blk = (o + 2 < s->exe_len) ? s->exe[o + 2] : 0;
    /* 0x3d16: the previous note is RELEASED first -- both
     * operators' sustain/release to 0xff and the channel
     * keyed off -- before the new instrument is programmed
     * and keyed on.  Keying over the top instead leaves the
     * old envelope running into the new note. */
    /* 0x3cf8 sounds a stream on up to THREE voices: channel
     * n, channel n+3, and a rhythm voice -- the melodic pair
     * sharing one pitch (0x422e sets bx = cx, dh = dl).  The
     * record's second block is the n+3 voice; playing only
     * the first leaves every instrument half-voiced. */
    m->fnum  = f;            /* the stream's current pitch */
    m->block = blk;
    og_release(s, i);
    if (og_voice_mode != 2) {
        og_sound_program_block(s, i, m->voice, 0);
        og_emit(0xa0 + i, f & 0xff);
        og_emit(0xb0 + i, ((f >> 8) & 3) | ((blk & 7) << 2) | 0x20);
        /* 0x3d99..0x3daf (and 0x3f4e..0x3f60 on the
         * same-instrument path): a music note takes the
         * instrument record's slide (+6), phase (+8), mode
         * (+10), DURATION (+11) and TAIL (+13) exactly as an
         * effect does, so it stops on its own.  Without them
         * the note rang on until the next note on the
         * channel -- or, at the end of a part, until the song
         * restarted -- and instrument 9's warble (+1 a tick,
         * snapping back every 5) was missing. */
        if (m->voice >= 0 && m->voice < OG_NUM_PATCHES &&
            s->patch[m->voice].used)
            og_music_time(s, i, s->patch[m->voice].raw, f, blk);
    }
    if (m->voice >= 0 && m->voice < OG_NUM_PATCHES &&
        s->patch[m->voice].used) {
        const unsigned char *r = s->patch[m->voice].raw;
        /* The second voice sounds when it HAS an F-number of
         * its own (+0x1b), the same gate the record path
         * uses.  Testing the block's first bytes instead
         * misses record 7, whose block reads 00 00 but which
         * carries a pitch. */
        /* The second voice is DETUNED: a capture of the
         * original shows its F-number consistently 2 or 4
         * above the first, and the record's byte +1 (1 or 2)
         * doubled gives exactly those.  Without it the two
         * voices are phase-locked and beat against each
         * other, which is the "chorus" that sounded wrong. */
        if (og_voice_mode != 1 && og_second_voice &&
            (r[0x1b] || r[0x1c]) && og_le16(r + 36)) {
            unsigned fd = (f + (unsigned)(r[1] * 2)) & 0x3ff;
            og_release(s, i + 3);
            og_sound_program_block(s, i + 3, m->voice, 1);
            og_emit(0xa0 + i + 3, fd & 0xff);
            og_emit(0xb0 + i + 3,
                    ((fd >> 8) & 3) | ((blk & 7) << 2) | 0x20);
            /* The second voice is counted down so it releases,
             * but it does NOT slide: the original writes 0xa5
             * six times where a slide would write it 139.  The
             * fields after the second block are not a slide
             * delta, and treating them as one made every
             * second voice warble. */
            s->ch_fnum [i + 3] = (int)fd;
            s->ch_base [i + 3] = (int)fd;
            s->ch_delta[i + 3] = 0;
            s->ch_block[i + 3] = blk & 7;
            s->ch_phase[i + 3] = 0;
            s->ch_mode [i + 3] = 0;
            /* 0x3f86/0x3f8c: WORDS at +0x22 and +0x24 */
            s->ch_ticks[i + 3] = og_le16(r + 34) ? (int)og_le16(r + 34) : 1;
            s->ch_tail [i + 3] = (int)og_le16(r + 36);
        }
    }
}

void og_music_tick(og_sound *s, og_music *mus)
{
    int i, guard, live = 0;
    if (!s || !mus || !mus->playing) return;

    /* 0x41cb: the player restarts the song through 0x4134 once
     * [0xa012] | [0xa022] | [0xa022] is zero -- the phrase pointers of
     * channels 0 and 1, with channel 1's tested twice and channel 2's
     * ([0xa032]) never.  Every shipped song's three parts are the same
     * length, so this only matters if channel 2 runs long.  Nothing is
     * released here: 0x4134 never touches the chip, and the last notes
     * run out on their own durations. */
    live = mus->ch[0].playing | mus->ch[1].playing;
    if (!live) {
        int song = mus->song_index;
        /* 0x41d8: only a song with an id restarts.  The anthems have
         * none, so the podium tune plays once and stops. */
        if (!mus->song) { mus->playing = 0; return; }
        og_music_start(s, mus, song);
        return;
    }

    for (i = 0; i < OG_MUSIC_CHANNELS; ++i) {
        og_music_ch *m = &mus->ch[i];
        if (!m->playing) continue;
        if (--m->tempo_ctr > 0) continue;
        m->tempo_ctr = mus->tempo;
        if (--m->note_ctr > 0) continue;

        for (guard = 0; guard < 64 && m->playing; ++guard) {
            unsigned b;
            if (!m->phrase && !og_music_next_phrase(s, m)) break;
            b = og_ds8(s, m->phrase++);
            if (b < 0x80) {                   /* a note on the current voice */
                og_music_note(s, m, i, b);
                m->note_ctr = m->len;
                break;
            }
            if (b < 0xb0) { m->len = (int)(b - 0x7f); continue; }  /* 0x42c8 */
            if (b < 0xd0) {
                /* 0x4277: play record b - 0xb0 AT ITS OWN PITCH, as an
                 * ordinary note on this stream's own channel -- the
                 * F-number at +4 with the block at +3.  It is NOT the
                 * rhythm path: routing it through the percussion writes
                 * 0xbd and turns the music into a buzz. */
                int rec = (int)(b - 0xb0);
                if (rec >= 0 && rec < OG_NUM_PATCHES && s->patch[rec].used) {
                    const unsigned char *r = s->patch[rec].raw;
                    unsigned rf = (unsigned)(r[4] | (r[5] << 8));
                    unsigned rb = r[3];
                    if (r[2]) {
                        /* A record with rhythm bits is a DRUM: the note-on
                         * walks them (0x3e16), takes the channel from the
                         * table at DS:0xb29f and strikes it through 0xbd.
                         * Playing it melodically on the stream's own
                         * channel is what put noise on channel 1 and left
                         * 6..8 silent. */
                        /* A drum is an ordinary TWO-operator voice built
                         * from block 2 on its rhythm channel: the original
                         * writes 31/71/91 and 34/54/74/94, which are
                         * channel 7's own operator pair, straight from
                         * that block's two groups. */
                        int bit = 0, mask = r[2], pch;
                        while (bit < 4 && !(mask & 1)) { mask >>= 1; ++bit; }
                        pch = og_rhythm_channel[bit];
                        og_drum_clobber(s, pch);      /* 0x3e3b */
                        og_sound_program_block(s, pch, rec, 2);
                        s->rhythm |= 0x20;
                        s->rhythm &= (unsigned char)~r[2];
                        og_emit(0xbd, s->rhythm);
                        s->rhythm |= r[2];
                        og_emit(0xbd, s->rhythm);
                        /* The strike has to STOP: the record's duration at
                         * +11 runs the channel down and the tick handler
                         * clears the rhythm bit.  Leaving it set holds the
                         * drum on for ever, which is what channel 8 was
                         * doing. */
                        /* The rhythm voice SWEEPS: the original walks
                         * channel 7's F-number down 0x80 a tick while the
                         * snare sounds (a7<-80 b7<-02, a7<-00, a7<-80
                         * b7<-01 ...), with no key bit.  The record's +6
                         * delta drives it exactly as a melodic slide. */
                        s->ch_rhythm[pch] = r[2];
                        s->ch_ticks [pch] = r[11] ? (int)r[11]
                                                  : OG_PERCUSSION_TICKS;
                        s->ch_tail  [pch] = s->ch_ticks[pch];
                        og_rhythm_strike(s, r[2], s->ch_ticks[pch]);
                        /* THE THIRD BLOCK is the rhythm voice.  Each block
                         * carries its own pitch and slide at a fixed
                         * offset below it: block B has its block byte at
                         * B-12, F at B-11 and delta at B-9.  For the third
                         * (B = 61) that is +49, +50 and +52 -- and record
                         * 0 holds 0x0380 there with a delta of -128,
                         * exactly the sweep the original performs. */
                        {
                            unsigned pf = og_le16(r + 50) & 0x3ff;
                            s->ch_fnum [pch] = (int)pf;
                            s->ch_base [pch] = (int)pf;
                            s->ch_delta[pch] = (int)(short)og_le16(r + 52);
                            s->ch_block[pch] = r[49] & 7;
                            s->ch_phase_rel[pch] =
                                (int)((og_le16(r + 54) >> 1) + 1);
                            s->ch_phase[pch] = s->ch_phase_rel[pch];
                            s->ch_mode [pch] = r[56];
                            og_emit(0xa0 + pch, pf & 0xff);
                            og_emit(0xb0 + pch,
                                    ((pf >> 8) & 3) | ((r[49] & 7) << 2));
                        }
                    } else {
                        /* Records 2 and 3 carry F-numbers of 512 at block
                         * 0 and 0 -- 24 Hz and no pitch at all -- so the
                         * record's own pitch cannot be what sounds here.
                         * Keep the stream's current note and let the
                         * record supply only the patch; writing F = 0
                         * keys an operator with no frequency, which is
                         * the rattle on this channel. */
                        /* 0x4277 loads TWO pitches -- +4/+3 for voice one
                         * and +0x1b/+0x1a for voice two -- and 0x3cf8
                         * gates each voice separately.  A voice whose
                         * F-number is zero does NOT sound: record 3 is
                         * zero on both, so it contributes a patch change
                         * and nothing else.  Keying it anyway drives an
                         * operator at no frequency, which is the rattle. */
                        const unsigned char *rr = s->patch[rec].raw;
                        unsigned f2 = (unsigned)(rr[0x1b] | (rr[0x1c] << 8));
                        unsigned b2 = rr[0x1a];
                        /* The first slide step is applied BEFORE the key-on:
                         * record 2 holds F 512 with a delta of -8 and the
                         * original keys at 504.  Starting at 512 and
                         * sliding into 504 crosses the F-number's high-bit
                         * boundary, which forces a second 0xb0 carrying the
                         * key bit -- a doubled attack.  The slide's BASE is
                         * that same starting point, so a phase reset
                         * returns here rather than jumping back to 512. */
                        unsigned f0 = (unsigned)((int)rf
                                        + (int)(short)og_le16(r + 6)) & 0x3ff;
                        og_release(s, i);
                        og_sound_program_block(s, i, rec, 0);
                        if (rf && og_voice_mode != 2) {
                            og_emit(0xa0 + i, f0 & 0xff);
                            og_emit(0xb0 + i,
                                    ((f0 >> 8) & 3) | ((rb & 7) << 2) | 0x20);
                        }
                        /* A record played by the MUSIC slides exactly as
                         * one played by an effect: +6 is a per-tick delta
                         * (record 2 carries -8), +8 the phase length and
                         * +10 the phases before it reverses.  The original
                         * writes 0xa0 ten times as often as we did --
                         * holding the pitch flat turns a swept thump into
                         * a buzz. */
                        s->ch_fnum [i] = (int)f0;
                        s->ch_base [i] = (int)f0;   /* the slide starts here */
                        s->ch_delta[i] = (int)(short)og_le16(r + 6);
                        s->ch_block[i] = (int)(rb & 7);
                        s->ch_phase_rel[i] = (int)((og_le16(r + 8) >> 1) + 1);
                        s->ch_phase[i] = s->ch_phase_rel[i];
                        s->ch_mode [i] = r[10];
                        s->ch_ticks[i] = r[11] ? (int)r[11] : 1;
                        s->ch_tail [i] = (int)r[13];
                        /* The second voice is keyed only when the word at
                         * +36 is set.  Record 2's is zero and the original
                         * programs channel 4's operators but never keys
                         * it -- keying anyway left a voice sounding with
                         * nothing to stop it. */
                        if (og_voice_mode != 1 && og_second_voice && f2 &&
                            og_le16(rr + 36)) {
                            f2 = (f2 + (unsigned)(rr[1] * 2)) & 0x3ff;
                            og_release(s, i + 3);
                            og_sound_program_block(s, i + 3, rec, 1);
                            og_emit(0xa0 + i + 3, f2 & 0xff);
                            og_emit(0xb0 + i + 3,
                                    ((f2 >> 8) & 3) | ((b2 & 7) << 2) | 0x20);
                            /* ...and counted down like any voice: with no
                             * duration here, record 5's second voice
                             * (song 16's drum part) stayed keyed on
                             * channel 4 until something else released it */
                            s->ch_fnum [i + 3] = s->ch_base[i + 3] = (int)f2;
                            s->ch_delta[i + 3] = 0;
                            s->ch_block[i + 3] = (int)(b2 & 7);
                            s->ch_phase[i + 3] = s->ch_phase_rel[i + 3] = 0x7fff;
                            s->ch_mode [i + 3] = 0;
                            s->ch_ticks[i + 3] = og_le16(rr + 34) ? (int)og_le16(rr + 34) : 1;
                            s->ch_tail [i + 3] = (int)og_le16(rr + 36);
                        }
                    }
                }
                /* [di+8] = [di+6]: the stream's own instrument is restored
                 * by the next note, which reprograms it anyway */
                m->note_ctr = m->len;
                break;
            }
            if (b < 0xfd) { m->voice = (int)(b - 0xd0); continue; }  /* 0x42ab */
            else if (b == 0xfd) continue;      /* 0x4263: nothing, read on */
            else if (b == 0xfe) { m->note_ctr = m->len; break; }  /* rest */
            else { m->phrase = 0; }                            /* 0xff   */
        }
    }
}

/* Play one note by hand, for the audio overlay's test controls: the
 * music's own note-on, on channel 0..2 with that instrument. */
void og_sound_note(og_sound *s, int channel, int instrument, int note)
{
    og_music_ch m;
    if (!s || !s->loaded || channel < 0 || channel >= OG_MUSIC_CHANNELS ||
        note < 0 || note >= OG_NUM_NOTES) return;
    memset(&m, 0, sizeof m);
    m.voice = instrument;
    og_music_note(s, &m, channel, (unsigned)note);
}

/* ------------------------------------------------------------------ */
/* Sequencer                                                           */
/* ------------------------------------------------------------------ */

int og_sound_start(og_sound *s, og_voice *v, int effect, int channel)
{
    if (v) v->effect = effect;
    if (!s || !v || effect < 0 || effect >= s->neffects) return OG_E_RANGE;
    if (channel < 0 || channel > 8) return OG_E_ARG;

    memset(v, 0, sizeof *v);
    v->pos     = s->effect[effect].bytes;
    v->start   = v->pos;
    v->end     = v->pos + s->effect[effect].len;
    v->channel = channel;
    v->patch   = -1;
    v->level   = 0;
    v->active  = 1;
    v->unknown_op = -1;
    return OG_OK;
}

/* One tick of the interpreter at GAME.EXE image 0x42d3.
 *
 * Verified opcodes: 0xff ends, 0xfe is a control byte, >=0xd0 selects a
 * patch, >=0x80 sets a level.  The remaining low-range encoding (notes
 * and delays) is not decoded, so those bytes are recorded in
 * v->unknown_op and skipped rather than being guessed at. */
int og_sound_step(og_sound *s, og_voice *v)
{
    unsigned char op;

    if (!s || !v || !v->active) return 0;

    if (v->wait > 0) { --v->wait; return 1; }

    v->loops = 0;
    while (v->pos < v->end) {
        op = *v->pos++;

        if (op == 0xff) {                    /* end of tick           */
            return 1;
        }
        if (op == 0xfe) {                    /* loop to start         */
            if (++v->loops > 64) break;      /* runaway guard         */
            v->pos = v->start;
            continue;
        }
        if (op >= 0xd0) {                    /* select patch          */
            v->patch = op - 0xd0;
            og_sound_program(s, v->channel, v->patch);
            continue;
        }
        if (op >= 0xb0) {                    /* other control          */
            v->unknown_op = op;
            continue;
        }
        if (op >= 0x80) {                    /* inferred: delay        */
            v->wait = op - 0x7f;
            return 1;
        }

        /* Note: F-number to 0xa0, block plus key-on to 0xb0. */
        {
            unsigned fnum  = s->note_fnum [op];
            unsigned block = s->note_block[op] & 7u;
            if (v->patch < 0) {              /* default to patch 0     */
                v->patch = 0;
                og_sound_program(s, v->channel, 0);
            }
            og_emit(0xb0 + v->channel, 0x00);          /* retrigger    */
            og_emit(0xa0 + v->channel, fnum & 0xffu);
            og_emit(0xb0 + v->channel,
                    0x20u | (block << 2) | ((fnum >> 8) & 0x03u));
            v->wait = 1;
            return 1;
        }

        v->unknown_op = op;                  /* not yet decoded       */
    }

    og_emit(0xb0 + v->channel, 0x00);
    v->active = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* The PC speaker                                                      */
/* ------------------------------------------------------------------ */

/* The period table at DS:0x9e89: a note byte indexes it and the word is
 * the PIT divisor, so the tone is 1193182 / divisor. */
static const unsigned short og_pcspk_period[96] = {
    65535, 65535, 65023, 61346, 57921, 54658, 51608, 48701,
    45962, 43388, 40946, 38652, 36489, 34435, 32503, 30681,
    28961, 27335, 25798, 24351, 22986, 21964, 20477, 19326,
    18242, 17218, 16251, 15340, 14479, 13666, 12899, 12175,
    11492, 10847, 10238,  9964,  9121,  8609,  8126,  7670,
     7240,  6833,  6450,  6088,  5746,  5424,  5119,  4832,
     4561,  4305,  4063,  3835,  3620,  3417,  3225,  3044,
     2837,  2712,  2560,  2416,  2280,  2152,  2032,  1918,
     1810,  1708,  1612,  1522,  1437,  1356,  1280,  1208,
     1140,  1076,  1016,   959,   898,   854,   806,   761,
      718,   678,   640,   604,   570,   538,   508,   479,
      452,   427,   403,   380,   359,   339,   320,   302
};

#define OG_PCSPK_TABLE  0x9d9a
#define OG_PIT_HZ       1193182.0f

int og_pcspk_load(const char *dir, og_pcspk *s)
{
    og_pages p;
    int k, rc;
    if (!s) return OG_E_ARG;
    memset(s, 0, sizeof *s);
    rc = og_pages_load(dir, &p);          /* the loaded data segment */
    if (rc != OG_OK) return rc;
    for (k = 0; k < OG_PCSPK_EFFECTS; ++k) {
        unsigned off = OG_PCSPK_TABLE + (unsigned)k * 4u;
        const unsigned char *e = (const unsigned char *)og_ds_string(&p, off);
        unsigned ptr;
        const unsigned char *sc;
        int n = 0;
        if (!e) break;
        ptr = (unsigned)(e[0] | (e[1] << 8));
        s->fx[k].priority = (int)(e[2] | (e[3] << 8));
        sc = (const unsigned char *)og_ds_string(&p, ptr);
        if (!sc) break;
        while (n < 64 && sc[n] != 0xfc) ++n;
        s->fx[k].script = sc;
        s->fx[k].len    = n + 1;
        s->loaded = 1;
    }
    if (s->loaded)
        og_load_note("GAME.EXE",
                     (long)og_le16(p.img + 8) * 16 + OG_GAME_DS_IMG + OG_PCSPK_TABLE,
                     OG_PCSPK_EFFECTS * 4,
                     "Loaded the sound effect table for the PC speaker: %d script pointers and priorities",
                     OG_PCSPK_EFFECTS);
    /* the pages object owns the image the scripts point into, so it is
     * deliberately not freed here */
    s->cur = -1;
    s->rate = 44100;
    return s->loaded ? OG_OK : OG_E_SHORT;
}

void og_pcspk_silence(og_pcspk *s)
{
    if (!s) return;
    s->cur = -1; s->pos = 0; s->priority = 0; s->divisor = 0;
}

void og_pcspk_trigger(og_pcspk *s, int effect)
{
    if (!s || !s->loaded) return;
    if (effect < 0 || effect >= OG_PCSPK_EFFECTS) return;
    if (!s->fx[effect].script) return;
    /* 0x3b68: a playing effect is only displaced by a higher priority */
    if (s->cur >= 0 && s->fx[effect].priority < s->priority) return;
    s->cur = effect;
    s->pos = 0;
    s->priority = s->fx[effect].priority;
}

void og_pcspk_tick(og_pcspk *s)
{
    const unsigned char *sc;
    unsigned char n;
    if (!s || !s->loaded || s->cur < 0) return;
    sc = s->fx[s->cur].script;
    while (s->pos < s->fx[s->cur].len) {
        n = sc[s->pos++];
        if (n == 0xff) continue;               /* the leading marker */
        if (n & 0x80) break;                   /* 0xfc: the end */
        /* bound, do not mask: 95 is 0x5F, whose bit 5 is clear, so a
         * note of 0x24 folded down to 4 and played two octaves low */
        s->divisor = og_pcspk_period[n < 96 ? n : 95];
        return;
    }
    og_pcspk_silence(s);
}

void og_pcspk_render(og_pcspk *s, short *out, int frames)
{
    int i;
    float step;
    if (!s || !out) return;
    if (!s->divisor) {
        for (i = 0; i < frames; ++i) out[i] = 0;
        return;
    }
    /* a square wave at 1193182 / divisor */
    step = (OG_PIT_HZ / (float)s->divisor) / (float)(s->rate ? s->rate : 44100);
    for (i = 0; i < frames; ++i) {
        s->phase += step;
        if (s->phase >= 1.0f) s->phase -= 1.0f;
        out[i] = (s->phase < 0.5f) ? 7000 : -7000;
    }
}
