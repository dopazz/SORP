# Super Off Road: notes for the PortMaster review

Two parts of this port differ from what most merged ports do. These notes explain each one.

## 1. The port bundles its own SDL3 (`libs.aarch64/libSDL3.so.0`)

**SORP is written for SDL3 only.** Its rendering, audio streams and gamepads all use the SDL3 API, and there is no SDL2 code path to build against instead.

**The CFWs don't ship SDL3.** On the test devices, the CFW images have only SDL2 in `/usr/lib` and `/usr/lib64`:

| CFW | SDL in the image |
|---|---|
| Knulli (Batocera 42 base) | `libSDL2-2.0.so.0.3200.8` |
| muOS 2606 | `libSDL2-2.0.so.0`, several 2.26 to 2.30 builds |

So SDL3 has to come either with the port or through a shim.

**It doesn't shadow the CFW's SDL2.** The bundled library's SONAME is `libSDL3.so.0`, which nothing else on the device asks for. The CFW's SDL2 stays the only SDL2 that gptokeyb2 and every other program loads. The failure cases in the porting guide, where a bundled `libSDL2` shadowed the system one, can't happen here.

**It's upstream SDL, unmodified.**
- Release 3.4.16, built from the release tag with no patches.
- Every video and audio back end it supports is loaded with `dlopen()` at run time: KMSDRM, Wayland, OpenGL ES, ALSA, PulseAudio and PipeWire. SDL picks the device's own back end, and the launch script sets no `SDL_VIDEODRIVER` or `SDL_AUDIODRIVER`.
- It's built for plain ARMv8-A, with no `-mcpu` tuning, so it also runs on Cortex-A35 devices.
- It needs glibc 2.29 or later, which is what `min_glibc` says.
- Its licence ships as `licenses/LICENSE-SDL3.txt`.
- The build is reproduced by `packaging/build.sh` in `sorp-source.zip`.

**Why not the `sdl3-sdl2-backend` shim.** The shim is now confirmed on several ports, but it has known limits: no joystick hot-plug (pad state comes from SDL2's cache), unimplemented copy and blit operations, and an immediate-mode GLES backend. SORP depends on the paths it translates:
- SDL3's renderer;
- an audio stream fed from SDL's audio thread;
- the SDL3 gamepad API.

Using the shim would trade upstream SDL3, tested with SORP on the devices below, for a translation layer under all three that SORP hasn't been tested on.

**Where a CFW's patched SDL would have helped, SORP handles it in its own code.** Both cases come from a confirmed symptom on a real device, and both are in `src/sorp.c`.
- **KMSDRM atomic page flips leak (Anbernic RG DS, Knulli).**
  - SDL 3.4.16 leaks a buffer and a fence file descriptor every time an atomic commit fails. On the RG DS that happens about once in forty frames.
  - The process runs out of its 1024 file descriptors within minutes, and the race then runs on a black screen.
  - Knulli exports `SDL_KMSDRM_ATOMIC=1` to every port, so SORP sets `SDL_HINT_KMSDRM_ATOMIC` to `0` at override priority. That gives the legacy page flips that SDL2 uses.
  - This is a setting of the KMSDRM driver, not a choice of driver.
  - Checked: the file descriptor count stays flat on the RG DS and the Miniloong Pocket under EmulationStation's own environment. About 30 minutes of attract mode on the RG DS ran with no freeze.
- **Devices where SDL has no usable video driver.**
  - The Allwinner H700 devices have no `/dev/dri`. The A133P has one, but no GBM that SDL can use.
  - Only when every SDL video driver has failed does SORP render in software with SDL's offscreen driver and copy each frame to `/dev/fb0` itself, allowing for the framebuffer's row pitch and rotation.
  - On any device where an SDL driver works, this path is never used.

**Tested on:**
- **Knulli:** Anbernic RG CubeXX, Anbernic RG DS, Anbernic RG35XX SP, Anbernic RG40XX, Anbernic RG ARC-S, Miniloong Pocket, Miyoo Flip, R36Max, TrimUI Hammer, TrimUI Smart Pro, BatleXP G350.
- **muOS:** TrimUI Brick.
- **ROCKNIX** (Panfrost): Anbernic RG35XX SP.
- **AmberELEC, dArkOS and ROCKNIX (Libmali):** tested by other users from the Discord testing thread, who reported it working.

## 2. `padmap.py` in the launch script

```bash
# Mappings counted in key-code order (patched SDL2s) are renumbered for SDL3
MAP="$(python3 padmap.py)" && export SDL_GAMECONTROLLERCONFIG="$MAP"
```

**The problem it solves.**
- Each mapping in `$sdl_controllerconfig` refers to buttons by number (`a:b0`, `start:b7` and so on), counted as the CFW's own SDL2 counts the pad's keys.
- Upstream SDL2 and SDL3 count the joystick buttons first (key codes from `BTN_JOYSTICK`, 0x120, up), then the lower keys, such as the volume keys.
- Batocera's SDL2 is patched to count every key in key-code order instead (`board/batocera/patches/sdl2/sdl2_input_as_retroarch_udev.patch`). So the mappings EmulationStation's configgen exports on the Batocera family (Knulli, Batocera, REG-Linux) are counted that way. muOS's `gamecontrollerdb.txt` lines are too.
- Read by SDL3 unchanged, those mappings put buttons in the wrong places, on any pad that also reports a key below 0x120.

**How it tells the two orders apart, with no CFW name.** A line doesn't say how it was counted, but its buttons do:
- `padmap.py` reads the pad's key codes from `/sys/class/input/*/device/capabilities/key`, finding the pad by name, or by the IDs in its GUID.
- If the pad has no keys below 0x120, both orders are the same, and the line is left alone. This is the RG35XX SP's `H700 Gamepad` on ROCKNIX.
- Otherwise it reads the line both ways and counts the pad buttons each reading puts on a key below 0x120: a, b, x, y, back, start, the shoulders, the triggers, the stick clicks and the d-pad. A real mapping puts none of those on a volume key. `guide` isn't counted, because on some pads it really is a low key (F10 on the RG DS).
- It renumbers only when the key-code reading is the better one; otherwise the line is left as it is. Each decision is written to `log.txt`.

**It fails safe.**
- A line whose pad isn't found, or where neither reading is better, is passed through unchanged.
- If `python3` is missing or the script fails, the `&&` skips the `export`, and the CFW's own mapping is used as before.
- It only reads `/sys` and the environment. It writes nothing and changes no device setting.

**Tested.**
- Offline, on input data captured from the devices (each pad's `/sys/class/input` facts and the CFW's mapping line), `padmap.py` run on each:
  - RG DS on Knulli, with the configgen line: renumbered.
  - The RG DS and the Miniloong Pocket, with lines already in SDL3's order: left alone.
  - RG35XX SP on ROCKNIX, with PortMaster's line: left alone.
- On the devices, with buttons checked and the decision read from `log.txt`:
  - Knulli: the RG DS, the Miniloong Pocket and the RG34XX SP, all renumbered (each pad has two keys below 0x120).
  - ROCKNIX: the RG35XX SP, left alone (no keys below 0x120).
  - Knulli: the TrimUI Hammer, buttons checked.
  - muOS: the TrimUI Brick, renumbered (its virtual pad, `muOS-Keys`, found by the IDs in the GUID).

**Why a script, not code inside SORP.** It corrects the mappings some CFWs hand to ports. Keeping it in the launch script leaves the binary free of that, and keeps the fix visible and easy to remove once those CFWs number buttons the SDL3 way.
