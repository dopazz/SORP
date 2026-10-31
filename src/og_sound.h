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

/* og_sound.h -- Adlib (OPL2) sound data for Super Off Road.
 *
 * WHAT THE GAME ACTUALLY HAS
 * --------------------------
 * There is no music.  The sound menu in GAME.EXE offers exactly three
 * options -- no sound, IBM (PC speaker) and Adlib board -- and the only
 * sound payload is a bank of instrument patches plus fifteen very short
 * effect sequences.  Nothing in the shipped files is long enough to be
 * a tune.  If you want music under this port it has to be your own.
 *
 * Everything below was traced in GAME.EXE.  Offsets are into the load
 * image (the file past its MZ header, i.e. e_cparhdr * 16 bytes in).
 * The data segment sits at image offset 0x5700, so a DS offset X is at
 * image offset 0x5700 + X.
 *
 *
 * THE OPL2 REGISTER WRITE  (routine at image 0x3c2d)
 * -------------------------------------------------
 * Register in BL, value in BH:
 *
 *     out 0x388, reg      ; address port
 *     read 0x388 N times  ; address settle delay
 *     out 0x389, val      ; data port
 *     read 0x388 M times  ; data settle delay
 *
 * N and M are word counts held in DS:0xa03e and DS:0xa040, calibrated
 * at startup.  On an emulated OPL these delays are irrelevant -- the
 * chip model handles its own timing -- so og_sound_write() ignores them.
 *
 *
 * THE SILENCE / INIT SWEEP  (routine at image 0x3c47)
 * ---------------------------------------------------
 * Interrupts off, then:
 *     registers 0x80..0x94  <- 0xff   (sustain/release to max: fast kill)
 *     registers 0xb0..0xb8  <- 0x00   (key off, all nine channels)
 *     register  0xbd        <- 0x20   (rhythm mode off, vibrato depth)
 * og_sound_reset() reproduces this exactly.
 *
 *
 * THE INSTRUMENT BANK  (ADLIB.SFX)
 * --------------------------------
 * ADLIB.SFX is 4608 bytes, loaded verbatim and copied to DS:0xa08c by
 * the 0x900-word copy loop at image 0x3c12.
 *
 * It is 64 records of 72 bytes.  The code at image 0x42ab computes a
 * record address as (byte - 0xd0) * 72 + 0xa08c, i.e. a stream byte of
 * 0xd0 or above selects patch (byte - 0xd0).  4608 / 72 = 64 exactly,
 * and 25 of the 64 records are non-empty in the shipped file.
 *
 * Within a record the player reads at least these (image 0x4293):
 *     +0x03  byte   modulator parameter
 *     +0x04  word   modulator parameter pair
 *     +0x1a  byte   carrier parameter  (mirrors +0x03)
 *     +0x1b  word   carrier parameter  (mirrors +0x04)
 * The +0x00..+0x19 / +0x1a..+0x33 split is the usual two-operator
 * layout, with 0xff bytes acting as field terminators.  The precise
 * meaning of every field is NOT fully established; og_patch keeps the
 * raw record so you can experiment without re-reading the binary.
 *
 *
 * THE EFFECT TABLE  (inside GAME.EXE)
 * -----------------------------------
 * A 15-entry table at image 0xf49a (DS:0x9d9a), four bytes per entry:
 *     +0  word  DS pointer to the sequence bytes
 *     +2  word  an attribute -- 0x40..0xf0, most likely priority or
 *               base volume; not pinned down
 * The sequences themselves start immediately after the table at image
 * 0xf4d6 and run 3 to 42 bytes each.
 *
 *
 * THE SEQUENCE BYTE STREAM  (interpreter at image 0x42d3)
 * -------------------------------------------------------
 * The interpreter does `lodsb` and dispatches on the byte:
 *     0xff        end of THIS TICK (handler is `stc; ret`), not end
 *                 of the sequence -- every stream opens with one
 *     0xfe        loop: reload the pointer from the voice base and
 *                 resume (handler at 0x42f2 does `mov si,[di+2]`)
 *     <  0x80     NOTE: index the note table, write F-number to
 *                 0xa0+ch and block|key-on to 0xb0+ch
 *     >= 0xb0     control range (the dispatch at image 0x4257 compares
 *                 against 0xb0, not 0xd0); >= 0xd0 selects a patch
 *     0x80..0xaf  treated here as a delay of (byte - 0x7f) ticks --
 *                 this one is still inferred, not proven
 * The low-range encoding is only partly decoded.  og_sound_step() runs
 * the cases above and reports anything it does not recognise rather
 * than guessing, so you can finish it against real output.
 *
 *
 * PLAYBACK
 * --------
 * This module produces OPL2 register writes; it does not synthesise.
 * Point og_sound_set_sink() at an OPL2/OPL3 emulator and mix its output
 * through SDL_AudioStream.  Two emulators worth considering:
 *
 *     ymfm            (Aaron Giles) BSD-3-Clause -- permissive, fine to
 *                     vendor straight into this tree.
 *     Nuked-OPL3      very accurate, but LGPL 2.1; check that suits you
 *                     before linking it into a project you ship.
 *
 * No sound data is compiled into this source.  The patches come from
 * the player's ADLIB.SFX and the sequences from their GAME.EXE, read at
 * run time, exactly like the graphics.
 */
#ifndef OG_SOUND_H
#define OG_SOUND_H

#include "og_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OG_PATCH_BYTES    72
/* ADLIB.SFX is 4608 bytes: SIXTY-FOUR 72-byte records, not fifteen.
 * The fifteen are only the sound EFFECTS -- the music's instrument
 * bytes (0xd0 and up) reach the rest, so clamping to 15 silently
 * substituted the wrong instrument for most of the score. */
#define OG_NUM_PATCHES 64
#define OG_NUM_EFFECTS    15

/* Image offsets inside GAME.EXE, past the MZ header. */
#define OG_EXE_DATA_SEG   0x5700
#define OG_EXE_SFX_TABLE  0xf49a
/* 4 bytes per note: u16 F-number, u8 block (octave), u8 pad.  The first
 * twelve F-numbers rise by 2^(1/12), i.e. a chromatic scale, and the
 * sequence repeats every twelve entries with the block byte stepping
 * up.  Indexed directly by a stream byte below 0x80. */
#define OG_EXE_NOTE_TABLE 0x109c9
#define OG_NUM_NOTES      128

typedef struct {
    unsigned char raw[OG_PATCH_BYTES];
    int           used;          /* record is non-empty              */
} og_patch;

typedef struct {
    const unsigned char *bytes;  /* into the owned GAME.EXE image    */
    int                  len;
    unsigned             attr;   /* table word: priority or level    */
} og_effect;

typedef struct {
    og_patch       patch[OG_NUM_PATCHES];
    og_effect      effect[OG_NUM_EFFECTS];
    int            neffects;
    unsigned short note_fnum [OG_NUM_NOTES];
    unsigned char  note_block[OG_NUM_NOTES];
    /* A sounding effect voice.  The countdown at 0x4089 works on the
     * voice slot PLUS 3, which is what lines these fields up with the
     * record: +4 the F-number, +6 the signed slide, +8 the phase length,
     * +11 the DURATION, +13 the tail, +10 how many phases before the
     * slide reverses. */
    int            ch_ticks[9];   /* duration, -1 once keyed off      */
    int            ch_tail [9];   /* ticks before the final release   */
    int            ch_fnum [9];
    int            ch_base [9];   /* what a phase resets the pitch to */
    int            ch_delta[9];
    int            ch_block[9];
    int            ch_phase[9], ch_phase_rel[9], ch_mode[9];
    /* record +1 on an effect's second voice: it follows the first's
     * pitch, 1 plus its own slide, 2 and up an octave higher (0x4018) */
    int            ch_link[9];
    /* DS:0xb296, the operator of each channel that the natural release
     * (0x4111) forces to 0xff -- 00 01 02 08 09 0a 10 11 12 as loaded.
     * A drum start CLOBBERS it: 0x3e3b means to clear the drum channel's
     * key byte [0xb28c + ch] but writes [0xb292 + ch], which for channels
     * 6, 7 and 8 is this table's entries 2, 3 and 4.  So after the first
     * snare or hi-hat, a voice ending on channel 3 releases channel 0's
     * operators and leaves its own alone -- as a DOSBox capture of the
     * original shows (effects 4 and 9).  Never restored, not even by the
     * driver's initialisation (0x3c47). */
    unsigned char  rel_op[9];
    unsigned char  ch_rhythm[9];  /* record +2: the 0xbd bits it beats  */
    /* Byte 0 of a record is its PRIORITY, and 0x4220 gates every music
     * note on it: a note only sounds if its instrument's priority is at
     * least that of whatever the voice is already playing.  Effects and
     * music share the nine channels, so this is what stops the music
     * treading on a crash or a fanfare. */
    int            ch_prio[9];
    /* the last 0xb0 byte written per channel, and whether it is keyed --
     * a slide must not re-assert the key bit or it retriggers */
    unsigned       ch_breg[9];
    int            ch_keyed[9];
    unsigned char  rhythm;        /* the driver's shadow of 0xbd, 0xb295 */
    /* How long each of the five drum bits of 0xbd has left, by bit.  Two
     * drums share each of channels 7 and 8, so ch_rhythm[] -- one mask
     * per channel -- forgets the first when the second is struck: nitro's
     * hi-hat followed by the music's snare left the hi-hat bit set for
     * good, the stuck nitro sound.  Timing the bits themselves cannot. */
    int            rhythm_ticks[5];
    unsigned char *exe;          /* GAME.EXE load image, owned       */
    size_t         exe_len;
    int            loaded;
} og_sound;

/* Sink for OPL2 register writes.  reg 0x00..0xf5, val 0x00..0xff. */
typedef void (*og_opl_write_fn)(void *user, unsigned reg, unsigned val);

/* Load ADLIB.SFX and the effect table out of GAME.EXE. */
int  og_sound_load(og_sound *s, const char *dir);
void og_sound_free(og_sound *s);

void og_sound_set_sink(og_sound *s, og_opl_write_fn fn, void *user);

/* Emit the init sweep from image 0x3c47. */
void og_sound_reset(og_sound *s);

/* Push one patch to a channel's operator pair. */
/* Program an OPL channel from one of a record's THREE voice blocks.
 * The feedback byte sits at +15, +36 and +57, each followed by two
 * four-byte operator patches -- the layout the driver parses at 0x3d85.
 * og_sound_program() is block 0. */
#define OG_VOICE_BLOCKS  3
extern const int og_voice_block_off[OG_VOICE_BLOCKS];

/* Which of a block's two patches goes to the CARRIER.  The driver writes
 * the first to operator n and the second to n+3, making the first the
 * modulator -- but the first patch is the one with the sharp, loud
 * envelope (effect 4: attack 15, level 9) while the second is quiet with
 * attack 0, which in FM is nearly inaudible.  Swapped, they read like
 * game effects.  Set this to 1 to swap; the F2 overlay toggles it. */
extern int og_sound_swap_ops;

int  og_sound_program_block(og_sound *s, int channel, int patch_index, int block);
int  og_sound_program(og_sound *s, int channel, int patch_index);

/* Play effect n the way the AdLib driver does (image 0x3c8e): record
 * 0x20 + n of ADLIB.SFX, its two voice blocks on `channel` and
 * channel+3, keyed on at the pitches the record carries.  The driver
 * itself tests a byte at DS:0xb2b7 + effect and returns at once when it
 * is zero -- effects 0, 7 and 8.  It is a GATE, not a priority: the
 * value is overwritten two instructions later by the per-song channel
 * byte and never compared as a magnitude.  Priority is a separate
 * field, the record's +0, weighed at 0x3ce2 against whatever voice is
 * already sounding.
 *
 * The two do line up, though: the three gated effects are exactly the
 * three whose priority is the lowest in use, 64, against 80..240 for
 * every other effect.  So the gate silences precisely the sounds that
 * would lose any contest for a channel anyway.
 *
 * The decision is left to the CALLER: og_sound_effect_enabled() reports
 * the gate, and this plays whatever it is handed.  This is the
 * original AdLib sound; og_sound_start()/step() play the PC speaker
 * table instead. */
#define OG_SFX_RECORD    0x20      /* effect n is record 0x20 + n      */
#define OG_PERCUSSION_TICKS 8      /* how long a drum hit holds its
                                    * rhythm bit; percussion records
                                    * carry no duration of their own   */
#define OG_SFX_ENABLE_DS 0xb2b7

/* An effect is not held: each voice carries a DURATION and the driver
 * clears its key-on bit when that runs out (image 0x40f5, which zeroes
 * the per-channel flag byte at DS:0xb28c).  The duration is the record's
 * word at +8 for the first voice and +29 for the second, halved and one
 * added, counted in driver ticks.  Call og_sound_effect_tick() at that
 * rate to key voices off as they expire.
 *
 * Voices also SLIDE: the step at 0x4096 adds a delta to the F-number
 * each tick and rewrites 0xa0/0xb0, which is where the sweep in these
 * effects comes from. */
/* An effect whose record carries a rhythm mask is PERCUSSION and must
 * play on the channel OPL rhythm mode gives that voice.  The driver
 * finds the set bit and translates it through DS:0xb29f, which holds
 * [7, 8, 8, 7, 6] -- hi-hat 7, cymbal 8, tom 8, snare 7, bass drum 6,
 * the standard layout.  The `channel` argument is then ignored. */
extern const unsigned char og_rhythm_channel[5];

int  og_sound_effect(og_sound *s, int effect, int channel);
void og_sound_effect_tick(og_sound *s);
void og_sound_effect_stop(og_sound *s, int channel);
void og_sound_effect_silence(og_sound *s);

/* ------------------------------------------------------------------ */
/* Music                                                               */
/* ------------------------------------------------------------------ */

/* og_music_start(s, n) plays song n the way 0x4134 does: record n-1 of
 * the twelve-byte table at DS:0xb449 gives the tempo and three phrase
 * lists, one per OPL channel.  Racing uses track + 11, the podium the
 * winner's country + 2, the menus 1.  Step it with og_music_tick() at
 * the same rate as the effects; songs loop on their own. */
#define OG_MUSIC_TABLE_DS  0xb449
#define OG_MUSIC_PHRASE_DS 0xb623
#define OG_MUSIC_TRACK1    11      /* track t is song t + 11           */
/* The finish fanfare: image 0x30bb takes the winner from the ranked list
 * at [0x9d2e], reads his country from [di+0x0e] and plays country + 2 --
 * records 1..9, the anthems.  Nothing stops the race music first; a new
 * song resets all three channels, which does it. */
#define OG_MUSIC_ANTHEM    2       /* country c is song c + 2          */
/* The Speed Shop plays song 1.  Its screen object, tag "S1", builds the
 * three player panels (RS, YS, BS) and the banner (ts) and then issues
 * op 55 with 1.  Song 1 carries an id, so it loops while the shop is up. */
#define OG_MUSIC_SHOP      1
#define OG_MUSIC_CHANNELS  3

typedef struct {
    unsigned stream, phrase;     /* DS offsets: phrase list, and notes */
    unsigned stream_start;
    int      tempo_ctr, note_ctr, len, voice, playing;
    unsigned    fnum;            /* the stream's current pitch, kept so a
                                  * record with no F-number of its own
                                  * does not key a silent operator */
    int         block;
} og_music_ch;

/* A song LOOPS only if its id -- the record's second word, kept at
 * [0xb2c8] -- is non-zero.  0x41d8 tests it before restarting: the eight
 * track tunes and the menu song carry an id, every anthem carries zero,
 * so an anthem plays through once and stops. */
typedef struct {
    og_music_ch ch[OG_MUSIC_CHANNELS];
    int         tempo, song, playing;
    int         song_index;      /* what to restart when all parts end */
} og_music;

/* Songs 1..19 in the table at DS:0xb449: 1 the menu, 2..10 the anthems,
 * 11..18 the tracks, 19 one more with no id. */
#define OG_MUSIC_SONGS     19

int  og_music_start(og_sound *s, og_music *mus, int song);
/* One music note by hand -- note 0..127 of DS:0xb2c9, instrument
 * 0..63 -- on channel 0..2 (and its second voice on +3), exactly as a
 * song's note byte sounds.  For the audio overlay's tests. */
void og_sound_note(og_sound *s, int channel, int instrument, int note);
void og_music_tick (og_sound *s, og_music *mus);
void og_music_stop (og_sound *s, og_music *mus);
int  og_sound_effect_enabled(const og_sound *s, int effect);

/* Voice state for one playing effect. */
typedef struct {
    const unsigned char *pos, *end, *start;
    int  channel;
    int  patch;
    int  level;
    int  wait;
    int  active;
    int  unknown_op;   /* last byte the interpreter did not know     */
    int  loops;        /* 0xfe iterations this tick                  */
    int  effect;       /* which effect this voice is playing, so a
                        * holding one (duration 0) can be stopped    */
} og_voice;

int  og_sound_start(og_sound *s, og_voice *v, int effect, int channel);

/* Advance one tick.  Returns 1 while playing, 0 when finished. */
int  og_sound_step(og_sound *s, og_voice *v);

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */
/* What the original actually plays                                    */
/* ------------------------------------------------------------------ */

/* GAME.EXE carries TWO sound drivers, chosen by [0x9d36], each filling a
 * five-slot vector table:
 *
 *      slot     speaker  AdLib   purpose
 *      0x9d3b   0x3b5d   0x3c8e  play an effect
 *      0x9d3d   0x3b7f   0x4134  play music
 *      0x9d3f   0x3b54   0x3c47  silence effects
 *      0x9d41   0x3b7f   0x4188  stop music
 *
 * Every place the game asks for an effect:
 *
 *     0x05a4  a pickup collected -- the number comes from the table at
 *             DS:0x48d by truck: 4, 5, 6, and truck type 3 plays nothing
 *     0x16e3  effect 0,  truck against truck
 *     0x17d6  effect 10, nitro fired
 *     0x1ae6  effect 8,  hitting a BARRIER: [0x4f7a] is the truck's
 *             collision state, and 0x1a7c skips the whole collision
 *             path while it is set, so the crash sounds once per hit.
 *             The branches there take the angle between the truck's
 *             travel and the surface normal from the POLY record and
 *             record 0x80..0x82 (from the table at DS:0x5248) for
 *             glancing hits, 0x83 for another case, and 3 for the one
 *             that plays this sound
 *     0x1c03  effect 7,  a wheel landing in water
 *     0x2bca  effects 11, 12, 14 and 13 from four small routines that
 *             look like interface sounds
 *
 * Effects are script op 54 (handler 0x3a60) and music op 55 (0x3a87);
 * either takes an index, or 0xffff to stop.  Both check [0x9d37], the
 * sound-off flag, first.
 *
 * IMPORTANT: the table this module plays from -- DS:0x9d9a, four bytes
 * an effect -- is the PC SPEAKER's (read by 0x3b5d).  Its two words are
 * a POINTER to that effect's data (0x9dd6 upward, the entries running
 * from 3 bytes to about 40) and a PRIORITY -- and those priorities are
 * the same numbers the AdLib records carry at +0: 0x40 for effect 0,
 * 0xf0 for effect 1, 0xe0 for effect 2.  The two drivers share one
 * priority scheme; only the sound data differs.  The AdLib driver
 * does not use it.  It indexes ADLIB.SFX, which is loaded whole at
 * DS:0xa08c and addressed as 64 records of 72 bytes -- exactly the
 * file's size -- with effects at index + 0x20:
 *
 *      records  0..31   music voices   (0..9 populated)
 *      records 32..63   effect voices  (all 15 effects populated)
 *
 * The record (parsed at 0x3d85) is a priority byte then a run of VOICE
 * BLOCKS, one per OPL channel.  A block, in the order the parser eats it:
 *
 *      +0  u16  control: the low byte lands in the channel state, the
 *               high byte is a mask OR'd into the rhythm register 0xbd
 *      +2  3 bytes stepped over
 *      +5  u16  -> channel +6
 *      +7  u16  -> channel +0x11, and (>>1)+1 as a counter at +8
 *      +9  u8   -> channel +0xa
 *      +10 u16  -> channel +0xb
 *      +12 u16  -> channel +0xd
 *      +14 u8   written to OPL register 0xc0 + n (feedback/connection)
 *      +15 4 bytes: operator patch, written by 0x3e88 to registers
 *               0x20, 0x40, 0x60, 0x80 + n -- multiplier, level,
 *               attack/decay, sustain/release
 *      +19 4 bytes: the same for the paired operator (+3)
 *      +23 3 bytes stepped over
 *
 * There are THREE such blocks, and together they take up essentially
 * the whole record:
 *
 *      block 1  +1 ..+26   its feedback byte goes to 0xc0 + n
 *      block 2  +27..+44   the same against channel n+3 (0xc3)
 *      block 3  +45..+65   parsed at 0x3e3f
 *
 * The third is what fills the channel fields +0x2d, +0x33 and +0x34 --
 * the ones the RE-TRIGGER path at 0x3eb9 reads.  That routine parses
 * nothing itself: it reprograms the channel from the state saved here,
 * which is how a new note reuses a voice already loaded.
 *
 * So a record is one priority byte and three channel voices, not a
 * single patch, and nothing in it is sequence data.
 *
 * The word at +4 is a PITCH, not a pointer.  Two structures share field
 * numbers and that is what makes it look like one:
 *
 *      channel state, 0xa00e + n*16 -- the sequencer's: +4 stream
 *          position, +6 voice record, +8 the voice saved under an
 *          effect, +0xa tempo counter, +0xb note length, +0xc its reload
 *      voice slot, 0x9f4e + n*64 -- what 0x3cf8 programs: +0 the record,
 *          +4 and +0xf an F-NUMBER, +3 and +0x13 its block byte, +0x14
 *          and +0x1f a second F-number for the paired channel
 *
 * The music path calls 0x3cf8 with the F-number and block the note table
 * gave it; the effect path (0x3c8e) takes them from its own record at +4
 * and +3, and +0x1b and +0x1a for the second voice.  So an EFFECT IS A
 * FIXED PITCH, not a melody -- its character comes from the envelopes in
 * its three voice blocks, which is why the records hold no notes.
 *
 * Playing one also negotiates for a channel:
 *
 *      [0xb2b7 + effect] must be non-zero, or the effect is disabled
 *      [0xb2a4 + song]   picks which channel this song lets effects take
 *      the music voice there is saved at channel +8 to be restored
 *      and the effect only displaces a playing voice when its priority,
 *      the record's +0 byte, is at least the other's
 *
 * MUSIC (0x4134) is a table of 12-byte records at DS:0xb449, selected by
 * index - 1.  A record holds two parameters and then THREE stream
 * offsets, one per OPL channel, each relative to 0xb449:
 *
 *      record  0        music 1, the menus
 *      records 1..9     the podium anthems, chosen by country + 2
 *      records 10..17   ONE PER TRACK, chosen by track + 11 (image 0x306)
 *      record 18        a further tune
 *
 * THE SEQUENCER (0x4196 per tick, 0x41e3 per channel).  Three channels
 * live at DS:0xa00e, 0xa01e and 0xa02e, each holding its stream pointer
 * at +4.  A channel steps like this:
 *
 *      dec [di+0xa]; while it is non-zero, nothing happens -- this is
 *          the tempo divider, reloaded from [0xb2c7], the byte the music
 *          record supplies
 *      dec [di+0xb], the note's remaining length, reloaded from [di+0xc]
 *      al = *si++ and the pointer is saved back to [di+4]
 *      a NEGATIVE byte is a command (0x4257); otherwise the note indexes
 *          the chromatic table at 0xb2c9 by al * 4, taking a WORD
 *          F-number and a BYTE block/octave
 *      0x3cf8 -- the same routine that starts an effect -- then programs
 *          the channel with the voice record at [di+8]
 *
 * When all three channels have run dry the player calls 0x4134 again, so
 * a song loops by construction.
 *
 * A channel's stream is NOT notes: each byte is a PHRASE id (0x42d3).
 * 0xfe loops back to the stream's start -- kept at [di+2] -- and 0xff
 * ends it; anything else indexes a table of word offsets at DS:0xb623,
 * and the phrase it points at is what the note reader then walks, one
 * note byte at a time, into the chromatic table at DS:0xb2c9.  So a song
 * is three lists of phrases, and the phrases hold the notes.  A phrase
 * byte of 0xb0..0xcf is not a voice select but a HIT: 0x4293 loads that
 * record from ADLIB.SFX and starts it with the record's own pitch, the
 * same way an effect plays.  Whole parts are written that way -- phrase
 * 0x32 is b2 b0 b2 b0, two percussion voices alternating -- so reading
 * them as selects leaves that channel silent.  Within a
 * phrase a byte >= 0x80 is a COMMAND (0x4257) and anything else a note:
 * phrase 3 reads 81 b3 d1 1c 1e 1e 85 1f 83 21 81 21, whose notes 28,
 * 30, 30, 31, 33, 33 are a rising line and whose 0x81/0x83/0x85 set how
 * long the notes that follow last.
 *
 * THE TICK RATE is the EGA's 70 Hz, not 60 and not the BIOS timer's
 * 18.2: the game restores the default divisor (bx = 0xffff at 0x4c3c)
 * on exit, so INT 8 never paces the music -- the display does.  Every
 * duration in the data is a frame at that rate.  The driver runs on INT 8: the game
 * takes the old vector (0x45a1) and keeps handler and divisor together
 * in a block at DS:0x126, which the routine at 0x45e0 uses to program
 * the PIT, loading the divisor from that block's +4.  Both copies are
 * zero in the file, so it is computed at run time by code not reachable
 * in the flat image -- the installers at 0x45bf and 0x45cf have no
 * callers there.  Every effect length and sweep, and the music tempo,
 * scale with this one number.
 *
 * So the effect NUMBERS this module uses are right (a splash really is
 * effect 7), but the data behind them is the beeper's.  Playing the
 * original AdLib sound means reading ADLIB.SFX record 0x20 + n instead,
 * and the music means walking the three streams of the track's record. */

#endif /* OG_SOUND_H */

/* ------------------------------------------------------------------ */
/* The PC speaker                                                      */
/* ------------------------------------------------------------------ */

/* The game ships speaker versions of all fifteen effects, never used
 * once AdLib is chosen.  The table at DS:0x9d9a holds a pointer and a
 * priority per effect; the script is a run of note bytes between 0xff
 * and 0xfc, one note a frame (70 Hz).  The player at 0x3b88 indexes the
 * period table at DS:0x9e89 with the note and writes the divisor to the
 * PIT -- so a note's frequency is 1193182 / divisor, a square wave.
 * A new effect only interrupts a playing one of lower priority. */
#define OG_PCSPK_EFFECTS 15

typedef struct {
    const unsigned char *script;   /* into the loaded image */
    int                  len, priority;
} og_pcspk_effect;

typedef struct {
    og_pcspk_effect fx[OG_PCSPK_EFFECTS];
    int             loaded;
    /* what is playing */
    int             cur, pos, priority;
    unsigned short  divisor;       /* 0 = silent */
    float           phase;
    int             rate;
} og_pcspk;

/* The nine OPL channels' meters and mutes, for the on-screen display:
 * channels 0..2 are the music's three streams, 3..5 the second voice of
 * an effect, 6..8 the rhythm voices. */
extern int           og_second_voice;   /* kept for the older switch */
extern int           og_voice_mode;     /* 0 both, 1 first, 2 second (F2) */
extern unsigned char og_ch_muted[9];
extern float         og_ch_level[9];
void og_sound_meters_decay(void);

int  og_pcspk_load(const char *dir, og_pcspk *s);
void og_pcspk_trigger(og_pcspk *s, int effect);
void og_pcspk_tick(og_pcspk *s);        /* one frame, 70 Hz */
void og_pcspk_render(og_pcspk *s, short *out, int frames);
void og_pcspk_silence(og_pcspk *s);
