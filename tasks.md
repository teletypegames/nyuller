# Tasks — Nyuller implementation plan

A phased, testable plan. Each phase ends with a *verify* step that
proves the phase works. Each phase is one or more commits. Stop at any
phase boundary and you have a working (if incomplete) program.

Conventions used below:
- `[ ]` todo
- `[ ]` + ⏳ in progress
- `[x]` + ✅ done
- **Verify** = the test that proves this phase works (a concrete
  command to run or a concrete thing to see in the emulator)

The 6502-side development happens in `./src/`. The asset pipeline
produces files in `./src/data/`.

## 0.1. Test environment

**The oscar64 built-in emulator is the primary test tool.** It's
invoked with `make run` (or `oscar64 -i=… -e source.c`) and
runs the same `.prg` file the C64 will run, with no setup, no
display, no ROMs, and at high speed. It is the default for the
development loop and is what every phase's *Verify* step uses.

**VICE works but is optional.** VICE 3.9 is installed at `/usr/bin/`
and needs a real X11/Wayland display to render. The `-drive8type 1541`
flag is required for autostart (we have the original 1541 ROM; VICE
defaults to 1541-II whose ROM we don't have). `make play` and
`make kill` manage the VICE lifecycle. No phase of this project
depends on VICE for verification.

| Tool | Headless? | Use it for |
|------|-----------|------------|
| `make run` (oscar64 built-in) | Yes | Default for every Verify step. Fast, deterministic, runs in CI. |
| `make run-vice` / `make run-vice-cycle` (VICE) | No | Optional, interactive only. Needs `-drive8type 1541`. |
| `make play` / `make kill` (VICE detached) | No | Optional, interactive play-testing. |
| `x128` / `xvic` / `xpet` | No | Out of scope (we target C64 PAL). |

**Bottom line for the plan:** every `Verify` step uses `make run`
(unless explicitly noted). VICE is referenced in Phase 8
(end-to-end testing) and in the optional Phase 9 (NTSC) — both
of which assume a developer with a real terminal session will run
the tests.

> **Prerequisite correction to GAME.md**: The screens are
> **multicolor bitmap mode (BMM=1, MCM=1)**: 160×200 with 2 bits per
> pixel, 4 colors per 4×8 cell, same 8000-byte bitmap size. The
> per-cell 4-color constraint means our asset pipeline has to
> quantize each 4×8 cell independently to a 4-color subset of the
> 16-color palette. This is the single biggest design decision in
> the project; the rest of the plan assumes it.

---

## Phase 0 — Baseline ✅

**Goal:** clean starting point, the existing `helloworld` builds and
runs.

- [x] ✅ Repo initialized, oscar64 is a submodule at `./oscar64/`.
- [x] ✅ `src/helloworld.c` + `make compile` produce
  `build/helloworld.prg`.
- [x] ✅ `GAME.md` written.
- [x] ✅ Source artwork in `./source_images/`.
- [x] ✅ **Verify:** `make run` runs the hello-world program in the
  oscar64 built-in emulator.

**Done means:** `build/helloworld.prg` exists and the emulator prints
"Hello World" then exits cleanly.

---

## Phase 1 — Asset pipeline ✅

**Goal:** convert the 5 source PNGs into 160×200 multicolor-bitmap
`.bin` files (8000 bytes each), and a Python script that does the
conversion deterministically. This is the foundation everything else
sits on.

### Tasks

- [x] ✅ Created `src/data/raw/` and `src/data/processed/`.
- [x] ✅ Created `tools/convert_screens.py`.
- [x] ✅ Ran the script on all 5 source images. Each output
  is exactly 8000 + 1000 + 2-3 = 9002-9003 bytes (.bin + .attr + .d021).
- [x] ✅ Committed the script and the generated files.

**Verify:**
- `python3 tools/convert_screens.py source_images/screen_title.png src/data/processed/title`
  produces `title.bin` (8000 B), `title.attr` (1000 B), `title.d021` (2-3 B).
- `file src/data/processed/title.bin` reports "data".

**Done means:** `src/data/processed/` has `{title,waiting1,waiting2,win_hare,win_scoot}.{bin,attr,d021}` and
the script is checked in. We can read the .bin back into a C array
and it'll be the right format.

---

## Phase 2 — Display a screen ✅

**Goal:** write a C program that displays the title screen at 160×200
multicolor bitmap mode, with the score bar overlaid on top, and exits
cleanly when fire is pressed. This is the first time we touch VIC
state.

### Tasks

- [x] ✅ Created `src/screens.h` and `src/screens.c` with the .bin
  data as `const char ScreenTitleBin[]` etc., using
  `#embed 8000 0 lzo "data/processed/title.bin"`. The .attr data
  similarly: `const char ScreenTitleAttr[]`.
- [x] ✅ Created `src/memmap.c` / `src/memmap.h` with `memmap_setup()`
  (calls `mmap_trampoline()`, `mmap_set(MMAP_RAM)`, `mmap_set(MMAP_NO_ROM)`)
  and `memmap_restore()` (restores `$01=$37`).
- [x] ✅ Created `src/show_screen(int n)` helper that takes a screen
  ID, LZO-decompresses the `.bin` to $E000-$FFFF, copies the `.attr`
  to $D000, sets the VIC registers (bank 3 via CIA2 PRA, D018=$48,
  ctrl1 BMM|DEN|RSEL, ctrl2 MCM|CSEL), and sets `$D021`.
- [x] ✅ Created `src/main.c` calling `memmap_setup()`, `audio_init()`,
  `score_init()`, `game_init()`, `rasterirq_setup()`, then spinning.
- [x] ✅ Created `src/input.c` / `src/input.h` with `input_fire(int port)`.
- [x] ✅ Screens region placed at `$BC00-$D000` (always-RAM, outside
  BASIC ROM — the original `$A000` placement was in ROM space on
  real hardware).

**Verify:**
- `make run` displays the title screen in the oscar64 built-in emulator.
- (Optional, interactive) `make play` displays the title screen
  on real timings via VICE.
- The .map file shows the code in the `$0801-$9C00` region, the
  `.attr` data in `$BC00-$CF88`, and the bitmap at `$E000-$FFFF`.

**Done means:** we have a working screen display and we can swap
between screens by changing one parameter. The bulk of the asset
plumbing is done.

---

## Phase 3 — Score bar ✅

**Goal:** render a score bar on the top 8 rows of the screen. Two
halves: "HARE  * * * * *" on the left, "* * * * *  SCOOT" on the
right, with pips filled/empty depending on score.

### Tasks

- [x] ✅ Created `src/score.h` and `src/score.c` with `score_p1`,
  `score_p2` globals, and `score_render()` that draws the pips
  and labels directly into the top 8 rows of the bitmap at $E000.
- [x] ✅ Pip rendering: 6×6 black fill with 1-pixel white border.
  5 pips per side, centered in each half of the 160px multicolor
  screen.
- [x] ✅ Custom 4×8 font: H, A, R, E, S, C, O, T, P, F, W, I, N, !, space.
- [x] ✅ `score_init()` zeros both scores. `score_render()` called
  after each `show_screen()`. Scores clamped to 0..5 at render time.

**Verify:**
- Title screen shows "HARE  [5 pips]    [5 pips]  SCOOT" at the top.
- Changing `score_p1 = 3` in main() shows 3 filled pips on the left.

**Done means:** the score bar is visible, on top of the bitmap,
and updates on a state change.

---

## Phase 4 — State machine skeleton ✅

**Goal:** implement the TITLE → READY → WAIT → DRAW → WIN flow
described in GAME.md §9, with no audio yet, fixed durations,
no score updates. Just the screens in the right order at the right
times.

### Tasks

- [x] ✅ Created `src/game.h` and `src/game.c` with the state enum,
  `game_init()`, `game_step()`, and per-state enter/step functions.
- [x] ✅ TITLE: flash border at 0.5 Hz, both-fire detection with
  8-frame de-bounce window.
- [x] ✅ READY: show `screen_waiting1`, count 60 frames → WAIT.
- [x] ✅ WAIT: show `screen_waiting2`, count 100 frames → DRAW.
- [x] ✅ DRAW: white screen, watch for fire → WIN_P1 or WIN_P2.
  500-frame fault timeout → TITLE (no point).
- [x] ✅ WIN_P1 / WIN_P2: show win screen, count 100 frames,
  → READY (or → GAMEOVER if score == 5).
- [x] ✅ GAMEOVER: show title screen, count 300 frames, → TITLE
  with scores reset to 0/0.
- [x] ✅ `score_render()` called on entry to each state.

**Verify:**
- `make run`: game shows title with flashing text. Press both fires
  → READY 1 sec → WAIT ~2 sec → white DRAW. Fire on port 1 →
  win_hare 2 sec → READY again (loop). 5 wins → title reset.

**Done means:** the game loops. No audio, no fancy DRAW screen, no
random WAIT duration — but the full state machine works.

---

## Phase 5 — Raster IRQ and timing ✅

**Goal:** replace the busy-wait frame counter with a 50 Hz raster
IRQ. Implement the random WAIT duration using $D41B.

### Tasks

- [x] ✅ Created `src/tick.c` / `src/tick.h` with a single RIRQ at
  line 311 (PAL stable line, `VIC_CTRL1_RST8` set). Handler:
  increments `frame_count`, calls `audio_state_step()` + `game_step()`.
- [x] ✅ `game_step()` uses `enter_frame = frame_count` timestamps
  and `elapsed = frame_count - enter_frame` comparisons.
- [x] ✅ Random WAIT: `100 + (PEEK(0xD41B) % 150)`, sampled at
  READY enter. Voice 3 freq set to $FFFF in `audio_init()` so
  the oscillator runs from the first sample.
- [x] ✅ CIA 1 + CIA 2 ICR masked (`$7F, $7F`).
- [x] ✅ Fault counter: 500 frames in DRAW → TITLE with stinger.
- [x] ✅ `mmap_trampoline()` installs trampoline at $FFFE/$FFFF;
  `rirq_init(false)` overwrites the IRQ half (trampoline is NMI-only).
  `rirq_start()` is NOT called — inline `asl $d019; cli` used instead.
- [x] ✅ `#pragma stacksize(0x400)` (1 KB), `#pragma heapsize(0)`.
  `#pragma nomain()` on tick.c to avoid stack collision.

**Verify:**
- `make run`: game still works, all transitions at correct timings.
- WAIT duration varies visibly across multiple rounds.
- Press fire during WAIT → nothing happens (input ignored).

**Done means:** the game has a proper 50 Hz frame tick. The timing
of READY → WAIT → DRAW is controlled by frame counts.

---

## Phase 6 — SID audio ✅

**Goal:** implement the 5 audio cues from GAME.md §6 + fault
stinger + transition stinger. The game becomes audible.

### Tasks

- [x] ✅ Created `src/notes.h` with pre-computed SID frequency values
  (A4, A5, C2, C3, C4, G4, C5, E5, G5, A5, C6 — all correct to
  ±1 unit for PAL 985248 Hz clock).
- [x] ✅ Created `src/audio.h` and `src/audio.c`:
  - `audio_init()`: master volume $0F, no filter, all voices silenced.
  - `audio_state_enter(state)`: calls `audio_stop()` first, sets up
    per-state SID registers (voice, waveform, ADSR, PWM).
  - `audio_state_step(state)`: advances the per-state schedule.
  - `audio_stop()`: gates off all 3 voices, clears registers.
- [x] ✅ 5 cues implemented:
  - TITLE: silent.
  - READY: voice 0 triangle A4 (8 frames) → A5 (8 frames).
  - WAIT: voice 0 square C2 + voice 1 triangle C3, 16-frame
    retrigger loop (voice 1 offset by 8).
  - DRAW: voice 2 noise, attack=0, decay=1, gated 4 frames.
  - WIN_P1/P2: voice 0 triangle C5-E5-G5-C6 arpeggio (10 frames/note),
    voice 1 triangle a major third below.
  - GAMEOVER: voice 0 triangle G4-C5-E5-G5-C6 fanfare (30 frames/note),
    voice 1 sustained C4.
- [x] ✅ Transition stinger: 5-frame low square wave on a per-state
  voice (avoids colliding with the new state's audio).
- [x] ✅ Fault stinger: 10-frame low square wave on voice 1 (triggered
  AFTER `game_enter_title()` so it isn't silenced).
- [x] ✅ No leakage between states: `audio_state_enter()` calls
  `audio_stop()` first; `audio_advance_stinger()` and
  `audio_advance_fault_stinger()` run independently and gate off
  their voices when done.

**Verify:**
- Each state has its recognizable cue (TITLE silent, READY ping,
  WAIT pulse, DRAW stab, WIN arpeggio, GAMEOVER fanfare).
- State transitions produce a brief stinger.
- DRAW fault produces a 10-frame stinger.

**Done means:** the game has sound. This is the phase where
the game becomes "the game" rather than "a tech demo".

---

## Phase 7 — DRAW screen and the counter ✅

**Goal:** the white DRAW screen shows a big counter incrementing
each frame, with a flash effect, tied to the raster IRQ.

### Tasks

- [x] ✅ Created `src/draw.c` / `src/draw.h` with `draw_render_counter(int value)`.
  7-segment-style digits rendered as 4×8 multicolor cells in
  the bitmap. 3 digits, starting at cell (15, 8).
- [x] ✅ Counter initialized to 1 (per spec "starts at 001"),
  incremented per frame in `game_step_draw()`, capped at 999.
- [x] ✅ Border flash: first 4 frames of DRAW show a white border
  strobe (W/B/W/B/W pattern), then white.
- [x] ✅ `show_white_screen()` fills $E000-$FFFF with 0s, clears
  color RAM, sets VIC to MCM bitmap mode with white background.

**Verify:**
- DRAW screen: white background, border strobes for 4 frames,
  then a 3-digit counter ticks up from 001 each frame.
- Counter caps at 999 and stays there.
- No flicker (counter updates happen in the raster IRQ).

**Done means:** the DRAW screen looks right and feels right.

---

## Phase 8 — Polish and end-to-end test ✅

**Goal:** the game is fully playable from power-on to match-end,
with no rough edges.

### Tasks

- [x] ✅ TITLE: "PRESS FIRE" text flashes (0.5 Hz: 50 on, 50 off).
  Uses the extended custom font (P, F, R, E, S, space).
- [x] ✅ WIN_P1/P2: "HARE WINS!" / "SCOOT WINS!" banner replaces
  the flashing prompt on row 1. Uses the same custom font
  (W, I, N, !).
- [x] ✅ GAMEOVER: shows winner banner, 300 frames → TITLE with
  scores reset to 0/0.
- [x] ✅ Brief transition stinger (0.1 sec) on every state change.
- [x] ✅ Full end-to-end test: 10+ random full matches, no crashes,
  no hangs, no stuck audio.
- [x] ✅ Build with `-O3`: .prg works identically (same timing).
- [x] ✅ .prg is 51,081 bytes — fits in 202 blocks (≤ 51,308 bytes).
- [x] ✅ DRAW cheat protection: `draw_was_pressed[]` initialized
  from `input_fire()` on entry, not zero. Holding fire from WAIT
  doesn't auto-win.
- [x] ✅ 7-agent code review: fixed screens region (BASIC ROM → RAM),
  DRAW cheat, first-round random, build.sh bugs (-v/-p, --kill
  regex, c1541 verification). See `src/KNOWN_ISSUES.md` for
  deferred items (LZO-in-IRQ tearing, trampoline footgun,
  audio schedule off-by-one).

**Verify:**
- `make run`: game plays end-to-end on real timings.
- A typical match takes 30-60 seconds.
- No crashes, no hangs, no leftover sound.
- .prg fits in 202 blocks.

**Done means:** the game ships.

---

## Post-Phase 8 work

These items were done after Phase 8 was marked complete:

- [x] ✅ `./src/build.sh` replaced by `./Makefile` (GNU make).
  Targets: help (default), compile, run, run-vice, run-vice-cycle,
  play, play-cycle, kill, clean. Optimization via `make OPT=O3`.
- [x] ✅ `-drive8type 1541` added to VICE launches (required for
  autostart; without it, `?DEVICE NOT PRESENT`).
- [x] ✅ Game renamed from "Whack Hare!" to "Nyuller" (all source,
  headers, docs, build scripts).
- [x] ✅ Build output moved from `src/build/` to `./build/` (repo root).
- [x] ✅ Code review fixes applied (see commit `31cbe00`):
  - Screens region moved from `$A000` (BASIC ROM) to `$BC00` (RAM).
  - `draw_was_pressed[]` initialized from `input_fire()`.
  - `audio_init()` sets voice 3 freq to `$FFFF` (random from frame 1).
  - Build script: mutual exclusion, c1541 verification, --kill regex.

---

## Phase 9 — (Optional) extra features

Pick from these based on time and interest. None of these are
required for the game to be done.

- [ ] **NTSC support.** 60 Hz, 263 lines, stable raster line
  ~261. Different frame counts, different note frequencies
  (the SID clock is the same; the game just runs 20% faster).
  Maybe a `-tm=ntsc` build target.
- [ ] **A "draw too early" penalty.** If a player presses fire
  during WAIT (not DRAW), they lose the round. The other
  player gets a free point. Adds strategic depth.
- [ ] **A practice mode.** Press fire on a keyboard key (e.g.
  SPACE) at the title to start a single-player mode where
  you have to react to the DRAW signal. High score = how
  fast you press.
- [ ] **A "best of N" mode.** Instead of first to 5, configurable
  best of 3, 5, 7, 9.
- [ ] **Sidetrack from the article:** digit scaling with
  dithering. A custom 2-color dithered font for the counter
  that looks more "C64" than the ROM font.
- [ ] **More art.** The five source screens are the minimum.
  If we want to add e.g. a separate "GAME OVER" screen with
  the winner standing on the loser's body, that's a new art
  asset and a new state.
- [ ] **LZO screen swap refactor.** Move the `show_screen()` LZO
  decompress out of the raster IRQ into the main loop (or a
  deferred flag pattern) to eliminate the ~170ms screen tearing
  on state transitions. See `src/KNOWN_ISSUES.md` §1.
- [ ] **Re-enable trampoline for CIA IRQs.** If CIA 1 Timer A is
  ever unmasked (e.g. for the jiffy clock), re-install the
  mmap_trampoline after `rirq_init_io()`. See
  `src/KNOWN_ISSUES.md` §2.

---

## Risks and unknowns

Things that might trip us up, in rough order of likelihood:

1. **The 4-color-per-4×8-cell multicolor constraint is going to
   hurt.** The source images have a lot of color variation.
   The conversion script has to be smart about which 4 colors
   it picks for each cell. A naive quantizer will produce
   muddy results. Budget time for iterating on the script.

2. **The SID 6581 is non-deterministic for timing-critical music.**
   The note schedule in WAIT is jiffy-accurate, but the SID's
   envelope generator adds a tiny bit of jitter. Test on the
   emulator first (which is bit-perfect), then on real hw.

3. **The raster IRQ handler is cycle-sensitive.** If we have a
   bug that lets the handler overrun its line budget, the
   next IRQ fires late and everything drifts. The fix is
   always: measure with `-O3 -g`, look at the .asm, and
   optimize the hot path. We have the Oscar64
   `rasterirq` library for the boilerplate; the hot path
   is just the few state-machine branches.

4. **`mmap_set` while the KERNAL trampoline is in use.** If we
   call `mmap_set` from inside a raster IRQ (we won't, but
   a bug might), the KERNAL ISR will read the wrong bank
   when it tries to update the jiffy clock. The fix: never
   call `mmap_set` from an ISR; do all banking at
   startup, before the IRQ is enabled.

5. **The `p""` PETSCII prefix matters in conio.h too.** If we
   forget to mark a string as PETSCII, it'll print garbage.
   Easy to fix but easy to forget.

6. **Color RAM updates are slow if we do them cell-by-cell.**
   The score bar rendering does a few hundred byte writes
   to $D800+. That's fine in the raster IRQ (it's not the
   badline line), but if the score bar grows we should
   consider pre-shifting the color RAM with a `memcpy` from
   a per-state color table.

7. **The DRAW counter might flicker if we don't time it right.**
   The 3 cells in screen memory are at $04xx; updating them
   in the IRQ handler at line 311 is fine, but if the user
   enables `-O3` and the optimizer moves the write to a
   different line, we get mid-screen garbage. Pin the
   counter update to a known-good line (line 311 in the
   IRQ handler, which fires *before* the visible area on
   the next frame).

---

## Commit cadence

Roughly one commit per phase, with intermediate commits as
phases get big. Suggested commit messages:

```
Phase 0: repo skeleton + helloworld baseline
Phase 1: asset pipeline (Python script + 5 processed .bin files)
Phase 2: display the title screen (mmap, vic, screen swap)
Phase 3: score bar (pips + labels)
Phase 4: state machine skeleton (no audio, no random)
Phase 5: raster IRQ + frame timing
Phase 6: SID audio (5 cues)
Phase 7: DRAW screen + counter
Phase 8: polish and end-to-end test
```

Within a phase, break up by file: "Phase 2: add memmap_setup and
show_screen helpers" before "Phase 2: wire into main.c".
