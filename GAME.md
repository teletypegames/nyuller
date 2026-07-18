# GAME.md — *Nyuller*

A two-player reaction-time duel for the Commodore 64. Two players stand
back-to-back, take ten paces, and the first one to hit fire when the
signal appears wins the round. First to **5 wins** wins the match.

The two players are **Hare** (player 1, on the left — a white rabbit
with claws and a bad attitude) and **Scoot** (player 2, on the right —
a kid on a kick scooter). The two facing-off screens, the win
screens, and the title screen all come from the artwork in
`./source_images/`.

## 1. The cast

| Player | Side | C64 control port | Artwork on the title |
|--------|------|------------------|----------------------|
| Hare (the rabbit) | Left  | Joystick port 1 (`$DC01`) | Lower-left of `screen_title.png` |
| Scoot (the kid on a scooter) | Right | Joystick port 2 (`$DC00`) | Lower-right of `screen_title.png` |

Fire is the only button used. Both ports are read every frame; the
joystick value is the standard `J = PEEK(0xDC00/0xDC01)` byte with bit
4 (mask $10) as the fire button (active low — pressed when bit 4 is
clear).

## 2. Artwork

All artwork lives in `./source_images/` as 1390×1130 PNGs (8-bit RGB,
dithered down to the C64's 16-color palette by whoever made them — the
colors in the files are the colors the C64 will display):

| File | Resolution | Used for |
|------|------------|----------|
| `source_images/screen_title.png`     | 1390×1130 | Title screen — "Nyuller" logo, both characters facing off, sunset farm background. |
| `source_images/screen_waiting1.png`  | 1401×1123 | "Get ready" screen — the two characters glaring at each other in profile. |
| `source_images/screen_waiting_2.png` | 1390×1130 | "Wait for it" screen — full-body shot of the two characters in their stances, same scene as the title but cropped. |
| `source_images/screen_win_hare.png`  | 1391×1131 | Hare won — Hare punching Scoot in the face mid-scooter. |
| `source_images/screen_win_scoot.png` | 1389×1132 | Scoot won — Scoot has ridden over Hare, who is sprawled on the ground. |

A practical note for converting to C64: the originals are 1390×1130, but
the C64 hires screen is 320×200. We'll have to either:

- Pre-crop / pre-scale the assets in a tool (recommended) to 320×200
  before embedding them, or
- Pick a 320×200 region of the original (the center 320×200 crop is
  usually the most interesting part) and embed that.

We'll do the cropping in an image tool (ImageMagick, GIMP, or a small
Python script) and let the C64 just blit the resulting 320×200 hires
bitmap. The Oscar64 `#embed lzo` directive is what pulls the cropped
`.bin` into the binary.

## 3. The screens (in play order)

The game is a simple state machine. The screens, in the order a player
sees them during one round:

1. **Title** (`TITLE` state) — `screen_title.png`. Static background.
   A short, flashing *"PRESS FIRE ON BOTH PORTS"* banner across the
   bottom half. The score is shown at the top: a row of "0"s on the
   left (Hare) and right (Scoot), reading `0    0`. No music, or a
   very low-key idle loop on voice 1.

2. **"Get ready"** (`READY` state) — `screen_waiting1.png`. Up for
   **exactly ~1.2 seconds**. A short two-note "ping" jingle (e.g. SID
   voice 1 plays A4 then A5 with a quick attack/decay envelope). The
   score at the top is visible. The text "READY…" or just the screen
   alone is fine.

3. **"Wait…"** (`WAIT` state) — `screen_waiting_2.png`. The suspense
   state. Stays up for a **random duration between 2.0 and 5.0
   seconds**, uniform. A looping suspenseful bassline plays on SID
   voice 1 (a slow two-note pulse, say C2–G1 alternating every ~16
   jiffies), with voice 2 doing a quiet pulse-width "heartbeat"
   arpeggio. No indication of how long is left — that's the point.

4. **"DRAW!"** (`DRAW` state) — white screen, large black digits
   (described below), a sharp percussive stab on the SID (voice 3,
   noise waveform, attack=0, decay=1, sustain=0, release=2, gated off
   after 4 frames). The counter starts at `001` and increments **once
   per frame** so a fast eye + finger can win in single-digit
   milliseconds.

5. **Win screen** (`WIN_P1` or `WIN_P2` state) — `screen_win_hare.png`
   or `screen_win_scoot.png`. Up for **exactly 2.0 seconds**. A short
   3-4 note "winner" jingle on the SID (e.g. C–E–G–C, major triad
   arpeggio, fast attack, long release, voices 1 and 2 stacked). The
   score at the top updates immediately (you see the new digit the
   moment the win screen appears).

6. **Loop or game-over.** If the winner's score is now < 5, jump back
   to step 2 (READY) for the next round. If the winner's score is 5,
   go to step 7.

7. **Game over** (`GAMEOVER` state) — same as the title screen, but
   with a "GAME OVER — HARE WINS!" / "GAME OVER — SCOOT WINS!"
   banner instead of "PRESS FIRE", and a 5–6 note "match over"
   fanfare. Stays up for 6 seconds (long enough to be appreciated),
   then resets to step 1 (TITLE) with scores at 0/0.

## 4. The "DRAW!" screen — what to draw

The C64 hires screen is 320×200 with a 16-color palette. The DRAW
screen is:

- Background: solid white (`$D021` = white, bitmap all 0s).
- A huge number in black, centered, drawn with the standard 8×8 ROM
  character set scaled up. The counter is 3 digits (001..999) so it
  stays in the visible screen at any size.
- Implementation: easiest is to use the 8×8 ROM font, scaled 4× (so
  each character is 32×32 pixels), centered. That gives 3 chars ×
  32px = 96px wide on a 320px screen, so each digit is huge but fits.
  Redrawing the screen is just "redraw the 3 screen memory cells with
  the new counter value". The VIC's redraw of a 96×32 region in
  character mode takes one frame's worth of work in the worst case;
  the counter is updated once per frame, in sync with the raster
  IRQ, so there's no flicker.

  A nicer version would use a custom multicolor font (the C64 has 4
  colors per 8×8 cell in multicolor character mode), but hires
  character mode is simpler and the white-with-black-text look is
  the classic quick-draw visual.

- Each frame, in the raster IRQ handler, the game logic does:
  ```c
  counter++;
  if (counter > 999) counter = 999;   // cap it
  screen[40 * 11 + 13] = '0' + (counter / 100);
  screen[40 * 11 + 14] = '0' + ((counter / 10) % 10);
  screen[40 * 11 + 15] = '0' + (counter % 10);
  ```
  (and similarly for the colors at $D800+, but they're all black, so
  set once at the start of the DRAW state).

- Optionally: each digit can have a small horizontal "shake" applied
  per frame to give a stressed-out look. Or a small black bar across
  the screen to make it look like a flash.

## 5. The score bar (top of every game screen)

- 1 character row, top of the screen. In hires + char mode, that means
  using the top 8 pixels of the bitmap as character cells, and the
  rest of the screen (200-8 = 192 pixels) as a hires image below.
  The easiest implementation is to set the VIC's video matrix
  pointer to start at a screen memory address that *visually* aligns
  with the top of the screen but in practice is wherever we want.
  Concretely: screen memory at $0400, but the screen "rows" we use for
  the score are just the very top 8 pixels of the bitmap (we draw the
  characters there by hand in the bitmap, OR we use a hybrid mode
  where the top 8 rows are character cells and the rest is hires — but
  C64 character rows are always 8 pixels high, so we'd have an
  8-pixel score bar and a 192-pixel art area. Good enough.)
- Layout, top row, left to right:
  - Cols 0-15: "HARE  " (label) + 5 small icons or just "HARE: " +
    the digit, centered on the left half.
  - Cols 24-39: "SCOOT: " + the digit, centered on the right half.
  - Total format: `   HARE  *    SCOOT  *  ` where the * is the
    number of filled-in score pip (0..5). Pips are simpler than
    digits: 5 small squares per side, filled or empty.
- Score pip rendering: a custom character for "filled pip" (a solid
  block in the foreground color) and "empty pip" (an outline or a
  blank). Each side takes 5 character cells of width.

The cleanest implementation: have two pre-loaded custom character sets
for the pip characters (filled and empty), and the rest of the score
bar is the standard ROM character set. Switch between them with
$D018. But that adds complexity — for the first pass, drawing the
pips as solid filled blocks in the bitmap is fine (the score bar is
only 8 pixels high and the pips are 6×6 blocks).

## 6. Audio (SID 6581) — the five cues

The SID has 3 voices. The game uses voice 1 for music, voice 2 for
harmony, voice 3 for sound effects. The filter is bypassed (volume
$0F, no filter routing) for the first pass — we can add filter
sweeps later for polish.

| State | Voice 1 | Voice 2 | Voice 3 | Notes |
|-------|---------|---------|---------|-------|
| TITLE | (silent, or a low idle pulse) | — | — |  |
| READY | triangle A4 then A5, attack 0, decay 9, release 0, no sustain, gated off after 8 frames each | — | — | Two notes, ~0.6s total |
| WAIT | square wave C2, pulse width 50%, slow envelope (attack 0, decay 15, sustain 8, release 15) | triangle 1 octave up, same envelope, slightly delayed | — | Looping, every ~16 jiffies re-trigger |
| DRAW  | — | — | noise, attack 0, decay 1, sustain 0, release 2, gated on for 4 frames | Sharp stab |
| WIN   | triangle C5–E5–G5–C6 arpeggio, fast attack, slow release, ~0.8s | triangle a third below, same notes, very quiet (volume 4) | — | Major chord jingle |
| GAMEOVER | triangle, fanfare notes (G4–C5–E5–G5–C6), long notes, ~3s | triangle, sustained fifth below | — | End-of-match feel |

(The actual note frequencies in Hz are listed in the SID 6581
datasheet — `freq = 16.777216 * note_hz / 16777216` in the 16-bit
frequency register. We'll define them in a `notes.h` lookup table in
the source.)

Voice 1 waveform is set per state; voice 2 is on only for WAIT and WIN.
The voice 3 noise in DRAW is the only sound effect that's not a
musical note.

The music is driven by a raster IRQ that updates the SID registers
on a per-note schedule. Since each state has a small, fixed set of
notes, the IRQ can be very simple — a "next note time" jiffy counter
and a pointer into a per-state note table.

## 7. Inputs

| Action | Player 1 (Hare) | Player 2 (Scoot) |
|--------|------------------|------------------|
| Fire | `PEEK(0xDC01) & 0x10 == 0` | `PEEK(0xDC00) & 0x10 == 0` |
| (other directions unused) | (ignore) | (ignore) |

`PEEK(0xDC00)` and `PEEK(0xDC01)` are the active-low joystick bytes on
CIA 1 ports A and B. Bit 4 is the fire button; the lower 4 bits are
the directions (up/down/left/right). In the title screen the game
also has to read both ports every frame to see if either player has
pressed fire.

Note: the C64's keyboard is wired in parallel with the joystick
matrix, so `PEEK(0xDC00/0xDC01)` will also return "pressed" if you
hold a key in the right column. For our purposes this is fine (most
players use joysticks), but if we want to be strict we can disable
the keyboard with `POKE 0xDC02, 0xE0` (set keyboard columns as inputs)
during gameplay and re-enable with `POKE 0xDC02, 0xFF` when returning
to BASIC.

## 8. Timing (PAL C64, 50 Hz / 312 lines)

The game runs at the PAL frame rate: 50 fields per second, 312 lines
per field. We use a single raster IRQ set to fire on a stable line
(line 311, the one right after vertical blank and before any badlines)
on every frame, so we have a 50 Hz tick to update game state, read
joysticks, update the SID, and redraw the screen.

- TITLE state: update at 25 Hz (every other frame) to flash the
  prompt. Read joysticks every frame so the input is responsive.
- READY state: fixed-duration, ~1.2 sec = 60 frames. Use a frame
  counter, not a real-time timer.
- WAIT state: random 100-250 frames. The random number is from the
  SID's $D41B oscillator register (read-only noise that's
  effectively random).
- DRAW state: indefinite. Counter increments every frame. The first
  fire press wins. If for some reason neither player presses for
  10 seconds (500 frames), declare it a "fault" and go to TITLE
  with no point awarded.
- WIN state: fixed 2.0 sec = 100 frames.
- GAMEOVER state: fixed 6.0 sec = 300 frames.

## 9. The state machine

```
                  both fire
        +-------------------------+
        |                         v
      TITLE  ----- fire1 only ---->  TITLE  (wait for player 2)
        |                         ^
        |  both still holding? no
        v
      READY   (fixed 60 frames, "ping" jingle)
        |
        v
      WAIT    (random 100..250 frames, suspense music)
        |
        v
      DRAW    (counter++, play stab, wait for fire)
        |    \
        |     \-- both never fire in 500 frames --> fault back to TITLE
        |
        v  fire1 first
      WIN_P1  (100 frames, jingle, +1 to player 1)
        |  \
        |   \-- player 1 score == 5 --> GAMEOVER
        v
      WIN_P2  (100 frames, jingle, +1 to player 2)
        |  \
        |   \-- player 2 score == 5 --> GAMEOVER
        v
      (back to READY for next round)
```

States are stored in a single `byte` or `enum` variable. Each frame,
the IRQ handler runs the state machine, updates the SID, and updates
the score / counter display.

## 10. Asset preparation pipeline

Before the C64 ever sees these PNGs, we have to:

1. **Crop** each PNG to 320×200 (or pick a 320×200 region that looks
   good). The center 320×200 of each is usually what we want; the
   aspect ratio is close to 16:10 which is what the C64 has, so a
   direct crop with no scaling works.
2. **Reduce colors** to the C64's 16-color palette. The originals
   look like they were already done with a C64 palette in mind
   (the orange/purple/yellow/brown/green range is classic C64), but
   we should still quantize to a 16-color palette that matches the
   VIC's actual 16 fixed colors. Tools: ImageMagick
   (`-colors 16 +dither` or similar), GIMP (Image → Mode →
   Indexed), or a small Python/PIL script.
3. **Convert to 320×200 hires bitmap format** — a single .bin file
   of 8000 bytes (320×200 / 8 bits per byte, row-major, MSB first).
4. **Compress with LZO** at build time via Oscar64's
   `#embed lzo "file.bin"` directive. The decompressed bitmap is
   stored in a RAM region at startup.
5. **Copy to $E000-$FF40 (or wherever the hires area is)** at the
   start of each state transition. Since all 4 screens are different
   bitmaps, we either keep them all in RAM (4 × 8000 = 32 KB, way
   too much) or we stream one in at a time from the .prg file at
   each state transition.

The practical solution: embed all 4 screens with `#embed lzo`, store
them in a reserved region of the .prg file (after the program code
and before the runtime data), and at each state transition, decompress
the right one into the hires area using `oscar_expand_lzo`. This is
the same pattern Oscar64 samples use for the charsets and sprites.

## 11. Memory budget (rough)

| Item | Size | Where |
|------|------|-------|
| Compiled C code (logic, IRQ, audio) | ~4-6 KB | `$0900-$1F00` |
| Stack (C locals, IRQ save area) | 1 KB | BSS |
| Heap (no malloc) | 0 KB | n/a |
| One decompressed hires bitmap | 8000 bytes | `$6000-$7F40` (or similar 8 KB-aligned window) |
| 4 compressed bitmaps in .prg | ~10-15 KB total | After code, in ROM/loaded region |
| Character set (1 KB) + Color RAM (1 KB) | 2 KB | RAM |
| Custom font (pips, counter digits) | 256 bytes | RAM |
| SID note tables | ~200 bytes | ROM |
| **Total loaded** | ~25-30 KB | Fits comfortably in `$0900-$A000` (38 KB) or in a no-ROM build |

The game is comfortably within C64 limits. We can even use the default
$0900-$A000 region with all ROMs banked in (so the KERNAL ISR
trampoline still works), leaving the full 38 KB available for code,
data, and one decompressed bitmap at a time.

## 12. What's *not* in scope (yet)

- Sound test menu (no — the audio is fixed per state).
- Player name entry (no — pure 2-player hot-seat).
- Difficulty levels (no — the WAIT duration is fixed; we can vary it
  per round if we want a "tiebreaker" round).
- AIs (no — strictly 2-player).
- Save/load high scores (no — sessions are short).
- NTSC (50 Hz vs 60 Hz) — PAL is the target. NTSC is a small port:
  different raster line counts, different stable-raster line numbers.
  The raster IRQ line (311) and frame rate (50 Hz) are the only
  numbers that need to change.
- The score bar's *exact* pixel layout — we'll iterate on the
  visual design after we have the gameplay working.

## 13. Source layout (proposed)

```
src/
├── main.c              # entry point, main loop
├── game.c / game.h     # state machine
├── screens.c / .h      # 4 compressed bitmaps + show_screen(N) helper
├── audio.c / .h        # SID note tables + play_state(N) helper
├── input.c / .h        # joystick read helpers
├── score.c / .h        # score bar renderer
├── draw.c / .h         # the white-screen + counter rendering
├── rasterirq.c / .h    # 50 Hz frame tick
├── notes.h             # SID frequency table
└── build/              # (gitignored) output
```

The plan is to keep the code as flat as possible — no `region` /
`section` pragmas in the first pass, just the default Oscar64 layout,
and we reach for `#pragma region(…)` if the linker complains.

---

The art is striking, the gameplay is dead simple, and the SID has
plenty of room for the audio we want. This should be a fun first C64
project.
