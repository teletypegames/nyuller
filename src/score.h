#ifndef WHACK_HARE_SCORE_H
#define WHACK_HARE_SCORE_H

// score.h — score state and rendering for the top-of-screen score bar.
//
// The score bar lives in the top 8 rows of the multicolor bitmap at
// $E000-$E13F.  Those rows are one full character row of the screen
// (40 cells × 8 px tall × 2 bpp = 320 bytes).  show_screen() loads the
// title artwork into the bitmap; score_render() overwrites the first
// 320 bytes with the score bar.
//
// Layout (40 cells = 160 px, since multicolor is half-width hires):
//
//   cell  0  1   2  3  4  5   6   7 ... 16   17 18 19  20 21  22 ... 31   32  33 34 35 36 37   38 39
//        |marg|  H  A  R  E  |gap| 5 pips  |marg|marg| 5 pips  |gap|  S  C  O  O  T  |marg|
//
//   HARE label: cells 2..5 (4 chars)
//   5 pips P1:  cells 7..16 (10 cells, 2 cells per pip, no gap)
//   5 pips P2:  cells 22..31 (10 cells)
//   SCOOT label: cells 33..37 (5 chars)
//
// Each pip is 8×8 pixels (2 cells wide × 1 cell tall) = 16 bytes of bitmap:
//   - filled: 1-pixel white border around a 6×6 black block
//   - empty:  just the white border (an 8×8 white square with no inside)
//
// Each label char is 4×8 pixels (1 cell) = 8 bytes of bitmap, drawn as
// "11" (white) on "00" (background black).  Cell attributes for the top
// row are rewritten: screen memory = 0 (both halves black), color RAM =
// 1 (white, so the "11" pixel value maps to white).
//
// All 4 colors used by the bitmap (00/01/10/11) per c64-wiki:
//   00 -> $D021 (background; set to 0 = black by show_screen)
//   01 -> high nibble of screen mem (we set 0 = black; unused here)
//   10 -> low nibble of screen mem (we set 0 = black; unused here)
//   11 -> color RAM nibble (we set 1 = white for the bar's cells)
// The score bar only uses 00 and 11, so the cell palette is effectively
// 2 colors: black and white.

#include <c64/types.h>

// Player 1 (Hare) score, 0..5.
extern byte score_p1;

// Player 2 (Scoot) score, 0..5.
extern byte score_p2;

// score_init() — zero both scores.  Called once at startup.
// (The variables already default to 0 in BSS; this is for symmetry with
// the future per-state initialization in Phase 4.)
void score_init(void);

// score_render() — draw the score bar into the top 8 rows of the active
// multicolor bitmap at $E000.  Overwrites the top row of the current
// screen's artwork; the rest of the screen is untouched.
//
// Call this after show_screen() to overlay the score bar on top of the
// screen art.  In Phase 4 it will be called on entry to each state.
void score_render(void);

#pragma compile("score.c")

#endif
