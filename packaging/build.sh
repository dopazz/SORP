#!/bin/sh
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

# packaging/build.sh ARCH -- ARCH is aarch64 or x86_64.
#
# Runs INSIDE the packaging/Dockerfile.ARCH container, with the source tree
# mounted at /src:  SDL3 for ARCH (once, cached in build-ARCH/), then SORP
# against it, then the package in dist-ARCH/ --
#   aarch64  PortMaster's superoffroad.zip, laid out as PortMaster
#            installs it (Super Off Road.sh beside superoffroad/, which
#            holds port.json and the rest), with SORP's own source in it
#            as superoffroad/sorp-source.zip
#
# aarch64 is built for plain ARMv8-A, never one core: the one binary runs
# on every device, the Cortex-A35 of an RK3326 included.
#   x86_64   a sorp/ folder for a desktop: run ./sorp from inside it
#
# SDL3 is built with every back end it can load at run time -- X11 (on
# x86_64), Wayland and KMSDRM for video, ALSA, PulseAudio and PipeWire
# for sound -- each one dlopen()ed, so the one library works whatever the
# machine runs.  libSDL3.so.0 travels beside sorp, found through $ORIGIN.
set -eu

ARCH=${1:?usage: build.sh aarch64|x86_64}
case "$ARCH" in
aarch64) CROSS=aarch64-linux-gnu-; CPU=-march=armv8-a;   X11=OFF ;;
x86_64)  CROSS=;                   CPU=;                 X11=ON ;;
*) echo "build.sh: no such target: $ARCH" >&2; exit 1 ;;
esac

SDL_TAG=release-3.4.16           # the same SDL3 as the Windows build
B=/src/build-$ARCH
P=$B/sdl3                        # SDL3's install prefix
OUT=/src/dist-$ARCH
D=$OUT/sorp                      # the x86_64 package; aarch64's staging

mkdir -p "$B"
if [ ! -f "$P/lib/libSDL3.so" ]; then
    [ -d "$B/SDL" ] || git clone --depth 1 -b "$SDL_TAG" \
        https://github.com/libsdl-org/SDL.git "$B/SDL"
    cmake -S "$B/SDL" -B "$B/sdl-build" -G Ninja \
        -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="$ARCH" \
        -DCMAKE_C_COMPILER="${CROSS}gcc" \
        -DCMAKE_CXX_COMPILER="${CROSS}g++" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$P" \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
        -DSDL_X11="$X11" -DSDL_WAYLAND=ON -DSDL_KMSDRM=ON \
        -DSDL_OPENGL=ON -DSDL_OPENGLES=ON -DSDL_VULKAN=OFF \
        -DSDL_ALSA=ON -DSDL_PULSEAUDIO=ON -DSDL_PIPEWIRE=ON \
        -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_IBUS=OFF
    cmake --build "$B/sdl-build"
    cmake --install "$B/sdl-build"
fi

cd /src
make clean
make CC="${CROSS}gcc" SDL3_PREFIX="$P" CFLAGS="-O2 -g $CPU"

rm -rf "$OUT"
mkdir -p "$D/gamedata"
"${CROSS}strip" -o "$D/sorp" sorp
"${CROSS}strip" -o "$D/libSDL3.so.0" "$P/lib/libSDL3.so.0"
chmod +x "$D/sorp"
cp LICENSE "$D/"                 # GPL-3.0-or-later, with every copy
# the bundled components' licenses, as PortMaster's package has them
mkdir -p "$D/licenses"
cp packaging/portmaster/source/superoffroad/superoffroad/licenses/* "$D/licenses/"
cp "$B/SDL/LICENSE.txt" "$D/licenses/LICENSE-SDL3.txt"
# SORP's source, as fully compressed as zip goes
zip -q -9 -r -X "$D/sorp-source.zip" LICENSE CMakeLists.txt Makefile         .vscode/tasks.json build.cmd src packaging         -x 'src/*.res' 'packaging/portmaster/source/superoffroad/*.png'         'packaging/portmaster/source/review_notes.md'         'packaging/portmaster/source/portmaster-ai-complete-reference.md' 'packaging/linux/*' 'packaging/windows/*'         'packaging/portmaster/superoffroad.zip' 'packaging/portmaster/pr/*' 'packaging/src/*'
# ...and a copy in packaging/src/, for the GitHub release
mkdir -p packaging/src
cp "$D/sorp-source.zip" packaging/src/
if [ "$ARCH" = aarch64 ]; then
    # PortMaster: packaging/portmaster/source/superoffroad/ is the
    # submission folder; the autoinstall zip has it as installed -- the
    # launcher at the top, and port.json, gameinfo.xml, the README and
    # pictures moved inside
    PM=$OUT/portmaster
    G=$PM/superoffroad
    SUB=packaging/portmaster/source/superoffroad
    mkdir -p "$G/libs.aarch64"
    cp "$SUB/Super Off Road.sh" "$PM/"
    cp -r "$SUB/superoffroad/." "$G/"
    cp "$SUB/port.json" "$SUB/gameinfo.xml" "$SUB/screenshot.png"        "$SUB/cover.png" "$G/"
    cp "$SUB/README.md" "$G/"
    cp "$D/sorp" "$G/sorp.aarch64"
    cp "$D/libSDL3.so.0" "$G/libs.aarch64/"
    cp "$B/SDL/LICENSE.txt" "$G/licenses/LICENSE-SDL3.txt"
    cp LICENSE "$G/licenses/LICENSE-sorp.txt"          # GPL-3.0-or-later
    cp "$D/sorp-source.zip" "$G/"
    chmod +x "$PM/Super Off Road.sh" "$G/sorp.aarch64"
    (cd "$PM" && rm -f superoffroad.zip &&
     zip -q -9 -r -X superoffroad.zip "Super Off Road.sh" superoffroad)

    # the pull request's layout, as it goes into PortMaster-New's ports/:
    # the metadata beside the launcher, the game folder without it, and
    # no testing_thread.txt
    PR=$OUT/portmaster-pr/superoffroad
    mkdir -p "$PR"
    cp "$SUB/Super Off Road.sh" "$SUB/port.json" "$SUB/gameinfo.xml" \
       "$SUB/README.md" "$SUB/screenshot.png" "$SUB/cover.png" "$PR/"
    cp -r "$G" "$PR/superoffroad"
    (cd "$PR/superoffroad" && rm port.json gameinfo.xml README.md screenshot.png cover.png)
else
    cp packaging/README-linux.txt "$D/README.txt"
fi
make clean

file "$D/sorp" "$D/libSDL3.so.0"
echo "glibc symbols needed:"
"${CROSS}objdump" -T "$D/sorp" "$D/libSDL3.so.0" |
    grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1
