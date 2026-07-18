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
invoked with `build.sh -e` (or `oscar64 -i=… -e source.c`) and
runs the same `.prg` file the C64 will run, with no setup, no
display, no ROMs, and at high speed. It is the default for the
development loop and is what every phase's *Verify* step uses.

**VICE is not used.** VICE 3.9 is installed at `/usr/bin/` but is
a GUI emulator that requires a real X11/Wayland display to render.
In this headless environment, the ROMs had to be fetched manually,
the KERNAL/BASIC/CHAR/1541 ROMs were not bundled with the package,
and the autostart mechanism produced blank screenshots (no display
to render to). We attempted to get VICE working and abandoned
the effort after deciding it wasn't worth the time. The `build.sh
-v` / `-V` flags remain in place for the rare case where someone
has a real terminal session and wants to use VICE interactively,
but **no phase of this project depends on VICE for verification**,
and the `-v` / `-V` flags are *optional* additions, not required.

| Tool | Headless? | Use it for |
|------|-----------|------------|
| `build.sh -e` (oscar64 built-in) | Yes | Default for every Verify step. Fast, deterministic, runs in CI. |
| `build.sh -v` / `-V` (VICE) | No | Optional, interactive only. VICE is unreliable in this headless env and not used for verification. |
| `x128` / `xvic` / `xpet` | No | Out of scope (we target C64 PAL). |

**Bottom line for the plan:** every `Verify` step uses `build.sh -e`
(unless explicitly noted). VICE is referenced only in Phase 8
(end-to-end testing) and in the optional Phase 9 (NTSC) — both
of which assume a developer with a real terminal session will run
the tests.

> **Prerequisite correction to GAME.md**: I said the screens would be
> 320×200 standard hires. That's wrong for these images — they have
> well more than 2 colors per 8×8 cell. The correct target is
> **multicolor bitmap mode (BMM=1, MCM=1)**: 160×200 with 2 bits per
> pixel, 4 colors per 4×8 cell, same 8000-byte bitmap size. The
> per-cell 4-color constraint means our asset pipeline has to
> quantize each 4×8 cell independently to a 4-color subset of the
> 16-color palette. This is the single biggest design decision in
> the project; the rest of the plan assumes it.

---

## Phase 0 — Baseline

**Goal:** clean starting point, the existing `helloworld` builds and
runs.

- [x] ✅ Repo initialized, oscar64 is a submodule at `./oscar64/`.
- [x] ✅ `src/helloworld.c` + `src/build.sh` produce
  `build/helloworld.prg`.
- [x] ✅ `GAME.md` written.
- [x] ✅ Source artwork in `./source_images/`.
- [ ] ⏳ **Verify:** `cd src && ./build.sh -e` runs the hello-world
  program in the oscar64 built-in emulator. (Optional: launch
  `x64 build/helloworld.prg` in a real terminal session
  to see the screen.)

**Done means:** `build/helloworld.prg` exists and the
emulator prints "Hello World" then exits cleanly.

---

## Phase 1 — Asset pipeline

**Goal:** convert the 5 source PNGs into 160×200 multicolor-bitmap
`.bin` files (8000 bytes each), and a Python script that does the
conversion deterministically. This is the foundation everything else
sits on.

### Tasks

- [ ] Create `src/data/raw/` and `src/data/processed/`.
- [ ] Create `tools/convert_screens.py`. Inputs: a source PNG.
  Outputs: two files — `<name>.bin` (8000-byte multicolor bitmap)
  and `<name>.attr` (1000-byte screen memory with the foreground
  color per cell). The script:
  1. Crops the source to 160×200 (or 320×200 with a 2× horizontal
     scale, depending on what gives the best result — see
     decision point below).
  2. For each 4×8 cell, quantizes to 4 colors from the fixed
     16-color C64 palette (black, white, red, cyan, purple, green,
     blue, yellow, orange, brown, light red, dark grey, medium
     grey, light green, light blue, light grey).
  3. Picks the most common color in the cell as the
     "background" (one of $D021-$D024, encoded as 2 bits in the
     screen memory high nibble).
  4. Picks the 2nd, 3rd, 4th most common as the foreground
     (the cell's screen memory low nibble), and the two multicolor
     registers (D022, D023) — encoded in screen memory high bits 4-5
     and 6-7.
  5. Writes the 2-bit-per-pixel bitmap (MSB-first within each 4-pixel
     pair) to `<name>.bin` and the 1000-cell attribute table to
     `<name>.attr`.
- [ ] **Decision point:** which of two crop strategies gives better
  results?
  - Strategy A: crop to 160×200 directly (loses horizontal detail).
  - Strategy B: crop to 320×200 and 2× downscale to 160×200 with
    area averaging.
  Try both on `screen_waiting1.png` and pick whichever looks better
  on the actual screen.
- [ ] Run the script on all 5 source images. Verify each output
  is exactly 8000 + 1000 = 9000 bytes.
- [ ] Commit the script and the generated `.bin`/`.attr` files.
  Generated files are checked in (not gitignored) so the build
  doesn't depend on Python being installed.

**Verify:**
- `python3 tools/convert_screens.py source_images/screen_title.png src/data/processed/title`
  produces `title.bin` (8000 B) and `title.attr` (1000 B).
- `file src/data/processed/title.bin` reports "data".
- Opening one of the .bin files in a hex editor shows it's not all
  zeros (sanity check that the script actually ran).

**Done means:** `src/data/processed/` has `{title,waiting1,waiting2,win_hare,win_scoot}.{bin,attr}` and
the script is checked in. We can read the .bin back into a C array
and it'll be the right format.

---

## Phase 2 — Display a screen

**Goal:** write a C program that displays the title screen at 320×200
multicolor bitmap mode, with the score bar overlaid on top, and exits
cleanly when fire is pressed. This is the first time we touch VIC
state.

### Tasks

- [ ] Create `src/screens.h` and `src/screens.c` with the .bin
  data as `const char ScreenTitleBin[8000]` etc., using
  `#embed "../data/processed/title.bin"`. The .attr data similarly:
  `const char ScreenTitleAttr[1000]`.
- [ ] Create `src/memmap_setup()` helper that calls
  `mmap_trampoline()` then `mmap_set(MMAP_RAM)` (so the
  $E000-$FFFF region is available for the bitmap), then `mmap_set(MMAP_NO_ROM)`
  to also bank out CHAR ROM (so $D000 is I/O, not character data).
  This gives us 8 KB free at $E000-$FFFF for the active bitmap.
- [ ] Create `src/show_screen(int n)` helper that takes a screen
  ID, copies the right `.bin` to $E000-$FFFF, copies the `.attr`
  to $D800-$DBE7, sets the VIC bank bits to point to the right
  RAM, sets the bitmap base to $E000 via $D018 VM bits, sets
  `vic_ctrl1` for BMM=1 (bit 5), `vic_ctrl2` for MCM=1 (bit 4),
  CSEL=1 (40 columns), and `vic.color_back` ($D021) to black.
- [ ] Modify `src/helloworld.c` (or replace with a new `src/main.c`)
  to call `memmap_setup()`, then `show_screen(SCREEN_TITLE)`,
  then poll joystick port 1 — when fire is pressed, restore
  `mmap_set(MMAP_ROM)` and exit.
- [ ] Add a joystick-read helper `input_fire(int port)` in
  `src/input.c` / `src/input.h`. Returns 1 if fire is currently
  pressed (active low: `(PEEK(0xDC00+port) & 0x10) == 0`).

**Verify:**
- `cd src && ./build.sh -e` displays the title screen in the
  oscar64 built-in emulator for 5 seconds (or until fire is pressed)
  then exits.
- (Optional, interactive) `x64 build/nyuller.prg` in a real
  terminal session displays the title screen on real timings.
  Compare visually to `source_images/screen_title.png`.
- The .map file shows our code is in $0900-$1100-ish, well within
  the 38 KB main region.

**Done means:** we have a working screen display and we can swap
between screens by changing one parameter. The bulk of the asset
plumbing is done.

---

## Phase 3 — Score bar

**Goal:** render a score bar on the top 8 rows of the screen (the
top 8 pixels of the hires bitmap, which is one character row in
character mode). Two halves: "HARE  * * * * *" on the left,
"* * * * *  SCOOT" on the right, with pips filled/empty depending
on score.

### Tasks

- [ ] Create `src/score.h` and `src/score.c` with a 5-element
  `byte score_p1`, `byte score_p2`, and a `score_render()` function
  that draws the pips directly into the top of the bitmap
  (the first 8×320 = 320 bytes of the active screen at $E000).
- [ ] Pip rendering: a filled pip is a 6×6 black block with a
  1-pixel white border. An empty pip is just the border.
  Center the 5 pips in the left half (col 0-159) and right half
  (col 160-319).
- [ ] Add a small text "HARE" / "SCOOT" label above each pip group
  (centered, in the same 8-row band). Implement with a 4-character
  custom font (just H, A, R, E, S, C, O, T — 8 chars total, 1 KB
  char ROM, but only 8 bytes per character needed so 64 bytes
  total; we can keep it in a `char` array).
- [ ] Initialize scores to 0 in `main()`. Call `score_render()`
  after each `show_screen()`.

**Verify:**
- Title screen now shows "HARE 0    0 SCOOT" (or pip equivalent)
  at the top.
- Changing `score_p1 = 3` in main() shows 3 filled pips on the
  left (manually verify by hardcoding the score and rebuilding).

**Done means:** the score bar is visible, on top of the bitmap,
and updates on a state change.

---

## Phase 4 — State machine skeleton

**Goal:** implement the TITLE → READY → WAIT → DRAW → WIN flow
described in GAME.md §9, with no audio yet, fixed (not random)
durations, no score updates. Just the screens in the right order
at the right times.

### Tasks

- [ ] Create `src/game.h` and `src/game.c` with an enum:
  `STATE_TITLE, STATE_READY, STATE_WAIT, STATE_DRAW,
  STATE_WIN_P1, STATE_WIN_P2, STATE_GAMEOVER`. A `byte state`
  global, and a `game_init()`, `game_step()` pair.
- [ ] Each state has an `enter` action (set the screen, start any
  audio, reset a frame counter) and a `step` action (check inputs,
  advance the frame counter, transition to the next state).
- [ ] Without an IRQ yet, the main loop polls joysticks and
  advances state. This is fine for testing the flow.
- [ ] TITLE: loop, flash "PRESS FIRE" text at 25 Hz, exit when
  both ports have fire pressed simultaneously (de-bounced: both
  must be pressed within ~8 frames of each other; otherwise wait
  for both to release and re-press).
- [ ] READY: show `screen_waiting1`, count 60 frames, → WAIT.
- [ ] WAIT: show `screen_waiting2`, count 100 frames (later:
  random 100-250), → DRAW.
- [ ] DRAW: show a white screen (just fill the bitmap with 0s
  and set $D021 to white). No counter yet, no audio. Just
  watch for fire on either port; first to fire → WIN_P1 or
  WIN_P2 respectively. If no fire for 500 frames → back to
  TITLE (the "fault" case).
- [ ] WIN_P1 / WIN_P2: show the appropriate win screen, count
  100 frames, → READY (or → GAMEOVER if score == 5).
- [ ] GAMEOVER: show title screen, count 300 frames, → TITLE
  (with scores reset to 0/0).
- [ ] Update `score_render()` to be called on entry to each state.

**Verify:**
- Run the program: it shows the title with flashing text. Press
  both fire buttons → READY screen for 1 sec → WAIT screen for
  ~1.6 sec → white DRAW screen. Press fire on port 1 → win_hare
  screen for 2 sec → READY again (loop). 5 wins → title reset.
- The state transitions are visible and predictable.

**Done means:** the game loops. No audio, no fancy DRAW screen, no
random WAIT duration, no score update on win — but the full
state machine works.

---

## Phase 5 — Raster IRQ and timing

**Goal:** replace the busy-wait frame counter with a 50 Hz raster
IRQ. Every screen, every animation, every input poll is now
synchronized to a stable frame tick. Also implement the random
WAIT duration using the SID's $D41B oscillator register as a
random source.

### Tasks

- [ ] Create `src/rasterirq.c` / `src/rasterirq.h` with a single
  RIRQ at line 311 (PAL stable line). The handler:
  - Increments a global `frame_count` (16-bit, 50 Hz).
  - Calls `game_step()`.
  - Reads both joysticks, updates the input state.
  - Calls any per-frame "redraw" functions needed (counter,
    flashing text).
  - Re-arms the IRQ for the next frame.
- [ ] Replace the busy-wait frame counter in `game_step()` with
  comparisons against `frame_count` and a per-state
  `enter_frame` timestamp.
- [ ] Use `PEEK(0xD41B)` (SID oscillator 3) for the WAIT random
  duration: 100 + (PEEK(0xD41B) % 150) frames, sampled at
  READY enter.
- [ ] Mask CIA 1 and CIA 2 IRQs in the IRQ setup so the
  jiffy-clock handler doesn't fire nested. (The oscar64
  `rasterirq` library does this for us, but doing it by hand
  teaches us what's happening.)
- [ ] Add a "fault counter" — if neither player fires in 500
  frames in DRAW state, go back to TITLE with a short stinger
  sound (a low square wave burst, ~0.2 sec) and no point awarded.

**Verify:**
- The game still works in the oscar64 built-in emulator
  (`build.sh -e`).
- (Optional, interactive) Launch `x64sc build/nyuller.prg`
  in a real terminal to confirm the game runs at cycle-exact PAL
  timing (50.125 Hz). The state transitions happen on the right
  raster lines. The 50 Hz tick is rock-solid.
- WAIT duration varies visibly across multiple rounds.
- Press fire during WAIT (cheating) — nothing happens, we
  ignore inputs in WAIT state. (Add this as an explicit
  assertion in the test plan.)

**Done means:** the game has a proper 50 Hz frame tick. The
"feel" of the game (the timing of the READY → WAIT → DRAW
transitions) is now under our control via frame counts rather
than wall-clock waits.

---

## Phase 6 — SID audio

**Goal:** implement the 5 audio cues from GAME.md §6. The game
becomes audible.

### Tasks

- [ ] Create `src/notes.h` with a frequency lookup table for one
  octave of equal-tempered notes (C2..C7) as 16-bit SID
  frequency values. Or just define the few specific notes we
  need by hand: A4, A5, C2, G1, C5, E5, G5, C6, G4.
- [ ] Create `src/audio.h` and `src/audio.c` with:
  - `audio_init()` — set master volume $0F, no filter.
  - `audio_state_enter(int state)` — called from each state's
    `enter` action. Sets up the SID voices for the cue
    appropriate to that state.
  - `audio_state_step(int state)` — called from the raster
    IRQ. Advances notes per the per-state schedule.
  - `audio_stop()` — silence all 3 voices.
- [ ] Implement the 5 cues per the table in GAME.md §6.
  Start with the simplest: the DRAW stab (voice 3 noise,
  attack=0, decay=1, gated for 4 frames). Add the others
  one at a time.
- [ ] The WAIT suspense loop is the trickiest — implement it
  as a fixed schedule (every 16 jiffies, retrigger voice 1
  with the next note). Keep voice 2 doing a quieter
  arpeggio offset by 8 jiffies.
- [ ] Make sure the audio doesn't "leak" between states:
  when transitioning out of WAIT, gate off both voices
  before starting the DRAW stab.

**Verify:**
- Each state has its cue, in isolation (by hardcoding the
  state in main and rebuilding):
  - TITLE: silent (or very subtle).
  - READY: ping-ping.
  - WAIT: pulse-pulse-pulse-pulse (about 3-4 pulses per
    second).
  - DRAW: staaaab.
  - WIN: arpeggio up.
  - GAMEOVER: longer fanfare.
- The cues are recognizable and feel right (timing, volume).
- (Optional, interactive) Run the audio cues under `x64sc
  build/nyuller.prg` in a real terminal to confirm the
  SID envelope generator timings are right; the oscar64 emulator
  is fast but not always bit-accurate on SID timing, and the
  `x64sc` cycle-exact path is the one that matches real hardware.

**Done means:** the game has sound. This is the phase where
the game becomes "the game" rather than "a tech demo".

---

## Phase 7 — DRAW screen and the counter

**Goal:** the white DRAW screen shows a big counter incrementing
each frame, with a flash effect, and the per-frame timing is
tied to the raster IRQ so the digits never flicker.

### Tasks

- [ ] Add a `draw_render_counter(int value)` function that writes
  3 digit characters to the screen memory (which is in character
  mode, not multicolor, during the DRAW state — so we'll need
  a "switch VIC to hires char mode briefly" helper or just write
  the digits directly into the bitmap as 32×32 pixel blocks).
- [ ] Decide on the implementation:
  - Option A: switch to hires char mode for DRAW, use the
    standard 8×8 ROM font, scale 4×. Simpler code.
  - Option B: keep multicolor bitmap mode, render the digits
    as a 4-color 4×8 cell layout. More code, looks more
    consistent with the rest of the game.
  - Start with Option A (simpler, ship faster).
- [ ] On entering DRAW, fill the bitmap with 0s and set
  $D021 (background) to white. Then call
  `draw_render_counter(0)` once.
- [ ] Each frame, increment the counter (cap at 999) and call
  `draw_render_counter(counter)`.
- [ ] Add a "flash" effect: for the first 2 frames of DRAW,
  invert the colors (white background, black border, screen
  briefly all black). Or simpler: the border $D020 is set
  to white for the first 4 frames, then back to black.

**Verify:**
- When DRAW starts, the screen goes white with a brief flash.
- A 3-digit counter starts at 001 and ticks up by 1 each frame.
- The counter caps at 999 and stays there.
- The screen doesn't flicker (counter updates happen in the
  raster IRQ, on the same line each frame).

**Done means:** the DRAW screen looks right and feels right.
This is the most visually exciting moment of the game, so it
matters.

---

## Phase 8 — Polish and end-to-end test

**Goal:** the game is fully playable from power-on to match-end,
with no rough edges. We also do the real-hardware test.

### Tasks

- [ ] On TITLE, flash "PRESS FIRE" at 1 Hz (50 on, 50 off).
- [ ] On GAMEOVER, show a "HARE WINS!" or "SCOOT WINS!" banner
  in place of the flashing prompt. (Same custom font as the
  score bar; needs 6 more chars: W, I, N, !, S, space — some
  are already in there.)
- [ ] Add a brief stinger sound (0.1 sec) on each state
  transition. Just a quick low square wave.
- [ ] Make sure scores are reset on entering GAMEOVER → TITLE.
- [ ] Run the full game in the oscar64 built-in emulator
  (`build.sh -e`) for the development loop, and (optionally,
  when you have a real terminal session) in `x64` / `x64sc` for
  interactive play-testing and cycle-exact validation. Coverage:
  - P1 wins 5 in a row (cheat test: hold fire on port 1 the
    whole DRAW).
  - P2 wins 5 in a row.
  - Tie game: both fire on the same frame (impossible to test
    deliberately, but log it if it happens).
  - "Fault" case: nobody fires for 10 sec.
  - 10 random full matches to sanity-check the WAIT duration
    distribution.
- [ ] Build with `-O3` and verify the .prg still works under
  `x64sc` (in a real terminal session) — optimization can
  change timing subtly.
- [ ] If we have a real C64 or a Turbo Everdrive, test on
  real hardware. Otherwise document that we tested in the
  oscar64 emulator + `x64sc` cycle-exact mode.
- [ ] Strip `-g` from the release build. Add a `-O3` build
  target to build.sh.
- [ ] Final pass: review the .map file, check no section is
  larger than expected, check no RAM region is over-allocated.
- [ ] Final sanity check: load the release build into `x64sc`
  in a real terminal session, play a full match, and confirm
  the .prg is small enough to
  load via `LOAD"*",8,1` (i.e. ≤ 202 blocks = 51,308 bytes).

**Verify:**
- The game plays end-to-end on real timings.
- A typical match takes 30-60 seconds.
- No crashes, no hangs, no leftover sound.
- The .prg is < 32 KB and fits in the C64's BASIC area
  (i.e. the standard 202-block load via `LOAD"*",8,1`).

**Done means:** the game ships.

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
