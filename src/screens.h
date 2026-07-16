#ifndef WHACK_HARE_SCREENS_H
#define WHACK_HARE_SCREENS_H

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
//   $D000-$D3E7  — screen memory (1000 bytes; the "color attributes")
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
//   memptr (D018) = 0x48      (screen at $D000, bitmap at $E000 within
//                              the selected 16K VIC bank)
//
// For Phase 2 only SCREEN_TITLE has embedded data; the other IDs are
// reserved for Phase 4 (state machine).  Calling show_screen() with an
// unsupported ID is a no-op.

void show_screen(int n);

#pragma compile("screens.c")

#endif
