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

/* tp_sound.h -- the arcade game's sound board, run from the player's own
 * ROMs: an Intel 80186 at 16 MHz with its program in six ROMs, two 8254
 * timers, six 8-bit DACs each with an 8-bit volume DAC, and a 10-bit DAC
 * (Leland's "3rd generation" board).  The master CPU's commands go in, a
 * mono stream comes out.
 *
 * The hardware is as MAME 0.251 describes it (cinematronics/leland_a.cpp,
 * cpu/i86/i186.cpp, machine/pit8253.cpp, sound/dac.cpp; BSD-3-Clause,
 * Aaron Giles, Paul Leaman and the MAME team), and checked against
 * MAME's own DAC output.
 */
#ifndef SORP_TP_SOUND_H
#define SORP_TP_SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

#define TP_SOUND_ROM_SIZE 0x100000      /* the 80186's 1 MB address space */
#define TP_SOUND_CLOCK    16000000      /* the board's crystal            */

typedef struct tp_sound tp_sound;

/* A board running the program in `rom` (TP_SOUND_ROM_SIZE bytes, laid out
 * as the 80186 sees it; the caller keeps it alive).  It starts as the
 * master starts it: held in reset, then released. */
tp_sound *tp_sound_new(const unsigned char *rom);
void      tp_sound_free(tp_sound *s);
void      tp_sound_reset(tp_sound *s);

/* The master's port 0xF0 (its upper five bits: /RESET, ZNMI, INT0,
 * /TEST, INT1), its sound command latch (ports 0xF4 high, 0xF2 low) and
 * the 80186's response (port 0xF2 read). */
void          tp_sound_control(tp_sound *s, unsigned char data);
void          tp_sound_latch(tp_sound *s, unsigned short command);
unsigned char tp_sound_response(const tp_sound *s);

/* Send one command as the master's 0x12b4 does: the parameter and the
 * command (bit 7 the toggle the master keeps) into the latch, then INT0
 * pulsed. */
void tp_sound_command(tp_sound *s, unsigned char command, unsigned char param);

/* Run the board for `n` output samples at `rate` Hz, writing the mixed
 * output (about -1..1) to `out`.  Each sample is the average of the DACs'
 * levels over its span. */
void tp_sound_render(tp_sound *s, float *out, int n, int rate);

/* Run without output until `ticks` of the 16 MHz clock have passed since
 * the board was made. */
void tp_sound_run_to(tp_sound *s, unsigned long long ticks);
unsigned long long tp_sound_now(const tp_sound *s);

/* For checking against MAME: called on every DAC write with the time in
 * 16 MHz ticks, the DAC (0-5, or 9 for the 10-bit one), the value and
 * the write's mask (0x00ff sample, 0xff00 volume, 0xffff both). */
typedef void (*tp_sound_dac_hook)(void *user, unsigned long long tick,
                                  int dac, unsigned value, unsigned mask);
void tp_sound_set_hook(tp_sound *s, tp_sound_dac_hook hook, void *user);

/* instructions run, and 16 MHz ticks spent halted, since it was made */
unsigned long long tp_sound_instructions(const tp_sound *s);

#ifdef __cplusplus
}
#endif

#endif
