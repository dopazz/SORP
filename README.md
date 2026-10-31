# SORP — Super Off Road Port

SORP rebuilds the 1990 DOS version of *Ivan "Ironman" Stewart's Super Off Road*, Graftgold's conversion of Leland's arcade racer, in C on SDL 3, from the DOS program's own logic. Four trucks race eight dirt tracks, forwards and reversed, buying nitro, tires, shocks, acceleration and top speed in the Speed Shop between races. Up to three players, on keyboards and gamepads.

It runs on Windows, on x86_64 Linux, and on aarch64 Linux handhelds through [PortMaster](https://portmaster.games/).

**SORP contains none of the game's files.** You need your own copy of the DOS game.

## Downloads

The [releases](https://github.com/dopazz/SORP/releases) page has:

| File | For |
|--|--|
| `superoffroad.zip` | PortMaster on aarch64 handhelds: copy it to `ports/autoinstall/` or install it from PortMaster |
| `sorp-linux-x86_64.zip` | x86_64 Linux desktops (glibc 2.31 or later; Wayland or X11) |
| `sorp-windows-x64.zip` | 64-bit Windows |
| `sorp-source.zip` | This source, as it is built into each package |

## The game's files

Copy the original DOS game's files into the `gamedata` folder beside the game:

`ADLIB.SFX`, `CONTOUR0.XX` to `CONTOUR7.XX`, `EEND.PCG`, `EGA.EXE`, `EGAME.PCG`, `ESHADOW.PCG`, `ESHOP.PCG`, `ETITLE.PCG`, `ETRUCK.PCG`, `GAME.EXE`, `IRONFONT.XX`, `LOADB.XX`, `POLY.BIN`, `QUAD.BIN`, `ROADFONT.XX`, `TITLES.XX` and `TRACK0.XX` to `TRACK7.XX`.

SORP checks every file. Any that is missing or is not the DOS release's own is named in the log.

### Optional: the arcade's tracks

With your own MAME sets of the arcade game in `gamedata/trackpak/`, SORP can also race on the arcade's tracks:

- `offroad.zip` is the original arcade game, with its eight tracks.
- `offroadt.zip` is the Track-Pak, with eight new tracks.

Either works alone, zipped or as loose chip files. SORP recognises the chips by their checksums, not their names. Turn on "arcade roms" in the options, then choose an "arcade mode": original, track-pak or all 16.

With the sets' sound chips, those tracks have the arcade's own music, effects and engine sound. Registration's nation list also gets a second page with the arcade's other eight nations.

## Controls

Each controller joins the game with its nitro button.

| Control | Keys / buttons |
|--|--|
| Keyboard 1 | A / D steer, W accelerates, Space fires nitro |
| Keyboard 2 | Arrow keys; Right Ctrl fires nitro |
| Keyboard 3 | Keypad 4 / 6 / 8; keypad 0 fires nitro |
| Gamepad | D-pad or left stick; A or B fires nitro |
| Options | F1, or a pad's Back / Select |
| Leave | Esc asks "Exit SORP?"; Esc again, or a pad's Start, leaves |
| Leave at once | Guide + Start, or Back + Start |
| Full screen | F11 |

To accelerate with a button instead of up, set the options' "gamepad controls" to "B accel" or "A accel".

The options (F1) also cover sound, volume, shaders (scanlines and MMPX pixel-art scaling) and the log level.

## Building

### Windows

Install Visual Studio or its Build Tools, with the C++ workload. From the repository's root, run:

```
build.cmd
```

The SDL 3 development kit is already in `SDL3/`. `build.cmd` finds the compiler on its own, and writes `sorp.exe`, with `SDL3.dll` beside it, to the root. Put the game's files in `gamedata\` there and run `sorp.exe`.

In VS Code, the "Build SORP" task runs `build.cmd`. `CMakeLists.txt` builds with CMake too.

### Linux, natively

With SDL 3 installed (`libsdl3-dev`, or SDL built from source):

```bash
make
./sorp gamedata
```

### Linux packages, in Docker

`packaging/build.sh` builds SDL 3.4.16 and SORP in a Debian container, and lays out the same packages as the releases. On an x86_64 Linux machine with Docker:

```bash
git clone https://github.com/dopazz/SORP.git
cd SORP

# x86_64 desktop build, written to dist-x86_64/sorp/
docker build -t sorp-x86_64 -f packaging/Dockerfile.x86_64 packaging
docker run --rm --user $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src -w /src sorp-x86_64 packaging/build.sh x86_64

# aarch64 cross-build for the handhelds; the PortMaster package is written to dist-aarch64/portmaster/
docker build -t sorp-aarch64 -f packaging/Dockerfile.aarch64 packaging
docker run --rm --user $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src -w /src sorp-aarch64 packaging/build.sh aarch64
```

The aarch64 build needs glibc 2.29 or later on the device.

## Layout

| Path | What it holds |
|--|--|
| `src/` | All of SORP's code, the arcade Track-Pak's included (`tp_*.c`) |
| `src/GAME_EXE_EGA_EXE.md` | Notes on the DOS program, which SORP follows |
| `packaging/` | `build.sh`, its Dockerfiles, and the Linux and Windows READMEs |
| `packaging/portmaster/source/` | The PortMaster port: the launcher, `port.json`, the README, `padmap.py` and the bundled licenses |
| `SDL3/` | The SDL 3 development kit for the Windows build |
| `build.cmd`, `Makefile`, `CMakeLists.txt` | The Windows, Linux and CMake builds |

## Credits

- **The game:** Leland Corporation's arcade original, and Graftgold's 1990 DOS version, which SORP rebuilds.
- **[SDL 3](https://libsdl.org/)** (zlib license).
- **[Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3)** by Nuke.YKT (LGPL 2.1 or later), for the AdLib sound.
- **MMPX**, the pixel-art scaler by Morgan McGuire and Mara Gagiu ([paper and code](https://casual-effects.com/research/McGuire2021PixelArt/)).
- **[MAME](https://www.mamedev.org/) 0.251** (BSD-3-Clause), which the arcade sound board's emulation follows.

Their licenses are in `packaging/portmaster/source/superoffroad/superoffroad/licenses/`, and in every package.

Super Off Road and its names belong to their owners. SORP is an unofficial fan project, and is not affiliated with them.

## License

SORP - Super Off Road Port. Copyright (C) 2026 dopaz.
SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. It is distributed WITHOUT ANY WARRANTY; see the GNU General Public License in [`LICENSE`](LICENSE) for details.

Developed with the assistance of Claude Opus 5 and Claude Opus 5.5.
