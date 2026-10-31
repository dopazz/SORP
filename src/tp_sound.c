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

/* tp_sound.c -- the arcade sound board (see tp_sound.h).
 *
 * The behaviour of the 80186's peripherals, the 8254s and the board's
 * glue follows MAME 0.251 (BSD-3-Clause: leland_a.cpp by Aaron Giles and
 * Paul Leaman, i186.cpp and i86.cpp by Carl, pit8253.cpp by Wilbert Pol
 * and Nathan Woods, dac.cpp by smf; licenses/LICENSE-MAME.txt), the
 * reference this is checked against: the DAC writes for the same
 * commands match.
 *
 * TIME is counted in ticks of the board's 16 MHz crystal.  The 80186
 * runs one cycle every 2 ticks (8 MHz), its own timers count every 8
 * (2 MHz), and the 8254s every 4 (4 MHz).
 *
 * THE BOARD, as the program sets it up:
 *   0x00000-0x1ffff  16 KB of RAM, mirrored
 *   0x20000-0x202ff  the peripherals (PACS 0x203c, MPCS memory-mapped),
 *                    0x40 words a select: 0 the DAC clock lines, 1 the
 *                    command and response latches, 2 and 3 the 8254s,
 *                    4 the 10-bit DAC
 *   0x20300          the 80186's own registers (RELREG 0x9203)
 *   0x40000-0xfffff  the program ROMs
 *   I/O              any write is a DAC: word (port >> 1) & 7, its low
 *                    byte the sample, its high byte the volume
 * The 8254s' outputs: PIT0 0 and 1 are DMA requests 0 and 1 (DACs 0 and
 * 1, fed by DMA), PIT0 2 and PIT1 0-2 the clock lines of DACs 2-5, which
 * the program polls; the 80186's timer 0 clocks the 10-bit DAC. */

#include "tp_sound.h"

#include <stdlib.h>
#include <string.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

#define NEVER 0xffffffffffffffffULL

enum { ES, CS, SS, DS };
enum { AX, CX, DX, BX, SP, BP, SI, DI };

/* DMA control bits (i186.cpp) */
#define DEST_MIO          0x8000
#define DEST_DECREMENT    0x4000
#define DEST_INCREMENT    0x2000
#define DEST_INCDEC_MASK  0x6000
#define SRC_MIO           0x1000
#define SRC_DECREMENT     0x0800
#define SRC_INCREMENT     0x0400
#define SRC_INCDEC_MASK   0x0c00
#define TERMINATE_ON_ZERO 0x0200
#define INTERRUPT_ON_ZERO 0x0100
#define SYNC_MASK         0x00c0
#define CHANNEL_PRIORITY  0x0020
#define TIMER_DRQ         0x0010
#define CHG_NOCHG         0x0004
#define ST_STOP           0x0002
#define BYTE_WORD         0x0001

#define EXTINT_CTRL_LTM     0x10
#define EXTINT_CTRL_CASCADE 0x20
#define EXTINT_CTRL_SFNM    0x40

typedef struct {
    u16 control, maxA, maxB, count;
    u64 expire;                    /* the tick it next runs out, or NEVER */
} tp_timer;

typedef struct {
    u32 source, dest;
    u16 count, control;
    int drq;
} tp_dma;

typedef struct {                   /* one 8254 counter, mode 2 only */
    u8  control, lowcount, wmsb, out, running;
    u32 count;                     /* the reload value                  */
    u64 fall, rise;                /* the next edges, or NEVER          */
} tp_pit;

struct tp_sound {
    const u8 *rom;
    u8   ram[0x4000];

    /* the CPU */
    u16  r[8], sr[4], ip;
    int  cf, af, zf, sf, of, df, ifl, tf;
    u8   pfv;                      /* the last result's low byte, for PF */
    int  halt, no_int, seg_ovr, rep;
    int  in_reset;                 /* /RESET held low by the master     */
    int  cyc;                      /* this instruction's cycles so far  */
    u16  seg_val;
    u64  now, insns;

    /* the 80186's peripherals */
    u16  reloc;
    struct { u16 upper, lower, peripheral, middle, middle_size; } mem;
    struct {
        u16 vector, pending, ack_mask, priority_mask, in_service, request;
        u16 status, poll_status, timer, dma[2], ext[4];
        u8  ext_state;
    } intr;
    tp_timer timer[3];
    tp_dma   dma[2];
    int      last_dma;

    /* the board */
    u16  pacs;                     /* leland's m_peripheral             */
    u32  pbase;                    /* where the peripherals are         */
    int  pmem;                     /* ...in memory (1) or I/O (0), or   */
    int  pmapped;                  /* ...not yet placed                 */
    tp_pit pit[2][3];
    u8   clock_active;
    u16  command, response;
    u8   last_control;
    u8   dac[6], dacvol[6];
    u16  dac9;

    u64  next_event;

    /* the output: the DACs' level, integrated over time */
    float level;
    u64   acc_t;
    double acc;
    double out_t;                  /* where the next output sample ends */

    tp_sound_dac_hook hook;
    void *hook_user;
};

/* ------------------------------------------------------------------ */
/* Time and events                                                     */
/* ------------------------------------------------------------------ */

static void tp_events_dirty(tp_sound *s) { s->next_event = 0; }

static void tp_find_next_event(tp_sound *s)
{
    u64 t = NEVER;
    int c, i;
    for (c = 0; c < 2; ++c)
        for (i = 0; i < 3; ++i) {
            const tp_pit *p = &s->pit[c][i];
            if (!p->running) continue;
            if (p->fall < t) t = p->fall;
            if (p->rise < t) t = p->rise;
        }
    for (i = 0; i < 3; ++i)
        if (s->timer[i].expire < t) t = s->timer[i].expire;
    s->next_event = t;
}

/* ------------------------------------------------------------------ */
/* The output                                                          */
/* ------------------------------------------------------------------ */

static void tp_level_update(tp_sound *s)
{
    float l = 0.0f;
    int i;
    /* dac.cpp: the volume DAC (8-bit binary weighted, gain 2, range
     * 0..1) is the AD7524's reference, +v and -v; the AD7524 gives
     * -v + code/256 * 2v; each is routed to the speaker at 0.2.  The
     * AD7533 has no reference inputs: -1 + code/1024 * 2, at 1.0. */
    for (i = 0; i < 6; ++i) {
        const float v = (float)s->dacvol[i] / 128.0f;
        l += 0.2f * v * ((float)s->dac[i] / 128.0f - 1.0f);
    }
    l += (float)(s->dac9 & 0x3ff) / 512.0f - 1.0f;
    s->level = l;
}

static void tp_integrate(tp_sound *s)
{
    if (s->now > s->acc_t) {
        s->acc += (double)s->level * (double)(s->now - s->acc_t);
        s->acc_t = s->now;
    }
}

static void tp_set_clock_line(tp_sound *s, int which, int state)
{
    if (state) s->clock_active |= (u8)(1 << which);
    else       s->clock_active &= (u8)~(1 << which);
}

/* leland_80186_sound_device::dac_w */
static void tp_dac_w(tp_sound *s, u32 offset, u16 data, u16 mask)
{
    const int d = (int)(offset & 7);
    tp_integrate(s);
    if (mask & 0x00ff) {
        if ((offset & 0x60) == 0x40)      s->dma[0].drq = 0;
        else if ((offset & 0x60) == 0x60) s->dma[1].drq = 0;
        if (d < 6) s->dac[d] = (u8)data;
        tp_set_clock_line(s, d, 0);
    }
    if ((mask & 0xff00) && d < 6) s->dacvol[d] = (u8)(data >> 8);
    tp_level_update(s);
    if (s->hook && d < 6) s->hook(s->hook_user, s->now, d, data & mask, mask);
}

/* ------------------------------------------------------------------ */
/* The 8254s (pit8253.cpp), mode 2, the only mode the program sets      */
/* ------------------------------------------------------------------ */

static void tp_pit_out(tp_sound *s, int chip, int ctr, int level)
{
    tp_pit *p = &s->pit[chip][ctr];
    if (p->out == level) return;
    p->out = (u8)level;
    if (chip == 0 && ctr == 0)      s->dma[0].drq = level;
    else if (chip == 0 && ctr == 1) s->dma[1].drq = level;
    else tp_set_clock_line(s, chip == 0 ? 2 : 3 + ctr, level);
}

static void tp_pit_write(tp_sound *s, int chip, int offset, u8 data)
{
    offset &= 3;
    if (offset == 3) {
        const int ctr = (data >> 6) & 3;
        tp_pit *p;
        if (ctr == 3) return;                 /* read-back: not used */
        /* access bits 00: the counter-latch command, which freezes a
         * reading and leaves the mode alone (the program sends it before
         * changing a count; nothing reads the 8254s back) */
        if (!(data & 0x30)) return;
        p = &s->pit[chip][ctr];
        p->control = (u8)(data & 0x3f);
        p->wmsb = 0;
        p->running = 0;                       /* phase 0: waiting   */
        p->fall = p->rise = NEVER;
        tp_pit_out(s, chip, ctr, ((data >> 1) & 7) ? 1 : 0);
    } else {
        tp_pit *p = &s->pit[chip][offset];
        u32 v;
        const int access = (p->control >> 4) & 3;
        if (access == 3 && !p->wmsb) { p->lowcount = data; p->wmsb = 1; return; }
        if (access == 3)      { v = p->lowcount | ((u32)data << 8); p->wmsb = 0; }
        else if (access == 1) v = data;
        else if (access == 2) v = (u32)data << 8;
        else return;
        if (v == 1) v = 2;                    /* load_count: mode 2   */
        if (v == 0) v = 0x10000;
        p->count = v;
        if (!p->running) {
            /* the load takes the next clock; the output falls when the
             * count reaches 1 and rises a clock later, with the reload */
            const u64 start = (s->now + 3) & ~(u64)3;
            p->running = 1;
            p->fall = start + 4ULL * v;
            p->rise = p->fall + 4;
        }
    }
    tp_events_dirty(s);
}

/* ------------------------------------------------------------------ */
/* The 80186's interrupt controller (i186.cpp), non-iRMX               */
/* ------------------------------------------------------------------ */

static void tp_update_interrupt_state(tp_sound *s)
{
    int priority, n;
    u16 new_vector = 0;
    for (priority = 0; priority <= s->intr.priority_mask; ++priority) {
        if ((s->intr.timer & 0x0f) == priority) {
            if (s->intr.in_service & 0x01) return;
            if (s->intr.status & 0x07) {
                if (s->intr.status & 1)      new_vector = 0x08;
                else if (s->intr.status & 2) new_vector = 0x12;
                else                         new_vector = 0x13;
                s->intr.ack_mask = 0x0001;
                goto generate;
            }
        }
        for (n = 0; n < 2; ++n)
            if ((s->intr.dma[n] & 0x0f) == priority) {
                if (s->intr.in_service & (0x04 << n)) return;
                if (s->intr.request & (0x04 << n)) {
                    new_vector = (u16)(0x0a + n);
                    s->intr.ack_mask = (u16)(0x0004 << n);
                    goto generate;
                }
            }
        for (n = 0; n < 4; ++n)
            if ((s->intr.ext[n] & 0x0f) == priority) {
                if ((s->intr.in_service & (0x10 << n)) && !(s->intr.ext[n] & EXTINT_CTRL_SFNM))
                    return;
                if (s->intr.request & (0x10 << n)) {
                    if (n >= 2 && (s->intr.ext[n - 2] & EXTINT_CTRL_CASCADE)) {
                        s->intr.request &= (u16)~(0x10 << n);
                        continue;
                    }
                    new_vector = (u16)(0x0c + n);
                    s->intr.ack_mask = (u16)(0x0010 << n);
                    goto generate;
                } else if ((s->intr.in_service & (0x10 << n)) && (s->intr.ext[n] & EXTINT_CTRL_SFNM))
                    return;
            }
    }
    s->intr.pending = 0;
    return;
generate:
    s->intr.poll_status = (u16)(0x8000 | new_vector);
    s->intr.pending = 1;
}

/* int_callback: the vector the CPU takes */
static int tp_int_ack(tp_sound *s)
{
    u8 vector;
    s->intr.pending = 0;
    if (s->intr.ack_mask & 0xf0) {
        int i;
        for (i = 0; i < 4; ++i) if (s->intr.ack_mask & (1 << (i + 4))) break;
        if (i < 4 && !(s->intr.ext[i] & EXTINT_CTRL_LTM)) s->intr.request &= (u16)~s->intr.ack_mask;
    } else
        s->intr.request &= (u16)~s->intr.ack_mask;
    s->intr.in_service |= s->intr.ack_mask;
    if (s->intr.ack_mask == 0x0001)
        switch (s->intr.poll_status & 0x1f) {
        case 0x08: s->intr.status &= (u16)~0x01; break;
        case 0x12: s->intr.status &= (u16)~0x02; break;
        case 0x13: s->intr.status &= (u16)~0x04; break;
        }
    vector = (u8)(s->intr.poll_status & 0x1f);
    s->intr.ack_mask = 0;
    s->intr.poll_status &= 0x7fff;
    return vector;
}

static void tp_handle_eoi(tp_sound *s, u16 data)
{
    int handled = 0, priority, n;
    if (!(data & 0x8000)) {
        switch (data & 0x1f) {
        case 0x08: case 0x12: case 0x13: s->intr.in_service &= (u16)~0x01; break;
        case 0x0a: s->intr.in_service &= (u16)~0x04; break;
        case 0x0b: s->intr.in_service &= (u16)~0x08; break;
        case 0x0c: s->intr.in_service &= (u16)~0x10; break;
        case 0x0d: s->intr.in_service &= (u16)~0x20; break;
        case 0x0e: s->intr.in_service &= (u16)~0x40; break;
        case 0x0f: s->intr.in_service &= (u16)~0x80; break;
        }
    } else {
        for (priority = 0; priority <= 7 && !handled; ++priority) {
            if ((s->intr.timer & 0x07) == priority && (s->intr.in_service & 0x01)) {
                s->intr.in_service &= (u16)~0x01; handled = 1;
            }
            for (n = 0; n < 2 && !handled; ++n)
                if ((s->intr.dma[n] & 0x07) == priority && (s->intr.in_service & (0x04 << n))) {
                    s->intr.in_service &= (u16)~(0x04 << n); handled = 1;
                }
            for (n = 0; n < 4 && !handled; ++n)
                if ((s->intr.ext[n] & 0x07) == priority && (s->intr.in_service & (0x10 << n))) {
                    s->intr.in_service &= (u16)~(0x10 << n); handled = 1;
                }
        }
    }
    tp_update_interrupt_state(s);
}

static void tp_external_int(tp_sound *s, int n, int state)
{
    if (!(s->intr.ext_state & (1 << n)) == !state) return;
    if (!state) {
        s->intr.request &= (u16)~(0x10 << n);
        s->intr.ack_mask &= (u16)~(0x10 << n);
        s->intr.ext_state &= (u8)~(1 << n);
    } else {
        s->intr.request |= (u16)(0x10 << n);
        s->intr.ext_state |= (u8)(1 << n);
    }
    tp_update_interrupt_state(s);
}

/* ------------------------------------------------------------------ */
/* The 80186's timers (i186.cpp)                                       */
/* ------------------------------------------------------------------ */

static void tp_drq_callback(tp_sound *s, int which);
static void tp_timer_elapsed(tp_sound *s, int which, u64 when);

/* from `when`, the time it ran out: an expiry handled an instruction
 * late must not push the next one later too */
static void tp_restart_timer(tp_sound *s, int which, u64 when)
{
    tp_timer *t = &s->timer[which];
    const u32 count = (t->control & 0x1000) ? t->maxB : t->maxA;
    if (!(t->control & 4))
        t->expire = when + 8ULL * (count ? count : 0x10000);
    tp_events_dirty(s);
}

static void tp_timer_sync(tp_sound *s, int which)
{
    tp_timer *t = &s->timer[which];
    if ((t->control & 0x8000) && !(t->control & 0x0c) && t->expire != NEVER) {
        const u64 left = t->expire > s->now ? (t->expire - s->now) / 8 : 0;
        t->count = (u16)(((t->control & 0x1000) ? t->maxB : t->maxA) - left);
    }
}

static void tp_inc_timer(tp_sound *s, int which)
{
    tp_timer *t = &s->timer[which];
    t->count++;
    if (t->control & 2) {
        if (t->count == ((t->control & 0x1000) ? t->maxB : t->maxA)) tp_timer_elapsed(s, which, s->now);
    } else if (t->count == t->maxA)
        tp_timer_elapsed(s, which, s->now);
}

static void tp_timer_elapsed(tp_sound *s, int which, u64 when)
{
    tp_timer *t = &s->timer[which];
    t->control |= 0x0020;
    if (t->control & 0x2000) {
        s->intr.status |= (u16)(0x01 << which);
        tp_update_interrupt_state(s);
    }
    if (which == 2) {
        if ((s->dma[0].control & (TIMER_DRQ | ST_STOP)) == TIMER_DRQ) tp_drq_callback(s, 0);
        if ((s->dma[1].control & (TIMER_DRQ | ST_STOP)) == TIMER_DRQ) tp_drq_callback(s, 1);
        if ((s->timer[0].control & 0x800c) == 0x8008) tp_inc_timer(s, 0);
        if ((s->timer[1].control & 0x800c) == 0x8008) tp_inc_timer(s, 1);
    } else if (which == 0) {
        /* timer 0's output clocks the 10-bit DAC; with ALT clear it is a
         * pulse, which leland_a.cpp takes as the line going high */
        const int out = !(t->control & 2) ? 1 : ((t->control & 0x1000) ? 1 : 0);
        tp_set_clock_line(s, 6, out);
    }
    if ((t->control & 1) || ((t->control & 2) && !(t->control & 0x1000))) {
        if ((t->control & 2) && !(t->control & 0x1000)) t->control |= 0x1000;
        else                                           t->control &= (u16)~0x1000;
        tp_restart_timer(s, which, when);
    } else {
        t->expire = NEVER;
        t->control &= (u16)~0x9000;
    }
    t->count = 0;
    tp_events_dirty(s);
}

static void tp_timer_update(tp_sound *s, int which, int new_count, int new_maxA, int new_maxB, int new_control)
{
    tp_timer *t = &s->timer[which];
    int update = 0;
    if (new_count != -1) {
        if (t->control & 0x8000) { tp_timer_sync(s, which); update = 1; }
        t->count = (u16)new_count;
    }
    if (new_maxA != -1 && new_maxA != t->maxA) {
        if (t->control & 0x8000) { tp_timer_sync(s, which); update = 1; }
        t->maxA = (u16)new_maxA;
    }
    if (new_maxB != -1 && new_maxB != t->maxB) {
        if (t->control & 0x8000) { tp_timer_sync(s, which); update = 1; }
        t->maxB = (u16)new_maxB;
    }
    if (new_control != -1) {
        const u16 resbits = (which == 2) ? 0x1fde : 0x1fc0;
        u16 diff;
        new_control = (new_control & ~resbits) | (t->control & resbits);
        if (!(new_control & 0x4000)) new_control = (new_control & ~0x8000) | (t->control & 0x8000);
        new_control &= ~0x4000;
        diff = (u16)(new_control ^ t->control);
        if (diff) {
            if ((diff & 0x8000) && !(new_control & 0x8000)) { tp_timer_sync(s, which); update = 1; }
            else if ((diff & 0x8000) && (new_control & 0x8000) && !(new_control & 0xc)) update = 1;
            if (!(diff & 0x8000) && (diff & 0x2000)) { tp_timer_sync(s, which); update = 1; }
        }
        if (!(new_control & 0x0002)) new_control &= ~0x1000;
        t->control = (u16)new_control;
    }
    if (update) {
        if ((t->control & 0x8004) == 0x8000 && (!(t->control & 0x10) || t->expire != NEVER)) {
            int diff = ((t->control & 0x1000) ? t->maxB : t->maxA) - t->count;
            if (diff <= 0) diff += 0x10000;
            t->expire = s->now + 8ULL * (u64)diff;
        } else
            t->expire = NEVER;
        tp_events_dirty(s);
    }
}

/* ------------------------------------------------------------------ */
/* Memory and I/O                                                      */
/* ------------------------------------------------------------------ */

static u16 tp_internal_r(tp_sound *s, u32 offset);
static void tp_internal_w(tp_sound *s, u32 offset, u16 data);

/* leland_80186_sound_device::peripheral_r/w, by word */
static u16 tp_periph_r(tp_sound *s, u32 offset)
{
    switch (offset / 0x40) {
    case 0: return (u16)((s->clock_active >> 1) & 0x3e);
    case 1: return s->command;
    case 2: case 3: return 0;     /* the 8254s' counts: not read */
    }
    return 0xffff;
}

static void tp_periph_w(tp_sound *s, u32 offset, u16 data, u16 mask)
{
    switch (offset / 0x40) {
    case 1: s->response = data; break;
    case 2: if (mask & 0x00ff) tp_pit_write(s, 0, (int)(offset & 3), (u8)data); break;
    case 3: if (mask & 0x00ff) tp_pit_write(s, 1, (int)(offset & 3), (u8)data); break;
    case 4:
        if (mask == 0xffff) {
            tp_integrate(s);
            s->dac9 = data;
            tp_level_update(s);
            tp_set_clock_line(s, 6, 0);
            if (s->hook) s->hook(s->hook_user, s->now, 9, data, 0xffff);
        }
        break;
    }
}

static int tp_in_internal_mem(const tp_sound *s, u32 a)
{
    return (s->reloc & 0x1000) && (a >> 8) == (u32)(s->reloc & 0xfff);
}

static int tp_in_periph(const tp_sound *s, u32 a, int mem)
{
    return s->pmapped && s->pmem == mem && a >= s->pbase && a < s->pbase + 0x300;
}

static u8 tp_rd8(tp_sound *s, u32 a)
{
    a &= 0xfffff;
    if (a < 0x20000) return s->ram[a & 0x3fff];
    if (tp_in_internal_mem(s, a)) {
        const u16 v = tp_internal_r(s, (a >> 1) & 0x7f);
        return (u8)((a & 1) ? v >> 8 : v);
    }
    if (tp_in_periph(s, a, 1)) {
        const u16 v = tp_periph_r(s, (a - s->pbase) >> 1);
        return (u8)((a & 1) ? v >> 8 : v);
    }
    return s->rom[a];
}

static u16 tp_rd16(tp_sound *s, u32 a)
{
    a &= 0xfffff;
    if (a < 0x1ffff && !(a & 1)) {
        const u32 r = a & 0x3fff;
        return (u16)(s->ram[r] | (s->ram[r + 1] << 8));
    }
    if (tp_in_internal_mem(s, a)) {
        const u16 v = tp_internal_r(s, (a >> 1) & 0x7f);
        return (a & 1) ? (u16)((v >> 8) | (v << 8)) : v;
    }
    if (!(a & 1) && tp_in_periph(s, a, 1)) return tp_periph_r(s, (a - s->pbase) >> 1);
    if (a >= 0x20000 && a < 0xfffff && !tp_in_periph(s, a, 1) && !tp_in_periph(s, a + 1, 1))
        return (u16)(s->rom[a] | (s->rom[a + 1] << 8));
    return (u16)(tp_rd8(s, a) | (tp_rd8(s, a + 1) << 8));
}

static void tp_wr8(tp_sound *s, u32 a, u8 v)
{
    a &= 0xfffff;
    if (a < 0x20000) { s->ram[a & 0x3fff] = v; return; }
    if (tp_in_internal_mem(s, a)) {
        tp_internal_w(s, (a >> 1) & 0x7f, (a & 1) ? (u16)(v << 8) : v);
        return;
    }
    if (tp_in_periph(s, a, 1))
        tp_periph_w(s, (a - s->pbase) >> 1, (a & 1) ? (u16)(v << 8) : v, (a & 1) ? 0xff00 : 0x00ff);
}

static void tp_wr16(tp_sound *s, u32 a, u16 v)
{
    a &= 0xfffff;
    if (tp_in_internal_mem(s, a)) {
        tp_internal_w(s, (a >> 1) & 0x7f, (a & 1) ? (u16)((v >> 8) | (v << 8)) : v);
        return;
    }
    if (!(a & 1) && tp_in_periph(s, a, 1)) { tp_periph_w(s, (a - s->pbase) >> 1, v, 0xffff); return; }
    tp_wr8(s, a, (u8)v);
    tp_wr8(s, a + 1, (u8)(v >> 8));
}

static int tp_in_internal_io(const tp_sound *s, u16 port)
{
    return !(s->reloc & 0x1000) && (port >> 8) == (s->reloc & 0xff);
}

static u8 tp_in8(tp_sound *s, u16 port)
{
    if (tp_in_internal_io(s, port)) {
        const u16 v = tp_internal_r(s, (port >> 1) & 0x7f);
        return (u8)((port & 1) ? v >> 8 : v);
    }
    if (tp_in_periph(s, port, 0)) {
        const u16 v = tp_periph_r(s, (u32)(port - s->pbase) >> 1);
        return (u8)((port & 1) ? v >> 8 : v);
    }
    return 0;
}

static u16 tp_in16(tp_sound *s, u16 port)
{
    if (tp_in_internal_io(s, port)) {
        const u16 v = tp_internal_r(s, (port >> 1) & 0x7f);
        return (port & 1) ? (u16)((v >> 8) | (v << 8)) : v;
    }
    if (!(port & 1) && tp_in_periph(s, port, 0)) return tp_periph_r(s, (u32)(port - s->pbase) >> 1);
    return (u16)(tp_in8(s, port) | (tp_in8(s, (u16)(port + 1)) << 8));
}

static void tp_out8(tp_sound *s, u16 port, u8 v)
{
    if (tp_in_internal_io(s, port)) {
        tp_internal_w(s, (port >> 1) & 0x7f, (port & 1) ? (u16)(v << 8) : v);
        return;
    }
    if (tp_in_periph(s, port, 0)) {
        tp_periph_w(s, (u32)(port - s->pbase) >> 1, (port & 1) ? (u16)(v << 8) : v,
                    (port & 1) ? 0xff00 : 0x00ff);
        return;
    }
    tp_dac_w(s, port >> 1, (port & 1) ? (u16)(v << 8) : v, (port & 1) ? 0xff00 : 0x00ff);
}

static void tp_out16(tp_sound *s, u16 port, u16 v)
{
    if (tp_in_internal_io(s, port)) {
        tp_internal_w(s, (port >> 1) & 0x7f, (port & 1) ? (u16)((v >> 8) | (v << 8)) : v);
        return;
    }
    if (!(port & 1)) {
        if (tp_in_periph(s, port, 0)) tp_periph_w(s, (u32)(port - s->pbase) >> 1, v, 0xffff);
        else                          tp_dac_w(s, port >> 1, v, 0xffff);
        return;
    }
    tp_out8(s, port, (u8)v);
    tp_out8(s, (u16)(port + 1), (u8)(v >> 8));
}

/* ------------------------------------------------------------------ */
/* The 80186's DMA (i186.cpp)                                          */
/* ------------------------------------------------------------------ */

static void tp_drq_callback(tp_sound *s, int which)
{
    tp_dma *d = &s->dma[which];
    int size;
    if (!(d->control & ST_STOP)) return;
    if (d->control & BYTE_WORD) {
        const u16 w = (d->control & SRC_MIO) ? tp_rd16(s, d->source) : tp_in16(s, (u16)d->source);
        if (d->control & DEST_MIO) tp_wr16(s, d->dest, w); else tp_out16(s, (u16)d->dest, w);
        size = 2;
    } else {
        const u8 b = (d->control & SRC_MIO) ? tp_rd8(s, d->source) : tp_in8(s, (u16)d->source);
        if (d->control & DEST_MIO) tp_wr8(s, d->dest, b); else tp_out8(s, (u16)d->dest, b);
        size = 1;
    }
    switch (d->control & DEST_INCDEC_MASK) {
    case DEST_DECREMENT: d->dest -= (u32)size; break;
    case DEST_INCREMENT: d->dest += (u32)size; break;
    }
    switch (d->control & SRC_INCDEC_MASK) {
    case SRC_DECREMENT: d->source -= (u32)size; break;
    case SRC_INCREMENT: d->source += (u32)size; break;
    }
    d->count--;
    if (((d->control & TERMINATE_ON_ZERO) || !(d->control & SYNC_MASK)) && d->count == 0)
        d->control &= (u16)~ST_STOP;
    if ((d->control & INTERRUPT_ON_ZERO) && d->count == 0) {
        s->intr.request |= (u16)(0x04 << which);
        tp_update_interrupt_state(s);
    }
}

static void tp_update_dma_control(tp_sound *s, int which, u16 new_control)
{
    tp_dma *d = &s->dma[which];
    if (!(new_control & CHG_NOCHG)) new_control = (u16)((new_control & ~ST_STOP) | (d->control & ST_STOP));
    new_control &= (u16)~CHG_NOCHG;
    d->control = new_control;
}

/* ------------------------------------------------------------------ */
/* The 80186's register block (i186.cpp internal_port_r/w)             */
/* ------------------------------------------------------------------ */

static void tp_chip_select(tp_sound *s, int offset, u16 data)
{
    /* leland_80186_sound_device::peripheral_ctrl */
    if (offset == 2) s->pacs = data;
    else if (offset == 4) {
        const u32 base = (u32)(s->pacs & 0xffc0) << 4;
        s->pmapped = 1;
        if (data & 0x0040) { s->pmem = 1; s->pbase = base; }
        else               { s->pmem = 0; s->pbase = base & 0xffff; }
    }
}

static u16 tp_internal_r(tp_sound *s, u32 offset)
{
    int temp, which;
    switch (offset) {
    case 0x10: return s->intr.vector;
    case 0x12:
        if (s->intr.poll_status & 0x8000) tp_int_ack(s);
        return s->intr.poll_status;
    case 0x13: return s->intr.poll_status;
    case 0x14:
        temp  = (s->intr.timer >> 3) & 0x01;
        temp |= (s->intr.dma[0] >> 1) & 0x04;
        temp |= (s->intr.dma[1] >> 0) & 0x08;
        temp |= (s->intr.ext[0] << 1) & 0x10;
        temp |= (s->intr.ext[1] << 2) & 0x20;
        temp |= (s->intr.ext[2] << 3) & 0x40;
        temp |= (s->intr.ext[3] << 4) & 0x80;
        return (u16)temp;
    case 0x15: return s->intr.priority_mask;
    case 0x16: return s->intr.in_service;
    case 0x17:
        temp = s->intr.request & ~0x0001;
        if (s->intr.status & 0x0007) temp |= 1;
        return (u16)temp;
    case 0x18: return s->intr.status;
    case 0x19: return s->intr.timer;
    case 0x1a: return s->intr.dma[0];
    case 0x1b: return s->intr.dma[1];
    case 0x1c: return s->intr.ext[0];
    case 0x1d: return s->intr.ext[1];
    case 0x1e: return s->intr.ext[2];
    case 0x1f: return s->intr.ext[3];
    case 0x28: case 0x2c: case 0x30:
        which = (int)(offset - 0x28) / 4;
        tp_timer_sync(s, which);
        return s->timer[which].count;
    case 0x29: case 0x2d: case 0x31: return s->timer[(offset - 0x29) / 4].maxA;
    case 0x2a: case 0x2e:            return s->timer[(offset - 0x2a) / 4].maxB;
    case 0x2b: case 0x2f: case 0x33: return s->timer[(offset - 0x2b) / 4].control;
    case 0x50: return s->mem.upper;
    case 0x51: return s->mem.lower;
    case 0x52: return s->mem.peripheral;
    case 0x53: return s->mem.middle;
    case 0x54: return s->mem.middle_size;
    case 0x60: case 0x68: return (u16)s->dma[(offset - 0x60) / 8].source;
    case 0x61: case 0x69: return (u16)(s->dma[(offset - 0x61) / 8].source >> 16);
    case 0x62: case 0x6a: return (u16)s->dma[(offset - 0x62) / 8].dest;
    case 0x63: case 0x6b: return (u16)(s->dma[(offset - 0x63) / 8].dest >> 16);
    case 0x64: case 0x6c: return s->dma[(offset - 0x64) / 8].count;
    case 0x65: case 0x6d: return s->dma[(offset - 0x65) / 8].control;
    case 0x7f: return s->reloc;
    }
    return 0;
}

static void tp_internal_w(tp_sound *s, u32 offset, u16 data)
{
    int which;
    switch (offset) {
    case 0x10: s->intr.vector = data & 0xf8; break;
    case 0x11: tp_handle_eoi(s, data); tp_update_interrupt_state(s); break;
    case 0x14:
        s->intr.timer  = (u16)((s->intr.timer  & ~0x08) | ((data << 3) & 0x08));
        s->intr.dma[0] = (u16)((s->intr.dma[0] & ~0x08) | ((data << 1) & 0x08));
        s->intr.dma[1] = (u16)((s->intr.dma[1] & ~0x08) | ((data << 0) & 0x08));
        s->intr.ext[0] = (u16)((s->intr.ext[0] & ~0x08) | ((data >> 1) & 0x08));
        s->intr.ext[1] = (u16)((s->intr.ext[1] & ~0x08) | ((data >> 2) & 0x08));
        s->intr.ext[2] = (u16)((s->intr.ext[2] & ~0x08) | ((data >> 3) & 0x08));
        s->intr.ext[3] = (u16)((s->intr.ext[3] & ~0x08) | ((data >> 4) & 0x08));
        tp_update_interrupt_state(s);
        break;
    case 0x15: s->intr.priority_mask = data & 0x0007; tp_update_interrupt_state(s); break;
    case 0x16: s->intr.in_service = data & 0x00ff;   tp_update_interrupt_state(s); break;
    case 0x17:
        s->intr.request = (u16)((s->intr.request & ~0x000c) | (data & 0x000c));
        tp_update_interrupt_state(s);
        break;
    case 0x18:
        s->intr.status = (u16)((s->intr.status & ~0x8007) | (data & 0x8007));
        tp_update_interrupt_state(s);
        break;
    case 0x19: s->intr.timer  = data & 0x000f; tp_update_interrupt_state(s); break;
    case 0x1a: s->intr.dma[0] = data & 0x000f; tp_update_interrupt_state(s); break;
    case 0x1b: s->intr.dma[1] = data & 0x000f; tp_update_interrupt_state(s); break;
    case 0x1c: s->intr.ext[0] = data & 0x007f; tp_update_interrupt_state(s); break;
    case 0x1d: s->intr.ext[1] = data & 0x007f; tp_update_interrupt_state(s); break;
    case 0x1e: s->intr.ext[2] = data & 0x001f; tp_update_interrupt_state(s); break;
    case 0x1f: s->intr.ext[3] = data & 0x001f; tp_update_interrupt_state(s); break;
    case 0x28: case 0x2c: case 0x30: tp_timer_update(s, (int)(offset - 0x28) / 4, data, -1, -1, -1); break;
    case 0x29: case 0x2d: case 0x31: tp_timer_update(s, (int)(offset - 0x29) / 4, -1, data, -1, -1); break;
    case 0x2a: case 0x2e:            tp_timer_update(s, (int)(offset - 0x2a) / 4, -1, -1, data, -1); break;
    case 0x2b: case 0x2f: case 0x33: tp_timer_update(s, (int)(offset - 0x2b) / 4, -1, -1, -1, data); break;
    case 0x50: s->mem.upper = data | 0xc038; break;
    case 0x51: s->mem.lower = (u16)((data & 0x3fff) | 0x0038); break;
    case 0x52: s->mem.peripheral = data | 0x0038; tp_chip_select(s, 2, s->mem.peripheral); break;
    case 0x53: s->mem.middle = data | 0x01f8; break;
    case 0x54: s->mem.middle_size = data | 0x8038; tp_chip_select(s, 4, s->mem.middle_size); break;
    case 0x60: case 0x68:
        which = (int)(offset - 0x60) / 8;
        s->dma[which].source = (s->dma[which].source & ~0x0ffffu) | data;
        break;
    case 0x61: case 0x69:
        which = (int)(offset - 0x61) / 8;
        s->dma[which].source = (s->dma[which].source & ~0xf0000u) | (((u32)data << 16) & 0xf0000u);
        break;
    case 0x62: case 0x6a:
        which = (int)(offset - 0x62) / 8;
        s->dma[which].dest = (s->dma[which].dest & ~0x0ffffu) | data;
        break;
    case 0x63: case 0x6b:
        which = (int)(offset - 0x63) / 8;
        s->dma[which].dest = (s->dma[which].dest & ~0xf0000u) | (((u32)data << 16) & 0xf0000u);
        break;
    case 0x64: case 0x6c: s->dma[(offset - 0x64) / 8].count = data; break;
    case 0x65: case 0x6d:
        which = (int)(offset - 0x65) / 8;
        tp_update_dma_control(s, which, data);
        if ((s->dma[which].control & (SYNC_MASK | ST_STOP | TIMER_DRQ)) == ST_STOP)
            while (s->dma[which].control & ST_STOP) tp_drq_callback(s, which);
        break;
    case 0x7f: s->reloc = data; break;
    }
}

/* ------------------------------------------------------------------ */
/* The CPU: an 8086 with the 80186's additions                         */
/* ------------------------------------------------------------------ */

static u8 tp_parity[256];

#define LIN(seg, off) ((((u32)(seg) << 4) + (u16)(off)) & 0xfffff)

static u8 fetch8(tp_sound *s)  { const u8 v = tp_rd8(s, LIN(s->sr[CS], s->ip)); s->ip++; return v; }
static u16 fetch16(tp_sound *s)
{
    const u16 v = (u16)(fetch8(s));
    return (u16)(v | (fetch8(s) << 8));
}

static u8 rb(tp_sound *s, int i)  { return (u8)(i < 4 ? s->r[i] : s->r[i - 4] >> 8); }
static void wb(tp_sound *s, int i, u8 v)
{
    if (i < 4) s->r[i] = (u16)((s->r[i] & 0xff00) | v);
    else       s->r[i - 4] = (u16)((s->r[i - 4] & 0x00ff) | (v << 8));
}

static u16 get_flags(const tp_sound *s)
{
    return (u16)(0xf002 | (s->cf ? 1 : 0) | (tp_parity[s->pfv] ? 4 : 0) | (s->af ? 0x10 : 0) |
                 (s->zf ? 0x40 : 0) | (s->sf ? 0x80 : 0) | (s->tf ? 0x100 : 0) |
                 (s->ifl ? 0x200 : 0) | (s->df ? 0x400 : 0) | (s->of ? 0x800 : 0));
}

static void set_flags(tp_sound *s, u16 f)
{
    s->cf = f & 1; s->pfv = (f & 4) ? 0 : 1;   /* parity table: 0 is even */
    s->af = (f >> 4) & 1; s->zf = (f >> 6) & 1; s->sf = (f >> 7) & 1;
    s->tf = (f >> 8) & 1; s->ifl = (f >> 9) & 1; s->df = (f >> 10) & 1; s->of = (f >> 11) & 1;
}

static void push(tp_sound *s, u16 v)
{
    s->r[SP] = (u16)(s->r[SP] - 2);
    tp_wr16(s, LIN(s->sr[SS], s->r[SP]), v);
}

static u16 pop(tp_sound *s)
{
    const u16 v = tp_rd16(s, LIN(s->sr[SS], s->r[SP]));
    s->r[SP] = (u16)(s->r[SP] + 2);
    return v;
}

static void tp_interrupt(tp_sound *s, int n)
{
    push(s, get_flags(s));
    s->ifl = 0; s->tf = 0;
    push(s, s->sr[CS]);
    push(s, s->ip);
    s->ip = tp_rd16(s, (u32)n * 4);
    s->sr[CS] = tp_rd16(s, (u32)n * 4 + 2);
}

/* THE CYCLES each instruction costs, as MAME 0.251 charges them -- the
 * reference this is checked against.  i186.cpp's m_i80186_timing (from
 * the 80186 data sheet), indexed as i86.h's enum, plus the 8086's cost
 * for working out a memory operand's address (i86inline.h get_ea), which
 * MAME charges the 80186 too.  How fast the program runs decides how many
 * of the DACs' clock ticks its polling loop catches, so it shows in the
 * sound. */
enum {
    T_EXCEPTION, T_IRET, T_INT3, T_INT_IMM, T_INTO_NT, T_INTO_T, T_OVERRIDE,
    T_FLAG_OPS, T_LAHF, T_SAHF, T_AAA, T_AAS, T_AAM, T_AAD, T_DAA, T_DAS,
    T_CBW, T_CWD, T_HLT, T_LOAD_PTR, T_LEA, T_NOP, T_WAIT, T_XLAT,
    T_JMP_SHORT, T_JMP_NEAR, T_JMP_FAR, T_JMP_R16, T_JMP_M16, T_JMP_M32,
    T_CALL_NEAR, T_CALL_FAR, T_CALL_R16, T_CALL_M16, T_CALL_M32,
    T_RET_NEAR, T_RET_FAR, T_RET_NEAR_IMM, T_RET_FAR_IMM,
    T_JCC_NT, T_JCC_T, T_JCXZ_NT, T_JCXZ_T, T_LOOP_NT, T_LOOP_T, T_LOOPE_NT, T_LOOPE_T,
    T_IN_IMM8, T_IN_IMM16, T_IN_DX8, T_IN_DX16, T_OUT_IMM8, T_OUT_IMM16, T_OUT_DX8, T_OUT_DX16,
    T_MOV_RR8, T_MOV_RM8, T_MOV_MR8, T_MOV_RI8, T_MOV_MI8,
    T_MOV_RR16, T_MOV_RM16, T_MOV_MR16, T_MOV_RI16, T_MOV_MI16,
    T_MOV_AM8, T_MOV_AM16, T_MOV_MA8, T_MOV_MA16, T_MOV_SR, T_MOV_SM, T_MOV_RS, T_MOV_MS,
    T_XCHG_RR8, T_XCHG_RM8, T_XCHG_RR16, T_XCHG_RM16, T_XCHG_AR16,
    T_PUSH_R16, T_PUSH_M16, T_PUSH_SEG, T_PUSHF, T_POP_R16, T_POP_M16, T_POP_SEG, T_POPF,
    T_ALU_RR8, T_ALU_RM8, T_ALU_MR8, T_ALU_RI8, T_ALU_MI8, T_ALU_MI8_RO,
    T_ALU_RR16, T_ALU_RM16, T_ALU_MR16, T_ALU_RI16, T_ALU_MI16, T_ALU_MI16_RO,
    T_ALU_R16I8, T_ALU_M16I8, T_ALU_M16I8_RO,
    T_MUL_R8, T_MUL_R16, T_MUL_M8, T_MUL_M16, T_IMUL_R8, T_IMUL_R16, T_IMUL_M8, T_IMUL_M16,
    T_DIV_R8, T_DIV_R16, T_DIV_M8, T_DIV_M16, T_IDIV_R8, T_IDIV_R16, T_IDIV_M8, T_IDIV_M16,
    T_INCDEC_R8, T_INCDEC_R16, T_INCDEC_M8, T_INCDEC_M16, T_NEGNOT_R8, T_NEGNOT_R16, T_NEGNOT_M8, T_NEGNOT_M16,
    T_ROT_REG_1, T_ROT_REG_BASE, T_ROT_REG_BIT, T_ROT_M8_1, T_ROT_M8_BASE, T_ROT_M8_BIT,
    T_ROT_M16_1, T_ROT_M16_BASE, T_ROT_M16_BIT,
    T_CMPS8, T_REP_CMPS8_BASE, T_REP_CMPS8_COUNT, T_CMPS16, T_REP_CMPS16_BASE, T_REP_CMPS16_COUNT,
    T_SCAS8, T_REP_SCAS8_BASE, T_REP_SCAS8_COUNT, T_SCAS16, T_REP_SCAS16_BASE, T_REP_SCAS16_COUNT,
    T_LODS8, T_REP_LODS8_BASE, T_REP_LODS8_COUNT, T_LODS16, T_REP_LODS16_BASE, T_REP_LODS16_COUNT,
    T_STOS8, T_REP_STOS8_BASE, T_REP_STOS8_COUNT, T_STOS16, T_REP_STOS16_BASE, T_REP_STOS16_COUNT,
    T_MOVS8, T_REP_MOVS8_BASE, T_REP_MOVS8_COUNT, T_MOVS16, T_REP_MOVS16_BASE, T_REP_MOVS16_COUNT,
    T_INS8, T_REP_INS8_BASE, T_REP_INS8_COUNT, T_INS16, T_REP_INS16_BASE, T_REP_INS16_COUNT,
    T_OUTS8, T_REP_OUTS8_BASE, T_REP_OUTS8_COUNT, T_OUTS16, T_REP_OUTS16_BASE, T_REP_OUTS16_COUNT,
    T_PUSH_IMM, T_PUSHA, T_POPA, T_IMUL_RRI8, T_IMUL_RMI8, T_IMUL_RRI16, T_IMUL_RMI16,
    T_ENTER0, T_ENTER1, T_ENTER_BASE, T_ENTER_COUNT, T_LEAVE, T_BOUND
};

static const u8 tp_timing[] = {
    45, 28,  0, 2, 4, 3,  2,  2, 2, 3,  8, 7, 19, 15,  4, 4,  2, 4,
    2, 18, 6, 2, 6, 11,
    14, 14, 14,  11, 17, 26,  15, 23,  13, 19, 38,  16, 22, 18, 25,
    4, 13, 5, 15,  6, 16, 6, 16,
    10, 10, 8, 8,  9, 9, 7, 7,
    2, 9, 12,  3, 12,  2, 9, 12,  4, 13,  8, 8, 9, 9,  2, 11, 2, 11,
    4, 17,  4, 17, 3,
    10, 16, 9, 9,  10, 20, 8, 8,
    3, 10, 10,  4, 16, 10,  3, 10, 10,  4, 16, 10,  4, 16, 10,
    26, 35, 32, 41,  25, 34, 31, 40,  29, 38, 35, 44,  44, 53, 50, 59,
    3, 3, 15, 15,  3, 3, 10, 10,
    2, 5, 1,  15, 17, 1,  15, 17, 1,
    22, 5, 22,  22, 5, 22,  15, 5, 15,  15, 5, 15,  12, 6, 11,  12, 6, 11,
    10, 6, 9,  10, 6, 9,  14, 8, 8,  14, 8, 8,
    14, 8, 8,  14, 8, 8,  14, 8, 8,  14, 8, 8,
    14, 68, 83,  22, 29,  25, 32,  15, 25, 4, 16, 8,  33
};

#define CLK(t)        (s->cyc += tp_timing[t])
#define CLKM(m, r, mm) (s->cyc += tp_timing[(m)->mod == 3 ? (r) : (mm)])

/* the ModRM operand; its address costs what get_ea charges */
typedef struct { int mod, reg, rm; u32 ea; u16 off; } modrm;

static const u8 tp_ea_cost[3][8] = {
    { 7, 8, 8, 7, 5, 5, 6, 5 },
    { 11, 12, 12, 11, 9, 9, 9, 9 },
    { 11, 12, 11, 11, 9, 9, 9, 9 }
};

static void decode_modrm(tp_sound *s, modrm *m)
{
    const u8 b = fetch8(s);
    u16 off = 0;
    int seg = DS;
    m->mod = b >> 6; m->reg = (b >> 3) & 7; m->rm = b & 7;
    if (m->mod == 3) return;
    s->cyc += tp_ea_cost[m->mod][m->rm];
    switch (m->rm) {
    case 0: off = (u16)(s->r[BX] + s->r[SI]); break;
    case 1: off = (u16)(s->r[BX] + s->r[DI]); break;
    case 2: off = (u16)(s->r[BP] + s->r[SI]); seg = SS; break;
    case 3: off = (u16)(s->r[BP] + s->r[DI]); seg = SS; break;
    case 4: off = s->r[SI]; break;
    case 5: off = s->r[DI]; break;
    case 6:
        if (m->mod == 0) off = fetch16(s);
        else { off = s->r[BP]; seg = SS; }
        break;
    case 7: off = s->r[BX]; break;
    }
    if (m->mod == 1)      off = (u16)(off + (signed char)fetch8(s));
    else if (m->mod == 2) off = (u16)(off + fetch16(s));
    m->off = off;
    m->ea = LIN(s->seg_ovr ? s->seg_val : s->sr[seg], off);
}

static u8 rm8(tp_sound *s, const modrm *m)  { return m->mod == 3 ? rb(s, m->rm) : tp_rd8(s, m->ea); }
static u16 rm16(tp_sound *s, const modrm *m) { return m->mod == 3 ? s->r[m->rm] : tp_rd16(s, m->ea); }
static void wm8(tp_sound *s, const modrm *m, u8 v)  { if (m->mod == 3) wb(s, m->rm, v); else tp_wr8(s, m->ea, v); }
static void wm16(tp_sound *s, const modrm *m, u16 v) { if (m->mod == 3) s->r[m->rm] = v; else tp_wr16(s, m->ea, v); }

/* the ALU: 0 ADD, 1 OR, 2 ADC, 3 SBB, 4 AND, 5 SUB, 6 XOR, 7 CMP */
static u32 alu(tp_sound *s, int op, u32 a, u32 b, int w)
{
    const u32 mask = w ? 0xffff : 0xff, sign = w ? 0x8000 : 0x80;
    u32 r = 0, c;
    switch (op) {
    case 0: case 2:
        c = (op == 2 && s->cf) ? 1 : 0;
        r = a + b + c;
        s->cf = r > mask;
        s->of = ((r ^ a) & (r ^ b) & sign) != 0;
        s->af = ((a ^ b ^ r) & 0x10) != 0;
        break;
    case 3: case 5: case 7:
        c = (op == 3 && s->cf) ? 1 : 0;
        r = a - b - c;
        s->cf = (a < b + c);
        s->of = ((a ^ b) & (a ^ r) & sign) != 0;
        s->af = ((a ^ b ^ r) & 0x10) != 0;
        break;
    case 1: r = a | b; s->cf = s->of = 0; s->af = 0; break;
    case 4: r = a & b; s->cf = s->of = 0; s->af = 0; break;
    case 6: r = a ^ b; s->cf = s->of = 0; s->af = 0; break;
    }
    r &= mask;
    s->zf = r == 0;
    s->sf = (r & sign) != 0;
    s->pfv = (u8)r;
    return r;
}

static u32 incdec(tp_sound *s, u32 a, int dec, int w)
{
    const u32 mask = w ? 0xffff : 0xff, sign = w ? 0x8000 : 0x80;
    const u32 r = (dec ? a - 1 : a + 1) & mask;
    s->of = dec ? (a == sign) : (r == sign);
    s->af = ((a ^ r) & 0x10) != 0;
    s->zf = r == 0; s->sf = (r & sign) != 0; s->pfv = (u8)r;
    return r;
}

static void szp(tp_sound *s, u32 r, int w)
{
    s->zf = (r & (w ? 0xffff : 0xff)) == 0;
    s->sf = (r & (w ? 0x8000 : 0x80)) != 0;
    s->pfv = (u8)r;
}

/* shifts and rotates: 0 ROL, 1 ROR, 2 RCL, 3 RCR, 4/6 SHL, 5 SHR, 7 SAR */
static u32 shift(tp_sound *s, int op, u32 v, int count, int w)
{
    const int bits = w ? 16 : 8;
    const u32 mask = w ? 0xffff : 0xff, sign = w ? 0x8000 : 0x80;
    if (!count) return v;
    switch (op) {
    case 0:
        while (count--) { s->cf = (v & sign) != 0; v = ((v << 1) | (u32)s->cf) & mask; }
        s->of = ((v & sign) != 0) ^ s->cf;
        break;
    case 1:
        while (count--) { s->cf = v & 1; v = (v >> 1) | (s->cf ? sign : 0); }
        s->of = ((v ^ (v << 1)) & sign) != 0;
        break;
    case 2:
        while (count--) { const int c = s->cf; s->cf = (v & sign) != 0; v = ((v << 1) | (u32)c) & mask; }
        s->of = ((v & sign) != 0) ^ s->cf;
        break;
    case 3:
        while (count--) { const int c = s->cf; s->cf = v & 1; v = (v >> 1) | (c ? sign : 0); }
        s->of = ((v ^ (v << 1)) & sign) != 0;
        break;
    case 4: case 6:
        if (count > bits) { s->cf = 0; v = 0; }
        else { s->cf = (v >> (bits - count)) & 1; v = (v << count) & mask; }
        s->of = ((v & sign) != 0) ^ s->cf;
        s->af = 1;
        szp(s, v, w);
        break;
    case 5:
        if (count > bits) { s->cf = 0; s->of = 0; v = 0; }
        else { s->of = (v & sign) != 0; s->cf = (v >> (count - 1)) & 1; v >>= count; }
        s->af = 1;
        szp(s, v, w);
        break;
    case 7: {
        long sv = w ? (long)(short)v : (long)(signed char)v;
        if (count >= bits) count = bits;
        s->cf = (int)((sv >> (count - 1)) & 1);
        sv >>= count;
        v = (u32)sv & mask;
        s->of = 0; s->af = 1;
        szp(s, v, w);
        break;
    }
    }
    return v;
}

static int cond(tp_sound *s, int c)
{
    int r;
    switch (c >> 1) {
    case 0: r = s->of; break;
    case 1: r = s->cf; break;
    case 2: r = s->zf; break;
    case 3: r = s->cf || s->zf; break;
    case 4: r = s->sf; break;
    case 5: r = tp_parity[s->pfv]; break;
    case 6: r = s->sf != s->of; break;
    default: r = s->zf || (s->sf != s->of); break;
    }
    return (c & 1) ? !r : r;
}

/* DIV/IDIV fault: interrupt 0 */
static void tp_div0(tp_sound *s) { tp_interrupt(s, 0); }

/* group 3 (F6/F7): TEST, NOT, NEG, MUL, IMUL, DIV, IDIV */
static void grp3(tp_sound *s, int w)
{
    modrm m;
    decode_modrm(s, &m);
    if (!w) {
        const u8 v = rm8(s, &m);
        switch (m.reg) {
        case 0: case 1: alu(s, 4, v, fetch8(s), 0); CLKM(&m, T_ALU_RI8, T_ALU_MI8_RO); return;
        case 2: wm8(s, &m, (u8)~v); CLKM(&m, T_NEGNOT_R8, T_NEGNOT_M8); return;
        case 3: { const u8 r = (u8)alu(s, 5, 0, v, 0); s->cf = v != 0; wm8(s, &m, r);
                  CLKM(&m, T_NEGNOT_R8, T_NEGNOT_M8); return; }
        case 4: { const u16 r = (u16)((s->r[AX] & 0xff) * v); s->r[AX] = r;
                  s->cf = s->of = (r & 0xff00) != 0; s->zf = r == 0;
                  CLKM(&m, T_MUL_R8, T_MUL_M8); return; }
        case 5: { const short r = (short)((signed char)(s->r[AX] & 0xff) * (signed char)v);
                  s->r[AX] = (u16)r; s->cf = s->of = (r != (signed char)r);
                  CLKM(&m, T_IMUL_R8, T_IMUL_M8); return; }
        case 6: {
            const u16 a = s->r[AX];
            u16 q;
            CLKM(&m, T_DIV_R8, T_DIV_M8);
            if (!v) { tp_div0(s); return; }
            q = (u16)(a / v);
            if (q > 0xff) { tp_div0(s); return; }
            s->r[AX] = (u16)(((a % v) << 8) | q);
            return;
        }
        case 7: {
            const short a = (short)s->r[AX];
            int q;
            CLKM(&m, T_IDIV_R8, T_IDIV_M8);
            if (!v) { tp_div0(s); return; }
            q = a / (signed char)v;
            if (q > 127 || q < -128) { tp_div0(s); return; }
            s->r[AX] = (u16)((((a % (signed char)v) & 0xff) << 8) | (q & 0xff));
            return;
        }
        }
    } else {
        const u16 v = rm16(s, &m);
        switch (m.reg) {
        case 0: case 1: alu(s, 4, v, fetch16(s), 1); CLKM(&m, T_ALU_RI16, T_ALU_MI16_RO); return;
        case 2: wm16(s, &m, (u16)~v); CLKM(&m, T_NEGNOT_R16, T_NEGNOT_M16); return;
        case 3: { const u16 r = (u16)alu(s, 5, 0, v, 1); s->cf = v != 0; wm16(s, &m, r);
                  CLKM(&m, T_NEGNOT_R16, T_NEGNOT_M16); return; }
        case 4: { const u32 r = (u32)s->r[AX] * v; s->r[AX] = (u16)r; s->r[DX] = (u16)(r >> 16);
                  s->cf = s->of = (r >> 16) != 0; s->zf = r == 0;
                  CLKM(&m, T_MUL_R16, T_MUL_M16); return; }
        case 5: { const long r = (long)(short)s->r[AX] * (short)v;
                  s->r[AX] = (u16)r; s->r[DX] = (u16)((u32)r >> 16);
                  s->cf = s->of = (r != (short)r);
                  CLKM(&m, T_IMUL_R16, T_IMUL_M16); return; }
        case 6: {
            const u32 a = ((u32)s->r[DX] << 16) | s->r[AX];
            u32 q;
            CLKM(&m, T_DIV_R16, T_DIV_M16);
            if (!v) { tp_div0(s); return; }
            q = a / v;
            if (q > 0xffff) { tp_div0(s); return; }
            s->r[DX] = (u16)(a % v); s->r[AX] = (u16)q;
            return;
        }
        case 7: {
            const long a = (long)(int)(((u32)s->r[DX] << 16) | s->r[AX]);
            long q;
            CLKM(&m, T_IDIV_R16, T_IDIV_M16);
            if (!v) { tp_div0(s); return; }
            q = a / (short)v;
            if (q > 32767 || q < -32768) { tp_div0(s); return; }
            s->r[DX] = (u16)(a % (short)v); s->r[AX] = (u16)q;
            return;
        }
        }
    }
}

/* the string instructions; with REP each pass costs what one costs, and
 * the prefix an OVERRIDE (i86.cpp's i_repne/i_repe) */
static void string_op(tp_sound *s, u8 op)
{
    static const u8 cost[16] = {
        0, 0, 0, 0, T_MOVS8, T_MOVS16, T_CMPS8, T_CMPS16,
        0, 0, T_STOS8, T_STOS16, T_LODS8, T_LODS16, T_SCAS8, T_SCAS16
    };
    const int w = op & 1, step = (s->df ? -1 : 1) * (w ? 2 : 1);
    const u16 srcseg = s->seg_ovr ? s->seg_val : s->sr[DS];
    const int t = (op >= 0xa4) ? cost[op & 0x0f] : (op < 0x6e ? (w ? T_INS16 : T_INS8) : (w ? T_OUTS16 : T_OUTS8));
    if (s->rep) CLK(T_OVERRIDE);
    for (;;) {
        if (s->rep && s->r[CX] == 0) break;
        switch (op) {
        case 0xa4: case 0xa5:
            if (w) tp_wr16(s, LIN(s->sr[ES], s->r[DI]), tp_rd16(s, LIN(srcseg, s->r[SI])));
            else   tp_wr8(s, LIN(s->sr[ES], s->r[DI]), tp_rd8(s, LIN(srcseg, s->r[SI])));
            s->r[SI] = (u16)(s->r[SI] + step); s->r[DI] = (u16)(s->r[DI] + step);
            break;
        case 0xa6: case 0xa7:
            if (w) alu(s, 7, tp_rd16(s, LIN(srcseg, s->r[SI])), tp_rd16(s, LIN(s->sr[ES], s->r[DI])), 1);
            else   alu(s, 7, tp_rd8(s, LIN(srcseg, s->r[SI])), tp_rd8(s, LIN(s->sr[ES], s->r[DI])), 0);
            s->r[SI] = (u16)(s->r[SI] + step); s->r[DI] = (u16)(s->r[DI] + step);
            break;
        case 0xaa: case 0xab:
            if (w) tp_wr16(s, LIN(s->sr[ES], s->r[DI]), s->r[AX]);
            else   tp_wr8(s, LIN(s->sr[ES], s->r[DI]), (u8)s->r[AX]);
            s->r[DI] = (u16)(s->r[DI] + step);
            break;
        case 0xac: case 0xad:
            if (w) s->r[AX] = tp_rd16(s, LIN(srcseg, s->r[SI]));
            else   wb(s, 0, tp_rd8(s, LIN(srcseg, s->r[SI])));
            s->r[SI] = (u16)(s->r[SI] + step);
            break;
        case 0xae: case 0xaf:
            if (w) alu(s, 7, s->r[AX], tp_rd16(s, LIN(s->sr[ES], s->r[DI])), 1);
            else   alu(s, 7, s->r[AX] & 0xff, tp_rd8(s, LIN(s->sr[ES], s->r[DI])), 0);
            s->r[DI] = (u16)(s->r[DI] + step);
            break;
        case 0x6c: case 0x6d:
            if (w) tp_wr16(s, LIN(s->sr[ES], s->r[DI]), tp_in16(s, s->r[DX]));
            else   tp_wr8(s, LIN(s->sr[ES], s->r[DI]), tp_in8(s, s->r[DX]));
            s->r[DI] = (u16)(s->r[DI] + step);
            break;
        case 0x6e: case 0x6f:
            if (w) tp_out16(s, s->r[DX], tp_rd16(s, LIN(srcseg, s->r[SI])));
            else   tp_out8(s, s->r[DX], tp_rd8(s, LIN(srcseg, s->r[SI])));
            s->r[SI] = (u16)(s->r[SI] + step);
            break;
        }
        CLK(t);
        if (!s->rep) break;
        s->r[CX]--;
        if ((op & 0xf6) == 0xa6) {             /* CMPS, SCAS: REPE/REPNE */
            if (s->rep == 1 && !s->zf) break;
            if (s->rep == 2 && s->zf) break;
        }
    }
}

/* one instruction; the cycles it took */
static int tp_step(tp_sound *s)
{
    u8 op;
    modrm m;
    u32 a, b;

    s->cyc = 0;
    s->seg_ovr = 0;
    s->rep = 0;
    for (;;) {
        op = fetch8(s);
        switch (op) {
        case 0x26: s->seg_ovr = 1; s->seg_val = s->sr[ES]; CLK(T_OVERRIDE); continue;
        case 0x2e: s->seg_ovr = 1; s->seg_val = s->sr[CS]; CLK(T_OVERRIDE); continue;
        case 0x36: s->seg_ovr = 1; s->seg_val = s->sr[SS]; CLK(T_OVERRIDE); continue;
        case 0x3e: s->seg_ovr = 1; s->seg_val = s->sr[DS]; CLK(T_OVERRIDE); continue;
        case 0xf2: s->rep = 2; continue;
        case 0xf3: s->rep = 1; continue;
        case 0xf0: case 0xf1: CLK(T_NOP); continue;   /* LOCK */
        }
        break;
    }
    s->insns++;

    if (op < 0x40 && (op & 7) < 6) {           /* the ALU, 00-3D */
        const int alop = op >> 3;
        switch (op & 7) {
        case 0: decode_modrm(s, &m); a = alu(s, alop, rm8(s, &m), rb(s, m.reg), 0);
                if (alop != 7) wm8(s, &m, (u8)a);
                if (alop == 7) CLKM(&m, T_ALU_RR8, T_ALU_RM8); else CLKM(&m, T_ALU_RR8, T_ALU_MR8);
                return s->cyc;
        case 1: decode_modrm(s, &m); a = alu(s, alop, rm16(s, &m), s->r[m.reg], 1);
                if (alop != 7) wm16(s, &m, (u16)a);
                /* MAME charges XOR and CMP as register-from-memory */
                if (alop == 7 || alop == 6) CLKM(&m, T_ALU_RR16, T_ALU_RM16); else CLKM(&m, T_ALU_RR16, T_ALU_MR16);
                return s->cyc;
        case 2: decode_modrm(s, &m); a = alu(s, alop, rb(s, m.reg), rm8(s, &m), 0);
                if (alop != 7) wb(s, m.reg, (u8)a);
                CLKM(&m, T_ALU_RR8, T_ALU_RM8); return s->cyc;
        case 3: decode_modrm(s, &m); a = alu(s, alop, s->r[m.reg], rm16(s, &m), 1);
                if (alop != 7) s->r[m.reg] = (u16)a;
                CLKM(&m, T_ALU_RR16, T_ALU_RM16); return s->cyc;
        case 4: a = alu(s, alop, s->r[AX] & 0xff, fetch8(s), 0);
                if (alop != 7) wb(s, 0, (u8)a); CLK(T_ALU_RI8); return s->cyc;
        case 5: a = alu(s, alop, s->r[AX], fetch16(s), 1);
                if (alop != 7) s->r[AX] = (u16)a; CLK(T_ALU_RI16); return s->cyc;
        }
    }

    if (op >= 0x40 && op <= 0x4f) { s->r[op & 7] = (u16)incdec(s, s->r[op & 7], op & 8, 1); CLK(T_INCDEC_R16); return s->cyc; }
    if (op >= 0x50 && op <= 0x57) { push(s, s->r[op & 7]); CLK(T_PUSH_R16); return s->cyc; }
    if (op >= 0x58 && op <= 0x5f) { const u16 v = pop(s); s->r[op & 7] = v; CLK(T_POP_R16); return s->cyc; }
    if (op >= 0x70 && op <= 0x7f) {
        const signed char d = (signed char)fetch8(s);
        if (cond(s, op & 0x0f)) { s->ip = (u16)(s->ip + d); CLK(T_JCC_T); }
        else CLK(T_JCC_NT);
        return s->cyc;
    }
    if (op >= 0x91 && op <= 0x97) { const u16 t = s->r[AX]; s->r[AX] = s->r[op & 7]; s->r[op & 7] = t; CLK(T_XCHG_AR16); return s->cyc; }
    if (op >= 0xb0 && op <= 0xb7) { wb(s, op & 7, fetch8(s)); CLK(T_MOV_RI8); return s->cyc; }
    if (op >= 0xb8 && op <= 0xbf) { s->r[op & 7] = fetch16(s); CLK(T_MOV_RI16); return s->cyc; }

    switch (op) {
    case 0x06: push(s, s->sr[ES]); CLK(T_PUSH_SEG); break;
    case 0x07: s->sr[ES] = pop(s); CLK(T_POP_SEG); break;
    case 0x0e: push(s, s->sr[CS]); CLK(T_PUSH_SEG); break;
    case 0x16: push(s, s->sr[SS]); CLK(T_PUSH_SEG); break;
    case 0x17: s->sr[SS] = pop(s); s->no_int = 1; CLK(T_POP_SEG); break;
    case 0x1e: push(s, s->sr[DS]); CLK(T_PUSH_SEG); break;
    case 0x1f: s->sr[DS] = pop(s); CLK(T_POP_SEG); break;
    case 0x27: {                                      /* DAA */
        const u8 al = (u8)s->r[AX]; const int oc = s->cf;
        u8 r = al;
        if ((al & 0x0f) > 9 || s->af) { r = (u8)(r + 6); s->af = 1; } else s->af = 0;
        if (al > 0x99 || oc) { r = (u8)(r + 0x60); s->cf = 1; } else s->cf = 0;
        wb(s, 0, r); szp(s, r, 0); CLK(T_DAA); break;
    }
    case 0x2f: {                                      /* DAS */
        const u8 al = (u8)s->r[AX]; const int oc = s->cf;
        u8 r = al;
        if ((al & 0x0f) > 9 || s->af) { r = (u8)(r - 6); s->af = 1; } else s->af = 0;
        if (al > 0x99 || oc) { r = (u8)(r - 0x60); s->cf = 1; } else s->cf = 0;
        wb(s, 0, r); szp(s, r, 0); CLK(T_DAS); break;
    }
    case 0x37:                                        /* AAA */
        if ((s->r[AX] & 0x0f) > 9 || s->af) { s->r[AX] = (u16)(s->r[AX] + 0x106); s->af = s->cf = 1; }
        else s->af = s->cf = 0;
        s->r[AX] &= 0xff0f; CLK(T_AAA); break;
    case 0x3f:                                        /* AAS */
        if ((s->r[AX] & 0x0f) > 9 || s->af) {
            s->r[AX] = (u16)(s->r[AX] - 6); wb(s, 4, (u8)(rb(s, 4) - 1)); s->af = s->cf = 1;
        } else s->af = s->cf = 0;
        s->r[AX] &= 0xff0f; CLK(T_AAS); break;
    case 0x60: {                                      /* PUSHA */
        const u16 sp = s->r[SP];
        push(s, s->r[AX]); push(s, s->r[CX]); push(s, s->r[DX]); push(s, s->r[BX]);
        push(s, sp); push(s, s->r[BP]); push(s, s->r[SI]); push(s, s->r[DI]);
        CLK(T_PUSHA); break;
    }
    case 0x61:                                        /* POPA */
        s->r[DI] = pop(s); s->r[SI] = pop(s); s->r[BP] = pop(s); (void)pop(s);
        s->r[BX] = pop(s); s->r[DX] = pop(s); s->r[CX] = pop(s); s->r[AX] = pop(s);
        CLK(T_POPA); break;
    case 0x62: {                                      /* BOUND */
        short v, lo, hi;
        decode_modrm(s, &m);
        v = (short)s->r[m.reg];
        lo = (short)tp_rd16(s, m.ea); hi = (short)tp_rd16(s, (m.ea + 2) & 0xfffff);
        CLK(T_BOUND);
        if (v < lo || v > hi) tp_interrupt(s, 5);
        break;
    }
    case 0x68: push(s, fetch16(s)); CLK(T_PUSH_IMM); break;
    case 0x6a: push(s, (u16)(short)(signed char)fetch8(s)); CLK(T_PUSH_IMM); break;
    case 0x69: case 0x6b: {                           /* IMUL r, rm, imm */
        long r;
        decode_modrm(s, &m);
        a = rm16(s, &m);
        b = (op == 0x69) ? fetch16(s) : (u16)(short)(signed char)fetch8(s);
        r = (long)(short)a * (short)b;
        s->r[m.reg] = (u16)r;
        s->cf = s->of = (r != (short)r);
        if (op == 0x69) CLKM(&m, T_IMUL_RRI16, T_IMUL_RMI16); else CLKM(&m, T_IMUL_RRI8, T_IMUL_RMI8);
        break;
    }
    case 0x6c: case 0x6d: case 0x6e: case 0x6f: string_op(s, op); break;
    case 0x80: case 0x81: case 0x82: case 0x83: {
        const int w = op & 1;
        decode_modrm(s, &m);
        a = w ? rm16(s, &m) : rm8(s, &m);
        b = (op == 0x81) ? fetch16(s) : (op == 0x83) ? (u16)(short)(signed char)fetch8(s) : fetch8(s);
        a = alu(s, m.reg, a, b, w);
        if (m.reg != 7) { if (w) wm16(s, &m, (u16)a); else wm8(s, &m, (u8)a); }
        if (op == 0x81)
            s->cyc += tp_timing[m.mod == 3 ? T_ALU_RI16 : m.reg == 7 ? T_ALU_MI16_RO : T_ALU_MI16];
        else if (op == 0x83)
            s->cyc += tp_timing[m.mod == 3 ? T_ALU_R16I8 : m.reg == 7 ? T_ALU_M16I8_RO : T_ALU_M16I8];
        else
            s->cyc += tp_timing[m.mod == 3 ? T_ALU_RI8 : m.reg == 7 ? T_ALU_MI8_RO : T_ALU_MI8];
        break;
    }
    case 0x84: decode_modrm(s, &m); alu(s, 4, rm8(s, &m), rb(s, m.reg), 0); CLKM(&m, T_ALU_RR8, T_ALU_RM8); break;
    case 0x85: decode_modrm(s, &m); alu(s, 4, rm16(s, &m), s->r[m.reg], 1); CLKM(&m, T_ALU_RR16, T_ALU_RM16); break;
    case 0x86: { u8 t; decode_modrm(s, &m); t = rm8(s, &m); wm8(s, &m, rb(s, m.reg)); wb(s, m.reg, t);
                 CLKM(&m, T_XCHG_RR8, T_XCHG_RM8); break; }
    case 0x87: { u16 t; decode_modrm(s, &m); t = rm16(s, &m); wm16(s, &m, s->r[m.reg]); s->r[m.reg] = t;
                 CLKM(&m, T_XCHG_RR16, T_XCHG_RM16); break; }
    /* MAME charges the MOVs as ALU operations */
    case 0x88: decode_modrm(s, &m); wm8(s, &m, rb(s, m.reg)); CLKM(&m, T_ALU_RR8, T_ALU_MR8); break;
    case 0x89: decode_modrm(s, &m); wm16(s, &m, s->r[m.reg]); CLKM(&m, T_ALU_RR16, T_ALU_MR16); break;
    case 0x8a: decode_modrm(s, &m); wb(s, m.reg, rm8(s, &m)); CLKM(&m, T_ALU_RR8, T_ALU_RM8); break;
    case 0x8b: decode_modrm(s, &m); s->r[m.reg] = rm16(s, &m); CLKM(&m, T_ALU_RR16, T_ALU_RM16); break;
    case 0x8c: decode_modrm(s, &m); wm16(s, &m, s->sr[m.reg & 3]); CLKM(&m, T_MOV_RS, T_MOV_MS); break;
    case 0x8d: decode_modrm(s, &m); s->r[m.reg] = m.off; CLK(T_LEA); break;
    case 0x8e:
        decode_modrm(s, &m);
        s->sr[m.reg & 3] = rm16(s, &m);
        if ((m.reg & 3) == SS) s->no_int = 1;
        CLKM(&m, T_MOV_SR, T_MOV_SM);
        break;
    case 0x8f: { const u16 v = pop(s); decode_modrm(s, &m); wm16(s, &m, v); CLKM(&m, T_POP_R16, T_POP_M16); break; }
    case 0x90: CLK(T_NOP); break;
    case 0x98: s->r[AX] = (u16)(short)(signed char)s->r[AX]; CLK(T_CBW); break;
    case 0x99: s->r[DX] = (s->r[AX] & 0x8000) ? 0xffff : 0; CLK(T_CWD); break;
    case 0x9a: { const u16 ip = fetch16(s), cs = fetch16(s);
                 push(s, s->sr[CS]); push(s, s->ip); s->ip = ip; s->sr[CS] = cs; CLK(T_CALL_FAR); break; }
    case 0x9b: CLK(T_WAIT); break;
    case 0x9c: push(s, get_flags(s)); CLK(T_PUSHF); break;
    case 0x9d: set_flags(s, pop(s)); CLK(T_POPF); break;
    case 0x9e: set_flags(s, (u16)((get_flags(s) & 0xff00) | ((s->r[AX] >> 8) & 0xd5))); CLK(T_SAHF); break;
    case 0x9f: wb(s, 4, (u8)get_flags(s)); CLK(T_LAHF); break;
    case 0xa0: { const u16 o = fetch16(s); wb(s, 0, tp_rd8(s, LIN(s->seg_ovr ? s->seg_val : s->sr[DS], o))); CLK(T_MOV_AM8); break; }
    case 0xa1: { const u16 o = fetch16(s); s->r[AX] = tp_rd16(s, LIN(s->seg_ovr ? s->seg_val : s->sr[DS], o)); CLK(T_MOV_AM16); break; }
    case 0xa2: { const u16 o = fetch16(s); tp_wr8(s, LIN(s->seg_ovr ? s->seg_val : s->sr[DS], o), (u8)s->r[AX]); CLK(T_MOV_MA8); break; }
    case 0xa3: { const u16 o = fetch16(s); tp_wr16(s, LIN(s->seg_ovr ? s->seg_val : s->sr[DS], o), s->r[AX]); CLK(T_MOV_MA16); break; }
    case 0xa4: case 0xa5: case 0xa6: case 0xa7:
    case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
        string_op(s, op); break;
    case 0xa8: alu(s, 4, s->r[AX] & 0xff, fetch8(s), 0); CLK(T_ALU_RI8); break;
    case 0xa9: alu(s, 4, s->r[AX], fetch16(s), 1); CLK(T_ALU_RI16); break;
    case 0xc0: case 0xc1: case 0xd2: case 0xd3: {
        /* the 80186's: the count masked to 5 bits, each bit a cycle */
        const int w = op & 1;
        int count;
        decode_modrm(s, &m);
        a = w ? rm16(s, &m) : rm8(s, &m);
        count = ((op <= 0xc1) ? fetch8(s) : (s->r[CX] & 0xff)) & 0x1f;
        s->cyc += tp_timing[m.mod == 3 ? T_ROT_REG_BASE : (op == 0xc0 ? T_ROT_M8_BASE : T_ROT_M16_BASE)];
        s->cyc += tp_timing[T_ROT_REG_BIT] * count;
        a = shift(s, m.reg, a, count, w);
        if (w) wm16(s, &m, (u16)a); else wm8(s, &m, (u8)a);
        break;
    }
    case 0xd0: case 0xd1: {
        const int w = op & 1;
        decode_modrm(s, &m);
        a = w ? rm16(s, &m) : rm8(s, &m);
        CLKM(&m, T_ROT_REG_1, T_ROT_M8_1);
        a = shift(s, m.reg, a, 1, w);
        if (w) wm16(s, &m, (u16)a); else wm8(s, &m, (u8)a);
        break;
    }
    case 0xc2: { const u16 n = fetch16(s); s->ip = pop(s); s->r[SP] = (u16)(s->r[SP] + n); CLK(T_RET_NEAR_IMM); break; }
    case 0xc3: s->ip = pop(s); CLK(T_RET_NEAR); break;
    case 0xc4: case 0xc5:
        decode_modrm(s, &m);
        s->r[m.reg] = tp_rd16(s, m.ea);
        s->sr[op == 0xc4 ? ES : DS] = tp_rd16(s, (m.ea + 2) & 0xfffff);
        CLK(T_LOAD_PTR);
        break;
    case 0xc6: decode_modrm(s, &m); wm8(s, &m, fetch8(s)); CLKM(&m, T_MOV_RI8, T_MOV_MI8); break;
    case 0xc7: decode_modrm(s, &m); wm16(s, &m, fetch16(s)); CLKM(&m, T_MOV_RI16, T_MOV_MI16); break;
    case 0xc8: {                                      /* ENTER */
        const u16 size = fetch16(s);
        const int level = fetch8(s) & 0x1f;
        u16 frame;
        int i;
        CLK(!level ? T_ENTER0 : level == 1 ? T_ENTER1 : T_ENTER_BASE);
        s->cyc += level * tp_timing[T_ENTER_COUNT];
        push(s, s->r[BP]);
        frame = s->r[SP];
        for (i = 1; i < level; ++i) {
            s->r[BP] = (u16)(s->r[BP] - 2);
            push(s, tp_rd16(s, LIN(s->sr[SS], s->r[BP])));
        }
        if (level) push(s, frame);
        s->r[BP] = frame;
        s->r[SP] = (u16)(s->r[SP] - size);
        break;
    }
    case 0xc9: s->r[SP] = s->r[BP]; s->r[BP] = pop(s); CLK(T_LEAVE); break;
    case 0xca: { const u16 n = fetch16(s); s->ip = pop(s); s->sr[CS] = pop(s); s->r[SP] = (u16)(s->r[SP] + n); CLK(T_RET_FAR_IMM); break; }
    case 0xcb: s->ip = pop(s); s->sr[CS] = pop(s); CLK(T_RET_FAR); break;
    case 0xcc: CLK(T_INT3); tp_interrupt(s, 3); break;
    case 0xcd: { const u8 n = fetch8(s); CLK(T_INT_IMM); tp_interrupt(s, n); break; }
    case 0xce: if (s->of) { CLK(T_INTO_T); tp_interrupt(s, 4); } else CLK(T_INTO_NT); break;
    case 0xcf: s->ip = pop(s); s->sr[CS] = pop(s); set_flags(s, pop(s)); CLK(T_IRET); break;
    case 0xd4: {                                      /* AAM */
        const u8 base = fetch8(s), al = (u8)s->r[AX];
        CLK(T_AAM);
        if (!base) { tp_div0(s); break; }
        s->r[AX] = (u16)(((al / base) << 8) | (al % base));
        szp(s, s->r[AX] & 0xff, 0);
        break;
    }
    case 0xd5: {                                      /* AAD */
        const u8 base = fetch8(s);
        const u8 al = (u8)(rb(s, 4) * base + (s->r[AX] & 0xff));
        s->r[AX] = al;
        szp(s, al, 0);
        CLK(T_AAD);
        break;
    }
    case 0xd7: wb(s, 0, tp_rd8(s, LIN(s->seg_ovr ? s->seg_val : s->sr[DS], (u16)(s->r[BX] + (s->r[AX] & 0xff)))));
               CLK(T_XLAT); break;
    case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
        decode_modrm(s, &m); CLK(T_NOP); break;      /* ESC: no coprocessor */
    case 0xe0: case 0xe1: case 0xe2: {
        const signed char d = (signed char)fetch8(s);
        const int lt = op == 0xe1 ? T_LOOPE_T : T_LOOP_T, lnt = op == 0xe1 ? T_LOOPE_NT : T_LOOP_NT;
        s->r[CX]--;
        if (s->r[CX] && (op == 0xe2 || (op == 0xe1 ? s->zf : !s->zf))) { s->ip = (u16)(s->ip + d); CLK(lt); }
        else CLK(lnt);
        break;
    }
    case 0xe3: { const signed char d = (signed char)fetch8(s);
                 if (!s->r[CX]) { s->ip = (u16)(s->ip + d); CLK(T_JCXZ_T); } else CLK(T_JCXZ_NT); break; }
    case 0xe4: wb(s, 0, tp_in8(s, fetch8(s))); CLK(T_IN_IMM8); break;
    case 0xe5: s->r[AX] = tp_in16(s, fetch8(s)); CLK(T_IN_IMM16); break;
    case 0xe6: tp_out8(s, fetch8(s), (u8)s->r[AX]); CLK(T_OUT_IMM8); break;
    case 0xe7: tp_out16(s, fetch8(s), s->r[AX]); CLK(T_OUT_IMM16); break;
    case 0xe8: { const u16 d = fetch16(s); push(s, s->ip); s->ip = (u16)(s->ip + d); CLK(T_CALL_NEAR); break; }
    case 0xe9: { const u16 d = fetch16(s); s->ip = (u16)(s->ip + d); CLK(T_JMP_NEAR); break; }
    case 0xea: { const u16 ip = fetch16(s), cs = fetch16(s); s->ip = ip; s->sr[CS] = cs; CLK(T_JMP_FAR); break; }
    case 0xeb: { const signed char d = (signed char)fetch8(s); s->ip = (u16)(s->ip + d); CLK(T_JMP_SHORT); break; }
    case 0xec: wb(s, 0, tp_in8(s, s->r[DX])); CLK(T_IN_DX8); break;
    case 0xed: s->r[AX] = tp_in16(s, s->r[DX]); CLK(T_IN_DX16); break;
    case 0xee: tp_out8(s, s->r[DX], (u8)s->r[AX]); CLK(T_OUT_DX8); break;
    case 0xef: tp_out16(s, s->r[DX], s->r[AX]); CLK(T_OUT_DX16); break;
    case 0xf4: s->halt = 1; CLK(T_HLT); break;
    case 0xf5: s->cf = !s->cf; CLK(T_FLAG_OPS); break;
    case 0xf6: grp3(s, 0); break;
    case 0xf7: grp3(s, 1); break;
    case 0xf8: s->cf = 0; CLK(T_FLAG_OPS); break;
    case 0xf9: s->cf = 1; CLK(T_FLAG_OPS); break;
    case 0xfa: s->ifl = 0; CLK(T_FLAG_OPS); break;
    case 0xfb: s->ifl = 1; s->no_int = 1; CLK(T_FLAG_OPS); break;
    case 0xfc: s->df = 0; CLK(T_FLAG_OPS); break;
    case 0xfd: s->df = 1; CLK(T_FLAG_OPS); break;
    case 0xfe:
        decode_modrm(s, &m);
        if (m.reg < 2) { wm8(s, &m, (u8)incdec(s, rm8(s, &m), m.reg, 0)); CLKM(&m, T_INCDEC_R8, T_INCDEC_M8); break; }
        s->cyc += 10; tp_interrupt(s, 6); break;
    case 0xff:
        decode_modrm(s, &m);
        switch (m.reg) {
        case 0: case 1: wm16(s, &m, (u16)incdec(s, rm16(s, &m), m.reg, 1)); CLKM(&m, T_INCDEC_R16, T_INCDEC_M16); break;
        case 2: { const u16 t = rm16(s, &m); push(s, s->ip); s->ip = t; CLKM(&m, T_CALL_R16, T_CALL_M16); break; }
        case 3: { const u16 ip = tp_rd16(s, m.ea), cs = tp_rd16(s, (m.ea + 2) & 0xfffff);
                  push(s, s->sr[CS]); push(s, s->ip); s->ip = ip; s->sr[CS] = cs; CLK(T_CALL_M32); break; }
        case 4: s->ip = rm16(s, &m); CLKM(&m, T_JMP_R16, T_JMP_M16); break;
        case 5: s->ip = tp_rd16(s, m.ea); s->sr[CS] = tp_rd16(s, (m.ea + 2) & 0xfffff); CLK(T_JMP_M32); break;
        case 6: push(s, rm16(s, &m)); CLKM(&m, T_PUSH_R16, T_PUSH_M16); break;
        default: s->cyc += 10; tp_interrupt(s, 6); break;
        }
        break;
    default:
        /* an opcode the 80186 does not have: its invalid-opcode trap */
        s->cyc += 10;
        tp_interrupt(s, 6);
        break;
    }
    return s->cyc;
}

/* ------------------------------------------------------------------ */
/* Running                                                             */
/* ------------------------------------------------------------------ */

static void tp_do_events(tp_sound *s)
{
    int c, i;
    for (c = 0; c < 2; ++c)
        for (i = 0; i < 3; ++i) {
            tp_pit *p = &s->pit[c][i];
            if (!p->running) continue;
            if (p->fall <= s->now) {
                p->fall = NEVER;
                tp_pit_out(s, c, i, 0);
            }
            if (p->rise <= s->now) {
                /* the reload, with the count written last, from when it
                 * was due */
                const u64 when = p->rise;
                tp_pit_out(s, c, i, 1);
                p->fall = when + 4ULL * (p->count - 1);
                p->rise = p->fall + 4;
            }
        }
    for (i = 0; i < 3; ++i)
        if (s->timer[i].expire <= s->now) {
            const u64 when = s->timer[i].expire;
            s->timer[i].expire = NEVER;
            tp_timer_elapsed(s, i, when);
        }
    tp_find_next_event(s);
}

void tp_sound_run_to(tp_sound *s, unsigned long long target)
{
    while (s->now < target) {
        if (s->in_reset) { s->now = target; break; }
        if (s->next_event == 0) tp_find_next_event(s);
        if (s->now >= s->next_event) { tp_do_events(s); continue; }

        /* the DMA channels first, as i186.cpp's execute_run */
        if ((s->dma[0].drq && (s->dma[0].control & ST_STOP)) ||
            (s->dma[1].drq && (s->dma[1].control & ST_STOP))) {
            int ch = s->last_dma ? 0 : 1;
            s->last_dma = !s->last_dma;
            if (!(s->dma[1].drq && (s->dma[1].control & ST_STOP))) ch = 0;
            else if (!(s->dma[0].drq && (s->dma[0].control & ST_STOP))) ch = 1;
            else if ((s->dma[0].control & CHANNEL_PRIORITY) && !(s->dma[1].control & CHANNEL_PRIORITY)) ch = 0;
            else if ((s->dma[1].control & CHANNEL_PRIORITY) && !(s->dma[0].control & CHANNEL_PRIORITY)) ch = 1;
            tp_drq_callback(s, ch);
            s->now += 2;
            continue;
        }

        if (s->intr.pending && !s->no_int && s->ifl) {
            const int v = tp_int_ack(s);
            s->halt = 0;
            tp_interrupt(s, v);           /* MAME charges no cycles for it */
            continue;
        }
        if (s->halt) {
            s->now = s->next_event < target ? s->next_event : target;
            continue;
        }
        if (s->no_int) s->no_int--;
        s->now += 2ULL * (u64)tp_step(s);
    }
}

/* ------------------------------------------------------------------ */
/* The board                                                           */
/* ------------------------------------------------------------------ */

static void tp_cpu_reset(tp_sound *s)
{
    int c, i;
    memset(s->r, 0, sizeof s->r);
    memset(s->sr, 0, sizeof s->sr);
    s->sr[CS] = 0xffff;
    s->ip = 0;
    set_flags(s, 0);
    s->halt = 0; s->no_int = 0;

    memset(&s->intr, 0, sizeof s->intr);
    s->intr.priority_mask = 0x0007;
    s->intr.timer = 0x000f;
    s->intr.dma[0] = s->intr.dma[1] = 0x000f;
    for (i = 0; i < 4; ++i) s->intr.ext[i] = 0x000f;
    s->reloc = 0x20ff;
    s->mem.upper = 0xfffb;
    for (i = 0; i < 2; ++i) { s->dma[i].drq = 0; s->dma[i].control = 0; }
    for (i = 0; i < 3; ++i) {
        s->timer[i].control = s->timer[i].maxA = s->timer[i].maxB = s->timer[i].count = 0;
        s->timer[i].expire = NEVER;
    }
    s->last_dma = 0;

    /* leland_80186_sound_device::device_reset */
    s->last_control = 0xf8;
    s->clock_active = 0;
    for (c = 0; c < 2; ++c)
        for (i = 0; i < 3; ++i) {
            memset(&s->pit[c][i], 0, sizeof s->pit[c][i]);
            s->pit[c][i].fall = s->pit[c][i].rise = NEVER;
        }
    tp_events_dirty(s);
}

tp_sound *tp_sound_new(const unsigned char *rom)
{
    tp_sound *s;
    int i;
    if (!rom) return NULL;
    s = (tp_sound *)calloc(1, sizeof *s);
    if (!s) return NULL;
    for (i = 0; i < 256; ++i) {               /* 1 for even parity */
        int b = i, n = 0;
        while (b) { n += b & 1; b >>= 1; }
        tp_parity[i] = (u8)!(n & 1);
    }
    s->rom = rom;
    tp_sound_reset(s);
    return s;
}

void tp_sound_free(tp_sound *s) { free(s); }

void tp_sound_reset(tp_sound *s)
{
    int i;
    memset(s->ram, 0, sizeof s->ram);
    for (i = 0; i < 6; ++i) { s->dac[i] = 0x80; s->dacvol[i] = 0; }
    s->dac9 = 0x200;
    s->command = s->response = 0;
    s->pmapped = 0;
    s->in_reset = 0;
    tp_cpu_reset(s);
    tp_level_update(s);
}

/* leland_80186_sound_device::leland_80186_control_w */
void tp_sound_control(tp_sound *s, unsigned char data)
{
    const int diff = (s->last_control ^ data) & 0xf8;
    if (!diff) return;
    s->last_control = data;
    tp_external_int(s, 0, (data & 0x20) != 0);
    tp_external_int(s, 1, (data & 0x08) != 0);
    s->in_reset = !(data & 0x80);
    if ((diff & 0x80) && (data & 0x80)) tp_cpu_reset(s);
}

void tp_sound_latch(tp_sound *s, unsigned short command) { s->command = command; }
unsigned char tp_sound_response(const tp_sound *s) { return (unsigned char)s->response; }

void tp_sound_command(tp_sound *s, unsigned char command, unsigned char param)
{
    s->command = (unsigned short)((param << 8) | command);
    tp_sound_control(s, (unsigned char)((s->last_control & ~0x20) | 0x00));
    tp_sound_control(s, (unsigned char)(s->last_control | 0x20));
}

void tp_sound_render(tp_sound *s, float *out, int n, int rate)
{
    const double span = (double)TP_SOUND_CLOCK / (double)rate;
    int i;
    /* an instruction may run a little past a sample's end; only a board
     * run on without output (or a first call) starts the timeline anew */
    if (s->out_t + 2.0 * span < (double)s->now) s->out_t = (double)s->now;
    for (i = 0; i < n; ++i) {
        /* each sample ends at an exact time, so nothing drifts; its value
         * is the DACs' level averaged from where the last one stopped */
        const u64 start = s->acc_t;
        s->out_t += span;
        tp_sound_run_to(s, (u64)s->out_t);
        tp_integrate(s);
        out[i] = s->now > start ? (float)(s->acc / (double)(s->now - start)) : s->level;
        s->acc = 0.0;
    }
}

unsigned long long tp_sound_now(const tp_sound *s) { return s->now; }
unsigned long long tp_sound_instructions(const tp_sound *s) { return s->insns; }

void tp_sound_set_hook(tp_sound *s, tp_sound_dac_hook hook, void *user)
{
    s->hook = hook;
    s->hook_user = user;
}
