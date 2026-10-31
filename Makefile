# SORP - Super Off Road Port
# Copyright (C) 2026 dopaz
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.  It is distributed WITHOUT ANY WARRANTY; see
# the GNU General Public License in LICENSE for details.
#
# Developed with the assistance of Claude Opus 5 and Claude Opus 5.5.

# Makefile -- SORP on Linux (native, or cross-compiled; see packaging/).
#
#   make                         uses pkg-config's sdl3
#   make SDL3_PREFIX=/opt/sdl3   uses an SDL3 installed there
#   make CC=aarch64-linux-gnu-gcc SDL3_PREFIX=...   cross-compiles
#
# The binary looks for libSDL3.so beside itself first ($ORIGIN, then
# $ORIGIN/lib), so a packaged build carries its own SDL3.

CC      ?= cc
CFLAGS  ?= -O2 -g
SRC      = src/sorp.c src/offroad_gfx.c src/og_data.c src/og_course.c \
           src/og_palette.c src/og_sound.c src/opl3.c \
           src/tp_rom.c src/tp_data.c src/tp_sound.c

ifdef SDL3_PREFIX
SDL3_CFLAGS = -I$(SDL3_PREFIX)/include
SDL3_LIBS   = -L$(SDL3_PREFIX)/lib -lSDL3
else
SDL3_CFLAGS = $(shell pkg-config --cflags sdl3)
SDL3_LIBS   = $(shell pkg-config --libs sdl3)
endif

sorp: $(SRC) src/*.h
	$(CC) $(CFLAGS) -std=gnu11 -pthread -Wall -Wno-unused-function -Isrc $(SDL3_CFLAGS) \
	    -o $@ $(SRC) $(SDL3_LIBS) -lm -ldl -pthread -Wl,-rpath,'$$ORIGIN:$$ORIGIN/lib'

clean:
	rm -f sorp

.PHONY: clean
