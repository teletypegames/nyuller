#ifndef WHACK_HARE_BANNER_H
#define WHACK_HARE_BANNER_H

// banner.h — text banner rendering in the multicolor bitmap.
//
// The banner renderer draws a NUL-terminated ASCII string using the
// same 4x8 custom font as the score bar (see score.c, FONT_COUNT
// entries).  The string is rendered in a single character row of the
// bitmap at the cell position passed to banner_render(); cells use
// 4 px width and 8 px height (one cell = 8 bytes of bitmap).
//
// Banner rows currently used by the game:
//   Row 0 (cells  0..39)  = score bar (HARE/SCOOT labels + pips).
//                          Banners that go HERE overwrite the score
//                          bar — score_render() must be called first
//                          or banners must use a different row.
//   Row 1 (cells 40..79)  = subtitle area.  The title screen art
//                          is partially overwritten by banners here.
//                          Used by the "PRESS FIRE" prompt on TITLE
//                          and the "HARE WINS!" / "SCOOT WINS!"
//                          banner on GAMEOVER.
//   Rows 2..24            = the rest of the screen art.  Banners
//                          can be placed in any of these rows.
//
// banner_render() overwrites only the cells it writes to; the rest
// of the screen is untouched.  Cells touched by banner_render() have
// their color RAM set to 1 (white) so the "11" pixel code maps to
// white on the (black) background.
//
// The banner font is 4 px wide; 10-char strings like "PRESS FIRE"
// or "HARE WINS!" fit comfortably in a 40-cell row.  11-char strings
// like "SCOOT WINS!" also fit (with 1 cell to spare).

#include <c64/types.h>

// banner_clear(row) — fill the given character row (0..24) of the
// bitmap with 0s and clear the color RAM.  Used to erase a banner
// before switching to a screen that should not show it.
void banner_clear(byte row);

// banner_render(text, row) — render a NUL-terminated ASCII string
// centered in the given character row of the bitmap.  The string is
// truncated to 40 characters; anything past that is ignored.  Any
// char not in the font renders as a blank cell (no visible glyph).
//
// If text is empty (""), the row is left untouched (call
// banner_clear() to explicitly clear it).
void banner_render(const char *text, byte row);

#pragma compile("banner.c")

#endif
