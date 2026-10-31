# Complete PortMaster Porting Reference (for AI-assisted porting)

This is a single, self-contained reference covering everything needed to take a game from
source/binary to a submittable PortMaster port: the exact file structure, full worked examples
of every required file, launch-script patterns for the common engine types, and the real-world
gotchas that the official site's porting.html/packaging.html pages don't mention. Nothing here
assumes access to any other document — copy what you need directly.

**Project policy as of this revision: every new port uses gptokeyb2 (`$GPTOKEYB2`, `.ini` config)
for control mapping, not classic gptokeyb (`.gptk`).** All launch-script examples below reflect
this. Classic `.gptk` material is kept, but demoted to a clearly-labeled legacy section for reading
or maintaining an already-shipped port that still uses it (§1.8.5) — don't use it as a template for
a new port.

---

# PART 1 — Complete Port Structure & File Examples

## 1.1 Directory layout

A port's top-level folder (named after the game, matching the `.sh`'s title) contains:

```
portname/
├── port.json
├── README.md
├── screenshot.png          (4:3, min 640x480, real gameplay capture — never generated/fabricated)
├── cover.png                (optional but expected for a real submission)
├── gameinfo.xml
├── testing_thread.txt       (temporary — content for the Discord testing-thread post, not launched)
├── Port Name.sh              (capitalized, ends in .sh — this is what actually launches the game)
└── portname/                 (lowercase, squashed: no underscores, no dashes — e.g. "daserbe" not "das_erbe")
    ├── portexecutable.aarch64          (binary renamed with .$DEVICE_ARCH suffix)
    ├── libs.aarch64/                    (bundled shared libraries, minimum needed only)
    │   └── libfoo.so.2                  (flattened real file, not a symlink chain — name matches DT_NEEDED SONAME exactly)
    ├── gl4es.aarch64/                   (only if the engine has no native GLES path — see §1.7.6)
    │   ├── libGL.so.1
    │   └── libEGL.so.1
    ├── licenses/                        (one file per bundled component, not one blanket file)
    │   ├── LICENSE-portname.txt
    │   ├── LICENSE-gptokeyb.txt
    │   └── LICENSE-libfoo.txt
    └── portname.ini                     (plain filename, no arch suffix — see §1.8)
```

Notes on this layout:

- **`port.json`'s `items` array only lists what's needed to launch the game** — the `.sh` and its
  data folder. `README.md`, `gameinfo.xml`, `cover.png`, `screenshot.png`, `testing_thread.txt`,
  and `licenses/` are all real files in the structure but are **not** listed in `items`.
- **The inner folder name is independent of the `.sh` filename.** The `.sh` keeps title case and
  spaces for display (`Das Erbe.sh`), the inner folder is always lowercase-squashed (`daserbe`).
- **`licenses/` lives inside the game subfolder**, at the same level as `libs.$DEVICE_ARCH`, not
  at the top level next to `port.json`.
- **Ask "does this need to be bundled at all" before asking "how do I build a leaner version."**
  Check the near-universal list first: `libexpat`, `libphysfs`, `libfreetype`, `libpng`, `libbz2`,
  `zlib`, **and `SDL2_image`/`SDL2_mixer` themselves** are near-universal on any real CFW base
  image already (see `cfw_requirements.md`'s SDL2-family list) — bundling them "just in case" only
  adds bloat. Confirmed real case: the `dreamchess` port built a whole custom, dependency-trimmed
  `SDL2_image`/`SDL2_mixer` pair to avoid a bloated distro `SDL_mixer`'s MIDI-synth dependency
  chain, without ever checking whether the CFW already had a working copy — a real-device test
  (deleting both bundled `.so` files from the installed port) proved byte-identical launch
  behavior, meaning the entire custom-build step was unnecessary. Only *after* confirming bundling
  is genuinely necessary (the library isn't on the base image, or the base image's copy is missing
  a codec/feature actually needed) does "build a leaner version from source" become the right move
  over shipping a distro package's full, bloated dependency chain.
- **Never bundle a port's own `libSDL2-2.0.so.0` (or any core `libSDL2*.so`) in `libs.$DEVICE_ARCH`,
  under any circumstances — even paired with an `sdl12-compat` shim.** The CFW already ships its
  own SDL2, built and patched specifically for that device's KMS/DRM, input, and audio stack; a
  port-bundled copy shadows it via `LD_LIBRARY_PATH` and can silently break things the CFW's own
  build already handles correctly. Confirmed real failure on the `amphetamine` port (SDL 1.2 engine
  + bundled `sdl12-compat` shim): bundling a chroot-built SDL2 2.26.2 alongside the shim produced a
  real on-device PulseAudio failure (`Failed to symlink ... Unknown error 524`) that disappeared
  once the bundled SDL2 was removed and the shim was left to `dlopen()` the device's own system
  SDL2 instead. This holds even when the chroot has a known-good custom SDL2 build used for
  compiling this project's other native SDL2 ports — that build is for linking against at compile
  time only, never for shipping in a port's `libs.$DEVICE_ARCH`. If a port needs `sdl12-compat`,
  bundle *only* the `sdl12-compat`-built `libSDL-1.2.so.0` shim and let it find the system SDL2 on
  its own. The same principle applies to the `sdl3-sdl2-backend` shim (§1.7.9): never bundle a real
  SDL2 alongside it either — it `dlopen`s the device's own system SDL2 the same way.

## 1.2 port.json — current schema (v4)

Generated conceptually via the Port JSON Generator, but hand-editable. Don't trust an older
port's committed `port.json` as a schema reference without checking its own `version` field
first — the schema has changed over time, and older merged ports can still be on `version: 3`.

Full real example, current schema:

```json
{
  "version": 4,
  "name": "portname.zip",
  "items": [
    "Port Name.sh",
    "portname"
  ],
  "items_opt": [],
  "attr": {
    "title": "Port Name",
    "porter": [
      "YourHandle"
    ],
    "desc": "Plain text description shown in the PortMaster app itself. No markdown, no literal \\n escapes.",
    "desc_md": null,
    "inst": "Plain text instructions shown in the PortMaster app itself. No markdown, no literal \\n escapes.",
    "inst_md": null,
    "genres": [
      "action"
    ],
    "image": null,
    "rtr": true,
    "exp": false,
    "runtime": [],
    "store": [],
    "availability": "full",
    "reqs": [],
    "arch": [
      "aarch64"
    ],
    "min_glibc": ""
  }
}
```

Field notes:

- **`name`**: the zip filename, `<foldername-from-.sh>.zip`.
- **`items`**: the `.sh` and the game data folder — nothing else.
- **`title`**: display name.
- **`porter`**: array of porter handle(s), never a placeholder.
- **`desc` / `inst`**: shown **in the PortMaster app itself**. Plain text only — no markdown, no
  literal `\n` escapes.
- **`desc_md` / `inst_md`**: shown **on portmaster.games** instead of the plain versions, if
  filled in (markdown supported, e.g. bullet lists). If left `null`, the plain version is used on
  the website too. These are two different renderings for two different surfaces — only set the
  `_md` variant when it genuinely adds something (real markdown formatting, or more content) over
  the plain one. Don't paste in a reformatted duplicate of the same text.
- **`genres`**: one or more of `action`, `adventure`, `arcade`, `casino-card`, `fps`, `platformer`,
  `puzzle`, `racing`, `rhythm`, `rpg`, `simulation`, `sports`, `strategy`, `visual-novel`, `other`
  (exact lowercase slugs may vary slightly by generator version — cross-check against the actual
  generator form if precision matters).
- **`rtr`**: "Ready To Run" — true if the port launches with everything bundled, no user-supplied
  game files needed.
- **`exp`**: experimental flag — true hides the port unless the user enables experimental ports.
  **This is for a *released* port PortMaster wants hidden from the default listing** (e.g.
  known-broken-on-most-devices, or deliberately soft-launched), **not** a general "still in
  testing/not yet verified" flag. Never set this true on a port still being developed/tested — that
  is the normal, expected state for every port before its first PR. Leave it `false` unless the
  port itself, not its testing status, should ship hidden.
- **`runtime`**: a **list**, not a single string. Leave `[]` unless a runtime squashfs is used and
  not already fully handled inside the `.sh`. **Critical gotcha**: this must be the literal
  runtime *catalog key*, not the generator's display label. E.g. the display label "Westonpack
  0.2" is not what goes here — the real key is `weston_pkg_0.2`. PortMaster's runtime-check code
  (`check_runtime()` in `harbourmaster/harbour.py`) does an exact, untranslated lookup of this
  string (plus `.squashfs`) against the device's cached runtime catalog (`PortMaster/config/
  runtimes.json`) — there is no nicename-to-key translation anywhere in that path. Using the
  display label instead of the real key **fails silently** on any device that doesn't already
  happen to have that runtime cached from an unrelated port install — a launch script's own
  `if [ ! -f .../weston_pkg_0.2.squashfs ]; then ... runtime_check ...` guard correctly uses the
  real key and only runs when the file is missing, so a device that already has it hides the bug
  entirely; only harbourmaster's own post-install runtime check, driven directly by this
  `port.json` field, actually exercises the broken path. If in doubt, check a real device's own
  `PortMaster/config/runtimes.json` for the actual key before trusting a generator's dropdown text.
- **`store`**: list of purchase links, `[]` if freeware/fully bundled. **Not a bare list of URL
  strings** — flagged directly on a live PR review (`arcanumce`) by a PortMaster team member. Each
  entry is an object:
  ```json
  "store": [
    {
      "name": "GOG",
      "gameurl": "https://www.gog.com/game/example",
      "developerurl": "https://example-studio.com/"
    }
  ]
  ```
  Fall back to a Wikipedia page for `developerurl` when the studio is defunct and its old domain
  has been squatted/redirected elsewhere — don't link a squatted domain.
- **`availability`**: e.g. `"full"` for a complete free/bundled game.
- **`reqs`**: leave `[]` unless the game or the `.sh` explicitly needs one of: a resolution
  requirement (`Not Lowres` excludes 480x320/351p/OGA/zpg pro/rgb10, or `Highres` requires above
  640x480), a RAM minimum (`No Minimum`/`2GB Minimum`/`4GB Minimum`), `power` (excludes weaker
  SoCs, any device above rk3326), or `opengl` (requires real desktop OpenGL, not just GLES).
- **`arch`**: `aarch64`, `armhf`, `x86_64`, `x86` — whichever the binary actually targets. If the
  port uses a full runtime (Godot/Mono/Java/etc. with no native binary of its own), no arch needs
  to be set.
- **`min_glibc`**: leave `""` unless a specific minimum glibc version is actually known/required.

## 1.3 gameinfo.xml

Feeds the CFW's frontend (EmulationStation-style) metadata on install, including the cover.
Generated via the Port Metadata Editor; the Release Date *form field* takes `dd/mm/yyyy` human
entry, which the generator converts into the XML's `YYYYMMDDT000000` output format shown below.

```xml
<?xml version="1.0" encoding="utf-8"?>
<gameList>
  <game>
    <path>./Port Name.sh</path>
    <name>Port Name</name>
    <desc>A real, specific description of the game — genre, what it plays like, notable features. Not a copy of port.json's desc verbatim if you can do better here.</desc>
    <releasedate>20230819T000000</releasedate>
    <developer>Original Developer or Team Name</developer>
    <publisher>Original Publisher (use "Open Source" for a self-published/community project)</publisher>
    <genre>Action</genre>
    <image>./portname/screenshot.png</image>
  </game>
</gameList>
```

Notes:

- **`<path>`** matches the `.sh` filename exactly, relative (`./Port Name.sh`).
- **`<image>`** path is relative to `gameinfo.xml`'s own top-level location. If a real,
  purpose-made `cover.png` exists, reference that instead of `screenshot.png`; if no cover was
  made, referencing `screenshot.png` as the fallback is the convention — PortMaster falls back to
  the screenshot automatically if no cover is provided anyway, but being explicit here is fine.
  Never invent a path to a `cover.png` that doesn't actually exist in the package. **The path must
  include the game-subfolder prefix** (`./portname/screenshot.png`, not a bare `./screenshot.png`),
  even though the file physically sits at the zip's top level next to `port.json`/`README.md` —
  PortMaster's own install step, not the submitted zip's own layout, is what EmulationStation's
  gamelist image lookup actually resolves against. The zip's internal layout and the on-device path
  `gameinfo.xml` needs to describe are two different things — don't reason from one to the other.
- **`<releasedate>`** format is `YYYYMMDDT000000`.
- **Developer/Publisher**: research the actual original game/engine's info online if unknown —
  don't leave placeholders.
- **`<desc>`** must be present in the XML itself even if the same text exists elsewhere (`port.json`)
  — don't skip it assuming it's redundant.
- **Don't copy extra fields from a real merged reference port without checking they're still part
  of the current schema.** A real, already-merged port's `gameinfo.xml` once picked up a stray
  `<players>2</players>` tag this way, copied from an older/non-conformant reference port even
  though no current schema documentation mentions a `players` field at all. Cross-check every
  field a reference port has against the actual current field list above before including it —
  don't include something just because a real example port happens to have it.

## 1.4 README.md

The real, current README template starts directly at `## Notes` — **no top-level `# GameName`
title above it.**

```markdown
## Notes

Thanks to [OriginalAuthorName](https://github.com/example/repo) for creating this game — a tight, replayable [genre] with [one genuine, specific, positive detail about what makes it worth playing].

## Controls

| Key | Action |
|--|--|
| D-Pad | Move |
| A | Confirm / Jump |
| B | Cancel / Attack |
| X | Use Item |
| Y | Special |
| L1 | Previous |
| R1 | Next |
| Start | Pause |
| Select | Menu |

## Compile

git clone --recurse-submodules https://github.com/example/yourgame.git
cd yourgame
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

Rules that matter here:

- **The first section is always a one-sentence thank-you to whoever actually wrote the thing
  being ported** — the original game/engine author or reimplementation maintainer, not (only)
  whoever packaged it for PortMaster. Verify the actual repo URL before linking it (e.g.
  `curl -s https://api.github.com/repos/<org>/<repo>`), don't guess or infer it from local git
  config/session context. Name them, link their site, add one genuine specific positive detail
  about the game — not a generic filler adjective ("this classic game"), and not a multi-clause
  elaboration either. Before crediting a fork with "adds features on top of upstream," actually
  diff it against upstream (e.g. the GitHub compare API) — a fork's own commit history existing
  doesn't mean it diverged; sometimes the commits are byte-identical to upstream (same SHAs) and it
  adds nothing.
- **No "Known Limitations" or similar meta-commentary section.** That's PR/session review
  content, not something that belongs in a shipped README.
- **Keep Notes to genuinely user-facing information.** Don't restate facts that only matter
  internally — "the original game files are bundled in this folder" tells a user nothing they
  don't already have in front of them once the port is installed.
- **The Controls table lists actual bindings.** Omit rows for buttons the port doesn't bind to
  anything — don't fill a row in with "Unused"; a button with no row is already understood to do
  nothing.
- **The Build/Compile section is a reproduction guide, not a packaging checklist.** It documents
  how to get from source to the finished build artifact, as a complete bootstrap from a bare
  environment (e.g. for a .NET port: installing the matching SDK, building any bundled translation
  layer from source, then the publish step) — stop there. Don't include: how the final port folder
  gets assembled from the build output (file-tree diagrams, "copy X here" — that's an internal
  packaging note, not something a reader compiling from source needs, and it goes stale the moment
  the layout changes), or post-build cleanup detail (stripping symbols, deleting a bundled library,
  content-pipeline quirks — that's build-script documentation, not README content). Every
  dependency that needs more than "install the distro package" gets its own numbered subsection
  with a real, complete, copy-pasteable command block — not a prose bullet describing what to do
  instead of showing the actual commands.
- **Never use an em dash anywhere in generated port content** (README, `port.json` fields, any
  other doc). Use a period, comma, colon, semicolon, or parentheses instead.

## 1.5 testing_thread.txt

Easy to skip entirely — nothing else in a port's file set implies it needs to exist, since no
game logic reads it and no other file references it, but it's expected content for the Discord
`#testing-n-dev` post before opening a PR:

```
Instructions to install the Testing Zip:
Drop the .zip into your Portmaster autoinstall folder and run Portmaster.
or
run via ssh: harbourmaster install "zip url from discord"

Game Information
Title: Port Name
URL: https://github.com/example/yourgame

Instructions:
Ready to run. No additional setup required.

| Button | Action |
|--|--|
|A| Jump |
|B| Shoot |
|X| Run (hold) |
|Y| Activate |
|L1| Previous weapon |
|L2| — |
|R1| Next weapon |
|R2| — |

CFW Tests:
[ ] AmberELEC
[ ] dArkOS
[ ] MuOS
ROCKNIX
-> [ ] Libmali
-> [ ] Panfrost
-> [ ] Adreno (Optional)
[ ] Knulli

Resolutions:
[ ] 480x320 (Optional)
[ ] 640x480
[ ] 720x720 (RGB30) (Optional)
[ ] Higher resolutions (e.g., 1280)
```

**The combined `A/B/X/Y` row format is only for the rare case where all four face buttons
genuinely do the same kind of thing** (e.g. all four fire the same action in different
directions) — it is not a template to fill in literally when the four buttons do different
things, which is the normal case. When each face button binds a different action (as in the
example above), give each one its own row so a reader can look up "what does X do" without
parsing a combined cell — this matches the same rule already applied to README.md's own Controls
table in §1.4.

Omit table rows entirely for buttons the port doesn't bind to anything at all, same rule as the
README — don't write "Unused" rows. ROCKNIX has no single pass/fail checkbox of its own; it's
tested per GPU driver variant (Libmali, Panfrost, Adreno) since behavior can genuinely differ
between them on the same CFW.

## 1.6 The GitHub Pull Request template

This is the section the official site's porting/packaging pages don't mention at all, and it's
the most important one for an AI-assisted port specifically. This is the current PortMaster repo
PR template (`.github/PULL_REQUEST_TEMPLATE.md` upstream) — it supersedes the older
checklist-only style that just mirrored the Discord testing thread; it now also requires an
authorship/self-review attestation, a specific file-structure listing, and a script-conventions
check. Paste this into the PR description when opening the port PR:

```markdown
## Game Information
- **New Port for**: { Game Title }
- **URL**: { Link to project page or source control page }

## Authorship & Testing

- [ ] I wrote and understand this script/patch myself, or clearly marked which parts came from an AI assistant and reviewed them
- [ ] I can explain every non-standard line in my launch script

## Submission Requirements

### CFW Tests
Ensure your game has been tested on all major CFWs:
- [ ] ArkOS
- [ ] AmberELEC
- [ ] ROCKNIX
- [ ] muOS
- [ ] Knulli
- [ ] Crossmix (Optional)
- [ ] Other (add here)

### Resolution Tests
Test all major resolutions:
- [ ] 480x320 (Optional)
- [ ] 640x480
- [ ] 720x720 (RGB30) (Optional)
- [ ] Higher resolutions (e.g., 1280x720)

## File Structure
- Your port should have the following structure:
  - portname/
    - port.json
    - README.md
    - screenshot.png
    - cover.png
    - gameinfo.xml
    - Port Name.sh
    - portname/
      - <portfiles here>

## Script Conventions

Before submitting, check your launch script against a few recently merged ports of a similar type.
If your script is significantly longer that or does over complicate things that don't appear in any merged port, remove that logic unless you can point to a game-specific reason it's needed. Reviewers will ask why non-standard logic is there.

## Additional Resources
For an in-depth guide on creating a pull request, refer to: [PortMaster Game Packaging Guide](https://portmaster.games/packaging.html#creating-a-pull-request)
```

**If AI wrote or helped write the launch script, that box genuinely needs to be checked
honestly, and it means you need to actually be able to explain every non-standard line, not just
paste what the AI produced.** This is exactly the checkbox an AI-assisted-porting user is most
likely to gloss over, and reviewers do ask.

## 1.7 The Launch Script (.sh)

### 1.7.1 Standard boilerplate header (identical in every port)

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
get_controls
```

Nothing — no comment, no variable assignment, nothing at all — goes above this block. It's the
one part of every launch script that should look completely identical across every port; a
diff against any recently-merged port's boilerplate should show zero differences here.

`source $controlfolder/control.txt` provides `$ESUDO`, `$directory`, controller-config helpers,
and the `$GPTOKEYB`/`$GPTOKEYB2` variables. `get_controls` pulls the correct SDL2
GameControllerDB GUID/config for the current device into `$sdl_controllerconfig`. See §2.7 for
the full environment-variable contract these scripts rely on.

### 1.7.2 Pattern A — basic native binary, own SDL2 input, no GL

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
BINARY=portexecutable.${DEVICE_ARCH}

mkdir -p "$GAMEDIR/conf"
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/portname.ini" &

pm_platform_helper "$GAMEDIR/$BINARY"

./$BINARY

pm_finish
```

`$GPTOKEYB2` (like `$GPTOKEYB`) is a multi-word command line, not a bare path (it expands to
something like `env LD_PRELOAD=... /path/gptokeyb2 -1`) — never quote it as `"$GPTOKEYB2"`, that
turns the whole string into one nonexistent literal filename.

### 1.7.3 Pattern B — gl4es, standalone (no Westonpack)

Use this only when the engine has no native GLES path at all (see §1.7.6 for when gl4es is/isn't
the right call). PortMaster centralizes the actual `LIBGL_ES`/`LIBGL_GL`/`LIBGL_FB` tuning via
`libgl_default.txt` (or a per-CFW override) — source it rather than hand-rolling those exports,
since it also contains a DRM-device existence check a hand-rolled script would miss. It must be
sourced *after* `cd`-ing into the game folder, since it resolves the bundled gl4es folder
relative to `$PWD`.

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
BINARY=portexecutable.${DEVICE_ARCH}

mkdir -p "$GAMEDIR/conf"
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

if [ -f "${controlfolder}/libgl_${CFW_NAME}.txt" ]; then
  source "${controlfolder}/libgl_${CFW_NAME}.txt"
else
  source "${controlfolder}/libgl_default.txt"
fi

# Some engines' GL-extension query doesn't reliably resolve through gl4es via
# LD_LIBRARY_PATH alone — pointing SDL2 at the exact files directly is the more reliable fix.
if [ -d "$GAMEDIR/gl4es.${DEVICE_ARCH}" ]; then
  export SDL_VIDEO_GL_DRIVER="$GAMEDIR/gl4es.${DEVICE_ARCH}/libGL.so.1"
  export SDL_VIDEO_EGL_DRIVER="$GAMEDIR/gl4es.${DEVICE_ARCH}/libEGL.so.1"
fi

$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/portname.ini" &

pm_platform_helper "$GAMEDIR/$BINARY"

./$BINARY

pm_finish
```

`libgl_default.txt`'s real contents (PortMaster-provided, not something a port ships, shown here
so the `gl4es.$DEVICE_ARCH`/`libs.$DEVICE_ARCH` naming requirement is clear — it looks for those
exact folder names via `$PWD`):

```bash
export LIBGL_ES=2
export LIBGL_GL=21
export LIBGL_FB=4

# Fall back off GBM if there's no DRM device.
if [ ! -e "/dev/dri/card0" ]; then
  export LIBGL_FB=2
fi

if [ -d "$PWD/gl4es.$DEVICE_ARCH" ]; then
  export LD_LIBRARY_PATH="$PWD/gl4es.$DEVICE_ARCH:$LD_LIBRARY_PATH"
elif [ -d "$PWD/gl4es" ]; then
  export LD_LIBRARY_PATH="$PWD/gl4es:$LD_LIBRARY_PATH"
fi

if [ -d "$PWD/libs.$DEVICE_ARCH" ]; then
  export LD_LIBRARY_PATH="$PWD/libs.$DEVICE_ARCH:$LD_LIBRARY_PATH"
elif [ -d "$PWD/libs" ]; then
  export LD_LIBRARY_PATH="$PWD/libs:$LD_LIBRARY_PATH"
fi
```

The explicit `SDL_VIDEO_GL_DRIVER`/`SDL_VIDEO_EGL_DRIVER` block is **not redundant** with
`libgl_default.txt` alone — a real on-device test (AmberELEC/RG351V, a MonoGame port) showed gl4es
itself initializing fine while the engine's own FBO-extension query still failed to resolve
through gl4es via `LD_LIBRARY_PATH` alone; pointing SDL2 at the exact files fixed it. Keep the
existence check so this stays a no-op on any CFW/port variant that doesn't ship a
`gl4es.$DEVICE_ARCH` folder.

### 1.7.4 Pattern C — Westonpack

Westonpack is a runtime providing a full Wayland/X11 environment plus GL compatibility layers, for
apps that need X11/Wayland/OpenGL and can't reach the display through plain SDL2. Controlled via
`westonwrap.sh`.

**Backend**: `drm` renders the desktop on screen via DRM/GBM (does NOT work with GL4ES); `headless`
runs Weston/Xwayland in the background only (use with GL4ES's GBM mode to render directly to
screen).

**Renderer**: `auto` (usually `gl` for drm, `noop` for headless); `gl` (assembles output using
GLES, doesn't mean the app itself is accelerated); `pixman` (software CPU compositing, currently
not working correctly); `noop` (no output rendering, headless only).

**Shell**: `desktop` (full Wayland desktop, taskbar/resizable windows); `kiosk` (minimal, renders
only the started app, auto-fullscreens if possible).

**GL Library**: `system` (whatever the system provides — without Panfrost likely no GL at all;
GLES 3.2 on native platforms); `llvmpipe` (needs mesapack — software GL4.5+GLES3.2, works with
more apps including GLX-background ones, very slow); `virgl` (needs mesapack — virtualized
GL2.1+GLES2 via LLVMPipe, works with most GL2.1 apps, more stable than GL4ES but slower);
`zink` (needs mesapack — Vulkan-based GL4.5, falls back to software if no Vulkan, very slow);
`crusty_gbm` (implements GBM/DRMKMS via SDL2, GLES 3.2 and lower, near-native perf, meh
compatibility); `crusty_glx_gl4es` (implements GLX via SDL2 with bundled GL4ES for OpenGL 2.1,
decent perf/compat); `crusty_x11egl` (implements EGL with X11 extensions via SDL2, GLES 3.2 and
lower, near-native perf/compat); `gl4es` (**do not use**, low device support — DRM backend usage
may cause empty windows/crashes); `crusty_virgl` (in development, not yet included).

**Mode-combination table**:

| App does | Mode |
|---|---|
| Uses Xlib, no OpenGL | `drm gl kiosk system` |
| OpenGL 2 + GLX, no Xlib drawing | `headless noop kiosk crusty_glx_gl4es` |
| OpenGL ES + EGL (X11), no Xlib drawing | `headless noop kiosk crusty_x11egl` |
| Made for GBM/DRMKMS directly | `headless noop kiosk crusty_gbm` |
| OpenGL 2 + GLX/EGL, Xlib drawing, low reqs | `drm gl kiosk virgl` |
| OpenGL 3/4 + GLX/EGL, Xlib drawing, very low reqs | `drm gl kiosk llvmpipe` (needs mesapack) |
| Unknown/high compatibility needed, don't care about speed | `drm gl kiosk llvmpipe` (needs mesapack) |

#### Variant 1: Xlib-only, no OpenGL (`drm gl kiosk system`)

Prerequisites: game must not use OpenGL or OpenGL ES. Required runtime: Westonpack 0.2.1+
(`weston_pkg_0.2.squashfs`).

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
game_executable="portexecutable.${DEVICE_ARCH}"
ini_filename="portname.ini"
game_libs=$GAMEDIR/libs.${DEVICE_ARCH}/:$LD_LIBRARY_PATH

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

CONFDIR="$GAMEDIR/conf/"
$ESUDO mkdir -p "${CONFDIR}"

weston_dir=/tmp/weston
$ESUDO mkdir -p "${weston_dir}"
weston_runtime="weston_pkg_0.2"
if [ ! -f "$controlfolder/libs/${weston_runtime}.squashfs" ]; then
  if [ ! -f "$controlfolder/harbourmaster" ]; then
    pm_message "This port requires the latest PortMaster to run, please go to https://portmaster.games/ for more info."
    sleep 5
    exit 1
  fi
  $ESUDO $controlfolder/harbourmaster --quiet --no-check runtime_check "${weston_runtime}.squashfs"
fi
if [[ "$PM_CAN_MOUNT" != "N" ]]; then
    $ESUDO umount "${weston_dir}"
fi
$ESUDO mount "$controlfolder/libs/${weston_runtime}.squashfs" "${weston_dir}"

cd $GAMEDIR
$GPTOKEYB2 "$game_executable" -c "$GAMEDIR/$ini_filename" &

$ESUDO env WRAPPED_LIBRARY_PATH=$game_libs \
$weston_dir/westonwrap.sh drm gl kiosk system \
XDG_DATA_HOME=$CONFDIR $GAMEDIR/$game_executable

$ESUDO $weston_dir/westonwrap.sh cleanup
if [[ "$PM_CAN_MOUNT" != "N" ]]; then
    $ESUDO umount "${weston_dir}"
fi
pm_finish
```

Passing extra env vars (`XDG_RUNTIME_DIR`, `ULTRADIR`, etc.) to the wrapped game process, not to
Weston itself, has to be done as **positional `VAR=value` arguments right before the binary
path** in the `westonwrap.sh` invocation, exactly like `XDG_DATA_HOME` above — an `export` earlier
in the script gets silently overridden, since `control.txt` is sourced a second time inside
`westonwrap.sh` itself.

#### Variant 2: OpenGL 2 (GLX), GL4ES (`headless noop kiosk crusty_glx_gl4es`)

Prerequisites: game uses OpenGL 2.1 or lower. Required runtime: Westonpack 0.2.5+. Same boilerplate
and mount/cleanup as Variant 1 above — only the `westonwrap.sh` mode line and `ini_filename`/
`game_executable` differ:

```bash
$ESUDO env WRAPPED_LIBRARY_PATH=$game_libs \
$weston_dir/westonwrap.sh headless noop kiosk crusty_glx_gl4es \
XDG_DATA_HOME=$CONFDIR $GAMEDIR/$game_executable
```

#### Variant 3: OpenGL 2 (GLX), VirGL (`drm gl kiosk virgl`)

Prerequisites: game uses OpenGL 2.1 or lower. Required runtimes: Westonpack 0.2.5+ **and**
Mesapack 0.1.1+ (`mesa_pkg_0.1.squashfs`) — this variant needs a **second, simultaneous** squashfs
mount alongside `weston_pkg_0.2`, a pattern that doesn't appear anywhere else in this document:

```bash
# ... weston_pkg_0.2 mount block exactly as Variant 1 ...

mesa_dir=/tmp/mesa
mesa_runtime="mesa_pkg_0.1"
if [ ! -f "$controlfolder/libs/${mesa_runtime}.squashfs" ]; then
    if [ ! -f "$controlfolder/harbourmaster" ]; then
        pm_message "This port requires the latest PortMaster to run, please go to https://portmaster.games/ for more info."
        sleep 5
        exit 1
    fi
    $ESUDO $controlfolder/harbourmaster --quiet --no-check runtime_check "${mesa_runtime}.squashfs"
fi
$ESUDO mkdir -p "${mesa_dir}"
if [[ "$PM_CAN_MOUNT" != "N" ]]; then
    $ESUDO umount "${mesa_dir}"
fi
$ESUDO mount "$controlfolder/libs/${mesa_runtime}.squashfs" "${mesa_dir}"

cd $GAMEDIR
$GPTOKEYB2 "$game_executable" -c "$GAMEDIR/$ini_filename" &

$ESUDO env WRAPPED_LIBRARY_PATH=$game_libs \
$weston_dir/westonwrap.sh drm gl kiosk virgl \
XDG_DATA_HOME=$CONFDIR $GAMEDIR/$game_executable
```

#### Variant 4: OpenGL ES (EGL/X11) (`headless noop kiosk crusty_x11egl`)

Prerequisites: game uses OpenGL ES 3.2 or lower. Required runtime: Westonpack 0.2.5+. Same shape
as Variant 1, mode line becomes `headless noop kiosk crusty_x11egl`. This and the two SDL2-over-X11
variants below add one extra sourcing line right after `control.txt`:

```bash
source $controlfolder/control.txt
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls
```

#### Variant 5: SDL2-over-X11 (GL2 / GLES)

Overlays an X11-built SDL2 on top of the system SDL2, via Crusty loading the system SDL2 in a
separate namespace hidden from the game — Crusty then treats the game + bundled X11 SDL2 as a
normal X11 app with a GLX or EGL(X11) dependency. Near-native performance; also works with games
run under Box64 (no x64 SDL2 build needed). Prerequisites: a build of SDL2 that supports X11; game
must support OpenGL 2 or OpenGL ES 3.2. Required runtime: Westonpack 0.2.5+. Setup: create a
folder called `x11sdllib/` in the port directory and place the X11 SDL2 library there.

```bash
GAMEDIR=/$directory/ports/portname
game_executable="portexecutable.${DEVICE_ARCH}"
ini_filename="portname.ini"
game_libs=$GAMEDIR/libs.${DEVICE_ARCH}/:$LD_LIBRARY_PATH
x11sdl_path="$GAMEDIR/x11sdllib/"

# ... mkdir/log/mount block as Variant 1 ...

cd $GAMEDIR
$GPTOKEYB2 "$game_executable" -c "$GAMEDIR/$ini_filename" &

# OpenGL 2 variant:
$ESUDO env WRAPPED_LIBRARY_PATH_MALI="$x11sdl_path" WRAPPED_PRELOAD_MALI="$x11sdl_path/libSDL2-2.0.so.0" \
WRAPPED_LIBRARY_PATH=$game_libs \
$weston_dir/westonwrap.sh headless noop kiosk crusty_glx_gl4es \
WAYLAND_DISPLAY= XDG_DATA_HOME=$CONFDIR $GAMEDIR/$game_executable

# OpenGL ES variant instead adds SDL_VIDEO_X11_FORCE_EGL=1 and targets crusty_x11egl:
$ESUDO env WRAPPED_LIBRARY_PATH_MALI="$x11sdl_path" WRAPPED_PRELOAD_MALI="$x11sdl_path/libSDL2-2.0.so.0" \
SDL_VIDEO_X11_FORCE_EGL=1 WRAPPED_LIBRARY_PATH=$game_libs \
$weston_dir/westonwrap.sh headless noop kiosk crusty_x11egl \
WAYLAND_DISPLAY= XDG_DATA_HOME=$CONFDIR $GAMEDIR/$game_executable
```

`WRAPPED_LIBRARY_PATH_MALI`/`WRAPPED_PRELOAD_MALI` scope the X11 SDL2 override to Mali devices
specifically (a `_PANFROST` variant also exists, useful to exclude X11-related libs on Panfrost);
`WAYLAND_DISPLAY=` is explicitly unset for the wrapped process.

### 1.7.5 Westonpack: env vars, terminology, and advanced tips

**Useful env vars**: `CRUSTY_RESOLUTION=320x240` (virtual screen resolution given to crusty/
weston); `CRUSTY_SHOW_CURSOR=1` (software mouse cursor, useful headless); `CRUSTY_FPS=1` (prints
FPS every second); `CRUSTY_GLES=32` (GLES version for Crusty: 1,10,11,2,20,21,3,30,31,32, default
32; 11 for SFML apps, 31 for Panfrost); `WESTON_HEADLESS_WIDTH`/`WESTON_HEADLESS_HEIGHT` (virtual
desktop resolution for weston headless mode); `WESTON_NO_HEADLESS_INPUT=1` (disables input capture
in headless mode, fixes some SDL2 double-input issues); `WESTON_KIOSK_NO_RESIZE=1` (disables
auto-resize in kiosk mode, fixes some SDL2 startup segfaults); `WRAPPED_LIBRARY_PATH`/
`WRAPPED_PRELOAD` (like `LD_LIBRARY_PATH`/`LD_PRELOAD` but scoped to the wrapped app only, not
Weston/supporting programs — the general mechanism used unexplained in the scripts above);
`WRAPPED_LIBRARY_PATH_MALI`/`_PANFROST` (device-specific variants); `XCURSOR_SIZE=24` (cursor
size); `crustydebug=_debug` (enables a debug crusty build with tracelog).

**Terminology**: X11 (classic Linux GUI display system); Wayland (newer display system, different
protocol — SDL2/GTK3/QT5 apps use it); GLX (X11 extension for GL(ES) in an X11 window, driver
support usually absent on these devices); EGL (graphics driver API for GL(ES) across FBO/GBM/
Wayland/X11 contexts, each needing separate driver extension support); SDL (every PortMaster
device ships a native SDL2 build); GBM (API for GL(ES) with no display system, via the EGL GBM
extension); fbdev (API for GL(ES) with no display system, via the "fbdev" EGL_PLATFORM); Weston
(flexible Wayland display server, supports Xwayland); Xwayland (compatibility layer for X11 apps
on Wayland); seatd (manages display/input resources for Weston); GL4ES (ptitSeb's library letting
GL1/GL2 apps run on GLES-only hardware, includes a GLX wrapper); Mesa (general-purpose Linux
GL(ES) library, software or virtualized); Gallium (a graphics driver within Mesa — Panfrost,
LLVMPipe, VirGL, Zink, v3d); VirGL (Mesa gallium driver virtualizing OpenGL, normally for VMs —
this setup's variant passes GL commands through a socket to a render server, final compositing on
CPU); LLVMPipe/Lavapipe (Mesa gallium software renderers, for OpenGL/Vulkan respectively); Zink
(Mesa gallium driver providing OpenGL on Vulkan-capable hardware); Crusty (custom wrapper bringing
GLX/Wayland EGL/GBM-DRMKMS support to devices lacking it, highly experimental); Mali/Panfrost
(graphics drivers for Rockchip handhelds — Panfrost mostly supports EGL Wayland extensions, Mali
mostly does not).

**Advanced tips**: unknown dependencies → try `drm gl kiosk llvmpipe` (with mesapack) first, slow
but very high compatibility; `headless noop kiosk crusty_glx_gl4es` has decent compat/perf for
GL2/GLES2 games; `strace -f -e file` for library-loading issues, `crustydebug=_debug` for GL
context issues, `gdb --args` for segfaults (see §2.5 for the full GDB workflow); unsetting
`LD_PRELOAD=` inside the westonwrap env can help in rare cases where the app loads libEGL/libGLX at
runtime; force an app to link against Crusty instead of system libs by patching `libEGL.so`/
`libGLX.so` references to `libCRU.so` in the binary (useful for GLAD EGL/GLX loaders that hardcode
those names):
```python
python3 -c "f=open('game_executable','r+b'); d=f.read().replace(b'libEGL.so', b'libCRU.so').replace(b'libGLX.so', b'libCRU.so'); f.seek(0); f.write(d); f.close()"
```

### 1.7.6 Pattern D — Godot 3 (FRT runtime)

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

runtime="frt_3.2.3"
if [ ! -f "$controlfolder/libs/${runtime}.squashfs" ]; then
  $ESUDO $controlfolder/harbourmaster --quiet --no-check runtime_check "${runtime}.squashfs"
fi
$ESUDO mount "$controlfolder/libs/${runtime}.squashfs" /tmp/${runtime}

$GPTOKEYB2 "${runtime}" -c "$GAMEDIR/portname.ini" &

"/tmp/${runtime}/${runtime}" --main-pack "$GAMEDIR/portname.pck"

$ESUDO umount /tmp/${runtime}
pm_finish
```

Verify the exact FRT binary/runtime filename against PortMaster's current runtime catalog before
shipping — version numbers in squashfs names do change. All control must go through gptokeyb2;
Godot's own joystick code is disabled in the PortMaster-specific FRT fork specifically because
Godot preconfigures controls and SDL grabs the keys before remap is possible.

### 1.7.7 Pattern E — Love2D

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

source "$controlfolder/runtimes/love_11.5/love.txt"   # provides $LOVE_BINARY / $LOVE_RUN / $LOVE_GPTK

$LOVE_RUN "$GAMEDIR/portname.love"

pm_finish
```

Verify the exact `love_<version>` runtime folder name against what's actually installed/available
— match it to whichever Love2D API version the game was actually built against
(11.4/0.10.2/0.9.2/0.8/0.7.2 all exist as separate runtimes with real API differences). If the
game manages its own mouse cursor under KMS/DRM/FBDEV (no cursor by default there), implement a
software cursor: load a cursor image in `love.load()`, `love.mouse.setVisible(false)`, draw the
cursor at the mouse position at the end of `love.draw()`, and scale mouse coordinates against the
render scale in `love.update()`.

### 1.7.8 Pattern F — GameMaker (gmloader-next)

**This pattern was substantially corrected in this revision** — an earlier version used a wrong,
invented env-var interface (`GMLOADER_DEPTH_DISABLE`, `GMLOADER_SAVEDIR`, `GMLOADER_PLATFORM`).
The real gmloader-next interface, confirmed from source (`JohnnyonFlame/gmloader-next`), is a JSON
config file. **Everything below is "verified from source," not "verified working on a real
device"** — no gmloader-next build/port has been through this project's full device-test pass yet.

GameMaker Studio (GMS) exports share the same bytecode/asset format across platforms
(`data.win` on desktop, `game.droid` on Android) interpreted by a per-platform native runner.
gmloader-next doesn't reimplement that runner — it takes the real Android runner binary
(`libyoyo.so`) unmodified, running it under glibc via a custom ELF loader plus an incomplete-but-
functional fake-JNI shim. **`gmloader-next` (not the old `droidports`/`gmloader`) is the correct
choice for this project's aarch64 target** — the old loader hardcodes ARMv7/Cortex-A9 flags with
no confirmed aarch64 path.

Build (needs a real LLVM/Clang library at build time, not just a GCC cross compiler, for the
softfp/hardfp thunk-generation step — confirm this chroot has a matching `libclang-*.so` + Python
`clang` bindings before a first attempt):
```bash
git clone --recursive https://github.com/JohnnyonFlame/gmloader-next.git
make -f Makefile.gmloader ARCH=aarch64-linux-gnu \
  LLVM_FILE=/usr/lib/llvm-11/lib/libclang-11.so.1 \
  LLVM_INC=/usr/aarch64-linux-gnu/include/c++/10/aarch64-linux-gnu \
  -j$(nproc)
```
Output: `build/${ARCH}/gmloader/gmloadernext.${DEVICE_ARCH}`.

`gmloader.json` config (all fields optional, parsed via nlohmann/json):
```json
{
    "save_dir" : "saves",
    "apk_path" : "game.port",
    "shader_dir": "./shaders/",
    "show_cursor" : false,
    "disable_controller" : false,
    "disable_depth" : false,
    "disable_extensions" : false,
    "disable_joystick" : true,
    "disable_rumble" : false,
    "disable_texhack" : false,
    "rumble_scale" : 1.0,
    "force_platform" : "os_android"
}
```
`apk_path` **must be a real zip (APK-shaped) archive containing `libyoyo.so`** at the path Android
would put it (e.g. `lib/arm64-v8a/libyoyo.so`) plus the game's `assets/` — it is not just a plain
assets bundle. Default `apk_path` if omitted is `"game.apk"`. `force_platform` accepts
`os_unknown`/`os_windows`/`os_macosx`/`os_ios`/`os_android`/`os_linux`/`os_psvita`/`os_ps4`/
`os_xboxone`/`os_tvos`/`os_switch`. Runtime CLI: `-c <path>` for the config, `-a <path>` overrides
`apk_path` directly.

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

$GPTOKEYB2 "gmloadernext" -c "$GAMEDIR/portname.ini" &

./gmloadernext.${DEVICE_ARCH} -c gmloader.json

pm_finish
```

**Real packaging structure deviates from this project's usual conventions in deliberate,
source-confirmed ways** (verified against a real working example package,
`GameMakerPorts/Test/portname.zip`):
- The loader binary and its **guest-side runtime libs** (`lib/arm64-v8a/`: `libc++_shared.so`,
  `libcompiler_rt.so`, `libm.so`, sourced from a prebuilt AOSP system image) ship *inside* the
  port folder — this is a guest (Android-ABI) library set the loader's own ELF loader resolves
  symbols against, not this project's usual host-side `libs.$DEVICE_ARCH` bundle. Don't confuse
  the two or "fix" this to match other ports.
- Licensing goes in **`license/` (singular)**, not this project's usual top-level `licenses/`
  (plural).
- `Portname.sh` does use this project's real boilerplate correctly. Two gmloader-specific
  additions worth keeping: `$ESUDO chmod +xwr "$GAMEDIR/gmloadernext.${DEVICE_ARCH}"` (and similar
  for `tools/splash`/`tools/patchscript`), and a `tools/splash` companion binary shown before the
  (potentially slow) first-run patch step below, since there's otherwise no feedback on screen.

**The blank-`.gptk`-template gotcha**: upstream's own template ships every control mapping
intentionally blank, which does **not** disable a button's compiled-in default keymap under
classic gptokeyb — a real port needs real mappings (or a gptokeyb2 `.ini`, per project policy,
which has no compiled-in default to worry about) or risks the double-input problem described in
§1.8.5, since GMS games commonly also read native `SDL_GameController` input directly.

**The first-run "patcher" step** (needs on-device/infra verification — treat as unconfirmed):
`Portname.sh` conditionally sources a PortMaster-provided hook the first time a port runs, which
runs the port's own `tools/patchscript`. From reading that script: it mounts two PortMaster
squashfs runtimes (`gmtoolkit.squashfs`, `dotnet-8.0.12.squashfs`), cleans junk out of `assets/`,
runs `UndertaleModCli.dll dump` to externalize the desktop `data.win` into an Android-format
`game.droid`, recompresses audio, and re-zips everything into `game.port`. This means the porter
seeds `assets/` with a real, legally-obtained extracted copy of the target game's desktop
`data.win` before first run — the patcher converts it on-device. Where `libyoyo.so` itself comes
from for a specific game is not fully confirmed from source alone; the likely intended workflow
(not traced end-to-end) is picking a wrapper APK matching the target game's GMS runtime version
from a bucketed collection of real GameMaker Android APKs, reused as a generic interpreter shell —
confirm hands-on before packaging a specific port, and apply the same redistribution scrutiny this
project applies to any bundled third-party binary before reusing one.

Check whether a target game's data is YYC (YoYo Compiler)-compiled before committing to a port —
YYC binaries are tightly bound to their original platform and kill portability; UndertaleModTool
flags this when you open the data file.

### 1.7.9 Pattern G — Pyxel

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

pyxel_runtime="pyxel_2.2.8"
python_runtime="python_3.11"
if [ ! -f "$controlfolder/libs/${pyxel_runtime}.squashfs" ]; then
  $ESUDO $controlfolder/harbourmaster --quiet --no-check runtime_check "${pyxel_runtime}.squashfs"
fi
if [ ! -f "$controlfolder/libs/${python_runtime}.squashfs" ]; then
  $ESUDO $controlfolder/harbourmaster --quiet --no-check runtime_check "${python_runtime}.squashfs"
fi
$ESUDO mount "$controlfolder/libs/${python_runtime}.squashfs" /tmp/${python_runtime}
$ESUDO mount "$controlfolder/libs/${pyxel_runtime}.squashfs" /tmp/${pyxel_runtime}

pyxel_dir="/tmp/${pyxel_runtime}"
source "${pyxel_dir}/bin/activate"
export PYTHONHOME="/tmp/${python_runtime}"
export PYTHONPYCACHEPREFIX="$GAMEDIR/conf/pycache"

$GPTOKEYB2 "pyxel" -c "$GAMEDIR/portname.ini" &

"${pyxel_dir}/bin/pyxel" play "${GAMEDIR}/gamedata/portname.pyxapp"

$ESUDO umount /tmp/${pyxel_runtime}
$ESUDO umount /tmp/${python_runtime}
pm_finish
```

Verify exact runtime version folder names against the current PortMaster runtime catalog.

### 1.7.10 Pattern H — mkxp-z (RPG Maker XP/VX/VXAce)

mkxp-z is a cross-platform reimplementation of the RGSS runtime (embedded MRI Ruby + C++ engine)
that plays RPG Maker XP/VX/VXAce games. One mkxp-z build is the *engine*; each individual RPG
Maker game gets its own PortMaster port bundling a copy of it plus that game's own data.
**Confirmed on real Mali-G31 hardware.**

Build is Meson-based, in two stages — `linux/Makefile` first statically builds ~15 third-party
libs (mostly lightly-patched forks) into `linux/build-<arch>/`, then Meson builds the app itself:
```bash
cd linux
make -j$(nproc)                # builds deps-core + ruby into build-<arch>/
source vars.sh                 # sets PKG_CONFIG_LIBDIR/PATH/LD_LIBRARY_PATH
cd ..
meson setup build -Dgfx_backend=gles -Denable-https=false --bindir=. --prefix=$PWD/build/local
cd build
ninja
ninja install
```
Use mkxp-z's own Ruby fork (`github.com/mkxp-z/ruby`, branch `mkxp-z-3.1.3`), not a stock build —
`linux/Makefile`'s `ruby` target clones it automatically. Ruby's own build needs a host "baseruby"
to bootstrap (`apt-get install ruby`, build-time only, never shipped). LTO makes this build very
slow under this chroot's QEMU emulation (the full Ruby build took over an hour in practice) — real,
not a hang; verify forward progress via `ps aux | grep lto1` rather than assuming it's stuck.

**Graphics: native GLES2 via SDL2, no gl4es needed.** `-Dgfx_backend=gles` is purely a compile-time
header/context-request switch (`SDL_opengles2.h` instead of `SDL_opengl.h`, requests a real
`SDL_GL_CONTEXT_PROFILE_ES` context) — no separate GLES library is linked, every GL function is
resolved dynamically via `SDL_GL_GetProcAddress` either way. Confirmed on real Mali-G31 hardware:
`GL Vendor: ARM / GL Renderer: Mali-G31 / GL Version: OpenGL ES 3.2` — zero gl4es involvement.

**SDL2 must be patched to link dynamically against the system SDL2**, or you bundle a bad one.
mkxp-z's own `linux/Makefile` clones and statically links its own SDL2 fork (X11+dbus, wrong for
this target). Two-part fix: in `src/meson.build`, force `dependency('SDL2', static: false)`; in
`linux/Makefile`, drop `sdl2` from the `sdl2image`/`sdlsound`/`sdl2ttf`/`deps-core` prerequisite
lists so their CMake configs fall through to the system SDL2 via pkg-config. Verify with
`readelf -d build/mkxp-z.aarch64 | grep NEEDED` — want `libSDL2-2.0.so.0` with no static SDL2
symbols baked in.

**Audio backend tuning** — don't just disable everything to save dependencies; ALSA is required
per `cfw_requirements.md` and stripping it caused a confirmed real-hardware regression
(`Could not detect an available audio device` even with `SDL_AUDIODRIVER` forced). Correct flags
(`linux/Makefile`'s CMake invocations):
```
-DALSOFT_BACKEND_ALSA=yes -DALSOFT_BACKEND_PULSEAUDIO=no -DALSOFT_BACKEND_OSS=no -DALSOFT_BACKEND_SDL2=yes
-Denable-openmp=no -Denable-alsa=no -Denable-pulseaudio=no -Denable-oss=no    # FluidSynth
```
OpenAL-soft loads backends via `dlopen()` at runtime, not link-time — `readelf -d` on the final
binary correctly shows no hard `libasound.so.2` dependency even with ALSA compiled in, so the
minimal-dependency goal and "ALSA works" aren't actually in tension.

**MIDI needs a bundled soundfont via mkxp-z's own config**, not an env var — `"midiSoundFont"` in
`mkxp.json` (talks to FluidSynth directly, unlike `SDL_mixer`'s `SDL_SOUNDFONTS` covered in §2.3).
Same `TimGM6mb.sf2` recommendation as §2.3 applies here.

**`mkxp.json`'s `"patches"` array** is the correct mechanism for shipping a fixed script without
re-encrypting/touching the original `Game.rgssad` — a list of directories/zips mounted with higher
priority than the main archive. Drop a patched, unencrypted `Data/Scripts.rxdata` at
`patches/Data/Scripts.rxdata`, set `"patches": ["patches"]`, and mkxp-z transparently prefers it.
RPG Maker XP's `Game.rgssad` (RGSSAD v1) is a simple XOR-keyed container (header `"RGSSAD\0"` + 1
version byte, 32-bit key starting at `0xDEADCAFE`, advancing via `key = key*7+3`) — not real
encryption, well-documented and extractable if a script fix is needed.

**Controls need no remapping at all** — `src/eventthread.cpp` already uses the high-level
`SDL_GameControllerAddMappingsFromFile`/`SDL_GameControllerOpen` API (the "known good" case, not
the `SDL_JoystickOpen` problem case from §2.1). A launch script only needs gptokeyb2 for the
Start+Select kill switch.

```bash
#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
get_controls

GAMEDIR=/$directory/ports/portname
BINARY=mkxp-z.${DEVICE_ARCH}

mkdir -p "$GAMEDIR/conf"
cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/portname.ini" &

pm_platform_helper "$GAMEDIR/$BINARY"

./$BINARY

pm_finish
```

Game data (`Game.ini`/`Data/`/`Graphics/`/`Audio/`) sits **flat, directly alongside the binary**
by default (`getDefaultGameRoot()` is just `SDL_GetBasePath()`), unless `mkxp.json`'s
`"gameFolder"` option is set. The Meson build's own install rpath (`$ORIGIN/lib64`) doesn't match
this project's `libs.$DEVICE_ARCH` convention, but it's a non-issue: the binary uses `DT_RUNPATH`,
and glibc's loader searches `LD_LIBRARY_PATH` before `RUNPATH`, so the standard
`export LD_LIBRARY_PATH=...` line above is all that's needed. **Minimal real `libs.aarch64/`
contents**: `libruby.so.3.1` (~9MB, hard dependency), plus `libcrypt.so.1` and `libffi.so.8`
(small transitive deps not on the near-universal list) — everything else (physfs, openal,
theora/vorbis/ogg, sdl2_ttf/image/sound, freetype, pixman, libpng, uchardet, fluidsynth) is
statically embedded into the engine binary. Ship the Ruby stdlib too
(`linux/build-<arch>/lib/ruby/3.1.0`, as e.g. `stdlib/`). `mkxp.json`'s `"enable-https"` (default
on) pulls in OpenSSL and forces GPLv3 licensing instead of GPLv2+ — set `-Denable-https=false`
unless a specific game's custom scripts actually need HTTP, since RPG Maker games essentially
never do.

### 1.7.11 .NET ports — three real shapes, not one recipe

A modern .NET game published with `dotnet publish -r linux-arm64 --self-contained
-p:PublishAot=true` produces one native ELF binary with no managed-runtime dependency at all —
this uses **Pattern A or B directly** (basic native binary, or gl4es standalone if the graphics
stack needs it), not a runtime-mount pattern. Which of A/B, and what else needs handling, depends
on the specific framework:

**Runtime choice** (pick based on target framework, before assuming Native AOT is always right):
- **`mono-6.12.0.122-aarch64.squashfs`**: the old Mono JIT, for legacy FNA/XNA-style ports. Only
  works for old-style .NET Framework/netstandard2.0-era assemblies — cannot load `net8.0`+
  assemblies using newer BCL/language features (e.g. `System.Threading.Lock`). Check the target's
  `<TargetFramework>` and grep for very-recent BCL types first.
- **`dotnet-8.0.12.aarch64.squashfs`**: a real .NET 8 shared runtime for framework-dependent
  deployment (`dotnet Game.dll`), if the project targets or can be retargeted to `net8.0`.
- **Native AOT, self-contained** (the common case): `dotnet publish -r linux-arm64
  --self-contained -p:PublishAot=true` (some projects gate this behind a custom MSBuild property
  like `EnableNativeAot` instead — check the `.csproj` first, don't assume the flag name). If the
  build chroot is itself aarch64, this is a native build, not a cross-compile — install the
  matching-arch SDK via `dotnet-install.sh` and publish directly. Prerequisites: `clang`,
  `zlib1g-dev`.

**Never bundle .NET's own pulled-in SDL2.** `dotnet publish --self-contained` copies MonoGame's
native SDL2 binary into the publish output; delete it before packaging — same rule as §1.1's
never-bundle-libSDL2 rule.

**A `BINARY=<name>` variable declared once and referenced everywhere** (gptokeyb2's argument,
`pm_platform_helper`'s argument, the final launch line) beats repeating the (often
arch-suffixed) executable filename three separate times inline.

**Port subfolder naming, `licenses/` placement, and image-path conventions are exactly as
described in §1.1-§1.3** — a real reference port's own layout has one extra nesting level specific
to how that port separates PortMaster-supplied vs. user-supplied files; don't copy that literally
if the port's data doesn't need the same split.

#### MonoGame.Framework.DesktopGL — needs gl4es, no way around it

Checked whether MonoGame or its forks can request a GLES context directly instead of needing
translation — they can't. `MonoGame.Framework.DesktopGL`'s context-creation code
(`GraphicsContext.SDL.cs`) always calls `Sdl.GL.CreateContext` with no ES option, throwing
`PlatformNotSupportedException` if it can't resolve FBO entry points. **KNI** (a MonoGame fork
with WebGL support) has the same gap on desktop. **FNA** (a different, non-MonoGame XNA
reimplementation) does have a native ES path (`FNA3D_OPENGL_FORCE_ES3=1`, no gl4es, no
Westonpack) — the technically cleanest end state if a game can be migrated to it, at the cost of a
real engine migration.

Build gl4es (no X11, GBM/EGL direct-to-screen):
```
cmake .. -DNOX11=ON -DGLX_STUBS=ON -DEGL_WRAPPER=ON -DGBM=ON
make
```
The FBO-requirement concern is a non-issue: gl4es explicitly advertises
`GL_ARB_framebuffer_object`/`GL_EXT_framebuffer_object` in its `GL_EXTENSIONS` string and
implements them via GLES2's native FBO support — verify with
`strings lib/libGL.so.1 | grep framebuffer_object`. Use Pattern B (§1.7.3) for the launch script,
including its `SDL_VIDEO_GL_DRIVER`/`SDL_VIDEO_EGL_DRIVER` explicit-pointer block, which is
genuinely required, not redundant, for MonoGame specifically (confirmed on real hardware:
`FramebufferHelper.Create()` throws without it even though gl4es itself initializes fine).

**Song content pipeline gotcha**: if the game uses `Song`/`MediaPlayer` (background music, not
`SoundEffect`), MGCB's `SongProcessor` output for DesktopGL is **two files** — a tiny `.xnb` stub
(~100 bytes) and a full-size companion audio file (e.g. `intro.ogg`) sitting next to it. `Song`'s
runtime playback opens that companion file directly by path; it is not embedded in the `.xnb`.
Easy to miss when assembling a minimal port tree — always check the publish output's `Content/`
folder for every file next to each processed asset, not just the `.xnb`.

**License check**: don't assume a NuGet package's own bundled `LICENSE` file is complete/correct
for the native binary it ships — `MonoGame.Library.OpenAL`'s nupkg includes only a BSD-3-Clause
notice for one ambisonics-related portion, while OpenAL Soft's actual primary license is LGPL v2.
Go to the actual upstream project's repo for the real license.

#### Raylib-cs — has a native GLES path, use it instead of gl4es

Raylib-cs is a genuine fork point from MonoGame: raylib **can** request GLES directly. Three
approaches were tried in order on a real port (`DasErbe2`) — don't repeat this exploration:

1. **`PLATFORM_DRM`** (raylib's own native KMS/GBM/EGL backend, no SDL2). Built cleanly, **failed
   on device**: `Failed to find a suitable DRM connector mode` / `Failed to initialize platform`,
   then a double-free crash. A second confirmed real case for the "never use an engine's own
   native DRM backend" rule (§1.7.4's Westonpack section exists partly because of this same class
   of failure).
2. **`PLATFORM_DESKTOP_SDL` + `OPENGL_VERSION=2.1` + gl4es** (mirroring the MonoGame recipe).
   Got much further (SDL2 init, gl4es loaded, textures/audio loaded) but **ended in a black
   screen**: `Shader: Compile error: Language version '120' unknown` and `FBO: Framebuffer has
   incomplete attachment`. Root cause (from reading `rlgl.h`): with `GRAPHICS_API_OPENGL_21`
   defined, raylib internally aliases to `GRAPHICS_API_OPENGL_33`, routing depth-renderbuffer
   creation into a code path that only picks a sized format under an ES2-only guard that never
   fires here — falls through to an unsized `GL_DEPTH_COMPONENT` token that GLES rejects. This is
   the confirmed source of a real bug specific to the OpenGL-2.1-through-gl4es combination on
   GLES-only hardware — don't use this combination for a Raylib-cs port even though it's what the
   MonoGame recipe effectively does under the hood.
3. **Winning recipe: `PLATFORM_DESKTOP_SDL` + `OPENGL_VERSION="ES 2.0"`, no gl4es at all.**
   ```
   cmake .. -DPLATFORM=SDL -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_EXAMPLES=OFF -DOPENGL_VERSION="ES 2.0"
   make -j"$(nproc)"
   ```
   raylib's `rcore_desktop_sdl.c` requests a genuine GLES2-profile context from SDL2 and calls the
   device's real GLESv2 entry points directly — no translation layer, and it takes the ES2-specific
   sized-format branch instead of the buggy one above. SDL2 still owns all windowing (satisfying
   the never-bypass-SDL2 rule). Confirmed working on two GPU vendors: RK3566/Mali-G31 and a Retroid
   Pocket 5 on ROCKNIX (Snapdragon 865/Adreno via freedreno). `readelf -d libraylib.so` shows only
   `libSDL2-2.0.so.0`/libc/ld-linux as `NEEDED` — no GLES/EGL link-time dependency.

Packaging consequence: **no** `gl4es.$DEVICE_ARCH/` folder, no `SDL_VIDEO_GL_DRIVER`/
`SDL_VIDEO_EGL_DRIVER`, no `libgl_default.txt` sourcing — plain Pattern A (§1.7.2), just
`libs.$DEVICE_ARCH/libraylib.so` and a normal `LD_LIBRARY_PATH` export.

Other Raylib-cs specifics: the NuGet package ships no `linux-arm64` native library at all (unlike
MonoGame's SDL/OpenAL packages) — `libraylib.so` must always be built from source, matching the
Raylib-cs version's own pinned upstream raylib tag (check its README, don't assume `master`).
raylib's audio is `miniaudio`, statically linked — no separate `libopenal.so` to bundle. Add
`#define MA_NO_PULSEAUDIO` next to raylib's existing `#define MA_NO_JACK` in `raudio.c` (same
PipeWire-probing friction as the general miniaudio gotcha in §2.3 — this is that finding's actual
source). On touchscreen devices, `export SDL_TOUCH_MOUSE_EVENTS=0` (§2.1) applies here too — raylib
itself is fine, the snap comes from SDL2 synthesizing mouse events from touch by default. Before
assuming a native-gamepad double-input risk (§2.1's `SDL_JoystickOpen` gotcha doesn't even apply
here — raylib uses the high-level API), grep for whether the specific game actually calls raylib's
gamepad functions at all; a mouse/keyboard-only game has nothing to conflict with gptokeyb2's mouse
emulation.

#### Old .NET Framework + Mono squashfs + hand-rolled SDL2 P/Invoke — cautionary case study, not a recommended pattern

**High risk. The real attempt this is drawn from was ultimately abandoned** after real progress
(built, ran on hardware, reached the main menu, survived three separate confirmed crashes) because
the three-layer debugging surface (managed C#, a hand-rolled P/Invoke SDL2 binding, and whatever
native `.so` actually loads at runtime) made every new symptom a multi-hour excavation with no
guarantee of being the last one. This is a different problem shape from the MonoGame/Native-AOT
case above — it applies to old-style `.NET Framework 4.5.2`-class projects with their own
hand-rolled SDL2 binding, not a maintained framework. Budget for this depth of debugging, or
prefer a target where the binding layer is maintained (FNA, MonoGame) over a single game's own
bespoke SDL2 wrapper.

What's still worth reusing if this shape of port is attempted: the **Mono launch pattern itself**,
validated end-to-end (game reached its main menu on real hardware). Build fixes needed on every
project in the dependency chain: add
`<PackageReference Include="Microsoft.NETFramework.ReferenceAssemblies" Version="1.0.3">`
build-time-only, and **force `<PlatformTarget>` from `x86` to `AnyCPU`** if set (a real confirmed
blocker — `x86` sets a 32-bit-required CLR flag that won't load on ARM64 Mono). Launch script:
```bash
monodir="$HOME/mono"
mono_runtime="mono-6.12.0.122-aarch64"
monofile="$controlfolder/libs/${mono_runtime}.squashfs"
if [ ! -f "$monofile" ]; then
  if [ ! -f "$controlfolder/harbourmaster" ]; then
    pm_message "This port requires the latest PortMaster to run, please go to https://portmaster.games/ for more info."
    sleep 5
    exit 1
  fi
  $ESUDO $controlfolder/harbourmaster --quiet --no-check runtime_check "${mono_runtime}.squashfs"
fi
$ESUDO mkdir -p "$monodir"
$ESUDO umount "$monofile" || true
$ESUDO mount "$monofile" "$monodir"
```
**The runtime provisioning guard is not optional**, even though the real upstream `Celeste.sh` this
pattern derives from doesn't visibly include one — launching outside PortMaster's own install flow
skips the runtime download otherwise. `$GPTOKEYB2`'s kill-switch target should be `"mono"`, the
actual OS process name, not the game's exe name.

**`SDL_CreateRenderer` auto-picking the Mali/GLES2 renderer is a hard `SIGSEGV` under Mono**, not
the soft `SDL_CreateTexture` error from §2.2 — same root cause, worse failure mode since a
P/Invoke crash mid-native-call has no managed exception to catch. Same fix: force
`SDL_RENDERER_SOFTWARE`. If porting any game to this device class that creates its own
`SDL_Renderer`, force software rendering preemptively.

Reusable debugging technique from this attempt: when a Mono crash produces a `mono_crash.json`
with managed-stack tokens but no method names and no `monodis`/`ildasm` is available, a from-scratch
ECMA-335 metadata table reader can decode `MethodDef` tokens to real names directly from the
crashing assembly's own bytes (cross-check the assembly's MVID against the crash report's `guid`
to confirm you're decoding the exact binary that crashed). Watch for an off-by-one in the
Event/Property table region (`0x12 EventMap` through `0x19 MethodImpl`) if writing one — it
silently corrupts every later table's row-size calculation, including the commonly-present
`NestedClass` (`0x29`).

### 1.7.12 sdl3-sdl2-backend — for genuinely SDL3-only engines

A fork of the SDL3 source tree (`bmdhacks/SDL`, branch `sdl2-backend`) that replaces SDL3's video/
audio/joystick backends with ones that `dlopen` the device's already-installed system SDL2 at
runtime; GPU is a standalone GLES 3.x backend via SPIRV-Cross that does not go through SDL2 at
all. **Only reach for this when an engine is genuinely SDL3-only with no SDL2 path** — if it has
an SDL2 path (MonoGame/raylib effectively do), build against system SDL2 directly instead, no
translation layer needed.

**Status: real-device tested (RG351V/AmberELEC), two confirmed bugs found and fixed** — a build-
flag issue and a bug in the shim's own source (below). Still unverified: whether the dlopen-to-
system-SDL2 path is fully correct end-to-end for audio/joystick/GLES rendering beyond "starts
without crashing," and which `SDL3SHIM_*` env var values a given CFW needs.

**Critical: never tune `-mcpu` to a single SoC**, despite what the fork's own README recommends.
This project ships one binary across its entire device matrix. A build with `-mcpu=cortex-a55`
(the project's primary RK3566 target) crashed with `SIGILL` on an RG351V (Cortex-A35) — an inlined
ARMv8.1 LSE atomic instruction (`swpa`) that Cortex-A35 can't execute. Fix: build with the generic
`-march=armv8-a` baseline, which makes GCC emit libgcc outline-atomics helper calls that check CPU
support at runtime instead of inlining the instruction.

```bash
git clone --recursive https://github.com/bmdhacks/SDL.git -b sdl2-backend
cd SDL
git clone https://github.com/KhronosGroup/SPIRV-Cross.git /path/to/SPIRV-Cross
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-march=armv8-a" \
  -DSDL_SDL2_BACKEND=ON \
  -DSDL_SPIRV_CROSS_DIR=/path/to/SPIRV-Cross \
  -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF \
  -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF \
  -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_JACK=OFF \
  -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF \
  -DSDL_DUMMYAUDIO=OFF -DSDL_DISKAUDIO=OFF \
  -DSDL_VULKAN=OFF -DSDL_GPU=ON -DSDL_RENDER_GPU=ON \
  -DSDL_UNIX_CONSOLE_BUILD=ON
make -j$(nproc)
```
The last flag is **not mentioned in the fork's README** but is required — without it, SDL's own
build summary hard-errors demanding X11/Wayland dev libraries even with all display backends off,
since it doesn't otherwise know a windowless/console-style Unix build is intentional.

Runtime env vars the shim reads: `SDL3SHIM_SDL2_LIB` (path to dlopen), `SDL3SHIM_SDL2_VIDEODRIVER`,
`SDL3SHIM_SDL2_AUDIODRIVER`. **Do not hardcode `SDL3SHIM_SDL2_VIDEODRIVER`/`_AUDIODRIVER` in a
launch script** — same failure shape as the hard `SDL_VIDEODRIVER` rule in §2.2 (baking in one
CFW's correct value can break a different CFW/device), even though this is a distinct mechanism
(tells the shim which driver name to hand to SDL2, not SDL2's own env-based selection). Confirmed
real bug in the shim's own event code: `SDL_sdl2events.c`'s `SDL2_MOUSEMOTION` case used
`e.motion.which != 0` (the device instance ID, not a relative-mode flag) to decide relative vs.
absolute mouse motion, and passed absolute `x`/`y` even when relative was intended instead of
`xrel`/`yrel` — the cursor raced toward one corner. Fixed via a local patch tracking real
relative-mode state through `SDL2Mouse_SetRelativeMouseMode()`; reapply on any fresh clone.

Packaging: ship the built `libSDL3.so` in `libs.aarch64/` — **never** ship a bundled SDL2 alongside
it, same shadowing risk as the `sdl12-compat` gotcha. The shim is a build-standalone, swap-in
artifact — the consuming port's own build should keep linking dynamically against real upstream
SDL3, never point its own dependency fetch at this fork.

## 1.8 The control-mapping file

### 1.8.1 gptokeyb2 `.ini` — the format to use for every new port

Two section types: `[config]` (global settings, read once) and `[controls]`/`[controls:name]`
(button bindings — `[controls]` is the default/root state, additional named states are
`[controls:hotkey]` etc. for layered input, see §1.8.3). `#` comments are safe to use in `.ini`
files (confirmed throughout real shipped configs) — unlike classic `.gptk`, where comment support
was never confirmed.

**`[config]` fields** (real clamped ranges from source):
```ini
[config]
repeat_delay        = 500     # ms, clamped 16-3000
repeat_rate         = 60      # ms, clamped 16-3000
mouse_slow_scale    = 50      # 1-100, default 50 — how much mouse_slow/R-analog-slow scales speed
deadzone_mode       = axial | scaled_radial
deadzone_scale      = 8       # 1-32768 — real ports use 6-18
mouse_scale         = 512     # LITERAL ALIAS for deadzone_scale, same field — don't set both expecting two effects
deadzone            = 2000    # sets BOTH deadzone_x and deadzone_y at once, 500-32768
deadzone_x          = 2000    # X-axis only, 500-32768
deadzone_y          = 2000    # Y-axis only, 500-32768
deadzone_triggers   = 3000    # L2/R2 analog trigger deadzone, 500-32768
mouse_delay         = 16      # ms, clamped 16-3000
dpad_mouse_normalize = true   # normalizes diagonal d-pad-as-mouse speed
mouse_wheel_amount  = 3       # set inside [controls]/[controls:x] blocks, not [config]

# Absolute-mouse-mode tuning (only relevant if a control uses absolute_mouse_movement):
absolute_center_x, absolute_center_y, absolute_step, absolute_deadzone,
absolute_rotate, absolute_screen_width, absolute_screen_height

# Custom charset/wordset registration (built-ins listed below need no registration):
charset = "hex" "0123456789abcdef"
wordset = "level_select" "01" "02" "03" ... "10"
```
**Confirmed**: `mouse_scale` writes `current_state.deadzone_scale` directly in `config.c` — it is a
pure alias, not a separate curve-shaping vs. movement-divisor pair the way classic gptokeyb had
(see §1.8.5). Setting both just means "last one wins." Built-in charsets need no registration:
`basic` (A-Z, space, `.,-_()`, digits), `extended` (`basic` + lowercase a-z), `full` (`extended` +
symbols), `digits`, `hex`, `alpha`.

**`[controls]` / `[controls:name]` button syntax**: button names are
`a b x y l1 l2 l3 r1 r2 r3 start back guide up down left right dpad left_analog right_analog
left_analog_up/down/left/right right_analog_up/down/left/right`. A field left blank (`y =`) or
omitted entirely is genuinely inert here — gptokeyb2's config structs start fully zeroed
(`gptk_malloc()` is `malloc()`+`memset(0)`), so there is **no** compiled-in default keymap to leak
through, unlike classic gptokeyb (§1.8.5). Values:
- A key name, quoted or unquoted: `a = "mouse_left"` or `a = mouse_left` — both forms are used in
  real shipped configs, quoting is cosmetic.
- `mouse_left` / `mouse_right` / `mouse_slow` (holds to halve cursor speed for precision).
- `dpad = mouse_movement` / `left_analog = mouse_movement` / `right_analog = mouse_movement` —
  shorthand binding all 4 directions of that input source to mouse movement at once.
- `right_analog = arrow_keys repeat` — binds all 4 directions to arrow keys with auto-repeat.
- `left_analog = absolute_mouse_movement` — moves the mouse to an absolute screen position based
  on stick deflection (uses the `absolute_*` config fields), instead of relative movement.
- A key plus a modifier token, space-separated: `l2 = f1 add_ctrl` (holds Ctrl while sending F1).
- State-machine verbs: `hold_state <name>` (switch to state `<name>` while held, revert on
  release), `push_state <name>` (switch and push current state onto a stack, for toggle-style
  layers), `pop_state` (pop back to the pushed state). `overlay = clear | parent | controls`
  controls how a named state's bindings relate to another's: `clear` starts with zero inherited
  bindings, `parent` inherits from whatever pushed/held into this state, `controls` inherits from
  the root `[controls]` state.
- Text-entry verbs inside a `text_input`-style state: `prev_letter`, `next_letter`, `add_letter`,
  `remove_letter`/`"backspace"`, `finish_text`, `cancel_text`, `prev_word`, `next_word`.

**Two real worked templates**, based directly on shipped ports:

Safe default (no mouse, digit/menu-driven game — shaped like `test.gptk.ini`):
```ini
[config]
deadzone_triggers = 3000
deadzone_mode = axial
deadzone = 2000
deadzone_scale = 8
mouse_delay = 16

[controls]
a =
b =
x =
y =

l1 =
l2 =
r1 =
r2 =

start = "enter"
back = "esc"

up = "up"
down = "down"
left = "left"
right = "right"

left_analog =
right_analog = arrow_keys repeat
```

Mouse-emulation (point-and-click UI, based directly on the real shipped `amuletsarmor.ini`):
```ini
[config]
mouse_delay = 16
deadzone_mode = scaled_radial
deadzone_scale = 6
deadzone_x = 2000
deadzone_y = 2000

[controls]
a = "mouse_left"
b = "mouse_right"
x = mouse_slow
y = "leftctrl"

l1 = "leftshift"
l2 = "f12"
r1 = "space"
r2 = "e"

start = "enter"
back = "esc"
guide =

up = "w"
down = "s"
left = "a"
right = "d"

left_analog = mouse_movement
right_analog =
```
This is genuinely simpler than the classic-`.gptk` mouse template used to be — no inert-placeholder
trick needed at all, since blank fields are safely inert under gptokeyb2. Map the D-pad to movement
as well when the game doesn't need it for anything else — some handhelds have no analog stick.

**`TEXTINPUTINTERACTIVE`** (interactive D-Pad text entry, trigger Start+D-Pad Down) still
auto-works in gptokeyb2 with **zero authoring**, but only when the `.ini` has **zero** `[controls]`/
`[controls:...]` section headers at all — the gate flag flips permanently false the instant the
parser sees even one real section header. Any mouse-emulation or state-machine `.ini` (i.e. nearly
every new port) loses that free compat path and needs an equivalent `[controls:text_input]` state
hand-authored using the text-entry verbs above (see the worked examples in §1.8.3). For real
full-QWERTY text entry (character names, chat, server IPs) beyond D-Pad letter-cycling, see §1.10
(OmniOSK).

### 1.8.2 gptokeyb vs gptokeyb2 — what actually differs

PortMaster exposes both as separate binaries via separate `control.txt` variables (`$GPTOKEYB`
classic, `$GPTOKEYB2` successor) — a launch script picks one explicitly, it's not auto-resolved
per device.

- **Invocation shape and kill-switch mechanism are identical**: pass the binary/process name as
  the first positional argument alongside `-c`, no `-1`/`-k`/`-X` flag needed:
  `$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/portname.ini" &`. Neither tool needs `-c` at all if no config
  file is used.
- **Confirmed bug in both tools: the kill switch silently fails when the target name is longer
  than 15 characters.** The Linux kernel's `comm` field (`TASK_COMM_LEN`) is hard-limited to 15
  usable characters, and both `pkill` and `ps` match against this truncated name, not the real
  filename. A binary named `arcanumce.aarch64` (17 chars) shows up in `ps`/`top` as
  `arcanumce.aarch` — silently missing the trailing `64`, so a `pkill` search for the full name can
  never match. Confirmed real-device symptom: Start+Select did nothing, and worse, all controller
  input then stopped working entirely (a manual process kill was needed to recover). **Fix**: pass
  a shorter identifying *prefix* (comfortably under 15 characters, still a literal prefix of the
  real name) as gptokeyb2's first positional argument — it only needs to be a string `pkill` can
  find as a substring of the truncated `comm`, nothing else uses that argument:
  ```bash
  $GPTOKEYB2 "arcanumce" -c "$GAMEDIR/arcanumce.ini" &
  ...
  pm_platform_helper "$GAMEDIR/$BINARY"     # still needs the real, full $BINARY
  "$GAMEDIR/$BINARY"                        # still needs the real, full $BINARY
  ```
  This does **not** affect `pm_platform_helper`/Wayland `app_id` matching — that's a separate
  name-resolution path (`readlink("/proc/pid/exe")`, the full untruncated path), not the kernel
  `comm` field. If a port's binary/process name is ever longer than 15 characters, verify
  Start+Select actually quits on a real device before shipping; don't assume the standard pattern
  just works.
- **The double-input trap (§1.8.5) is classic-gptokeyb-only.** gptokeyb2's config structs start
  fully zeroed — a button never mentioned in any config file simply does nothing. The safe-default
  templates in §1.8.1 don't need the inert-placeholder trick classic gptokeyb requires.
- **`TEXTINPUTINTERACTIVE` works in both, but the gate differs** — see §1.8.1 above for
  gptokeyb2's specific condition (zero `[controls]` sections).
- `$GPTOKEYB2` (like `$GPTOKEYB`) is a multi-word command line, not a bare path — never quote it.

### 1.8.3 Layered controls and text entry — worked examples

Hotkey layering, real pattern from gptokeyb2's own advanced-usage docs:
```ini
[controls]
a = "mouse_left"
b = "mouse_right"
x = "q"                      # secondary action
y =

l1 =
l2 = "end"                   # rotate CCW
l3 =

r1 = mouse_slow
r2 = "home"                  # rotate CW
r3 =

start = "p"                  # pause
back = "esc"

dpad         = mouse_movement
left_analog  = mouse_movement
right_analog = arrow_keys repeat

hotkey = hold_state hotkey

[controls:hotkey]
l1 = u                       # decrease game speed
l2 = f1 add_ctrl              # save
r1 = +                        # increase game speed
r2 = f1                       # load
```

Simultaneous-hold combined state (L2+R2 both held produces a third state):
```ini
[controls]
overlay = clear
x = i
b = k
l2 = hold_state hotkey1
r2 = hold_state hotkey2

[controls:hotkey1]
overlay = parent
r2 = hold_state hotkey_both
x = g

[controls:hotkey2]
overlay = parent
l2 = hold_state hotkey_both
x = e

[controls:hotkey_both]
overlay = parent
x = p
```

Text entry state (hand-authored, needed the moment any real `[controls]` section exists):
```ini
[controls]
overlay = clear
back = hold_state text_input

[controls:text_input]
charset = extended
start = pop_state
up = prev_letter
down = next_letter
right = add_letter
left = "backspace"
a = "enter"
b = "backspace"
```

Word-list entry (digit codes, cheat words — define the wordset in `[config]`, reference by name):
```ini
[config]
wordset = cheats "enable_cheats" "set god" "unset god"

[controls:word_input]
overlay = clear
wordset = cheats
right_analog_up = "f1"
right_analog_left = "prev_word"
right_analog_right = "next_word"
right_analog_down = "finish_text"
```
(`charset =` and `wordset =` can't both be set in the same controls group.)

### 1.8.4 Choosing button placement — decision framework

Written after a real mapping pass got called out as unusable (Activate on L1, Run on R1, leaving
Jump/Shoot/weapon-cycle with no ergonomic relationship). Scope: 2D, non-analog games (platformers,
top-down action) driven through digital buttons/d-pad. Dual-stick 3D shooters have their own,
different conventions (triggers for fire, bumpers for secondary fire, stick clicks for
sprint/crouch) — don't cross-apply either direction.

**Critical PortMaster-specific caveat**: PM handhelds physically use **Nintendo-style** face-button
positions (confirmed on real hardware, RG351V/AmberELEC/OSH PB Controller: **A=east, B=south,
Y=west, X=north**, the SNES diamond), even though gptokeyb2's config keys are still literally
named `a`/`b`/`x`/`y`. The logical letter names do **not** auto-remap to compensate. The rest of
this framework (and the accessibility-tier research it cites) is written assuming the **Xbox**
diamond (A=south, B=east, X=west, Y=north) as its reference convention. **Practical instruction**:
get the real physical layout from whoever is testing on-device first (don't assume either
convention), then assign the logical `a`/`b`/`x`/`y` config keys so that whichever *physical*
positions are most reachable (the Tier 1 pair below, physically) carry the Tier 1 actions — don't
bind an action to the literal letter "A" and assume it lands physically south.

**Core principle**: two questions decide placement — (1) press frequency relative to the game's
other actions (most-pressed → most reachable button); (2) what it's combined with (actions
frequently held/pressed together belong on buttons the same thumb can roll between without
leaving the face-button cluster, not split across a face button and a shoulder button).

**Face buttons** (Xbox-reference terms):
- **A (south)** = the single most core action (almost always Jump, or primary confirm/interact) —
  matches player muscle memory by default.
- **A+X is the most-reachable pair, not A+B.** Game Developer's "Designing Game Controls"
  documents an explicit Xbox 360 accessibility-tier system: **Tier 1 (most accessible) = A, X, and
  the sticks; Tier 2 = B, Y, D-pad; Tier 3 (least accessible) = Start/Back, LT/RT.** The two
  most-frequent/most-combined-with-movement actions go in Tier 1 together (A+X). Historical
  precedent, position-corrected: SNES Super Mario World puts Jump=B, Run(hold)=Y on the *SNES*
  diamond (B=bottom, Y=left, A=right, X=top) — translated to Xbox-diamond physical positions
  (bottom=A, left=X), that's Jump=A/Run=X, independently landing on the same A+X pair. Carry over
  the *physical positions*, not the letters.
- Within A+X, which slot goes to Run vs. a primary attack depends on whether it's a **continuous
  hold-alongside-movement** action (earns the Tier-1 slot, functionally like Jump) vs. a
  **discrete burst-fired** combat action (fits Tier 2/B fine even at similar raw press count).
- **Y (north)**, Tier 2, is right for the *least*-frequent face-button action.

**Shoulder buttons (L1/R1)**: best for cycling/toggling a fixed option set (prev/next weapon,
prev/next item) — L1=previous, R1=next is a near-universal convention. Don't put a
frequently-combined-with-movement action on a shoulder button when a face button is available —
reaching a shoulder while holding a face button is harder than rolling a thumb between two
adjacent face buttons. L2/R2 can be left unbound rather than forcing a low-value action onto them.

**D-pad**: in a 2D game with no analog requirement, it's the natural movement input (and doubles
for menu nav) — don't overload it with extra action bindings alongside movement.

**Worked example** (`amphetamine`, 4 real-time actions plus weapon cycling): `a = space` (Jump,
south), `b = leftctrl` (Shoot, adjacent), `x = c` (Run-hold, adjacent-other-side, Tier-1 pair with
Jump), `y = tab` (Activate, least-frequent, opposite corner), `l1`/`r1` = prev/next weapon. This
replaced an earlier version (Activate on L1, Run on R1 — no ergonomic relationship at all), and a
second rejected proposal (Jump=A/Run=B/Shoot=Y — checked against sources and rejected since both
the accessibility-tier research and the corrected SNES precedent independently point to A+X, not
A+B).

### 1.8.5 Classic `.gptk` (legacy — reading/maintaining an already-shipped port only, do not use for a new port)

Kept for reading or revisiting a port that already ships classic gptokeyb — **new ports use
gptokeyb2's `.ini` format (§1.8.1) per project policy.**

**Recognized key names** (from gptokeyb's own source):
- Arrow keys: `up`, `down`, `left`, `right`
- Special: `mouse_left`, `mouse_right`, `space`, `esc`, `end`, `home`, `shift`/`leftshift`,
  `rightshift`, `ctrl`/`leftctrl`, `rightctrl`, `alt`/`leftalt`, `rightalt`, `backspace`, `enter`,
  `pageup`, `pagedown`, `insert`, `delete`, `capslock`, `tab`, `pause`, `menu`
- Alphanumeric: `a`-`z`, `0`-`9`
- Numpad: `numpad0`-`numpad9`, `numpad*`, `numpad/`, `numpad+`, `numpad-`, `numpad,`, `numpad.`,
  `numpadenter`
- Function keys: `f1`-`f12`
- Symbols: `@ # % & * - + ( ) ! " ' : ; / ? . , ~ \` | { } $ ^ _ = [ ] \ < >` — each maps to the
  base key plus SHIFT modifier where noted in source
- Unrecognized strings resolve to keycode 0 (no mapping) — no error, just silently does nothing.

**The double-input trap: compiled-in default keymaps are NOT cleared by blank config values.**
This is the single most important classic-gptokeyb-specific gotcha, and it does **not** apply to
gptokeyb2 at all (gptokeyb2's config structs start fully zeroed via `gptk_malloc()`'s
`memset(0)` — there is no struct-literal default keymap anywhere in its design). Under classic
gptokeyb, every button has a compiled-in *default* key mapping, populated before any config file
is read:
```
a = KEY_X, b = KEY_Z, x = KEY_C, y = KEY_A
l1 = KEY_RIGHTSHIFT, l2 = KEY_HOME, r1 = KEY_LEFTSHIFT, r2 = KEY_END
up/down/left/right = arrow keys
left_analog_up/down/left/right = KEY_W/KEY_S/KEY_A/KEY_D
back = KEY_ESC, start = KEY_ENTER, guide = KEY_ENTER
```
A blank value (`a = ` with nothing after `=`) does not clear the mapping — the parser's
`fscanf(fp, "%s = %s", ...)` fails to match two tokens and the line is skipped, leaving whatever
was already in that struct field untouched. A key entirely absent from the file behaves
identically, for a more basic reason: `readConfigFile()` never resets or zeroes the config struct
at all — it only mutates fields it actually finds. **So a `.gptk` file that "leaves buttons blank"
does not actually disable them.** For a game that recognizes vi/WASD-style movement letters as
arrow-key synonyms, the compiled default `y = KEY_A` means pressing Y (bound in-game to something
unrelated via native `SDL_GameController`) will *also* synthesize a keyboard `a` press, which the
game's own keyboard handler may recognize as "move left" — a real, silent, simultaneous wrong
input.

**Don't fix this by mapping to the "same" key either** (`up = up`) — gptokeyb is a separate
process injecting through a separate virtual input device with its own poll/uinput latency; its
synthetic event isn't guaranteed to land in the same frame as the native one, so cross-frame
overlap can leave a bit looking "still pressed" a frame after the real release.

**The actual fix**: ship the full field list (every button and analog direction present), but
never assign a real, game-recognized key to a field just because the compiled default happens to
already match the native action. Every field gets the same inert placeholder value unless it
specifically needs a different one:
```
back = \"
start = \"
a = \"
b = \"
x = \"
y = \"
l1 = \"
l2 = \"
r1 = \"
r2 = \"
up = \"
down = \"
left = \"
right = \"
left_analog_up = \"
left_analog_down = \"
left_analog_left = \"
left_analog_right = \"
right_analog_up = \"
right_analog_down = \"
right_analog_left = \"
right_analog_right = \"
deadzone_mode = scaled_radial
deadzone = 2000
deadzone_scale = 8
deadzone_delay = 16
```
`\"` (a backslash-escaped quote) is the community-standard inert placeholder — confirmed against
gptokeyb2's own shipped `test.gptk` example. For each button, ask: (1) does the game's own
`SDL_GameController` code already handle it natively? If yes, the placeholder is enough regardless
of the compiled default. (2) If not read natively, does the compiled default collide with a key
the game's keyboard handler recognizes for something else? Either way the placeholder handles it.
Don't include a `guide` line at all (not even the placeholder) unless the target device population
genuinely has a working physical Guide button reaching the game — most PortMaster handhelds don't.
Don't use `;`/`#` comment lines inside a `.gptk` file — never confirmed the parser treats either as
a comment marker.

**Mouse-emulation variant** (classic format):
```
back = esc
start = enter
a = mouse_left
b = mouse_right
x = mouse_slow
y = \"
l1 = \"
l2 = \"
r1 = \"
r2 = \"
up = up
down = down
left = left
right = right
left_analog_up = mouse_movement_up
left_analog_down = mouse_movement_down
left_analog_left = mouse_movement_left
left_analog_right = mouse_movement_right
right_analog_up = \"
right_analog_down = \"
right_analog_left = \"
right_analog_right = \"
deadzone_mode = scaled_radial
deadzone = 2000
deadzone_scale = 8
deadzone_delay = 16
mouse_scale = 512
mouse_delay = 16
```
`x = mouse_slow` holds to slow cursor movement for precision. Map the D-pad to `mouse_movement_*`
too when the game doesn't need it for anything else.

## 1.9 gamepad-picker — reusable startup-choice tool

A small, generic, standalone SDL2 tool for showing a gamepad-navigable choice screen before a
port's main game launches (language, difficulty, game variant — anything with a short list of
options). Source at `/root/compile/gamepad-picker/gamepad-picker.c`. Not tied to any one port —
build once, copy the compiled `gamepad-picker.$DEVICE_ARCH` binary into any port that needs a
startup picker. Carries zero external assets: no bundled font, no shared library beyond `libSDL2`
itself.

**Why it exists**: PortMaster launch scripts have no other reliable, portable way to show an
interactive multi-choice menu — raw tty+`dialog` isn't supported everywhere, and PortMaster's own
IPC dialog system (`pugwash`, via `$PM_PIPE`) only supports message boxes, not real selection
menus.

**Design**: CLI contract `./gamepad-picker.$DEVICE_ARCH "Label 1|command 1" "Label 2|command 2"
...` — each argument is a label and a full command line, split on the first `|` (an argument with
no `|` is used as both). On confirm, prints the *chosen command line* to stdout and exits 0; on
cancel, exits 1 with no output. Input via `SDL_GameControllerOpen` directly (not gptokeyb — no
gptokeyb involvement for the picker step itself), genuinely cross-device via
`$sdl_controllerconfig`. Rendering via `SDL_RENDERER_SOFTWARE`, no GL/GLES — fully independent of
whatever graphics stack the actual game needs. Text via an embedded 8x8 monochrome bitmap font
(public domain, `font8x8_basic.h`), compiled directly in.

**Result-capture gotcha**: don't use `LAUNCH_CMD=$(./gamepad-picker ...)` — unrelated stray stdout
(a graphics-library init banner, a CFW version string) can corrupt the captured value. Use
`GAMEPAD_PICKER_OUTPUT` set to a file path instead, and read the result from there:

```bash
# ... standard controlfolder/control.txt/get_controls/GAMEDIR/BINARY/log-capture/cd, unchanged ...

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

export GAMEPAD_PICKER_OUTPUT="$GAMEDIR/.picker_result"
./gamepad-picker.${DEVICE_ARCH} "Deutsch|./$BINARY" "English|./$BINARY --language en" > /dev/null
LAUNCH_CMD=$(cat "$GAMEPAD_PICKER_OUTPUT")
LAUNCH_CMD="${LAUNCH_CMD:-./$BINARY}"

export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"

$GPTOKEYB2 "$BINARY" -c "./portname.ini" &
pm_platform_helper "$GAMEDIR/$BINARY"
$LAUNCH_CMD
pm_finish
```

Don't hardcode `$BINARY` as gptokeyb2's kill target if different picker choices could launch
genuinely different binaries — resolve the kill target from `$LAUNCH_CMD` instead in that case.

## 1.10 OmniOSK — on-screen keyboard overlay

Use when a port needs real text entry (character names, save names, chat, server IP) and
gptokeyb2's own `TEXTINPUTINTERACTIVE`/`text_input` state (D-Pad letter-cycling, §1.8.1/§1.8.3)
isn't good enough UX — a full QWERTY grid instead of cycling one letter at a time. A
controller-navigable Dear ImGui overlay, loaded via `LD_PRELOAD` into the target game's process.

### 1.10.1 Two variants — pick by what SDL the target game actually runs on

- **`binarycounter/OmniOSK`** (original, real SDL2 rendering) — local checkout
  `/root/compile/OmniOSK`. Use for anything whose actual runtime SDL is real SDL2, **including a
  game reached through `sdl12-compat`/SDL1.2 emulation**.
- **`Cebion/OmniOSK_sdl3`** (fork, SDL3 rendering via the `sdl3-sdl2-backend` shim, §1.7.12) —
  local checkout `/root/compile/github/OmniOSK_sdl3`. Use **only** for a game that is genuinely
  SDL3-native itself (e.g. `arcanumce`).

Check with `readelf -d libomni_osk.so | grep NEEDED`: the original shows `libSDL2-2.0.so.0`, the
fork shows `libSDL3.so.0`. **Picking the SDL3 fork for an sdl12-compat game produced a confirmed
real `*** stack smashing detected ***` crash** (two incompatible SDL major versions loaded in one
process). Switching to the real-SDL2 original removes that crash, but getting the *original*
variant working against an sdl12-compat game needs the bridge below.

### 1.10.2 The sdl12-compat bridge — every fix confirmed on real hardware

Root cause behind every fix in this section: `sdl12-compat` resolves its own calls into real SDL2
via a private `RTLD_LOCAL` `dlopen`+`dlsym` (`LookupSDL20Sym`), never through the process's normal
global PLT/GOT table that `LD_PRELOAD` interposition depends on. Anything `sdl12-compat` does
internally to real SDL2 is invisible to a preloaded library. Needs `-DOMNI_SDL12_BRIDGE=ON`
(requires real `SDL/SDL.h` on disk). Confirmed fixes, in order:

1. **Event struct layout mismatch**: SDL 1.2's `SDL_Event.type` is `Uint8`, SDL2's is `Uint32` —
   a whole-struct layout difference, not just different keysym numbers. Fixed via a translation
   unit (`sdl12_bridge.c`) compiled against real SDL 1.2 headers that decodes the raw buffer and,
   for KEYDOWN/KEYUP, round-trips through SDL 1.2's `SDL_GetKeyName()` → SDL2's
   `SDL_GetKeyFromName()` name strings — automatically correct for the whole keyspace without a
   hardcoded numeric table.
2. **No presentation-target discovery**: a game using old `SDL_SetVideoMode()` never calls the
   window/renderer creation functions OmniOSK normally hooks. Fixed by calling real SDL2's
   `SDL_GetKeyboardFocus()` directly to find the window, then `SDL_GetRenderer(window)` to reuse
   sdl12-compat's own renderer (falls back to a new software renderer only if none is found).
3. **No present-call hook fires**: `sdl12-compat` presents through its own invisible internal
   path. Final fix: hook `SDL_UpdateRect`/`SDL_UpdateRects`/`SDL_Flip` directly (real SDL 1.2-only
   function names, zero SDL2 name-collision risk, called by the game itself so ordinary
   interposition *does* reach them) — draw and present after calling through to the real
   implementation, guaranteeing OmniOSK is always the last thing drawn each real frame.
4. **Flicker on an accelerated, vsync'd renderer**: every real frame got two competing presents on
   the same shared renderer. Several timing-based mitigations were tried and rejected (disabling
   vsync — improved but not clean and device-variable; forcing software rendering — made it worse;
   quad-presenting — no difference). **Actual fix**: skip calling through to the real
   `SDL_UpdateRect`/`Flip` implementation entirely while the overlay is active (the game's own
   content simply stops reaching the screen, frozen on its last frame), resuming real forwarding
   the instant the overlay closes. Confirmed clean (no flicker) on two different devices.
5. **Input leaking through while the overlay is open**: two separate leaks. (a) Mouse events
   passed through unconditionally regardless of overlay state — fixed by adding mouse-event kinds
   to the bridge's own event classifier and explicitly suppressing them while active. (b)
   `SDL_GetKeyState()` (SDL 1.2's raw key-state poll, a *different function name* than SDL2's
   `SDL_GetKeyboardState()`, not just a rename — different indexing too) bypasses event-queue
   suppression entirely, since `sdl12-compat` updates that array as a side effect of its own
   internal event processing. Fixed with a dedicated hook, same shape as the present hooks: calls
   through to the real implementation first, then returns an all-zeroed shadow array while active.
6. **Display mis-scaling on non-native-resolution devices**: `SDL_RenderSetLogicalSize`
   letterboxing (set by sdl12-compat to match the game's requested video mode) isn't accounted for
   by OmniOSK's own layout math or by Dear ImGui's SDL2 backend, both of which read the window's
   raw physical size. Fix: query `SDL_RenderGetLogicalSize` and use that instead, in both places,
   when non-zero. **Not yet ported to the SDL3 fork** even though the same symptom was seen there.
7. **Text write-back**: submitted text needed two separate fixes — a real stack-buffer overflow
   (OmniOSK's synthesized events assumed an SDL2-shaped 56-byte event, but the real caller buffer
   for a bridge target is SDL 1.2's own 24-byte one), and the fact that most SDL 1.2-era games
   don't read text from the event queue at all (they poll `SDL_GetKeyState()`). Fixed via a
   dedicated `SDL_GetKeyState()` combo-injection state machine that bypasses the event queue
   entirely for text delivery on a bridge build.

**Testing bed pattern**: iterate against a minimal app (`checkkeys`, SDL 1.2's own real
upstream keyboard-test tool) rather than the full game — much faster, and it caught a real
unrelated shell bug too (`$GPTOKEYB2` is a multi-word command line, never quote it — see §1.8.2).

### 1.10.3 Conventions and env vars

**Overlay toggle is always L2** — confirmed real precedent in two real shipped `.ini`s. Keep this
consistent across every OmniOSK-using port; players shouldn't have to relearn which button opens
text entry.

**Never `export LD_PRELOAD`** — same rule as the general CLAUDE.md gotcha. It leaks into
gptokeyb2's own subsequently-launched process, which links its own real SDL2 for joystick reading;
a second SDL pulled in via the preload breaks gptokeyb2's `SDL_Init()`. Set it as a bare,
unexported prefix only on the actual game binary's launch line, after gptokeyb2 is already running
in the background:
```bash
export OMNI_MODE=buffered
export OMNI_EVENT_MODE=auto
# ... gptokeyb2 launch, pm_platform_helper ...
LD_PRELOAD="$GAMEDIR/libs.${DEVICE_ARCH}/libomni_osk.so${LD_PRELOAD:+:$LD_PRELOAD}" ./$BINARY
```
`OMNI_*` vars are safe to `export` early — only `LD_PRELOAD` itself is dangerous. Also export
`SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"` before launching gptokeyb2 — needed for correct
L2/R2 mapping (often analog-trigger-shaped) so the OmniOSK toggle registers correctly.

**`OMNI_EMIT_RETURN`** (default `1`) auto-appends a synthesized Enter after submitted text — check
whether the target game's own Enter key does double duty (e.g. also "advance the dialog") before
leaving this on; set `OMNI_EMIT_RETURN=0` if so.

**`OMNI_EVENT_MODE`**: if the game polls raw key state/scancodes every frame (most SDL 1.2-era
games), `auto`/`key` mode is correct — and note that a scancode-polling game's *ordinary* gameplay
input was never actually at risk from any of the bridge bugs above (sdl12-compat updates its own
key-state array before OmniOSK's hook even runs); only OmniOSK's own toggle/nav recognition was at
risk. `text` mode for SDL text-input-API-driven fields is unconfirmed, not yet needed on a real
port.

**`OMNI_BACKSPACE_KEY`/`OMNI_CHARSET_KEY`** default to literal BACKSPACE/TAB. The physical button
sending that key keeps sending it regardless of overlay state — only decode/consumption is
overlay-gated, not the binding itself. Check whether the target game's own keybinds (including
hardcoded ones, not just configurable ones) give that raw key a real in-game meaning the player
doesn't want triggered by accident while the overlay is closed; only remap
(`OMNI_BACKSPACE_KEY`/`OMNI_CHARSET_KEY` to different letters) if so, or if the port's own gptokeyb2
state machine already needs those keys for something else. If the in-game meaning is something the
player *wants* while the overlay is closed, there's nothing to fix — the overlay's own suppression
already keeps it from reaching the game while open.

**Dependency bundling** (`libs.$DEVICE_ARCH`): original variant is just `libomni_osk.so`
(`NEEDED`s `libSDL2-2.0.so.0`, `libstdc++.so.6`, `libgcc_s.so.1` — nothing else to bundle). SDL3
fork variant ships two files: `libomni_osk.so` (`NEEDED`s `libSDL3.so.0`, must be the shim) plus
the shim `libSDL3.so.0` itself (dlopens system SDL2 at runtime, doesn't link it, so bundling it
doesn't trigger the never-bundle-SDL2-alongside-a-shim gotcha).

---

# PART 2 — Real-World Gotchas Not Covered by the Official Site

## 2.1 Input handling

**The double-input trap covered in full above (§1.8.5) is the single most important classic-
gptokeyb-specific gotcha in this whole document.** It does not apply to gptokeyb2 at all (§1.8.1).

**`SDL_JoystickOpen` vs `SDL_GameControllerOpen`.** Games using the low-level joystick API often
hand-roll button mapping instead of using `gamecontrollerdb.txt`, which commonly shows up as
swapped or wrong buttons. The usual fix is disabling the game's native joystick init entirely and
driving all input through gptokeyb2 instead. Check which API a game actually uses before assuming
a mapping bug is a config-file problem — some engines (mkxp-z, raylib via its high-level API) are
already the "known good" case and need zero remapping at all.

**Touchscreen devices** (some Retroid and Anbernic models) have SDL2 synthesizing mouse-move
events from touch by default, which fights gptokeyb2's own synthetic mouse events for any port
driving its cursor via `mouse_movement`/`left_analog = mouse_movement` mapping. Fix:
`export SDL_TOUCH_MOUSE_EVENTS=0` in the launch script proactively, on any port using gptokeyb2
mouse emulation, regardless of engine.

**Quit-button placement.** Don't bind quit to a face button sitting right next to a primary,
frequently-mashed action button (B next to A, when A is fire/jump/throttle) — real risk of
accidental quits mid-game. If the CFW's own dedicated Back button already quits, don't also
duplicate that onto a face button "for convenience."

**Never `export LD_PRELOAD` before launching gptokeyb2** — it leaks into gptokeyb2's own process,
which links its own real SDL2 for joystick reading; if it also inherits a preloaded library that
itself pulls in another SDL (e.g. an SDL3-via-SDL2-backend shim's `libSDL3.so.0`), gptokeyb2 ends
up with two incompatible SDLs loaded at once and its `SDL_Init()` can get silently rerouted through
the wrong one and fail outright. On-device symptom: total, silent controller failure with no
visible connection to the actual cause unless gptokeyb2's own log output is read. Fix: set the var
as a bare prefix on the one command line that actually needs it, placed after gptokeyb2 is already
launched, never as an `export` earlier in the script. See §1.10.3 for the full OmniOSK-specific
pattern.

**Choosing which button gets which action**: see §1.8.4's full decision framework (Tier 1/Tier 2
accessibility tiers, the Nintendo-vs-Xbox physical-layout caveat, shoulder-button cycling
conventions) — read that before writing any control-mapping file for a keyboard-only/non-native-
gamepad game.

## 2.2 Display / graphics

**Never let an engine use its own native DRM backend**, even when it builds cleanly and looks
architecturally cleaner (no X11, no SDL2 translation layer). Two independent confirmed real cases:
a bespoke "talk to KMS/GBM/EGL directly" mode that compiled fine and failed on real hardware with
`Failed to find a suitable DRM connector mode` / `Failed to initialize platform`, then crashed; and
raylib's own `PLATFORM_DRM` backend hitting the identical failure on a second, unrelated port
(§1.7.11). Every CFW patches SDL2's own DRM/KMS handling for its specific connector quirks; a
generic engine DRM backend has none of that device-specific handling. Always reach KMS/DRM only
through SDL2's `kmsdrm` video driver.

**NEVER set `SDL_VIDEODRIVER` (or force any other specific video/audio driver) in a port's own
launch script, under any circumstances.** Each CFW sources its own SDL2 backend selection via
`control.txt`/`mod_${CFW_NAME}.txt` *before* the port's own script logic runs — not every device
uses `kmsdrm` specifically. A port forcing `SDL_VIDEODRIVER=kmsdrm` "to be safe" can silently break
on any CFW/device that actually needs a different driver. Confirmed real incident: the
`amphetamine` port added `export SDL_VIDEODRIVER=kmsdrm` and it caused the game to fail silently on
a real device (Knulli/H700). Never add this or a similar env var unless a specific, confirmed
real-device symptom points at needing an override — and even then, prefer a CFW-specific
`mod_${CFW_NAME}.txt` override path over hardcoding it in the port. The same failure shape applies
to `sdl3-sdl2-backend`'s `SDL3SHIM_SDL2_VIDEODRIVER`/`_AUDIODRIVER` (§1.7.12) even though it's a
technically distinct mechanism — don't hardcode those either without real multi-CFW verification.

**Prefer a native GLES2/3 build path over gl4es whenever the engine actually has one.** gl4es is a
translation layer of last resort, not a default choice. Confirmed real cases where the native path
wins: raylib's `GRAPHICS_API_OPENGL_ES2`/`OPENGL_VERSION="ES 2.0"` (§1.7.11) worked cleanly across
two different real GPU vendors (Mali and Adreno), while routing OpenGL 2.1 through gl4es on the
same engine hit two real, confirmed bugs specific to that exact combination on GLES-only hardware:
an unsized depth-renderbuffer format GLES drivers reject, and a shader version string that didn't
convert correctly in one call path — net result, a black screen despite gl4es itself initializing
fine. mkxp-z's `-Dgfx_backend=gles` (§1.7.10) is a second confirmed native-path case, working on
real Mali-G31 hardware with zero gl4es involvement. gl4es genuinely is the right and only choice
for engines that have **no** native GLES path at all — confirmed by reading source:
`MonoGame.Framework.DesktopGL` has no ES option anywhere in its actual SDL context-creation code
(§1.7.11), and neither does its KNI fork. But wherever an engine *can* request GLES natively while
still using SDL2 for windowing, try that first.

**`SDL_CreateTexture` can fail with "Texture format not supported"** on Mali-GPU/GLES2-backed
renderers, right after `SDL_CreateWindowAndRenderer` itself *succeeds* — specifically for pixel
formats used by manual/streaming textures (e.g. a hand-packed `SDL_PIXELFORMAT_RGBA8888` buffer),
even though identical code runs fine on desktop Linux. Confirmed on a real device (RG351V/
AmberELEC, RK3326, Mali GPU). Under Mono/P/Invoke, the same root cause has been confirmed to
manifest as a **hard `SIGSEGV`** instead of this soft error (§1.7.11) — a worse failure mode since
a P/Invoke crash mid-native-call has no managed exception to catch. Fix either way: don't use the
combined `SDL_CreateWindowAndRenderer` call (it doesn't let you pick renderer flags); split into
`SDL_CreateWindow` + `SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE)` instead — negligible
perf cost for a typical low-resolution game buffer. If porting any game to this device class that
creates its own `SDL_Renderer`, force software rendering preemptively rather than waiting to hit
this.

**Never bundle a port's own `libSDL2-2.0.so.0`** — see §1.1 for the full rule and the confirmed
`amphetamine`/PulseAudio failure case. Applies equally to any `sdl12-compat` or `sdl3-sdl2-backend`
shim a port bundles: let the shim `dlopen()` the device's own system SDL2, never ship a paired one.

**gl4es standalone launch setup is more than three env vars** (see Pattern B, §1.7.3 — source
`libgl_default.txt`, and on top of that some engines specifically need `SDL_VIDEO_GL_DRIVER`/
`SDL_VIDEO_EGL_DRIVER` pointed at the exact bundled files, since `LD_LIBRARY_PATH` alone doesn't
always get an engine's GL-extension query to resolve correctly through gl4es on real hardware).

## 2.3 Audio

**`SDL_mixer` MIDI playback can silently do nothing.** PortMaster CFWs generally don't ship a
system `timidity.cfg` or default soundfont, so `Mix_PlayMusic`/`Mix_LoadMUSType_RW` on a `.mid`
file either produces no sound or fails with `Couldn't open timidity.cfg`, easy to miss if calling
code doesn't check the return value. Two proven fixes, both with real merged-PortMaster-port
precedent:
1. **Classic Timidity (GUS patches)**: bundle a `timidity.cfg` + GUS `.pat` patch set directly
   inside the game's own folder — `SDL_mixer`'s built-in decoder searches the process's current
   working directory for `./timidity.cfg` by default, no env var needed. Real examples:
   `ports/freedink`, `ports/openclaw`. Typical patch set ("freepats", GPLv2) is ~33MB for a full
   General MIDI set — bundle it whole, don't try to trim it.
2. **FluidSynth + a single `.sf2` soundfont**: set `SDL_SOUNDFONTS` to an absolute path pointing
   at a bundled `.sf2`. **Only works if the target's `libSDL2_mixer` was actually built with
   FluidSynth support** — verify first:
   ```bash
   ldd /usr/lib/aarch64-linux-gnu/libSDL2_mixer-2.0.so.0 | grep -i fluid
   ```
   Real example: `ports/swordofjade`, bundling the full `FluidR3_GM.sf2` (~140MB, split across
   five `.7z` volumes, extracted on first run). **A much lighter option**: `TimGM6mb.sf2`, only
   ~5.7MB, GPLv2, ships via the Debian/Ubuntu `timgm6mb-soundfont` package at
   `/usr/share/sounds/sf2/TimGM6mb.sf2` — for a small-scope port this is ~9x lighter than the
   freepats patch set, one file, one launch-script env var line:
   ```bash
   export SDL_SOUNDFONTS="$GAMEDIR/TimGM6mb.sf2"
   ```
   mkxp-z (§1.7.10) uses the same `TimGM6mb.sf2` file via its own `mkxp.json`
   `"midiSoundFont"` config field instead of this env var — same recommendation, different
   mechanism.

Check both options' actual footprint before committing to one.

**Engines using `miniaudio` for audio can fail to init on PipeWire-based CFWs** even when the
audio environment is genuinely healthy — miniaudio's PulseAudio backend probing doesn't always
get along with PipeWire's Pulse-compatibility shim on some CFW/version combinations. Confirmed
real case: a raylib port on ROCKNIX/Retroid Pocket 5 (PipeWire, `pactl info` and `aplay -l` both
healthy) hit `Failed to initialize playback device` outright. Fix: force miniaudio straight to
ALSA — `#define MA_NO_PULSEAUDIO` next to raylib's own existing `#define MA_NO_JACK` in
`raudio.c`, before `#include "external/miniaudio.h"` (one-line patch to the local vendored copy,
reapply on any fresh raylib checkout).

**Allegro 4's `ALLEGRO_MODULES` env var is silently ignored when running as root** (a deliberate
security decision in Allegro's own source — `geteuid() == 0` skips reading it entirely), and
PortMaster always runs as root. Symptom is a misleading "Insufficient digital voices available"
error that has nothing to do with voice-count config; the real cause is no digital driver ever
loading, silently falling back to a dummy driver. Fix: `dlopen()` the ALSA digital driver module
manually and call its `_module_init()` directly, bypassing Allegro's broken auto-loader entirely.

**Westonpack overrides `XDG_RUNTIME_DIR` for its own Wayland socket**, which can break
PipeWire/PulseAudio's ALSA plugin for the wrapped game process on some CFWs (confirmed CFW-
dependent — broken on one CFW, fine on another with the identical port). Fix: capture the real
`XDG_RUNTIME_DIR` before `westonwrap.sh` runs and pass it back in as a positional `VAR=value`
argument in the actual wrapped invocation (same pattern as `XDG_DATA_HOME`/`ULTRADIR`).

**`SDL_AUDIODRIVER` is a no-op for engines that never initialize SDL's audio subsystem at all** —
don't assume every port needs it set just because a reference/upstream script sets it. Confirmed
case: a PopLib/SDL3-based port's upstream script set `SDL_AUDIODRIVER=alsa` under a "both SDL and
OpenAL probe PulseAudio" theory, but the actual engine only ever calls
`SDL_Init(SDL_INIT_VIDEO)` — all audio goes through OpenAL directly (`alcOpenDevice`). Verify by
reading the actual `SDL_Init()` call's flags before trusting an upstream env var as load-bearing;
see §2.8's methodology note for the general lesson this exemplifies.

## 2.4 Filesystem / process assumptions

**A bare-`fopen()`-only engine (common in older DOS-era codebases with a straight SDL2 bolt-on)
needs a guarded `chdir()` inside the binary's own `main()`**, not a launch-script `cd`, if its
assets live in a subfolder the code assumes is cwd:
```cpp
#include <filesystem>
std::error_code ec;
if( std::filesystem::is_directory( "assets", ec ) )
{
    std::filesystem::current_path( "assets", ec );
}
```
This is a no-op when `assets` doesn't exist relative to cwd (doesn't break existing IDE/dev
debug-directory setups), and it means the launch script can stay in the standard
`cd $GAMEDIR && ./$BINARY` form. Doing the `cd` in the launch script instead works but gets
fragile fast: any other file the binary needs to find via an *absolute* `$GAMEDIR` path (a bundled
soundfont, a control-mapping file) still needs to resolve correctly regardless of which folder the
script `cd`d into — handling it inside the binary keeps cwd-relative game assets and
`$GAMEDIR`-absolute launch-script-managed files from fighting each other.

**When a port ships bulk game data as a `.tar.gz` archive extracted on first launch** (to keep
the submitted zip's file count/size down instead of shipping hundreds of loose files — a
legitimate, established pattern in this project, not something to avoid), **always extract with
`gunzip -c "$ARCHIVE_FILE" | tar xf -`, never `tar -xzf "$ARCHIVE_FILE"`.** Some CFWs' BusyBox
`tar` builds don't support `-z` at all (`tar: invalid option -- 'z'`) - confirmed real-device
failure on muOS. `gunzip -c | tar xf -` works everywhere: `gunzip`/`gzip` are themselves required
tools (§2.7), and a plain `tar xf -` reading from stdin is far more universally supported than
`tar`'s own `-z` flag. Don't special-case per-CFW (`if [ "$CFW_NAME" = "muOS" ]; then ... else
tar -xzf ...; fi`) — the `gunzip | tar` form is a strict superset that works on every CFW tested so
far, so just use it unconditionally. Four real ports (`angrydd`, `amuletsarmor`, `motogt`,
`lastscenario`) were unified onto this form after hitting the bug reactively per-port first.

**`bind_directories <target> <source>`'s second argument (the port's own persistent storage
folder, e.g. `$GAMEDIR/conf/.gamename`) must already exist as a directory *before* the call, or
the whole bind mount silently no-ops.** The function checks `if [ -d "$2" ]` before doing anything
at all: if that exact path doesn't exist yet, it just echoes `"no directory found at $2"` and
returns — it never creates `$2` itself, and never runs `mount --bind`. Only `$1` (the target the
game itself hardcodes, e.g. `~/.gamename`) gets a `mkdir -p` inside the function. Pre-creating just
`$1`'s or `$2`'s *parent* (e.g. the launch-script template's own `CONFDIR`/
`mkdir -p "$GAMEDIR/conf"` block) is not enough — the exact full `$2` path itself must exist first.
Confirmed real silent failure on the `dreamchess` port
(`bind_directories ~/.dreamchess "$GAMEDIR/conf/.dreamchess"` never bind-mounted because only
`$GAMEDIR/conf` had been pre-created, not the `.dreamchess` subfolder). Fix: `mkdir -p` the *exact*
second-argument path before calling `bind_directories` —
`mkdir -p "$GAMEDIR/conf/.dreamchess"`, not `mkdir -p "$GAMEDIR/conf"`.

**Global variable names can collide with libc/POSIX function declarations** when cross-compiling
an old Windows/DOS-era codebase for Linux — e.g. a global `unsigned char pause` compiles fine on
Windows and fails on Linux with `redeclared as different kind of entity`, because `<unistd.h>`'s
`pause()` function gets pulled in transitively. Pure naming collision; rename the variable. Worth
a quick grep (`pause`, `read`, `write`, `index`, `yield`, `y1`, `j0`, and similar common libc/POSIX
names) on any first Linux cross-compile of an older codebase hitting this error class.

## 2.5 Debugging (a completely separate skill from packaging, but worth having ready)

- **Always get a stack trace or thread dump before touching code.** `gdb -batch -ex run -ex bt
  -ex quit --args ./program` for a reproducible crash; `gdb -p <pid>` then `thread apply all bt`
  for a hang; `gdb ./program core -ex bt -ex quit` for a core dump; `coredumpctl list` /
  `coredumpctl gdb <pid>` if systemd-coredump is active. Enable core dumps if needed:
  `ulimit -c unlimited` or `echo "core" > /proc/sys/kernel/core_pattern`.

- **On a PortMaster handheld, assume GDB is present and reach for it — don't fall back to
  log-only debugging out of caution.** Confirmed present and working on a real device (RG351V/
  AmberELEC) during a real port's bring-up. The proven pattern for getting a real backtrace out of
  a launch script (the crash only reproduces under the exact environment the script already
  built — `LD_LIBRARY_PATH`, `SDL_GAMECONTROLLERCONFIG`, gl4es sourcing, etc. — so GDB has to run
  as a temporary substitution for the script's own final launch line, not a bare separate
  invocation):
  1. Ship a small `gdb_commands.txt` alongside the binary:
     ```
     set pagination off
     run
     bt
     quit
     ```
     (`set pagination off` matters even under `-batch` — without it a long backtrace can stall on
     a `---Type <return> to continue---` prompt with no TTY to answer it.)
  2. Temporarily replace the launch script's final `./$BINARY` line with:
     ```bash
     gdb -batch -x "$GAMEDIR/gdb_commands.txt" --args "./$BINARY" > "$GAMEDIR/gdb_backtrace.txt" 2>&1
     ```
     Redirect to its own file, not the outer `tee "$GAMEDIR/log.txt"` capture — a per-command `>`
     takes over that command's stdout/stderr for its duration even inside an already-
     `exec > >(tee ...)`-redirected script.
  3. Have the tester re-run once, then read back `gdb_backtrace.txt`.
  4. **This is temporary debugging instrumentation, not something to ship.** Pull both the
     `gdb`-wrapped line and `gdb_commands.txt` back out once the crash site is identified and
     fixed — a real submission's launch script should read like a normal, unremarkable one.

- **glibc's fortified buffer-overflow checks (`*** buffer overflow detected ***`) are not caught
  by ASan.** A clean ASan run on a debug build is not proof the overflow doesn't exist in the
  Release build — this is glibc's own `_chk` function family (`__strncpy_chk`, `__memcpy_chk`,
  `__sprintf_chk`), a completely separate check. Investigate in order: (1) get the stack trace —
  the `__chk_fail` frame's caller names the exact function and file; (2) read the source at that
  call site; (3) look for a sub-object vs. parent-object `sizeof` mismatch — the destination
  pointer likely points into a struct member or array element, but the size/copy-length
  expression uses the parent object's size, which the compiler doesn't see. Valgrind is a good
  second opinion: `valgrind --tool=memcheck --num-callers=30 ./program`.

- **For a hang, dump every thread's backtrace, not just the one that looks frozen** — thread A
  being stuck is frequently caused by thread B holding a lock and never releasing it
  (`gdb -p <pid>`, then `thread apply all bt`). Per-thread state triage: `futex`/`__lll_lock_wait`
  = mutex wait (scroll up to see which mutex/caller); `pthread_cond_wait` = condvar wait;
  `clock_nanosleep`/`poll`/`read` = usually benign I/O wait, not the culprit; identical program
  counter across repeated `continue`+`bt` = confirmed spin loop, not a block. Near-zero CPU with
  threads parked in futex waits means deadlock; CPU pinned near 100% with no progress means
  livelock/infinite loop (`perf top` or `perf record`/`perf report` to see where cycles go).

- **Classify the deadlock once threads are stuck**: **Class A** (mutex contention, one holder) —
  most common root cause is a recursive-mutex assumption ported from Windows: `pthread_mutex_t` is
  non-recursive by default on Linux, but Windows mutexes are recursive by default, so code doing
  `lock(); helper(); unlock()` where `helper()` also locks the same mutex self-deadlocks only on
  Linux; fix with `std::recursive_mutex` or restructure lock boundaries. **Class B** (classic
  ABBA) — Thread A holds mutex 1 waits on mutex 2, Thread B holds mutex 2 waits on mutex 1, both
  seen in `__lll_lock_wait` on different futex words; fix by enforcing a single, consistent lock
  acquisition order across all threads. **Class C** (signal-induced hang) — SIGINT/SIGTERM
  delivered but a handler just sets a flag nothing polls (e.g. SDL installs its own SIGINT handler
  that just sets an internal flag).

- **Common mistake: fixing the wrong side.** If Thread A can't acquire a lock Thread B holds, the
  fix is almost never a timeout on Thread A's wait — that just converts a hang into silent data
  corruption or a retried failure. Understand *why* the holder holds too long, or make the locking
  model genuinely recursive if re-entrancy is expected.

- **Porting to a newer library major version (e.g. SDL2 to SDL3) that crashes/hangs**: the root
  cause is almost always a changed return-type convention (`bool` vs `int` success values) or a
  renamed/restructured struct field (`event.key.scancode` moving, `char*` becoming `char[32]`) —
  diff the old and new header signatures for the exact failing call site, then check every call
  site actually handles the new convention correctly.

- **Tracing without rebuilding**: `strace -f -e trace=open,openat,read,write ./program` for
  file/device access, `strace -f -e trace=connect,send,recv ./program` for network,
  `perf record -g ./program && perf report` for CPU profiling.

- **When logging beats a debugger**: only in one specific case — finding where a hang occurs in
  production code you can't attach a debugger to. Use sparingly (one well-placed log pair beats
  ten scattered guesses, since each log line costs a full edit/compile/run/wait cycle GDB skips
  entirely): `SDL_Log("[phase] entering render loop");` bracketing the suspected region.

- **All chroot/dev-machine verification is build/package-only, never functional proof.** Neither
  a chroot nor "it compiled" proves a port works — real GPU/KMS/input access doesn't exist there.
  Every actual functional check (does the window open, does gptokeyb2 move the cursor, does audio
  play) happens on a real device in a separate pass. Never claim or imply "tested" in a PR or
  README based on build-only work — the most likely gap for an AI specifically, since an AI has
  no device to test on at all.

## 2.6 Packaging gaps beyond what Part 1 already covers

**Don't trust a NuGet/packaging layer's bundled license file as automatically correct** for the
library it's shipping — go to the actual upstream project's repo for the real license. A real
case: a package's included `LICENSE` file only covered a small BSD-licensed sub-component, while
the actual library being shipped is LGPL upstream.

**Bundled `.so` files need to be flattened to one real file, not a symlink chain.** A dev
machine's install often has `libfoo.so -> libfoo.so.2 -> libfoo.so.2.0.6`; copying that chain
into a port can silently break across zip/scp to the device. Resolve it and ship one real file,
named to match the exact SONAME the binary's own dynamic section actually requests (`ldd`/
`readelf -d`), not whatever name the dev machine happened to use.

**A screenshot must always be a real capture from the actual device or build.** Never generate,
crop, or fabricate one, and never substitute existing game art as a stand-in.

**`port.json`'s `store` field is a list of objects, not bare URL strings** — see §1.2 for the real
schema (`name`/`gameurl`/`developerurl`); this was flagged directly on a live PR review.

## 2.7 CFW shell / runtime environment contract

Every launch script relies on a real environment the CFW guarantees — worth knowing what's
actually load-bearing vs. merely convenient.

**Environment variable contract** (sourced via `control.txt`/`device_info.txt`, used unexplained
throughout Part 1's scripts):

| Variable | Content |
|----------|---------|
| `$DEVICE_ARCH` | `aarch64`, `armhf`, or `x86_64` |
| `$DEVICE_CPU` | SoC identifier (`rk3326`, `rk3566`, `h700`, etc.) |
| `$DEVICE_NAME` | Human-readable device name |
| `$DEVICE_RAM` | RAM in GB (integer) |
| `$DISPLAY_WIDTH` / `$DISPLAY_HEIGHT` | Screen resolution in pixels |
| `$CFW_NAME` | CFW identifier (matched by port scripts, e.g. `mod_${CFW_NAME}.txt`) |
| `$ESUDO` | `sudo`, `doas`, or empty string |
| `$GPTOKEYB` / `$GPTOKEYB2` | Multi-word command lines for the gamepad mapper binaries |
| `$directory` | Base path for ports storage |
| `$PM_CAN_MOUNT` | `Y` or `N` — squashfs mounting support |
| `$TASKSET` | `taskset` command for CPU affinity (or empty) |

**Shell baseline safe to rely on**: GNU Bash 4.x+ (`[[ ]]`, `${var,,}`, process substitution,
arrays, `trap`) — BusyBox ash/dash would silently break hundreds of real ports, so this is a hard
CFW requirement, not just a convention. Full GNU coreutils, not BusyBox (`sed -i`, `find -maxdepth`,
`grep -P` all safe). Present but not universal: `dos2unix`, `xdelta3` (binary delta patching),
`dialog` (TUI menu selection), `rsync`, `7z`/`7za` (PortMaster bundles `7zzs` as primary, these are
fallback). Assume absent unless bundled: `innoextract`, `ffmpeg`, `ar`.

**`/dev/uinput` is required** — gptokeyb(2) needs it, used by 99%+ of ports. Some scripts
explicitly `$ESUDO chmod 666 /dev/uinput`; worth knowing if a gptokeyb2 invocation silently fails
to inject input.

**Squashfs + loop devices are the single most impactful kernel requirement** — this is *why* every
runtime pattern in this document (Pattern D/F/G/H, Westonpack) works at all. CFWs guarantee
`CONFIG_SQUASHFS` (zlib/lzo/xz/zstd) + `CONFIG_BLK_DEV_LOOP` with `loop.max_loop=16`+ (some ports,
like the VirGL Westonpack variant in §1.7.4, mount multiple runtimes simultaneously).

**zram/swap pattern** for memory-hungry ports:
```bash
zramctl --reset /dev/zram0 && mkswap /dev/zram0 && swapon /dev/zram0
# fallback:
dd if=/dev/zero of=/swapfile bs=1M count=512 && mkswap /swapfile && swapon /swapfile
```

**Never set a system-wide `LD_PRELOAD`** — it breaks the CFW's own library-override mechanism
(the same one ports rely on for their `libs.$DEVICE_ARCH`/`LD_LIBRARY_PATH` overrides). Westonpack's
`WRAPPED_PRELOAD`/`WRAPPED_LIBRARY_PATH` (§1.7.5) and OmniOSK's never-export rule (§1.10.3) are the
scoped, port-safe versions of this same principle.

**exFAT/FUSE caveat**: exFAT is the standard target filesystem (cross-platform drag-and-drop).
FUSE-based exFAT has known deadlock issues under heavy I/O on older BSP kernels — a mysterious I/O
hang on an older device may be a documented CFW-level issue, not necessarily the port's own bug.

## 2.8 Methodology: re-derive requirements from source, don't trust upstream's own launch script

A real, generalizable lesson from a completed port (Insaniquarium Deluxe, a PopLib/SDL3 fork):
every env var and file requirement was independently re-derived by reading the actual engine
source and the repo's own asset-verification tooling, not by trimming upstream's shipped `.sh`.
This caught two real, concrete mistakes upstream's own script would have carried forward:

- Upstream set `SDL_AUDIODRIVER=alsa` under a "both SDL and OpenAL probe PulseAudio" theory;
  reading the actual engine source showed `SDL_Init(SDL_INIT_VIDEO)` only — SDL's audio subsystem
  is never initialized, all audio goes through OpenAL directly, so the env var was a pure no-op
  and got dropped (see §2.3).
- Upstream had a post-crash diagnostic retry block that set `SDL_VIDEODRIVER=kmsdrm`/`mali`
  explicitly to narrow down driver failures — this directly conflicts with the hard
  never-hardcode-`SDL_VIDEODRIVER` rule (§2.2), so it wasn't carried over in any form, not even
  scoped to a failure-only retry path.

**When converting an existing port's classic `.gptk` tuning to gptokeyb2's `.ini`, don't invent
new numbers by guessing at a conversion formula** — `mouse_scale` and `deadzone_scale` are
described as "aliases" (§1.8.2) but classic gptokeyb's old two-field split (`deadzone_scale` for
curve shaping, `mouse_scale` for movement-delta divisor) doesn't carry over numerically once
gptokeyb2 merges them into one field. With no way to verify new numbers without a real device,
reuse another already-proven port's exact baseline values instead of guessing (e.g. the
Insaniquarium port reused `amuletsarmor.ini`'s proven `deadzone_scale = 6` baseline and
`arcanumce`'s proven `deadzone_scale = 18` high-resolution bump, rather than attempting to convert
upstream's old `mouse_scale`/`mouse_delay`/`dpad_mouse_step` values directly).

---

# Bottom Line

If only a few things from this whole document get absorbed, make it these:

1. **Project policy is gptokeyb2/`.ini` for every new port (§1.8.1), not classic gptokeyb/`.gptk`
   (§1.8.5, legacy-only now).** The classic tool's double-input trap is real but only matters when
   reading/maintaining an already-shipped classic-gptokeyb port — don't reach for it as a template.
2. **PM handhelds are physically Nintendo-style, not Xbox-style, and gptokeyb2's logical
   `a`/`b`/`x`/`y` keys don't auto-remap to compensate (§1.8.4).** Get the real physical layout
   from whoever's testing on-device before assigning bindings.
3. **`SDL_VIDEODRIVER` (and its `SDL3SHIM_*` equivalent) must never be hardcoded in a port's
   launch script (§2.2)** — this is one of the most repeatedly-hit mistakes in this project's
   history and is easy to reach for "just to be safe."
4. **The PR template's Authorship & Testing checkbox (§1.6)** is what turns an AI-generated port
   from "compiles and looks structurally correct" into a PR bounced for a reason nobody saw
   coming — it genuinely needs to be true, not just checked.

Everything else here is real, drawn from actual on-device debugging and actual PortMaster PR
review feedback across many real ports, but those four are the ones most likely to matter on the
very first port someone tries to ship with AI help.
