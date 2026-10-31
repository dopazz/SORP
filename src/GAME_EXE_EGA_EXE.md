# Ivan Stewart's Super Off Road (DOS, 1990) — what we know about GAME.EXE and EGA.EXE

Everything below was established by disassembly, by decoding the shipped
data files, or by reading live memory dumps from DOSBox-X. Where a fact
came from a dump or a measurement rather than from code, it says so.

---

## 1. The two executables

| file | role |
|---|---|
| `GAME.EXE` | the game: logic, data, the page engine, all screen scripts |
| `EGA.EXE` | the EGA video driver: the sprite bank table, text styles, blitters |
| `CGA.EXE`, `TANDY.EXE` | the same driver for other displays, each with its own styles |
| `READKEY.EXE` | a launcher helper used by `IRONMAN.BAT` |

`IRONMAN.BAT` chooses a driver and runs the game; the driver is loaded
into a segment the game calls through a vector table.

### Segments

- The game's **data segment** begins at image offset **`0x5700`**. Every
  `DS:xxxx` in this document is relative to that.
  At runtime under DOSBox-X the data segment was **`124C`**, so a dump of
  `124c:0000` maps one-to-one onto these offsets.
- A second code segment, **`0x42f`** statically, is the **runtime library
  and page engine**. At runtime it was **`CS 110B`**, confirmed by matching
  dump bytes against the image at `dump + 0x42f0`. An address seen as
  `110B:xxxx` is image `0x42f0 + xxxx`.
- The load base therefore is `0x110B - 0x42F` = **`0CDC`**, and the **main
  code segment** — physics, screen builders, the loader, the keyboard
  handler, everything at image `0x0000`–`0x5700` — is `0CDC` at runtime.
  `0x0CDC + 0x570 = 0x124C`, the data segment, which checks out.

### Startup

The entry point is `0x42f:0x000a`: DOS version check, shrink the block,
allocate the stack, seed the RNG from the BIOS timer at `40:6c` xor
`0xd0c`, hook interrupts, walk a subsystem vector table at `DS:cc50`,
then `ljmp 0:0` into the main program. That begins with the driver's
init (`es:[0x10]`, `es:[0x1c]`, `es:[0x30]`, `es:[0x20]`), installs the
font list (`0x42f:0ed9`) and the **sub-page table** `0x83de`
(`0x42f:0ef9`, which keeps it in `[0x73a]`), and at `0x0063` issues
`mov al, 9; call 0x1193` — **the first file load is `loadb.xx`**.

### The driver's entry vectors

`GAME.EXE` calls the driver through `es:[n]` with `es` from `[0xcb90]`.
Known entries include `es:[0x20]` and `es:[0x30]` (mode and palette
setup), `es:[0x74]`, `es:[0x78]`, `es:[0x7c]` and `es:[0x80]` (the
scoreboard: configure, elapsed time, digits, nitro counts).

---

## 2. The file loader

A filename table at **`DS:0x4a9d`** holds the `.xx` data files, and the
loader at **`0x1193`** takes an index in `al`:

```
0..7   track0.xx .. track7.xx
8      titles.xx
9      loadb.xx
10     loada.xx
```

The tracks by file.  The game's own numbering runs 1..8 with Blaster as
8, so each file carries its number and Blaster is `track0.xx`:

| file | track |
|---|---|
| track0.xx | Blaster |
| track1.xx | Big Dukes |
| track2.xx | Cliffhanger |
| track3.xx | Sidewinder |
| track4.xx | Fandango |
| track5.xx | Hurricane Gulch |
| track6.xx | Wipeout |
| track7.xx | Huevos Grande |

Sidewinder (3) and Wipeout (6) are the schedule's first two races
(`DS:0x1a3`: 03 06 ...), and track 0's score board sits bottom left
(`DS:0x3f0`: 9,152), as it does on Blaster.  All eight confirmed by
sight in SORP.

It reads the name from `[bx + 0x4a9d]` and a companion word from
`[bx + 0x4b1f]`, then calls the loader proper at `0x42f:0x0d30`.

Callers: `0x0063` (`mov al, 9` — **loadb.xx**, the loading screen, the
first file the game reads), `0x2468` (`mov al, 8` — titles.xx) and
`0x02c9` (a track, from `[0x1a0]`, masked to 0..7). **Nothing ever passes
10**, so `LOADA.XX` — the same screen with the Virgin Mastertronic logo
instead of Virgin Games — is shipped but never displayed.

A **second loader** at `0x11c7` uses its own table at `DS:0x4b37`, holding
`contour0.xx` … `contour7.xx`, indexed by the same track number. So a
track change loads a `trackn.xx` through the first and a `contourn.xx`
through the second.

### The disk-swap prompt

The prompt is the loader's **failure path**, not a scheduled step:

```
0x11aa  lcall 0x42f:0x0d30        ; try the load
0x11b0  jb   -> fail               ; carry: could not open
0x11b2  cmp ax, cx; jne -> fail    ; short read
fail:   draw DS:4bc1 "DISK REQUEST / PLEASE INSERT DISK 2 / AND PRESS
        SPACE", wait on Space, then RETRY the same file
```

With every file in one directory it can never fire. Alt+B and Esc are
polled inside that wait loop. `DS:4c10` follows with "LOADING..." and
`"ARE YOU USING A JOYSTICK (Y\N)"`.

---

## 3. The page engine (segment `0x42f`)

A "page" is a byte script mixing text with control codes. Text is any
byte ≥ 0x20; anything below is a code. The dispatch table is at
**`DS:0x98c`**, two bytes per entry, holding handler offsets **within the
page-engine segment**; each handler consumes its own operands.

### Control codes

| code | operands | meaning |
|---|---|---|
| `0x00` | 0 | end the page (`pop ax; retf`) |
| `0x01` | 0 | no-op |
| `0x02` | 2 | set x (big-endian word) |
| `0x03` | 1 | set y |
| `0x04` | 3 | set x (word) and y |
| `0x05` | 2 | add to x |
| `0x06` | 1 | add to y |
| `0x07` | 3 | move |
| `0x08` | 0 | newline: x to the margin, y += font height + leading |
| `0x09` | 1 | set the margin |
| `0x0c` | 1 | **draw a sub-page** (see below) |
| `0x0d` | 1 | repeat the last glyph n times |
| `0x0e` | 1 | **letter spacing**, signed, added to every advance (`0x1013`) |
| `0x0f` | 1 | **leading**, signed |
| `0x10` | 0 | proportional pitch (`[0x98a] = 0`) |
| `0x11` | 0 | fixed pitch (`[0x98a] = 1`) |
| `0x17` | 4 | **select a font** by name, `"IRON"` or `"ROAD"`; also **resets the margin and the pitch mode** (`0x1125`) |
| `0x18` | 2 | select a style by two-letter name |

The pen advances by the glyph's own advance, or by the font's fixed width
in fixed-pitch mode, **plus** the letter spacing (`0x1013`). Pages chain:
a prologue page sets font, pitch, spacing and leading and ends, and the
content page follows.

### Sub-pages (`0x0c`)

The handler at `0x10af` is explicit about the addressing:

```
bx = index * 2
si = [0x73a]                 ; the table base, installed at startup = 0x83de
si += [bx + si + 6]          ; entries start SIX bytes in
draw the page at si          ; base PLUS the entry -- relative to the table
```

So entries are **offsets relative to the table base**, not absolute and
not relative to `0x8000`. With `0x83de`:

| index | page |
|---|---|
| 10, 11, 12 | the three `$xxx,xxx + $xxx,xxx` prize lines |
| 13–16 | MADMAN, HURRICANE, JAMMIN', IRONMAN |
| 17–20 | Sam Powell, Earl Stratton, John Morgan, Ivan Stewart |

**A sub-page inherits the caller's whole text state** — margin, letter
spacing, leading, pitch, font and style. Starting one with fresh state
makes it draw in the default: the plaque's prize line loses its −1
spacing and every line after the first loses the column's colour.

### Fonts (`IRONFONT.XX`, `ROADFONT.XX`)

Header: id, first char at +9, last at +0xb, nominal height at +0xc (BE),
fixed width at +0xe (BE, 8 in both), glyph offsets from +0x12, two bytes
each, big-endian.

Per glyph: `+4` advance, `+6` height, **`+8` the glyph's TOP OFFSET within
the line** — 0 for tall brackets, 1 for capitals and digits, 2 for `i j t`,
4 for lowercase, and 7, 8, 9 for `,` `.` `_`. This is what puts lowercase
on its own x-height line and lets `g` and `,` descend. Rows follow: plane 0
the fill mask, plane 1 the outline.

**ROADFONT has no lowercase** — those slots are degenerate (advance 0,
height 0, top 32).

### Styles

Styles live in the **video driver**, not the game — EGA, CGA and Tandy
each carry their own, since the mapping is per display mode. In `EGA.EXE`
the nodes start at **`0x236d`**, **24 bytes apart**: a two-letter name, then
at +8 a sixteen-byte row mapping a glyph's pixel value to a colour (fill at
+9, outline at +10). With the node's flag byte zero, `0x11d3` copies that
row 32 times, so the row is the whole style.

| | | | | | |
|---|---|---|---|---|---|
| Wh 14/13 | Bk 0/13 | Ye 5/13 | Bl 12/13 | Gr 8/13 | Re 6/13 |
| B2 14/14 | W2 12/14 | WR 10/6 | WB 10/1 | W3 10/14 | WY 1/3 |
| E1 4/14 | E2 2/14 | E3 6/14 | E4 10/14 | E5 13/14 | |

**The E styles, by truck.** `0x31d5` patches the plaque page's style
to `"E"` + (`'1'` + `[di]`, the truck's colour number: red 0, yellow 1,
blue 2, white 3 — set at `0x12d3`), so E1 red, E2 yellow, E3 blue, E4
white; a human's plaque (`0x3220`) and the time board (`DS:0x981c`) use
**E5**. The driver's glyph routine (`9:2fca`, driver vector `[0x70]`)
draws each pixel as `map[value]` exactly — a pixel's value is a bit
from each of the row's four words (the first is bit 0), translated by
`xlat` (`9:3092`) — an AND pass clears the glyph,
an OR pass lays the colour, no inversion. The results screen's colours
come out as **15 − the style value** in SORP's index space: SORP's
`og_pal_end` and EEND pixels are the original's complement (the picture
decoder inverts every plane, `9:2531`/`9:256d`; the palette was read
with the registers reversed). So E1–E4 show as 11, 13, 9, 5 and the
outline 14 as 1, black — exactly the capture. E5 (fill 13) shows as
2, white: a human's plaque and the time board, white on black, as the
original shows them.

A style with fill and outline equal means **no border** — both pixel
values map to one colour; it does not mean "stamp the outline too".

**`Ex` is not a style.** The six pages that ask for it are the
`"xxx Place : xxx"` templates, where the x's are placeholders patched at
runtime — the style name is patched the same way.

**E1..E5 are the truck colours**, not the places: a capture shows
IRONMAN's plaque grey, HURRICANE's yellow and JAMMIN's blue whatever
order they finish in. (An earlier reading here paired them wrongly;
see the table note above.)

**Index meanings are per screen.** The title's 5 is yellow, the shop's 5
is white, the results' 13 is yellow. An index alone says nothing without
naming its palette.

---

## 4. Screen scripts in the data segment

### Title screen

Object `TI` at `DS:0x366c`. Four views per pass, the fourth cycling over
three passes:

| view | page | ticks |
|---|---|---|
| logo | `0x8c34` | 180 |
| copyright | `0x88f1` | 360 |
| publisher | `0x8b0d` | 360 |
| hints / wins / credits | `0x8c55` / `0x8e0f` / `0x904f` | 540 / 528 / 528 |

A full rotation is 71.6 seconds; one pass of the four views is 24.

### Registration

| page | contents |
|---|---|
| `0x8424` | prologue: `font<ROAD> FIXED spacing=2 leading=8` |
| `0x847d` | prologue: `font<ROAD> FIXED spacing=-2 leading=0` |
| `0x842f` | `A B C D E` … `Z _ &`, `x+=4 spacing=-2 " SKIP"`, `INITIALS:` |
| `0x8489` | `JAN FEB MAR` … `OCT NOV DEC`, ` &   SKIP`, `BIRTHDATE :` |
| `0x84f1` | `01 02 03 04 05` … `31 &  SKIP`, `BIRTHDATE :` |
| `0x8566` | years `30`…`99` — fourteen rows of five |
| `0x8648` | years `00`…`69` — the alternate table |
| `0x872d` | `USA ITALY GERMANY BRITAIN FRANCE JAPAN CANADA AUSTRALIA` |

The two year tables behave as one list of 140: scrolling off one lands on
the other's far end. `&` renders as the back arrow.

**The input children.** Each step is a child object with its own handler.
Positions are inside the panel (origin `8 + 104p, 31`); the shape is the
ESHOP frame `[si+0x5c]`; pieces are consecutive frames side by side.

| step | child | op / handler | cells | cursor x, y | shape | starts on |
|---|---|---|---|---|---|---|
| initials | `0x43cc` | 70 `0x26b2` | 29: A–Z, `_`, back, SKIP | `(c%5)*20+3`, `(c/5)*17+17` | 5; SKIP 2 | A |
| month | `0x4412` | 71 `0x2805` | 14: 12, back, SKIP | `(c%3)*30+7`, `(c/3)*18+26` | 3; SKIP 2 | cell 10 (NOV) |
| day | `0x4464` | 72 `0x2925` | 33: 31, back, SKIP | `(c%5)*18+3`, `(c/5)*17+17` | 4; SKIP 2 | cell 26 (27th) |
| year | `0x44b6` | 73 `0x2a45` | 70 a table | `(c%5)*18+3`, `(c/5)*9+17` | 4 | 30..99 table, cell 38 ('68) |
| nation | `0x44fc` | 74 `0x2b4d` | 8, up/down too | `2`, `[0x9536+2c]` = `6+21c` | 0+1 (64 wide) | USA |

A child ends with `[si+0x4c]` = 4 (chosen), 8 (SKIP — the parent's flag
`0x4000`) or 0xc (back — flag `0x8000`). **SKIP ends the whole
registration from any step**: the panel jumps to `0x3fa4` (op 86, then
"Get Ready!"), and the nation (`[di+0xe]`) is left as it was. Back returns
to the previous step. For the year, 8 instead **flips tables**: off the
left of 30..99 to cell 69 of 00..69, off the right of 00..69 to cell 0 of
30..99; the outer ends stop.

**The entry line.** Every handler first draws its field page (`[si+0x10]`:
`0x9240` initials, `0x92af` month, `0x9323` day, `0x93d5` year), which
leaves the pen at **(12, 186)**, then its value page, erasing with the `^`
glyph before drawing: initial n at x `0x3c + 8n + [[si+0x1c]]` (the
panel's x); month at pen +16 (`0x92e9`, digits patched at `0x92f1` from
`0x92f4`); day at +34 (`0x9367`, from `0x9372`); year at +52 (`0x9435`,
from `0x9440`). The day and year wrapper pages draw the slashes, at x 40
and 58. A field still waiting alternates between its table's last two
entries, `^^` and `_ ` — a flashing underscore. The panel is cleared with
rectangle `0x3218` (to y 200) before the initials, month and nation steps,
but with **`0x3220` (to y 184) before the day and year**, so the line
builds up as `_`, `11/_`, `11/27/_`.

The grid pages are sub-pages 1, 3, 5, 6, 7 and 8, drawn by wrapper pages
whose pen starts at panel (4, 17) — month (8, 17), nation (2, 6). The
nation page has **flags**: eight "fl" children (`0x3898`–`0x3978`) at
panel (63, 1 + 21n), ESHOP 20, 17, 21, 19, 16, 18, 22, 23 for USA through
AUSTRALIA.

### Podium plaques

Complete pages, one per place, each with its own style and sub-page calls:

```
DS:96eb  font<IRON> spacing=-1 leading=-1 style<E1> "3rd Place"
         NL x+=7 subpage[10] NL subpage[13] subpage[17] END
DS:9716  the same with style<E4> and "1st Place"
DS:9741  the same with style<E3> and "2nd Place"
DS:976c, 97a4, 97dc   "xxx Place : xxx" templates
```

They carry **absolute screen positions**. Their sub-page offset table sits
just above at roughly `DS:0x96c4`, entries `9641 9716 97eb 9616 9741 976c
97a4 97dc 9717`.

### The Ironman Club

```
DS:989c  font<IRON> style<E5> at(116,12) "The Ironman Club",
         then style<E2> spacing -1, which the rows inherit
DS:98c3  11 "xx. xxx " 00                 fixed pitch: place and initials
DS:98cd  02 00 00  11 "$xx" 10  05 ff fe "," 11 "xx0" 10  05 ff fe "," 11 "000" 00
                                          the money: fixed digits,
                                          proportional commas pulled -2
DS:98e7  " 1 2 3 4 5 6 7 8 910"           the place pairs, two bytes a row
DS:9872  "Save HiScore table (Y/N)"
```

**The draw loop (`0x34e3`).** Each page carries on in the text state the previous page left, so every row is drawn in E2 with −1 letter spacing:

```
si = 0x989c; draw                the heading
cx = 10                          ten entries
y  = 13 * row + 0x26             [0x97e], the pen's Y: 38, 51, ...
x  = 0x4d                        [0x97c], the pen's X: 77
[0x98c4] = the place pair from 0x98e7 + 2*row
[0x98c8..] = the initials
draw 0x98c3
```

**The money (`0x3532`–`0x35b4`).**

1. `0x3930` turns `[di+0xa]` (BCD, units of $10,000) into four digits, d0..d3. Leading zeros become spaces, as far as the first digit that isn't zero.
2. The digits are written backwards: d3 at `0x98db`, d2 before it.
3. With a third digit, `bx -= 7`, and the six bytes at `0x98dd` (`10 05 ff fe 2c 11`) are copied back over whatever a shorter earlier amount left. d1 and then d0 go before them.
4. `$` goes before the first digit, with `0x11` before the `$`.
5. `0x42f:0xf34` measures the string from that `0x11`: each glyph's advance (or the fixed width) plus the letter spacing, plus every `0x05` move. It restores the pitch and spacing afterwards.
6. `02 hi lo` with x = **`0xdc` − width** is written three bytes before the string, and the string is drawn from there.

So every amount **ends at x 220**: 5 → `$50,000`, 25 → `$250,000`, 1234 → `$12,340,000`.

**Code `0x05` adds a signed word.** `ff fe` is −2, which pulls each comma back two pixels.

### Other strings

```
DS:8760  the eight nations          DS:878f  "$80,000 + $10,000"   (3rd)
DS:8781  "Press  Button"            DS:87a3  "$100,000 + $30,000"  (1st)
DS:87ce  MADMAN HURRICANE JAMMIN' IRONMAN (each a small page)
DS:87ff  Sam Powell, Earl Stratton, John Morgan, Ivan Stewart
DS:883a  "Avg : xx.xx (Last) "      DS:8876  "Best : xx.xx (Last) "
DS:88d5  "Player 1" " Player 2" "Player 3"
DS:96b1  "1st" "2nd" "3rd" "4th" …
```

The prize and lap-summary strings are **templates**: the digits are
patched in at runtime, which is why there are three copies of each.

---

### Object scripts

The stepper at `0x8c5` reads an object's script at `[si+0x3c]`. An
instruction is a word `w`: the op is `(w >> 3) & 0xfff`, dispatched
through the table at **`DS:0x29ae`**; `w & 7` is the number of operand
words that follow (the first arrives in `ax`); bit 15 means **yield** —
stop stepping this object until the next tick.

| op | handler | meaning |
|---|---|---|
| 4 | `0x93f` | wait n ticks |
| 6 | `0x970` | jump by the operand (relative) |
| 8 / 9 | `0x973` / `0x97d` | jump if `[si+8]` is non-zero / zero |
| 34 | `0xaab` | spawn a child from an init record |
| 42 | `0xc02` | nothing (used with the yield bit) |
| 43 | `0xc03` | end this object (`[si+4] = 0`) |
| 46 / 47 / 48 | `0xc2b` / `0xc2f` / `0xc35` | set / clear / test bits of `[si+0xa]` |
| 53 | `0xc5d` | store a word at field `[si + operand 2]` |
| 54 / 55 | | sound effect / music |
| 60 | `0x2651` | draw a page |
| 64 | `0x2627` | `[si+8]` = live children (`[si+0x78]`) |
| 49 / 50 / 51 | `0xc3c` / … / `0xc4c` | clear / set / test bits of the byte at operand 2 |
| 65 | `0x262e` | `[si+8]` = live children − **computer** trucks (`[+2]` bit 1, value 2, at `0x4c6a`, `0x4d2c`, `0x4dee`) |
| 82 | `0x2f3a` | `[si+8]` = −1 if any truck placed ahead of this panel's truck is a computer |
| 83 / 84 | `0x2f63` / `0x2f6f` | `[si+8]` = the truck's credits `[+1]` / spend one |
| 85 | `0x2f76` | the player's game ends: into the club, `[+2]` gains bit value 2 (computer); if all three trucks are now computers, `[0x19c]` \|= `0x118`, which ends the shop that tick |
| 86 | `0x35c9` | find the player's initials in the Ironman table at `0x98fb`; a new name gets a blank record (times `99.99`) |

`op 64; op 8 -8 | yield` is the idiom "wait until every child is gone".

**The flagman's script** (`DS:0x3454`, tag FM, sprite `0x1d6`): wait
0x28 ticks; op 54 (1); op 50 clears bit 3 of `[0x19c]` (the trucks go);
wave. Then the loop at `0x3492`, once a tick:

| bit of `[0x19c]` | branch | sound | flag |
|---|---|---|---|
| 1 (lap 4) | `0x34c8` | op 54 (`0xffff`) silences, then op 54 (3) | chequered, `0x1be` |
| 0 (lap 3) | `0x34aa` | op 54 (2) | white, `0x1ca`; op 50 clears bit 0, then back to the loop |

The chequered bit is tested first. All three waves (`0x3476`, `0x34b8`,
`0x34da`) share one frame list at `DS:0x34f8`: op 31's first operand is
the list's offset from the op.

**A shop panel's script** (red's from `DS:0x3de0`): panel colour
(`0x377e`) and the girl-and-trophy figure "lg" (`0x3874`, sprite 621,
three pieces, at +0, +56), page `0x9190` (`Player 1` / `Press Button`);
on the button, the registration pages `0x9232` (initials), `0x92a1`,
`0x9310`, `0x93b4`/`0x93c7` (birthdate), `0x950c` (nation), each waiting
on an input child; then op 86, the colour and the figure again, **page
`0x91a8` — sub-page 4, `Get Ready!` at (24, 56)** — and op 43. A new
player is never shown the upgrade boxes before their first race. Yellow's
and blue's pages are `0x91c6`/`0x91de` and `0x91fc`/`0x9214`; each panel
also has a `Game Over` page (`0x91b7`, `0x91ed`, `0x9223`).

**The shop object** ("1S", `DS:0x453a`) plays song 1, waits up to 5400
ticks, and polls op 65 — so a panel whose script has ended (Get Ready)
no longer holds the shop open. The count is panels minus computer
trucks, so the invite panels of free trucks (scripts that never end)
balance out, and the shop closes once every human's panel has ended.

**A game shop panel's script** (red's at `DS:0x458a`, truck `0x4c68`):

| at | test | then |
|---|---|---|
| `0x45a4` | `[+2]` & 4 (setter not found) | → `0x465a` |
| `0x45ae` | `[+2]` & 2 — a computer truck | → `0x45fa`: become the join script `0x3dc6` (invite, registration) |
| `0x45b8` | op 82 — beaten by no computer | → `0x45be`: the upgrade boxes (child `0x4854`), then "Get Ready!" `0x91a8`, end |
| `0x4604` | op 83 — no credits | → `0x4652` |
| `0x460a` | | op 84 spends a credit **before** asking; page `0x9607` and op 81's countdown (`0x9625`). A press → the boxes; running out → `0x4652` |
| `0x4652` | | op 85; op 83 — credits left → the join script; none → `0x465a` |
| `0x465a` | | clear `[+2]` bit 4; colour and figure; page **`0x91b7`, "Game Over"** (sub-page `0x1b`, `IRON` `WR`, at (28, 56)); then `op 6 -4 \| yield` forever — it stays for the visit and nobody joins on it |

So "Game Over" shows when a player is out with no credits — including
one who let the countdown run out on their last credit — and never for
the last player out, whose op 85 ends the shop at once. A human beaten
only by other humans (op 82) pays nothing and goes straight to the boxes.

## 5. Keys

Every key is tested through `0x42f:0778`. `[0x674]` is a bitmask of held
modifiers, maintained by the keyboard interrupt handler: bit 0 right
shift, **bit 1 left shift, bit 2 Ctrl, bit 3 Alt**, then the lock keys.

| keys | where | what |
|---|---|---|
| **Alt + B** | `0x342` | the boss key: pauses and drops to a COMMAND.COM prompt, any time |
| **Ctrl + S** | `0x34ac` | on the Ironman Club: "Save HiScore table (Y/N)", answered Y or N |
| **F1**, then **F10** | `0x030a`–`0x0341` | F1 **pauses** a race with human players (not a demo race): once F1 is let go, `0x0323` loops on the keys — F1 again (waited out at `0x0337`) carries on, **F10** returns carry. `0x018c` → `0x01d0` turns the music off (`0x3ac8`) and leaves the race with carry; the game loop (`0x0115`) goes to `0x00d9`: demo flag, race 0 and standings (`0x1e4`), the title (`0x2468`) — no podium, shop or club entry. Nothing is drawn during the pause; the sound driver, on the timer interrupt (`0x39d6`), plays on. (An earlier note here read it as F1 + F10 held together.) SORP: F1's overlay pauses the race, and F10 with it up ends the game. |
| **1 / 2 / 3** | `0x3afe`–`0x3b14` | the audio question at launch |
| **Esc** | `0x358` | calls the driver's `es:[0x38]` |
| **F2** | `0x3b2f` | toggles `[0x9d38]` |
| **Space** | `0x1227` | only in the disk-swap wait |

## 6. Sprite banks

`EGA.EXE` holds the bank table at **`0x0ad6`** (after a 0x200 header),
eight bytes an entry: `+4` rows, `+5` top offset, `+6` width, `+7` x
offset. The `.PCG` files hold the pixels, four planes a row, in bank
order.

| file | first entry | notes |
|---|---|---|
| `ETRUCK.PCG` | — | 412 frames, 24 px wide, variable height |
| `EGAME.PCG` | — | 116 effects sprites, **complemented** |
| `ESHOP.PCG` | 587 | 70 sprites, the Speed Shop |
| `EEND.PCG` | 657 | 45 sprites, the results screen |
| `ETITLE.PCG` | 560 | 27 sprites, **complemented** |
| `ESHADOW.PCG` | — | 32 shadows |

**Complemented** banks store the inverse of the attribute, and their clear
index is 15 rather than 0. ETITLE's being complemented was established by
testing every plane permutation with and without: only complement gives
the Virgin box index 7 (red) with 14 (white), and the Off Road logo 5 and
14.

**Every piece carries its own top offset** (`+5`). Multi-piece artwork
must apply it per piece or it shears — the Off Road logo's five pieces are
6, 1, 0, 0, 0; the shop figure's three are 1, 0, 28.

### ETITLE groups

| entries | group |
|---|---|
| 560–564 | "Racing Hints" heading |
| 565–569 | "Awesome Wins" heading |
| 570–574 | Graftgold |
| 575–576 | Virgin |
| 577–581 | 16 px wide, colour indices unlike any neighbour — almost certainly the **Virgin Mastertronic** wordmark, never displayed |
| 582–586 | the Off Road logo |

### ESHOP's first sixteen — the registration cursor

Eight shapes, then the same eight again as the flash frame: 32×9 (0–3),
16×9 (4, 5), 32×46 (6), 16×46 (7). Index 0 is also the **inactive panel**
background and index 8 the **registering** one.

---

## 7. Screen layouts, measured from live memory

Objects store their position in **field `0x2a` (x)** and **`0x2e` (y)**,
unadjusted. Field `0x08` is the piece count, `0x5a` the sprite, `0x18` the
display-list sort key, `0x16` the next node.

### The Speed Shop

```
panels    sprite 587 (index 0, inactive) / 595 (index 8, registering)
          at x 8, 112, 216, y 31
figure    index 34, THREE pieces, at the panel origin, y 87
cursor    index 8 at x 115, y 48 — the panel origin +3, +17
```

**The gauges are objects, not sprite pixels.** op 77 (`0x2c2f`) spawns
one per box from record `DS:0x3ace` (draw type `[+0x1a]` = 0x18, no
sprite). EGA.EXE's draw-list walker (`9:257c`) calls the routine for each
object's type through `cs:0xa2a`, and type 0x18 is `9:65ab`. It writes
`[+0xc]` runs of colour **12**, each `[+0xe]` rows, then `[+0xd]` runs of
colour **5**, each `[+0xf]` rows, skipping `[+0x10]` rows after every
run. Each run is 7 px wide (masks at `9:659b`).

Those are **hardware** colour numbers. SORP's ESHOP pixels number the
planes the other way round, so a colour's 4 bits are reversed: 12 (1100)
is SORP's 3, the bar's grey, and 5 (0101) is SORP's 10, green. Confirmed
by the user against the original.

| gauge | at (panel +) | runs | rows each |
|---|---|---|---|
| nitro `[di+0x5d]` | 38, 31 | 1 empty + 1 full | 40 − n and n, n capped at 40; gap 0 |
| accel `[di+0x60]` | 85, 42 | 5 − level, then level | 5, with a 1-row gap |
| tires `[di+0x5f]` | 38, 88 | same | same |
| top speed `[di+0x61]` | 85, 88 | same | same |
| shocks `[di+0x5e]` | 38, 134 | same | same |

So nitro shows **one row per nitro** up to 40, not a fraction of 99. Its
bar sprite has no "LEVEL" heading, so the column spans rows 1–40.

The letter grid's cells step 20 across and 17 down, which follows from the
page's own metrics: ROADFONT fixed-pitch is 8 wide with letter spacing 2,
and its 9-row height plus leading 8 gives 17.

### The podium

```
banner    index 25, x 0, 80, 160, 240 at y 0, three pieces each
board     index 32, x 104, y 0, four pieces
figures   index  0, x 13, 121, 229 at y 25
flags     index 36, x 31, 139, 247 at y 122
plaques   index  3, x 7, 115, 223 at y 145
stage     index 24, x -13, 95, 203, 311 at y 154 (the 108 pitch)
field     signature pairs (28/30) on a 48 grid from x 40, y 36
```

The column pitch of 108 is shared by figures, plaques, flags and stage.

---

### The race screen's draw order

**One display list.** Each tick, `0x07e0` inserts every visible object into one list (head `DS:0x29a6`), skipping any whose `[si+0x1a]` has bit 15 set. The list is ascending by sort key `[si+0x18]`, and a new object goes after any with an equal key. The game hands the list to EGA.EXE with `lcall es:[0x60]`. That entry, `9:257c`, draws each object through `cs:0xa2a` by its type `[si+0x1a]`:

| type | routine | what |
|---|---|---|
| 0 | `9:57aa` | a truck: its shadow (`9:60a0`), then the truck |
| 0x18 | `9:65ab` | a shop gauge |

**Sort keys.**
- trucks and effects: world Y >> 5
- pickup labels: 0
- the flagman: 0x7fff

**The shadow darkens, it does not paint.** It writes with Map Mask = plane 0 only, setting bit 0 of whatever is already on the screen. Over another truck it turns that truck's colours to their dark twins.

**Only trucks are masked — and only near the ground.** The foreground mask applies to the truck bank (ETRUCK loads with flags 0x87). EGAME and ESHADOW are painted straight over the foreground. `0x15e8` sets bits 0–1 of the truck object's `[si+0xa]` and clears bit 0 once `[di+0x7d] - [di+0x7f]` (body over ground) is at least `[0x4f78]` — the track's byte from `DS:0x5240` × 256, set at `0x1374`. EGA.EXE's truck routine adds 7 to its blitter choice for bit 0 (`9:582e`): those blitters (`9:5af5` …) AND each byte with the mask at `ss:[di]`; the others (`9:59ea` …) copy straight. **A truck in the air is drawn over bridges and banks.** (White, `[si+0x1e]` = 3, adds 14: a third set.)

**The lift.** `0x15d0` projects the GROUND mean `[di+0x7f]` (`0x165f` takes its high byte alone) and stores it, −15/−14, as the object's point — the shadow's. EGA.EXE `9:57c4` then lifts the truck sprite by `[di+0x7e] - [di+0x80]`, the two heights' high bytes, one pixel a unit.

**Draw types** (`[si+0x1a]`, EGA.EXE far table at `cs:0xa2a`): 0 truck `9:57aa`, 4 sprite `9:3326` (the flagpole), 8 bunting `9:6394`, 0xc `9:346e`, 0x10 `9:643d`, 0x14 `9:64c6`, 0x18 shop gauge `9:65ab`. **The bunting** (type 8) takes its sprite as `0x1dc + [si+0x5c]`, not from `[si+0x5a]`, and draws that ONE table entry as a 32 px blit, then — `di + 4`, 32 px on — a 16 px blit with the source running on: the 48 px bunting is one sprite whose second half is the next frame's data.

**Object pools.** `0x6a0` builds a 64-object pool (0x7a bytes each). `[0x618]` counts the animated effects alone; 0x696 and 0x69b count them in and out, and the effect spawns cap them at 16. The two pieces of a pickup label (`0x5d1`–`0x622`, sprite 0x209 + n, then 0x213/0x214/0x215) come from the main pool and never compete with dust.

## 8. Truck physics

The per-wheel routine is at **`0x18df`**, called four times a tick after
`[0x4f7a]` is cleared at `0x154d`.

### Sampling

`0x1890` forms the sampling position as the truck's own **plus this
tick's movement step**, so wheels are tested where the truck is about to
be and the direction is corrected before the move is committed. Wheel
offsets come from the table at `DS:0x50dc`, amplitude 256 — one cell.

Each wheel's cell is the high byte of its world coordinate, the low byte
the fraction. The ground is a **plane per cell** (`0x1963`): the cell's
own height plus `gradient × (frac − 128)`, i.e. `gradient × (f − 0.5)` —
the stored height is the cell **centre** and the gradient tilts about it,
so neighbouring planes meet at the edges.

### Suspension

```
displacement = ground - wheel height
 wheel rising:  velocity += displacement × [di+0x63] / 65536   (the shock table)
 0x19ed:  if the result < 0x200 -> 0        (a dead zone)
          if >= 0x800 -> 0x800              (a CEILING)
          the wheel is snapped to the ground
 airborne: velocity -= 0x100 per tick       (gravity)
```

Shock constants: player 15360…9472, computer 19712…512.

### Impassable cells

A wheel over one is **not given the terrain at all** (`0x1a68`): its
height comes from `[di+0x7d]` (the mean wheel height) and its ground from
`[di+0x7f]` (the mean of the four **grounds**, averaged at `0x1dc0`), its
difference is zeroed, and the routine falls into the collision response
and **returns** — the suspension at `0x19ae` never runs for it. Such a
wheel still counts as *down* for steering.

### The collision response

Each tick `0x154f` clears the state `[0x4f7a]`, `0x1871` forms the step
from the speed `[di+0x85]` and the travel **as they stand**, and the four
wheels are placed in the order `0x1552` installs them — 225, 135, 315 and
45 degrees. `0x1871` cuts the step's direction to 32 by rounding the
travel's whole part **up** (`add 7, and 0xf8`). `0x18df` places each wheel
**when its turn comes**, from `(rel + [di+0x81]) >> 11` — the heading as it
stands, cut **down** to 32 — one cell out from the truck plus that step, so a
response on an earlier wheel has already turned the later ones. Each wheel
in a wall goes through `0x1a68`; one that is not runs its suspension:

- **state clear** — the full response, `0x1a86`;
- **state set** — `0x1b7c` only: if the travel is 64..191 from the normal
  (into the wall), it turns by half that angle (`shr`, unsigned) and the
  state becomes `0x83`.

The impact angle is `heading ^ [di+0x89] − normal`, where **`[di+0x89]` is
a travelling-backwards flag** computed at `0x1cad`: bit 7 of the slip
angle (heading − travel) plus 64, so 0x80 when the truck moves more than a
quarter turn from its nose. Exclusive-oring it adds a half turn, so the
impact is judged by the way the truck is actually going. `0x1a92` tests the
byte by **sign**:

```
192..255, 0..63  0x1aed  reflect: travel = heading ^ flag; state stays clear
64..119          0x1afa  travel = (normal + 0x40) ^ flag, heading - 0x1000
120..135         0x1a9e  head-on (below)
136..191         0x1b0e  travel = (normal - 0x40) ^ flag, heading + 0x1000
```

The 192..255 quarter — moving away from the wall — is a reflect, not a
glance. The glancing travel goes one way around the normal and the heading
is nudged the **other**; then (`0x1b28`) the state comes from the normal's
high nibble through `DS:0x5248`, a throttle `[di+0x75]` over 0x40 loses a
quarter (not below 0x40), and the speed is **set** to top `[di+0x6f]` (half
again under boost `[di+0x77]`, capped at 0xf0) × throttle >> 8.

The **head-on crash** `0x1a9e`: speed = 0xa0 × top >> 8, throttle and boost
zeroed, the heading turned ±0x20 at random (`0x42f:0x644`, bit 5), the flag
flipped, travel = new heading ^ new flag — the truck **bounces back** — and
state 3. Effect 8 plays on a glancing or head-on hit (`0x1ae0`), every
tick it happens.

The drive (`0x1d24`) then brakes a truck whose flag is 0x80 toward 0 by
`[di+0x6d]`; braking through zero (`0x1d5d`) stops it, sets travel =
heading and zeroes the throttle and the flag. The travel chase at `0x1ce7`
uses slip + flag, so a truck going backwards keeps going backwards.

**Penetration is prevented by rejecting the move per axis.** `0x1580`
commits the step formed at `0x1871` — the one the wheels tested, not one
from the new travel:

```
al = [0x4f7a]
test al,1 -> keep the old x   (the move is not committed)
test al,2 -> keep the old y
```

The table's values `0x80..0x83` carry those two bits.

### The throttle

`0x1769`: `[di+0x75]` gains 0x20 a tick while input bit 3 (up) is held and
loses 0x10 otherwise, kept within 0..0x100. A computer truck runs the same
code on the input byte its driver `0x2189` builds; the drive target is
top × throttle >> 8.

### Truck against truck

**Contact** comes from the object pass `0x0e25`. Each object ORs its
identity bit into column buckets (`0x2b10`) and row buckets (`0x2d70`)
over its shape, in coarse units (world >> 5) with 4-unit buckets. An
object's contacts `[si+0x6e]` are the identities found in **both** its
columns and its rows, mapped to class words. A truck's shape is its
per-heading footprint at `DS:0x52f8`.

**The response.** `0x153b` calls `0x169d` when `[si+0x6e] & [si+0x6c]` is
non-zero, before the throttle and the step. For each truck bit (1, 2, 4,
8) in it:

- clear this truck's class bits from the other's `[+0x6e]`, so the pair is
  settled once;
- skip it if the bit is in `[di+0x59]`, last tick's contacts, so only a
  **new** contact counts;
- play effect 0;
- `d = other's [0x55/0x57] − own`, less a quarter (`sar 2`);
- own = own + d, other = other − d;
- `0xf3e` turns each back into a direction byte, written to the travel's
  **high byte** only, and a speed (Newton square root). With both parts
  zero it returns direction 0 and speed 0;
- floor each throttle at 0x20.

**Which velocities.** `[0x55]` and `[0x57]` are the step parts `0x1871`
formed **last tick**, cut to 32 directions. A bump does not change them,
so a truck with two new contacts trades its old step in both and keeps the
second result. Positions are not moved apart.

### Steering

`0x1cc3`: at least one front wheel must be down (`ch & 3`); the rate is
`[di+0x67]`, halved when only one wheel is down. **Speed is not
consulted**, so a stationary truck turns freely.

---

## 9. Terrain data

`CONTOURn.XX` is exactly 3872 bytes — 44 × 44 words, no header or tail. A
cell word spends **bit 0** on the indirect flag and **bits 1..13** on the
offset, reaching 13256, so no bits are spare.

- Direct: the offset is a `POLY.BIN` record.
- Indirect: it is a `QUAD.BIN` record, whose corner is chosen by the
  **diagonals**, not by quadrant: +4 when `frac_x + frac_y` does **not**
  carry (`0x1930`: `add al,dl / jb` skips the `add si,4`), +2 when
  `frac_x >= frac_y`. SORP had the carry backwards until the arcade
  comparison showed it: the arcade's QUAD slots are the same four
  triangles, and its rule, with Y flipped, is this one.

`POLY.BIN` is 1823 records of four bytes: surface (0 ground, 2 water, 0x80
impassable), the surface normal's direction in sixteenths, and signed x
and y gradients (−80..76, −119..116). `QUAD.BIN` is 1658 records of four
corner words.

**Nothing is left unread**: every QUAD corner lands inside POLY, and all
1823 POLY records are reached from the eight tracks.

A **QUAD cell's corners may differ** while its height is stored once for
the whole cell, so a cell whose centre is a barrier can hand back a
drivable corner at the wall's height. 20% of layout 0's indirect cells mix
barrier and drivable corners.

The **height field** is integrated with the gradients, resolving a QUAD
cell at **fixed corners** — `[bx+2]` walking in x, `[bx+4]` stepping in y
— and accumulating `gradient × 128`. Drivable cells stay within their
layout's slope cap (`DS:0x6b0a`); 3–8% of adjacent pairs differ by 20 or
more, and those are the walls.

---

## 10. Sound

`ADLIB.SFX` holds 72-byte records: priority at +0, then **three voice
blocks** at +15, +38 and +61, each five-byte operators (`0x20 0x40 0x60
0x80 0xE0`) and a feedback byte. Effect fields: `+4` F-number, `+6` signed
slide, `+8` phase counter, `+11` duration, `+13` tail. Duration 0 means
percussion, struck through `0xBD`; the routing table is at `DS:0x529f`.
Effects 0, 7 and 8 are gated silent by `DS:0xb2b7`.

**The tick is the screen's refresh — 70 Hz on VGA.** The INT 8 handler
(`0x4d3a`) writes the low byte of the divisor `[0x6b0]`, waits on
`0x3da` for the retrace edge (counting down `[0x6ae]`), then writes the
high byte, so every tick is re-locked to the retrace; it counts
`[0x6b7]`/`[0x6b9]` and jumps on through `[0x6b3]`. Start-up (`0x4c32`)
times one frame with the PIT from `0xffff`, halves the count (mode 3
counts by two) and takes off `0x180` so the interrupt lands just before
the retrace it waits for; `[0x6b2]` gets the rate it found — **70**
under `0x4780` counts (above ~65 Hz), **60** under `0x5280` (above
~56 Hz), else **50** — which `0x4cd1` uses on exit to put the BIOS clock
right (`0x1234dc / rate`). `0x39d6` chains the sound in: `[0x6b3]` →
`0x3a08`, `call [0x9d39]`, one of the four vectors F2 gates. So the
sound driver runs once a video frame. (An earlier note here said 60 Hz:
that is only an EGA monitor's rate.)

Music: the song table at `DS:0xb449`, twelve bytes an entry — tempo, song
id, three stream offsets. Stream bytes are **phrase ids**; phrases live at
`DS:0xb623`, where a byte under 0x80 is a note, `0xB0..0xCF` strikes a
record, `0x80..0xAF` sets the note length, `0xFE` rests and `0xFF` fetches
the next phrase. A song loops only when its id word is non-zero — tracks
and the menu loop, anthems play once.

**An effect is three voices (`0x3c8e` → `0x3cf8`).** The record's
block 0 sounds on the effect's channel and block 1 on channel + 3, each
with its OWN countdown; when byte `+2` (the rhythm mask) is set, block 2
is a drum on the channel `DS:0xb29f` gives it, struck through `0xbd`.
Each voice is loaded from the record at `+3`, `+0x1a` and `+0x31`:
block, F-number word, slide, phase word, mode byte, duration, tail —
then `c0` and the two operators. `0x3cf8` writes no pitch: the first
tick's step writes `0xa0`/`0xb0`, the slide already applied. Byte `+1`
LINKS the second voice to the first (`0x4018`): each tick, before its
step, it takes the first's pitch plus its own slide (1), or an octave up
(2 and more). The phase starts at half its word plus one but RELOADS
from the word itself (`0x40c9` reads `[+0xe]`, stored unhalved). The
drum's `0xbd` bits clear when its duration runs out (`0x4052`).

So the white flag (effect 2) is a hi-hat over two voices sweeping down
(`0x280` at block 3, −1 a tick, 64 + 32 ticks; `0x200` at block 4, −2,
128 + 80), and nitro (10) a hi-hat over a short sweep down and one up.
A DOSBox capture (`guest os_002.dro`: effects 9, 4 and 2 twice) matches
SORP's own register log, register for register.

**The release table is corrupted by every drum.** The natural release
(`0x4111`) takes the operator from `DS:0xb296` (`00 01 02 08 09 0a 10
11 12`). Starting a drum, `0x3e3b` means to clear the drum channel's key
byte `[0xb28c + ch]` but writes `[0xb292 + ch]` — for channels 6, 7, 8
that is the table's entries 2, 3 and 4. After the first snare or hi-hat,
a voice ending on channel 3 forces channel 0's operators to `0xff` and
leaves its own: the capture shows exactly that. Nothing restores it.

**No effect holds.** Every effect record `0x20..0x2e` has a finite
duration (2–64 ticks; the drums 1 and 7 carry 0). Nitro, effect 10, is a
hi-hat strike of 8 ticks.

**Every music note is timed.** `0x3cf8` (a new instrument) and `0x3eb9`
(the same instrument) load the voice's countdown from the instrument
record exactly as an effect does: slide `+6`, phase `+8`, mode `+10`,
duration `+0x0b`, tail `+0x0d` for the first voice, and duration `+0x22`,
tail `+0x24` (words) for the second. It **never touches `0xbd`**: a drum
bit is cleared only by the next strike (`0x3d71`).

**The tick at `0x4089`, per channel:**
- **Duration `[+8]` running:** count it down. At zero (`0x40f5`), clear the key byte `[0xb28c+ch]`. With a tail, carry straight on to the step; without one, release (`0x4111`).
- **Tail `[+0xa]` running:** count it down, stepping each tick while it runs. At zero, release (`0x4111`: `0x80+op = 0xff`, then `0xb0 = 0`).
- **The step (`0x4096`):** count the phase `[+5]`. At zero, reload it and act on the mode `[+7]`: 0 snaps back to the base pitch, 1 reverses the slide, anything higher does both. The mode is **never decremented**. Otherwise add the slide `[+3]`. Then write `0xa0` and `0xb0` (`0x3c2d` has no cache, so this happens every tick), with the key bit from `[0xb28c]`.

So the **key-off keeps the pitch**: the `0xb0` written at the end of the duration still carries the block and the F-number's top bits. Writing `0xb0 = 0` there instead drops the fading note to a much lower pitch. SORP did that, and it put a stray note at the end of every anthem, whose last chord is the only note left to run its full duration.

**Phrase byte `0xFD` does nothing** (`0x4263` jumps back to read the next byte).

**The loop test checks two channels.** `0x41cb` restarts the song when
`[0xa012] | [0xa022] | [0xa022]` is zero — channel 1's phrase pointer is
OR'd twice and channel 2's (`[0xa032]`) never. The restart (`0x4134`) does
not touch the chip. Every shipped song's three parts are the same length,
so the quirk is inaudible.

**Channel 1 is the drum part.** In every race song channel 1 is almost all
record strikes: record 0 (snare, rhythm bit 3, sounding on OPL channel 7)
and record 2 (a kick at F 512 block 0 sliding down by 8 a tick — about
24 Hz).

---

## 11. Other tables

```
DS:0x1a3    race -> class          DS:0x7286  class -> ladder
DS:0x7276   ladder bias            DS:0x6b0a  per-layout slope cap
DS:0x53a0   nitro burst offsets, 8 octant pairs
DS:0x53c0   splash variants (4)
DS:0x5248   the collision state by normal nibble
DS:0x410    scoreboard cells (19)  DS:0x3f0   per-track origins
DS:0x96a8   the country -> flag permutation
DS:0x4fac   player steer (2..8)    DS:0x5038  computer steer (1152..1920)
DS:0x4f7c   player shocks          DS:0x4fe0  computer shocks
```

### Where the tables lie in the files

`GAME.EXE`'s MZ header is `0x400` bytes (`e_cparhdr` = 0x40 paragraphs),
so an image offset is file offset − `0x400`, and the data segment starts
at file offset **`0x5B00`**: `DS:xxxx` is file `0x5B00 + xxxx`. `EGA.EXE`
offsets below are already file offsets, header included. SORP's console
prints each of these as it loads it, as
`[OG_DataLoad: FILE] [Offset 0x… Bytecount 0x…] Loaded …`; the figures
here are from that log.

| file | offset | bytes | table |
|---|---|---|---|
| `GAME.EXE` | `0x5CA3` | `0x10` | race → ladder class (DS:0x1a3) |
| `GAME.EXE` | `0x5EF0` | `0x20` | scoreboard corner per track (DS:0x3f0) |
| `GAME.EXE` | `0x5F10` | `0x26` | scoreboard cells, 19 (DS:0x410) |
| `GAME.EXE` | `0x5F90` + 16·track | `0x10` | pickup selection (DS:0x490) |
| `GAME.EXE` | `0x8CB0` | `0x8` | dust and nitro effect script pointers (DS:0x31b0) |
| `GAME.EXE` | `0x8CC8` | `0x14` | scenery and flagman script pointers (DS:0x31c8) |
| `GAME.EXE` | `0xAA7C` | `0x54` | player upgrade tables, 7 × 6 words (DS:0x4f7c) |
| `GAME.EXE` | `0xAAE0` | `0xFC` | computer driver tables, 7 × 18 words (DS:0x4fe0) |
| `GAME.EXE` | `0xAC1C` | `0x64` | truck attitudes, 25 × (base, table) (DS:0x511c) |
| `GAME.EXE` | `0xAD40` | `0x8` | airborne threshold per track (DS:0x5240) |
| `GAME.EXE` | `0xAD58` | `0xA0` | start X, Y, heading per layout; grid slots (DS:0x5258–0x52f7) |
| `GAME.EXE` | `0xADF8` | `0x80` | truck collision box per 16 headings (DS:0x52f8) |
| `GAME.EXE` | `0xAEA0` | `0x20` | nitro burst offsets (DS:0x53a0) |
| `GAME.EXE` | `0xAEC0` | `0x20` | splash variants (DS:0x53c0) |
| `GAME.EXE` | `0xAEE2` | `0x20` | track screen origins X then Y (DS:0x53e2) |
| `GAME.EXE` | `0xC60A` | `0x10` | slope limits (DS:0x6b0a) |
| `GAME.EXE` | `0xCD76` | `0x8` | ladder bias per class (DS:0x7276) |
| `GAME.EXE` | `0xCD86` | `0x10` | ladder pointer per class (DS:0x7286) |
| `GAME.EXE` | `0xD03F` | `0x10` | rubber band (DS:0x753f) |
| `GAME.EXE` | `0xF1A8` | `0x9` | country → flag (DS:0x96a8) |
| `GAME.EXE` | `0xF3FB` | `0x404` | default Ironman Club (DS:0x98fb) |
| `GAME.EXE` | `0xF89A` | `0x3C` | sound effects, 15 × (pointer, priority), read by both the PC speaker and AdLib code (DS:0x9d9a) |
| `GAME.EXE` | `0x10DB7` | `0xF` | AdLib effect switches (DS:0xb2b7) |
| `GAME.EXE` | `0x10DC9` | `0x200` | AdLib note table, 128 (DS:0xb2c9) |
| `GAME.EXE` | `0x10F49` | `0xE4` | music, 19 songs × 12 bytes (DS:0xb449) |
| `EGA.EXE` | `0x0CD6` | `0xCE0` | sprite table, ETRUCK's 412 entries |
| `EGA.EXE` | `0x19B6` | `0x100` | ESHADOW's 32 entries (412–443) |
| `EGA.EXE` | `0x1AB6` | `0x3A0` | EGAME's 116 (444–559) |
| `EGA.EXE` | `0x1E56` | `0xD8` | ETITLE's 27 (560–586) |
| `EGA.EXE` | `0x1F2E` | `0x230` | ESHOP's 70 (587–656) |
| `EGA.EXE` | `0x215E` | `0x168` | EEND's 45 (657–701) |
| `EGA.EXE` | `0x236D` | `0x1B0` | text styles, 18 × 24 bytes |
| `POLY.BIN` | `0x0` | `0x1C7C` | 1823 polygon gradient records, 4 bytes each |
| `QUAD.BIN` | `0x0` | `0x33D0` | 1658 quad corner records, 8 bytes each |

The per-layout tables (the route program at DS:0x7550's pointers, the
checkpoints at DS:0x6b1a's, the scenery at DS:0x1ee's, the pickup spawn
points at DS:0x510's) sit wherever their pointers say; the log gives each
one's offset and length when its race loads.

### `IRONHI.BIN` and the club table

**The table.** IRONHI.BIN is `"RI"` followed by the club table, `DS:0x98fb`, 0x404 bytes:
- eleven u16 offsets, relative to the table, then a zero word;
- eleven 52-byte records. The first ten offsets are the standings. The eleventh slot is spare.

A record:
- `+0`: initials (3 bytes).
- `+3`: birthdate (6 bytes).
- `+9`: country.
- `+0xa`: money, a BCD word in units of $10,000.
- `+0xc`: 40 bytes copied from the truck's `[+0x13]`:
  - eight BCD words, the player's best **winning** race time on each track (`[+0x13 + track·2]`);
  - then three bytes per track at `[+0x23 + track·3]`: a race count and a binary word, the player's **average race time** there in ticks (1/20 s). Both are zero in the shipped file.

Past the records, the table keeps per-**track** figures (`[0x1a0]`, the physical track), all BCD seconds.hundredths:

| Table offset | DS | Contents |
|---|---|---|
| `+0x254` | `0x9b4f` | the track record, a word per track |
| `+0x264` | `0x9b5f` | its holder's initials, 4 bytes per track |
| `+0x284` | `0x9b7f` | per track, 12 words: the ten best **winning** times, a spare at `+0x14`, a zero |
| `+0x344` | `0x9c3f` | the same for the players' **average** times |

The shipped file has every figure at `9999` and every holder `XXX`.

**The code:**
- **Load (`0x3635`), at startup (`0xbf`):** read the file. If it starts with `"RI"`, copy 0x404 bytes over the table. Otherwise keep the table already in the data segment, which is byte-for-byte the shipped file. That is the default.
- **op 86 (`0x35c9`), at the end of registration (SKIP too, which jumps to the same place):** compare the 10 bytes at the truck's `[+5]` (initials, `[+8]` month, `[+0xa]` day, `[+0xc]` year, `[+0xe]` country) with each record. On a match, `[+3]` points at the record and its 40 bytes are copied to `[+0x13]`. Otherwise `[+3]` = 0 and the times become 8 × `9999`, then zeros.
- **The race's figures (`0x371a`), for each HUMAN truck once the race is over** (`0x36d3` runs `0x3708` for all four trucks, then `0x371a` for all four, and `0x371d` skips the computers):
  - **The money (`0x3727`–`0x374e`):** the place's prize (table `0x9d0a`) plus the pickups `[+0x62]`, added in BCD to the cash `[+0xf]` and the total `[+0x11]`. Each is a BCD word in units of $10,000, and the carry out of the top digit is dropped, so past 9999 ($99,990,000) it would wrap round to almost nothing. A computer truck gets none of it: `0x371d` skips it all. SORP, which paid every truck until 2026-10-19 so that attract mode piled up millions, now skips computers too. It also stops the cash and total at 9999 instead of wrapping (`OG_CASH_MAX`); that's the most the shop, the club and IRONHI.BIN can show.
  - First `0x381c` sets the race time: `[+0x46]` in ticks, and `[+0x44]` = BCD(ticks × 5), or `9999` from `0x7d0` (100 s) up. A truck still running gets a **projected** time: the lap began at `[+0x46]`; `0x37fe` turns the progress `[+0x42]` into a 1/65536 fraction of the lap; and the lap time is elapsed ÷ fraction (`dx:ax` = elapsed:`0x7d1`). The total is capped at `0x7d0`. A fraction no bigger than the elapsed ticks gives `0x7d0`. No fraction leaves the count as it is.
  - **A win** (`[+0x3b]` = 0, `0x3758`): `[+0x13 + track·2]` = min(itself, `[+0x44]`). `[+0x44]` goes into the best list's spare slot, which is then sorted (`0x38b5`, a rising bubble sort up to the zero). If it beats the track record, it becomes the record and `[+5..7]` the holder (`0x378a`).
  - **Every race** (`0x37a5`): average = (average × count + `[+0x46]`) ÷ (count + 1), count + 1. A count that reaches 128 is halved and 1 added. The average, as BCD(× 5) or `9999` from `0x7d0`, goes into the average list's spare slot, which is then sorted.
- **op 85 (`0x2f76`), when a player's game ends:** this is just before the "Game Over" page, and it also hands the truck to the computer. Write the player into the spare slot: initials, then `[+0xa]` `[+8]` `[+0xc]`, so the birthdate goes in **day-month** order, then the country, the money `[+0x11]` and the 40 bytes. Copy the 40 bytes back into the record op 86 found, if any. Then sort.
- **Sort (`0x36ac`):** bubble-sort the eleven offsets by money, most first.
- **Save (`0x3669`):** write `"RI"` plus the table, 0x406 bytes. With AL = 0 it first checks that the file exists (`0x42f:0xd09`, DOS find-first) and skips the save if not. With AL = 1 it saves unconditionally.
  - **After every game (`0x134`):** AL = 0, then the club screen (`0x347e`).
  - **Ctrl+S on the club screen (`0x34ac`):** draws `"Save HiScore table (Y/N)"` (page `0x9872`) and waits for Y (scan `0x15`) or N (`0x31`). Y saves with AL = 1. Either answer redraws the club screen from the start.

**An original bug.** The shipped records hold the birthdate in the truck's month-day-year order, but op 85 writes day-month-year. So a player op 85 has entered is not recognised on their next game unless the day and month bytes happen to match.

**SORP's version:**
- **Reading:** SORP reads IRONHI.BIN from beside sorp.exe. Only if that is missing does it read gamedata's copy, and only if both fail does it fall back to the table in GAME.EXE.
- **Writing:** it saves only to the copy beside sorp.exe, the way the original does: after a game if that file exists, and with Ctrl+S then Y on the club screen.

---

## 12. Palettes, and how to read them

**An index means nothing without naming its palette.** The title's 5 is
yellow, the shop's 5 is white, the results' 13 is yellow. Three separate
colour bugs in this project came from assuming an index's meaning.

The **title palette** was derived by decoding `TITLES.XX` and reading off
what each index shows as in a capture — every index with enough pixels
agreed 96–100%, and it does **not** change between the four views.

The **results palette**: `0` black, `1` skin, `2` white, `3` green,
`4` blue-grey, `5` light grey, `6` brown, `7` orange, `8` dark blue,
`9` light blue, `10` dark red, `11` orange-red, `12` gold, `13` yellow,
`14` skin, `15` dark brown.

> The palette debug row in a DOSBox capture is drawn **right to left**.
> Reading it left to right makes 14 and 15 look black, which sent us down
> a long wrong turn — the figures went black and EEND looked complemented
> when it is not.

## 13. Demo mode, and the computer drivers

`[0x1a1]` is the **demo flag**: zero while humans are racing (which is
why F1 pauses, and F10 then ends the game, only then, at `0x030a`), non-zero through the attract
races.

### The rubber band, which demo mode skips

At `0x2321`, run only when `[0x1a1]` is clear. **Each computer truck is
banded individually against the human** -- `di` is the truck being
adjusted (pushed and restored around the sort), and `si` is found by
walking `DS:0x9d2e` for the first truck whose computer bit is clear:

```
for each of the four trucks (0x4c68, 0x4d2a, 0x4dec, 0x4eae -- 194 apart):
    0x37fe:  [di+0x42] = ([di+0x42] << 16) / divisor[[0x19f]],
             saturating at 0xffff
0x3878:  bubble-sort the standings by lap [+0x3c] then progress [+0x42],
         each swap moving the trucks' places in [+0x3b]
         -- so a place is ALWAYS current, never assigned at the finish
then THIS truck against the human:
    a lap behind -> +5        a lap ahead -> -5
    same lap     -> al = [di+0x43] - [si+0x43]
                    sar al,1 x4            SIGNED
                    xlatb from DS:0x753f   UNSIGNED index
    level = clamp([di+0x5c] + that, 0..0x11);  [di+0x5b] = level
    0x14cc applies it
```

`[di+0x43]` is the **high byte** of the progress word, so a lap spans
0..255 there and `>> 4` gives sixteenths.

**The signed-shift quirk is where the difficulty lives.** The shift is
arithmetic, so a truck BEHIND the human yields a negative value -- and
`xlatb` then reads it as 0xf0..0xff, past the sixteen deliberate
entries:

```
index 0..15   (ahead)   0 0 -1 -1 -1 -2 -2 -2 -2 -3 -3 -3 -4 -4 -5 -5
index 240..255 (behind) -125 11 14 13 14 10 11 9 8 -125 12 4 7 7 5 7
```

The deliberate half only ever **pegs back** a truck that is ahead; a
trailing truck falls into the second row and is usually clamped to level
17, which is why the opposition surges when it drops away. Taking the
first table for the whole story makes the field far too passive.

`0x231a` jumps the whole block in demo mode, so **an attract race never
rubber-bands** -- the computer drivers hold their base level.

### A driver level is one row of seven tables

`0x14cc` copies a level's row into the truck:

| table | into | what SORP calls it |
|---|---|---|
| 0x4fe0 | [di+0x63] | OG_CPU_SHOCK_DS |
| 0x5004 | [di+0x6d] | OG_CPU_DRAG_DS |
| 0x5028 | [di+0x67] and [di+0x69] | OG_CPU_STEER_DS |
| 0x504c | [di+0x6b] | OG_CPU_SLIP_DS |
| 0x5070 | [di+0x65] | OG_CPU_GRIP_DS |
| 0x5094 | [di+0x71] | OG_CPU_ACCEL_DS |
| 0x50b8 | [di+0x6f] | OG_CPU_TOP_DS |

Eighteen levels. The columns do NOT all run the same way -- 0x4fe0 falls
from 19712 to 512 while 0x50b8 rises from 126 to 192 -- so a single
column tells you nothing about whether a level is "fast".

### The ladder

`0x01ed`: the race number `[0x19e]` indexes the table at **DS:0x1a3** --

```
03 06 0b 0e 09 03 06 01 05 0b 0e 09 0d 0f 03 06
01 05 0d 0f 04 00 02 0b 0e 09 08 0a 03 06 01 05
04 00 02 0b 0e 09 0d 0f 0c 08 0a
```

**Forty-three entries**, not sixteen -- `xlatb` applies no mask. The
first sixteen reach only tracks 1, 3, 5, 6 and 7; tracks 4, 0 and 2
arrive later, and all sixteen layouts have appeared by race 42. The
byte at +43 is `0x38`, outside the set, which is where it ends.

Bit 3 of each byte is the **REVERSE flag**, so the whole byte IS the
layout. Attract mode walks the same table (see "The attract Speed Shop")
rather than a list of its own.

The whole byte becomes `[0x19f]`, the difficulty that picks the divisor
above; its **low three bits** become `[0x1a0]`, the track. So race 0 is
track 3, race 1 track 6, race 2 track 3 again at a higher difficulty,
and so on.

### The computer trucks never fire nitro

`[di+0x74]` is the **control word** -- bit 0x40 is the fire button, set
by the player's handlers at `0x2d78` and friends. `0x17bc` spends a
nitro from `[di+0x5d]` when it is set and the boost timer `[di+0x77]`
has run down.

Nothing in the AI sets that bit on its own judgement. The only thing
that does is the **route program**: the dispatch table at `DS:0x7519`,
indexed by `op & 0x7f` (`0x22ab`), sends opcode **`0x88`** to the firing
handler at `0x2301`:

```
0x2301:  lodsw                      the opcode's two bytes
         call 0x238d                a LEVEL RANGE test:
                                      bl = [di+0x5b]
                                      fail if bl < al or bl > ah
         jb skip
         al = [di+0x77];  jne skip  refuse while a boost runs
         or [di+0x74], 0x40         press the button
```

So a track marks where nitro should be used and which drivers may use
it. **No shipped layout contains `0x88`.** Counting every opcode the
route walk sees across all sixteen layouts:

```
80: 138   83: 92    85: 110   87: 22
81: 9     84: 214   86: 16    8a: 16
82: 3
```

0x85 (re-weigh the band) appears 110 times, so the walk is plainly
reaching opcodes -- and `0x88` appears **zero** times. The machinery
ships complete and the data never asks for it.

### The joystick question, and calibration

`DS:0x4c32` is `"ARE YOU USING A JOYSTICK (Y\\N)"`, asked at launch
beside the 1/2/3 sound question:

```
0x1278  al = 0x31 (N)  ->  al = 0, no stick
0x1283  al = 0x15 (Y)
0x128c  di = 0x676;  lcall 0x42f:0x7e8     calibrate stick 1
0x1294  di = 0x688;  lcall 0x42f:0x7e8     calibrate stick 2
```

The calibration itself just records where the stick is sitting as its
centre:

```
0x42f:07e8   [0x6a6] = 0xff            enable reading
             0x42f:088e                sample the axes into [di], [di+2]
             [di+4] = [di], [di+6] = [di+2]      the centre
             restore [0x6a6]
```

so `[di]`/`[di+2]` are the live axes and `[di+4]`/`[di+6]` the centre
that `0x83c` compares against. `[0x6a6]` is the master "a stick is
fitted" flag -- `0x83c` returns nothing at all when it is clear.

### A joystick descriptor is a whole STICK, not one axis

`0x42f:0x88e` reads the PC game port at 0x201 and times its **four
axes**, writing each as a word into the two descriptors:

```
0x8e6   [0x676] = axis 0        stick 1 X
0x8ed   [0x678] = axis 1        stick 1 Y
0x8f4   [0x688] = axis 2        stick 2 X
0x8fb   [0x68a] = axis 3        stick 2 Y
0x907   [0x686] = buttons 1 and 2   (0x30, with 0x687 the previous)
0x915   [0x698] = buttons 3 and 4   (shifted down two)
```

So **0x676 and 0x688 are the two sticks**, each holding BOTH its axes
at +0 and +2 -- not four separate axes. `0x42f:0x83c` confirms it by
producing all four directions from one descriptor:

```
[di+0] X live, [di+4] X centre  ->  bit 2 left, bit 1 right
[di+2] Y live, [di+6] Y centre  ->  bit 8 up,   bit 4 down
[di+0x10] the previous bits, [di+0x11] a mask;
dl << 2 & 0xc0 adds the EDGE bits -- newly pressed this poll
```

and the calibration at `0x42f:0x7e8` stores a centre for each axis of
one descriptor (`[di+4] = [di]`, `[di+6] = [di+2]`), which only makes
sense for a two-axis device.

### The first two players are on the KEYBOARD

`0x1508` picks a descriptor AND a reader by player number:

```
player 0 -> si = 0x5378  \  lcall 0x42f:0x7bc    the KEY reader
player 1 -> si = 0x538c  /
player 2 -> di = 0x676   \  lcall 0x42f:0x83c    the JOYSTICK reader
player 3 -> di = 0x688   /
```

So the two game-port sticks are players 3 and 4; players 1 and 2 share
the keyboard. `0x42f:0x7bc` walks a list of triples from the descriptor
-- a byte index into the key bitmap at `DS:0x61a`, a bit mask, and the
control bit to set -- ending at 0xff, then applies the same edge logic
as the joystick reader:

```
PLAYER 1 -- DS:5378          PLAYER 2 -- DS:538c
   Q        up                  ]        up
   A        fire                '        fire
   Z or X   left                M or ,   left
   C or V   right               . or /   right
```

Two keys per direction, and the two sets sit on the left and right
halves of the keyboard so two people can share it. Note there is no
DOWN in either map -- a truck has no reverse.

SORP supports three players, as the original and the arcade game do
(white is always IRONMAN): red and yellow on sticks, blue on WASD.
The original's own layouts above are a closer match for two players at
one keyboard.

### The attract Speed Shop (`0x2bf8`)

The attract loop (`0xe6`) is title `0x2468` -> race `0x140` -> `0x36d4`
-> podium `0x309a` -> club `0x347e` -> shop `0x2bf8` -> `0x1d5`, then
round again. Each step leaves early with carry when a player presses fire.
`0x1d5` moves on to the **next schedule entry** (`[0x19e]`+1, wrapping
43 to 27). Only a game's end (`0xd9`, `0x1e4`) puts it back to 0. So
the demo races walk the championship schedule; they don't alternate
two tracks.

`0x2bf8` is the same routine the game uses after each race. With
`[0x1a1]` set it spawns from `DS:0x31f8` (record `0x48ec`) instead of
`0x31f4` (`0x4538`). It runs until the script sets bit `0x10` of `[0x19c]`
or fire is pressed (`0x253c`), and it ends with `0x3ac8`, music off. Unlike
the game's shop (`op 55 music 1`), it starts **no music**.

`0x48ec` spawns the background (`0x3764`, `0x3852`) and three panels,
`0x4924`, `0x4970` and `0x49bc`:

| panel | truck `[+6]` | `[+0xc]` (cursor, counter) | money page `[+0x10]` | end page |
|---|---|---|---|---|
| red | `0x4c68` | 0, 5 | `0x9574` | `0x9190` |
| yellow | `0x4d2a` | 0, 10 | `0x95a5` | `0x91c6` |
| blue | `0x4dec` | 0, 15 | `0x95d6` | `0x91fc` |

Each panel does three things:

**The Speed Shop's numbers (op 78, `0x2d53` → `0x2e88`).** Each panel's
credits and cash are ONE page, its `[+0x10]`: `DS:0x9574`, `0x95a5`,
`0x95d6`, 0x31 bytes apart:

```
+00 17 "IRON" 18 "W3" 10 0e ff        IRON, W3, proportional, spacing -1
+0b 04 xx xx yy                       at(panel x + 0x11, panel y + 0xd)
+0f "^" 05 00 10 "^^^^^^^" 05 ff fc "^"   blocks over both insets
+1e 02 xx xx  +21 '0'                 x = panel x + 0x11; the credits
+22 02 xx xx  +25 "$00,000,000"       x = panel x + 0x5c - width
```

The panel is the position at its `[+0x1c]`, the origin op 77 places the
gauges from (`0x2d40`). `0x2e88` writes `'0' + [di+1]` at `+0x21` (one
character), blanks `+0x25`..`+0x2a`, lays the BCD word `[di+0xf]` in
backwards from `+0x2a` — a nibble a digit while any is left, a comma
before the third — then `'$'`; the template's own `0,000` follows, so no
money reads `$0,000`. `0x2eed` sets `[0x984]` = −1 and `[0x98a]` = 0 and
swaps IRON (`[0x160]`, the load record of `ironfont.xx`) into `[0x730]`,
measures the string from `+0x25` — its blanks included — with
`0x42f:0xf34`, and puts `x = panel x + 0x5c − width` at `+0x23`; then
`0x42f:0xf99` draws the page.

IRON's `^` is a solid 8×12 block of pixel value **15**. Every style maps
0 to "leave", 1 to its fill and 2–15 all to its outline colour (the rows
above), so `^` is drawn in W3's outline, 14 — black — and the eight
blocks repaint the insets: credits x 17–24, cash 40–92 (at spacing −1),
y 13–24. Only `^` uses a value above 2, in both fonts.

1. **480 ticks: the panel shows the shop.** It spawns the shop child `0x4a08`, the same boxes as the game's: op 77 `0x2c2f` builds them, and op 78 `0x2d53` / `0x2e88` writes the credits and money. All of it comes from that truck's real fields, so nothing is substituted. The cursor is driven by **op 76 (`0x2e40`)**, not the stick:
   - `[si+0xd]` counts down; every 16th tick (`& 0xf` = 0) the cursor takes one random step, +1 or -1 by bit 0 of `0x42f:0x644`.
   - It wraps over the six boxes, plays effect 0xb (`0x2bc0`), and is placed from `DS:0x955c` / `0x9568`.
   - The panels' counters start at 5, 10 and 15, so the three cursors move at different times. Nothing is ever pressed.
2. **240 ticks: the panel shows the invite.** The girl and trophy (`0x377e`, `0x3874`, `0x3794`) appear, with "Player N" (sub-page `0x1d`..`0x1f`, `DS:0x88d5`) at (32, 40) and "Press  Button" (sub-page 9, `DS:0x8781`) at (28, 56), panel-relative. These are the same pages as the real invite panel.
3. **The panel ends.** When all three have ended, `0x48ec`'s op 64 loop falls through and sets bit `0x10`.

**What the boxes show.** At each race start a computer truck gets nitro 10
(`0x14c7`), and pickups add to it. Its upgrade levels `[+0x5e..0x61]` are
never set for a computer. The 25 nitro and zeroed levels and money of
`0x13a3` are the reset a **joining** truck gets (`[+2]` bit 0). So the
demo panels show each computer truck as the demo race left it, plus
whatever a former player's truck kept after its game ended.

**The game's shop handler (`0x2d68`)** reads the stick every 2nd frame
with no wait for a release:
- right (bit 0) steps **+1**, clockwise from Nitro to Acceleration; left steps -1;
- every press plays effect 0xd (`0x2beb`); a box already at its top plays 0xe (`0x2bde`).

The START NEXT RACE thumbnail (`0x2c4d`) is the track of the race **after** the one just run: `[0x19e]` + 1, wrapped from 43 to 27 as `0x1d5` does, looked up in `DS:0x1a3` and masked with 7. The shop runs before `0x1d5` moves the race number on.

**SORP's version:**
- **Demo panels:** red, yellow and blue show their own shop boxes for 480 frames, with each cursor stepping at random every 16 frames from counters 5, 10 and 15.
- **Invite:** then the invite panels for 240 frames.
- **Music:** the club and both attract shop screens are silent. SORP stops the anthem as the club appears, and the attract shop starts no song (it opens through the podium's path, which plays song 1 only in a game).
- **Order:** the shop opens only once the club screen is done (podium, club, shop, as `0xe6` runs them). The podium's own 8 s hold is for games only; in attract mode it used to open the shop behind the 5 s club, and the club's ENTER then skipped the demo.
- **Demo races:** they walk the schedule from race 0. A player joining puts the race number back to 0 (`0x107` -> `0x1e4`), and so does a game's end. After entry 42 the schedule continues from entry 27, as `0x1d5` does.

### What ends an attract stage

`0x2c04` and `0x30c1` both call `0x253c` **only when `[0x1a1]` is set**,
and break out of their loop if it returns carry. `0x253c` polls all four
players for the fire button:

```
0x1508:  player 0 -> DS:0x5378    player 1 -> DS:0x538c
         player 2 -> DS:0x676     player 3 -> DS:0x688
         then 0x42f:0x83c reads the descriptor
```

All four are **joystick** descriptors (`0x83c` compares axis values
against a centre), zero in the file and filled at run time -- the game
polls four sticks, as its arcade parent did. The bits it builds:

| bit | meaning |
|---|---|
| 0 (0x01) | right |
| 1 (0x02) | left |
| 2 (0x04) | down |
| 3 (0x08) | up |
| 4 (0x10) | fire -- what `0x253c` tests |

### Seeding the field (0x20c6 -> 0x2105)

The level the field starts from is built from **three** things, not one:

```
0x20c6:  al = 0
         walk DS:0x74e6, SKIPPING computer trucks:
             al = max(al, [di+0x5c])     the best HUMAN's ladder level
0x20e6:  al >>= 1                        HALVED
0x20e8:  ah = [0x19e] >> 2               a QUARTER of the race number
0x20f2:  al = track_base[[0x1a0]] + ah + al      DS:0x74f0
         clamp 0..0x11
0x2105:  for each computer truck, in the order DS:0x74e6 lists them:
             [di+0x5c] = al      the base the band drifts from
             [di+0x5b] = al      the level in use
             dec al;  if it goes negative, clamp to 0
```

`DS:0x74f0` is `2 2 2 2 2 2 1 2` -- every track but 6.

The halving is why driving well moves the field only half as much as the
ladder figure suggests, and the race-number term is the **season ramp**:
without it the opposition never hardens as a championship goes on.

`DS:0x74e6` is `4eae 4dec 4d2a 4c68 0000` -- **white, blue, yellow,
red** -- so the field is not uniform and WHITE takes the highest level.

A demo race sets `al = 0x11` for every truck instead.

### The ladder's own figure (0x2132)

Run for a HUMAN truck only (`[di+2]` bit 1 clear):

```
bx = [0x7286 + [0x19f]*2]      a step table for this class
al = 0
while table[al] >= [di+0x48]:  the player's AVERAGE LAP
    ++al, until 0x11
al -= [0x7276 + [0x19f]]       a per-class bias
clamp 0..0x11  ->  [di+0x5c]
```

`[di+0x48]` is `[di+0x44] / laps` (`0x3853`), capped at 0x7d0. The step
tables run **456 down to 176**, which at ~20 Hz is 22.8 s to 8.8 s --
real lap times, confirming the lap timer's rate.

## 14. The results screen, and the credit economy

### The standings, and dead heats

The standings are one list, `DS:0x9d2e`: four truck pointers, first
place first. A truck's place `[di+0x3b]` is only ever its index in that
list, so **two trucks can never share a place**.

- **Reset (`0x1e4` -> `0x12f9`):** white, blue, yellow, red, only when the race number goes back to 0 (attract or a game starting). Between races the list carries over, so each race starts in the previous race's finishing order and a dead heat goes to the truck that placed higher last time.
- **The sort (`0x3878`):** bubble sort by lap `[+0x3c]`, then progress
  `[+0x42]`. A pair swaps only when the lower truck is **strictly** ahead
  (`0x389c ja`, `0x38a6 jae`), so trucks level on both keep their
  existing order: a dead heat goes to whichever stood higher before.
- **When it runs:** with the rubber band (`0x233b`, skipped in demo mode),
  and once more at the race's end (`0x36ec`, after `0x3708` has scaled
  every truck's progress by the layout's divisor with `0x37fe`).
- **Crossing the line records no order.** Lap 4 sets bit 1 of `[0x19c]`
  (`0x1fc5`) for the chequered flag; the trucks drive on, and the final
  sort ranks them by lap and progress at that moment.
- **Everything uses the sorted places:** the prize (`0x3727`, table
  `DS:0x9d0a` by place), the club's "won" (`0x3751`, place 0) and the
  plaques.
- **Script ops 62 (`0x25e3`) and 63 (`0x3088`)** flag a truck (`[di+2]`
  bit 0 or bit 3) and walk it down below every unflagged truck (`0x25f8`).
  They follow the "Press Button" invite page, so they are for a human
  dropping out. `0x3708` clears both flags before the end sort.

### Which trucks get a plaque

`0x30d6` builds three plaques, from the trucks at `0x4c68`, `0x4d2a`
and `0x4dec` -- **red, yellow, blue, left to right at x 7, 115, 223**.
A column is bound to a TRUCK, not to a finishing position; what varies
is the place printed on it, from that truck's own `[di+0x3b]`.

**The white truck substitutes into one column.** `0x3111` opens by
asking, for the truck in each column:

```
0x3112  al = [di+2]; test al, 2
        je 0x3140              a HUMAN column -- leave at once, no swap
        bl = [di+0x3b]         this truck's PLACE
        bx = place*2 + 2       walk the standings DOWNWARD from there
0x3123  cmp bx, 8;  je 0x313d  ran off the end -> use WHITE
        bx = [bx + 0x9d2e]     the truck standing there
        al = [bx];   cmp al, 3     white?  -> step on
        al = [bx+2]; test al, 2
        je 0x3120                  human?  -> step on
        jmp 0x3140             a non-white COMPUTER below -> keep this one
0x313d  di = 0x4eae            THE WHITE TRUCK
```

So a column shows white when **no non-white computer truck places below
it** -- that is, when it holds the lowest-placed computer truck. The
three observed rules all follow:

| field | columns |
|---|---|
| all computers | red / yellow / WHITE |
| red human | red / yellow / WHITE |
| red and blue human | red / WHITE / blue |
| all three human | red / yellow / blue |

Note it does **not** test whether white won -- the swap happens on
placing, so white appears whenever at least one computer truck is in
the field.

### Country, flag and anthem

`0x12ae`, called once from start-up (`0xc2`), sets every truck — white
included — to initials `XXX` ([di+5..7]), birthdate `11 27 68`
([di+8..0xd], ASCII) and **country 8** ([di+0x0e]), "no nation". Only the
nation picker (`0x2bb3`) writes the country again, so a computer driver
always has 8, and a player who SKIPs keeps whatever the truck had.

- **Flag:** `0x3183` reads the country of the truck **standing in the
  column** — so the white truck substituted in (Ivan Stewart) brings his —
  and maps it through `DS:0x96a8` = `4 1 5 3 0 2 6 7 8` to EEND 36 +
  n: France, Italy, Japan, Britain, USA, Germany, Canada, Australia, and
  44, the **signature banner**, for country 8.
- **Anthem:** `0x30b6` plays the winner's country + 2: songs 2–9 for the
  eight nations, **song 10** for a computer winner.
- **The flags are the arcade's own** (ESHOP 16–23 as they are, EEND 36–44
  through a pen map, 44 being the arcade's "no nation" flag), and a club
  record's country is only compared (op 86) and copied, never shown or
  used as an index.
- **SORP's addition** (not GAME.EXE's): with "arcade roms" on, the nation
  list has a second page, the arcade's other eight as countries 9–16,
  their flags converted to the DOS colours the same way; 9–16 play song 10
  with the DOS sound and their own anthem with the arcade's.

### Plaque names follow the truck in the column

0x3111 rewrites each column's template at fixed offsets before every draw. Here `[di]` is the identity of the truck standing in that column (0 red ... 3 white), and `[di+0x3b]` is its place:

| Template | Offset | Value | Meaning |
|---|---|---|---|
| computer | +0x0b | `'1' + [di]` | style E1–E4 |
| computer | +0x16 | "1st"... from DS:0x96b1 | place |
| computer | +0x27 | `13 + [di]` | nickname sub-page: MADMAN, HURRICANE, JAMMIN', IRONMAN |
| computer | +0x29 | `17 + [di]` | name sub-page: Sam Powell, Earl Stratton, John Morgan, Ivan Stewart |
| human | +0x16 | place | as above |
| human | +0x22 | `[di+5..7]` | registered initials |

When 0x313d substitutes the white truck into a column, the plaque therefore reads IRONMAN / Ivan Stewart.

The finish-time board at 0x3428 names the winner, `di = [0x9d2e]`, not column 1's truck.

### Two plaque templates

`0x318f` picks by `[di+2]` bit 1, the same flag as everything else:

```
DS:0x96df   96eb 9716 9741   COMPUTER: "xxx Place", nickname, driver name
DS:0x96e5   976c 97a4 97dc   HUMAN:    "xxx Place : xxx" -- the place and
                                       the player's initials, one line,
                                       no nickname and no driver name
```

A human's plaque is not the computer's with a name swapped: it is a
different page, and the driver name is simply absent.

### The time board

`DS:0x981c`, filled by `0x33d7` and `0x3428`:

```
"xxxx Finish Time:  00.00"      the name at +16, the time at +35
"Record (XXX) :      00.00"     initials at +52, the record at +64
```

The name follows the same rule: a COMPUTER truck shows its
four-character first name from `DS:0x9862` (`" Sam"`, `"Earl"`,
`"John"`, `"Ivan"`), a HUMAN a space and their three initials.

`0x33d7` takes the record from **`DS:0x9b4f`** (`[bx - 0x64b1]`,
indexed by `[0x1a0] * 2`), with the holder's initials at `DS:0x9b5f`
(4 bytes a track). Both are **inside the club table**, so they are saved
in IRONHI.BIN. An earlier reading of these as `DS:0x924f`/`0x925f` was
wrong: the negative displacement had been mis-converted.

### The plaque's time lines (`0x32b2`, `0x3346`)

Only the HUMAN plaque (`0x31d9`) has them. Both rewrite their template
from the first `x`: the sub-page + 6 for `"Avg : "` and + 7 for `"Best : "`.
They end the string themselves, so the template's `"(Last)"` is **never**
shown.

- **Avg:** the average word `[+0x24 + track·3]` × 5, as BCD (`9999` from
  `0x270f` up), printed as `dd.dd` with `0x3930`, which does no blanking.
- **Best:** `[+0x13 + track·2]`. If it is `9999` (no win yet), it prints `" ---"`
  and nothing else.
- **The place:** `0x38d4` finds the first **equal** entry in the track's list
  (`0x9c3f` for Avg, `0x9b7f` for Best). Only when the index is below 10
  does it add `" ("`, the name from `DS:0x96b1` (`"1st"` .. `"10th"`, 4
  bytes each) and `")"`. Otherwise the line ends at the time.

So these are the player's club figures for the track, an **average race
time** and a **best winning race time**, not this race's laps.

### Credits

```
0x131a   [di+1] = 2        a truck starts with two credits
0x2f63   bx = [si+6];  al = [bx+1]      a script action: read them
0x2f6f   bx = [si+6];  dec [bx+1]       a script action: spend one
```

Both are object-script actions, which is why the loss penalty is not
inline near the race-end code. Only a player beaten by a COMPUTER truck
pays (op 82, `0x2f3a`, walks the standings above them for one); a win,
or a loss to humans only, costs nothing. The credit is spent first (op
84), then "Press Button / to continue" (`DS:0x88c6`, beside `"Game
Over"` at `DS:0x88b4`) counts down; letting it run out ends their game
even with credits in hand, and with none left it shows "Game Over". See
the game shop panel's script under section 4.

### What a joining player gets

`0x1383`, applied only when `[di+2]` bit 0 is set (a human has joined):

```
[di+0x5e] [di+0x5f] [di+0x60] [di+0x61] = 0     all four upgrades
[di+0x0f] [di+0x11]                     = 0     cash and the total
[di+0x5d]                               = 25    nitro
[di+0x5b] [di+0x5c]                     = 6     the driver level
```

A computer truck skips all of it and keeps what it had.

## 15. Open questions

- **`LOADA.XX`.** Confirmed never requested: nothing passes index 10. The
  artwork and the filename table entry both ship unused, alongside
  ETITLE 577–581, which is probably the same Mastertronic branding.
- **The money page's per-row rebuild.** *Solved:* SORP lays the club's
  digits in as `0x3532`–`0x35b4` does (see The Ironman Club). The Speed
  Shop's credits and cash are op 78's page, now rebuilt the same way
  (see The Speed Shop's numbers).
- **The podium's text colours.** *Solved* (see Styles): E1–E4 by the
  truck's colour number, shown as 15 − value in SORP's complemented
  index space. E5 — a HUMAN's plaque text and the time board — comes
  out white (index 2), as the original shows (confirmed by eye: white
  with a black edge). The capture's figures were:

  | truck | fill | plaque |
  |---|---|---|
  | red MADMAN | 11 | 10 |
  | yellow HURRICANE | 13 | 15 |
  | blue JAMMIN' | 9 | 8 |
  | white IRONMAN | 5 | 4 |

  No style in the seventeen-entry table carries fill 9 or 11, so these
  cannot come from patching a style name. The glyph map is built at
  `0x11e5` -- the style's 16-byte row copied 32 times into a 512-byte
  table at `DS:0x77c`, indexed `map[row*16 + pixel]`, or 32 DIFFERENT
  rows when `[si+6]` is set, which is how gradient text works. The
  results screen's fills 9 and 11 are SORP's complement of E3 and E1
  (see Styles).
- **Four-player input.** Solved: players 1 and 2 are keyboard maps
  (0x5378, 0x538c, read by `0x42f:0x7bc`), players 3 and 4 the two
  game-port sticks (0x676, 0x688, read by `0x42f:0x83c`). Four input
  devices, but only THREE trucks a person can drive -- red, yellow and
  blue; white is always IRONMAN, as in the arcade game, which also had
  three player stations. SORP has the same three, with a different
  keyboard layout and gamepads.
- **The second Y/N prompt at `0x1270`.** *Solved:* page `DS:0x4c32`,
  `"ARE YOU USING A JOYSTICK (Y\N)"` in ROAD. N stores 0 in `[0x6a6]`;
  Y calibrates both game-port sticks (`[0x676]`, `[0x688]`, through
  `0x42f:0x7e8`) and stores `0xff`.
- **Truck flag bit value 4** (`[+2]`). *Solved:* never set. The only
  writes to a truck's `[+2]` are `0x1328` (= 2), `0x25ef` (op 62: set 1,
  clear 2), `0x2f86` (op 85: set 2), `0x308f` (op 63: set 8) and
  `0x370d` (clear 1 and 8); the records start at 0, and the bit-op
  script actions touch it only to test (op 51, `0x45a4` …) and clear
  (op 49, `0x465a` …) in the three game shop panels. Their Game Over
  route through it is dead.
- **The sound tick.** *Solved:* the screen's refresh (section 10).
- **Airborne trucks and the bunting.** *Solved:* section 7.

(F2 is solved: it toggles `[0x9d37]`, which gates four sound vectors --
the sound on/off switch. `[0x9d38]` is only its debounce latch.)

## 16. Lessons that cost time

- Read a **stride** from the data, not from a guess: the style nodes are
  24 bytes apart, and reading 48 gave half the list with wrong names.
- **Complement** is per bank and must be tested, not assumed: EGAME,
  ETITLE and EEND are complemented; ESHOP, ETRUCK and ESHADOW are not.
  Test by checking whether a known object lands on a plausible colour.
- **Every piece carries its own top offset.** Multi-piece artwork shears
  without it — the Off Road logo, the shop figure and the trophy each
  caught this separately.
- **Correlating sprites against screenshots is unreliable.** Three
  attempts produced confident wrong answers. A memory dump gives object
  positions directly and settles it in one step.
- **A fixed address in the code is not a fixed meaning.** `0x30d6`
  walks three hard-coded truck addresses, which looked like "the podium
  shows trucks 0-2" -- but a capture shows the white truck on it. Read
  what the data holds at that moment, not what the pointer is called.
- **Check the whole table before trusting the first row.** The band's
  gap table looked like sixteen entries that only ever slow a truck
  down; the arithmetic reaches 256 of them, and the rest are what give
  the game its bite.
- **A mechanism existing does not mean the data uses it.** The nitro
  gate opcode is fully implemented and no shipped layout contains one.
  Count the opcodes before building behaviour on them.
- Templates in the file are **not** what a live dump shows: `xxx Place`,
  `$xxx,xxx`, `Avg : xx.xx` and even the style name `Ex` are patched at
  runtime. Trust the file for structure and the dump for values.
