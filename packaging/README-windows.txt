SORP -- Super Off Road Port, for Windows (x64)

  sorp.exe        the game
  SDL3.dll        SDL 3, found beside sorp.exe
  gamedata\       YOUR copy of the original DOS game's files

Copy the DOS game's files into gamedata\, then double-click sorp.exe.
From a command prompt, another folder holding the game files can be
named instead:

  sorp.exe D:\Games\OFFROAD

The Ironman Club table is kept in IRONHI.BIN here, and the options (F1)
and the window's size and position in sorp.cfg.

With F1's "write sorp.log" on, the log goes to sorp.log here, written
afresh each run.  F1's "log level" sets how much: INFO, the default,
gives errors, warnings and the main events; DEBUG adds what was loaded
and from where, TRACE the race tick by tick.

Controls -- each controller joins with its nitro button:
  keyboard 1       A / D steer, W accelerates, SPACE nitro
  keyboard 2       arrow keys, RIGHT CTRL nitro
  keyboard 3       keypad 4 / 6 / 8, keypad 0 nitro
  gamepad          d-pad or left stick, A or B nitro
  ESC              asks "Exit SORP?"; ESC again (or a pad's Start) leaves
  F1               options -- sound, volume, full screen, gamepad
                   controls, shaders, "Exit SORP", and "About" (the
                   build stamp); a pad's Back button opens it too
  F2 / F3 / F4     audio tests / sprite viewer / debug
  F11              full screen on and off
  Guide + Start    leave at once, from a pad
  Back + Start     leave at once too

License

SORP - Super Off Road Port
Copyright (C) 2026 dopaz
SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.  It is distributed WITHOUT ANY WARRANTY; see
the GNU General Public License in LICENSE for details.  SORP's source
is in sorp-source.zip, and the licenses of what is built into it (SDL3,
Nuked OPL3, MMPX, MAME's sound board code) are in licenses\.

Developed with the assistance of Claude Opus 5 and Claude Opus 5.5.
