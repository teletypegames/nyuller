#ifndef NYULLER_SCREENS_H
#define NYULLER_SCREENS_H

// Screen IDs.  Pass to show_screen() to switch to a new screen.
#define SCREEN_TITLE		0
#define SCREEN_WAITING1		1
#define SCREEN_WAITING2		2
#define SCREEN_WIN_HARE		3
#define SCREEN_WIN_SCOOT	4

// show_screen(n) — load screen `n` into the VIC.
//
// The 5 game screens (title, waiting1, waiting2, win_hare, win_scoot)
// are 160x200 multicolor bitmaps, 8000 bytes of pixel data each.  Each
// screen has a 1000-byte color attribute table (one byte per 4x8 cell)
// and a single-byte $D021 background color value.
//
// The pixel data and color attribute table are produced by
// tools/convert_screens.py (see its docstring for the exact format).
// This function copies the appropriate ones into place and configures
// the VIC to display them.
//
// Memory layout used here (after memmap_setup()):
//   $C400-$C7E7  — screen memory (1000 bytes; the "color attributes")
//                  Per cebix-vic-article §3.7.3.4, in multicolor bitmap
//                  mode the screen memory byte holds the "01" color in
//                  its high nibble and the "10" color in its low
//                  nibble.  The "11" color comes from color RAM at
//                  $D800+cell; we leave color RAM zeroed (black) for
//                  now since the .attr files don't store it (see
//                  tools/convert_screens.py for why).
//   $D800-$DBE7  — color RAM (1000 nibbles).  Cleared to 0 here.
//   $E000-$FFFF  — 8 KB bitmap (the .bin data).
//   $D021        — background color (the "00" color in the multicolor
//                  scheme).  Set to the per-screen value from the
//                  .d021 sidecar file.
//
// VIC config written here:
//   bank = 0 (CIA2 PRA low 2 bits = 0, selects CPU $C000-$FFFF)
//   ctrl1 = BMM | DEN | RSEL  (multicolor bitmap, display on, 25 rows,
//                              no vertical scroll)
//   ctrl2 = MCM | CSEL        (multicolor, 40 columns, no horiz scroll)
//   memptr (D018) = 0x18      (screen at $0400, bitmap at $E000 within
//                              the selected 16K VIC bank)
//
// Calling show_screen() with an unsupported ID is a no-op.

void show_screen(int n);

// show_white_screen() — switch to the DRAW screen: solid white fill
// over the entire visible area (including the score bar, which becomes
// invisible).
//
// Implementation:
//   1. Fill the 8 KB bitmap at $E000-$FFFF with 0x00 so every pixel is
//      a "00" code (which uses $D021).
//   2. Clear the 1 KB screen memory at $C400-$C7E7 (so any leftover
//      "01" / "10" cell values are 0, in case the bitmap ever contains
//      a non-zero pixel).
//   3. Clear the 1 KB color RAM at $D800-$DBE7.
//   4. Set $D021 (background) = white, $D020 (border) = white.  Also
//      set $D022 / $D023 to white for safety (they're not used for
//      "00" pixels, but any stray "01" / "10" pixel would be black
//      otherwise).
//   5. Same VIC mode config as show_screen().
//
// The score bar overlay (drawn by score_render() in game.c) writes
// "00" (black, from $D021) and "11" (white, from color RAM) pixels in
// the top 8 rows.  With $D021 and color RAM both white, the entire
// score bar is white and invisible against the white background.
// The bitmap rewrite on the score bar cells is harmless; it just
// produces more white.
//
// Called by game.c on entry to STATE_DRAW.
void show_white_screen(void);

#pragma compile("screens.c")

#endif
