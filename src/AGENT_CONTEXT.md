# Project context for Nyuller subagent

You are implementing one phase of a C64 game called "Nyuller."
(quick-draw duel between a rabbit and a kid on a scooter, first to 5
wins). You will be given a specific phase task. This document is the
context you need to do the work.

## Project layout (read-only)

```
/home/ballz/work/teletype/nyuller/
├── oscar64/                # Oscar64 C cross-compiler (git submodule)
│   ├── bin/oscar64         # The compiler binary (built by make)
│   └── include/            # Runtime headers (c64/, c128/, gfx/, stdio.h, etc.)
├── docs/c64/               # Downloaded C64 reference material
│   ├── vic/                # VIC-II register map, graphics modes, cebix article
│   ├── cia/                # CIA 1 + CIA 2 register maps
│   ├── sid/                # SID register map, ADSR, filter
│   ├── kernal/             # KERNAL jump table, full PRG text
│   ├── memory/             # Memory map, zeropage, hardware internals
│   ├── interrupts/         # Raster IRQ, IRQ/NMI flow, joystick
│   ├── sprites/            # Sprite (MOB) programming
│   └── cpu/                # 6510 overview
├── OSCAR64.md              # How the Oscar64 compiler works + C64 best practices
├── PROG_C64.md             # C64 hardware reference (synthesized)
├── GAME.md                 # The game design spec
├── tasks.md                # The phased implementation plan (your roadmap)
├── source_images/          # The 5 source PNGs for the game screens
└── src/                    # YOUR CODE GOES HERE
    ├── helloworld.c        # Existing minimal working program
    ├── build.sh            # Legacy build script (replaced by Makefile)
    └── build/              # Output directory (gitignored)
```

## Build and test (this is the ONLY way to verify your work)

```sh
cd /home/ballz/work/teletype/nyuller
make                     # show help with all targets
make compile             # compile → build/nyuller.prg
make run                 # compile + run in oscar64 emulator (HEADLESS, fast)
make play                # compile + launch VICE in background
make kill                # kill any detached VICE
```

**The oscar64 built-in emulator (`make run`) is the only test tool.** VICE
does not work in this headless environment (no display, blank
screenshots, manual ROM fetching). Do not waste time on VICE.
The `run` target runs the same `.prg` file the C64 will run, with no
setup, no ROMs, and at high speed.

`make` will auto-build the oscar64 compiler if it's missing.
It runs `cd src && oscar64 -i=…/include -o=…/build/nyuller.prg main.c`
to compile, and `cd src && oscar64 -i=…/include -o=…/build/nyuller.prg -e main.c` to run.

The build artifacts in `build/` are: `helloworld.prg` (the C64 program),
`helloworld.asm` (6502 listing), `helloworld.map` (region/section/object
placement), `helloworld.lbl` (VICE monitor labels — still useful even
though we don't use VICE), `helloworld.int` (intermediate code).

## CRITICAL: the 4-color-per-4×8-cell multicolor constraint

The 5 source PNGs have well more than 2 colors per 8×8 cell. They
**must** be displayed using **multicolor bitmap mode** (VIC-II
`$D011` BMM=1, `$D016` MCM=1): 160×200 with 2 bits per pixel, 4 colors
per 4×8 cell, same 8000-byte bitmap size. Per-cell only 4 colors are
available; the asset pipeline must quantize each 4×8 cell independently
to 4 colors from the 16-color C64 palette (black, white, red, cyan,
purple, green, blue, yellow, orange, brown, light red, dark grey,
medium grey, light green, light blue, light grey).

The bitmap goes to 8 KB at $E000-$FFFF (or wherever the linker
places it after `mmap_set(MMAP_RAM)` + `mmap_set(MMAP_NO_ROM)`).
The 1000-byte attribute table (one entry per 4×8 cell, low nibble =
foreground color, high nibble = the 2-bit background register selector
$21-$24) goes to $D800-$DBE7.

## C64 hardware facts you'll need

- **CPU**: MOS 6510 (6502 + 6-bit I/O port at $00/$01). 1 MHz PAL.
  Banks BASIC/KERNAL/CHAREN via writes to $01.
- **VIC-II** at $D000-$D3FF. 47 registers, mirrored every 64 bytes.
  The 16-color palette is fixed (see docs/c64/vic/vic_registers.md).
  The raster counter is 9 bits: $D012 is the low 8, bit 7 of $D011
  is bit 8.
- **SID** (6581) at $D400-$D41C. 3 voices, ADSR envelopes, filter.
  Volatile freq registers, no NaN, master volume at $D418.
- **CIA 1** at $DC00: keyboard, joystick, paddles, datasette,
  IRQ timer. Joystick bytes are active-low: bit 4 = fire.
  - Port 1 (right joystick, Hare) = $DC01
  - Port 2 (left joystick, Scoot) = $DC00
- **CIA 2** at $DD00: serial bus, RS-232, NMI, **VIC bank bits**
  (low 2 bits of PRA select one of 4 VIC banks).
- **KERNAL** at $E000-$FFFF. Jump table at $FF81-$FFF3.
  IRQ vector at $0314 (default → $EA31, the standard KERNAL ISR).
  NMI vector at $0318 (default → $FE47, the soft-reset / RESTORE handler).
- **Color RAM** at $D800-$DBE7: 4-bit wide (low nibble only).
- **Character ROM** at $D000-$DFFF in banks 0 and 2; can be relocated
  to RAM by changing $D018.
- **PAL timing**: 312 lines, 50 Hz. The "stable raster" line for IRQs
  is line 311 (right after vertical blank, before any badlines).
- **Badlines** cost 40 cycles every 8 text lines. Cycle-accurate
  code has to know this.

## Oscar64 patterns to use

- `mmap_trampoline()` once at startup, then `mmap_set(MMAP_RAM)` to
  bank out BASIC + KERNAL (gives you ~38 KB contiguous RAM at
  $0900-$A000). `mmap_set(MMAP_NO_ROM)` also banks out CHAR ROM
  ($D000 becomes I/O, not char data — needed for the bitmap at $E000).
- `#pragma region(name, start, end, , , {sections})` to remap
  memory (you probably won't need this for the first pass — the
  default layout works).
- `#pragma stacksize(N)` and `#pragma heapsize(N)` to size those.
- `#pragma compile("foo.c")` is what `#include <stdio.h>` etc. use
  to drag in implementation files. You never list multiple `.c`
  files on the command line; the headers do it.
- `__assume(x < 8)` etc. to give the optimizer value-range hints.
  Helps the 6502 backend use 8-bit ops instead of 16-bit.
- `__striped` qualifier for arrays where the 6502's lack of
  indirect-with-offset hurts.
- `p""` prefix for PETSCII string literals (not ASCII). The C64's
  char ROM is not ASCII.
- `__hwinterrupt` for ISRs (saves A/X/Y, exits with RTI).
- `__native` for a function you want to force to native 6502 code.
- `__zeropage` for global variables you want placed in ZP
  (or use `-Oz` to auto-place).
- The runtime headers in `./oscar64/include/` are the right way to
  touch hardware: `#include <c64/vic.h>` (VIC-II struct at $D000),
  `#include <c64/cia.h>` (joystick reading helpers),
  `#include <c64/memmap.h>` (mmap_set, mmap_trampoline),
  `#include <c64/rasterirq.h>` (raster IRQ API — use this, don't
  write your own IRQ handler from scratch),
  `#include <c64/sprites.h>` (sprite setup, alignment),
  `#include <gfx/bitmap.h>` (hires drawing helpers),
  `#include <stdio.h>` / `<conio.h>` for text I/O.

## Game-specific design (full version in GAME.md)

| Element | Value |
|---------|-------|
| Title screen | `source_images/screen_title.png` — full-screen image with "WHACKED" logo |
| Waiting 1 | `source_images/screen_waiting1.png` — 1.2 sec, "ping" jingle |
| Waiting 2 | `source_images/screen_waiting_2.png` — random 2-5 sec, suspense music |
| DRAW | white screen, big counter incrementing each frame, sharp stab |
| Win Hare | `source_images/screen_win_hare.png` — Hare won (player 1) |
| Win Scoot | `source_images/screen_win_scoot.png` — Scoot won (player 2) |
| Score bar | top 8 pixels of the screen, "HARE 0..0 SCOOT" (5 pips per side) |
| Match | first to 5 wins, then reset to title |
| Player 1 (Hare) | left side, joystick port 1, fire = `(PEEK(0xDC01) & 0x10) == 0` |
| Player 2 (Scoot) | right side, joystick port 2, fire = `(PEEK(0xDC00) & 0x10) == 0` |
| State machine | TITLE → READY → WAIT → DRAW → WIN_P1/P2 → (back to READY or GAMEOVER) |
| Game timer | 50 Hz PAL, line 311 for the raster IRQ |
| Random | use `PEEK(0xD41B)` (SID oscillator 3, effectively random) |

## Commit conventions

- One commit per phase, with a clear `Phase N: <summary>` prefix.
- Don't commit generated build artifacts in `build/` (gitignored).
- Generated data files (e.g. `src/data/processed/*.bin`) are checked
  in so the build doesn't depend on Python being installed.

## What to do if you get stuck

1. Re-read the relevant section of `OSCAR64.md`, `PROG_C64.md`,
   `GAME.md`, or `tasks.md` (your phase section).
2. Look at the Oscar64 samples: `../oscar64/samples/`. They're
   small, focused, and have working patterns. Good ones to look at:
   - `../oscar64/samples/memmap/allmem.c` — mmap_set usage
   - `../oscar64/samples/rasterirq/colorbars.c` — raster IRQ
   - `../oscar64/samples/memmap/easyflash.c` — memory layout
   - `../oscar64/samples/hires/lines.c` — hires drawing
   - `../oscar64/samples/sprites/` — sprite setup
3. Look at the runtime headers you need to use:
   `ls /home/ballz/work/teletype/nyuller/oscar64/include/c64/` etc.
4. Read the relevant docs/c64/ file for the chip you're working on.
5. Do NOT spend time on VICE, xvfb, environment setup, or anything
   not in this project. The build environment is set up.

## Output for this phase

When you're done, report back:
1. What you built (1-2 sentence summary)
2. The output of `make run` (proves it compiled and runs)
3. The git commit hash and one-line summary
4. Any concerns or follow-up work for the next phase
