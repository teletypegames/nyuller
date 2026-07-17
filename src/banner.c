// banner.c — text banner rendering in the multicolor bitmap.
//
// The banner renderer draws a NUL-terminated ASCII string in a
// character row of the active multicolor bitmap (at $E000+).  Each
// character is 4 px wide × 8 px tall = 8 bytes of bitmap, the same
// as the score bar's labels.  The font is the 16-entry table in
// score.c; banner.c reaches into score.c's font via an extern
// declaration in score.h.
//
// banner_render(text, row) centers the text in the row's 40 cells.
// The first cell of row N is at byte offset N*320 in the bitmap
// (40 cells × 8 bytes/cell = 320 bytes per row).  Color RAM for
// the row is set to 1 (white) so the "11" pixel code used by the
// glyphs maps to white.  Screen memory for the row is set to 0 so
// the "01" and "10" pixel codes (which we never write) are harmless
// black.
//
// Characters not in the font render as a blank cell.  Strings longer
// than 40 chars are truncated (the rightmost chars are dropped).
// Empty strings are no-ops (call banner_clear() to explicitly clear).

#include "banner.h"
#include "score.h"

// Extern the font from score.c.  We could go through score_render()
// to use the font, but the banner is a separate concern: it doesn't
// touch the score bar's labels or pips, and it renders below the
// score bar (row 1) or in the body of the screen (rows 2+).
extern const char font[FONT_COUNT][8];

// Pixel-pair expansion table (same as score.c).  Each 4-bit font
// nibble is expanded to a byte where each "1" becomes "11" and each
// "0" becomes "00".  This is the standard multicolor bitmap byte
// encoding for 2 bpp.
static const char expand4[16] = {
	0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
	0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF
};

// Bitmap base, color RAM base, screen memory base.  Same as score.c
// — duplicated here so banner.c is self-contained.
#define BM_BASE		((char *)0xE000)
#define CRAM_BASE	((char *)0xD800)
#define SMEM_BASE	((char *)0xD000)
#define CELLS_PER_ROW	40
#define BYTES_PER_CELL	8
#define BYTES_PER_ROW	320

// banner_clear(row) — fill the given row of the bitmap with 0s
// (black "00" pixels via $D021) and zero the color RAM for that
// row's cells.  Use this before rendering a banner into a row that
// might have leftover pixels from a previous banner or from the
// screen art.
void banner_clear(byte row)
{
	char *bm_row = BM_BASE + (unsigned)row * BYTES_PER_ROW;
	char *cram_row = CRAM_BASE + (unsigned)row * CELLS_PER_ROW;
	for (unsigned i = 0; i < BYTES_PER_ROW; i++)
		bm_row[i] = 0;
	for (unsigned i = 0; i < CELLS_PER_ROW; i++)
		cram_row[i] = 0;
}

// Render a single glyph at the given cell position.  Writes 8 bytes
// to the bitmap and sets the color RAM for that cell to 1 (white).
// char_idx is a font index (0..FONT_COUNT-1); out-of-range indices
// produce a blank cell.
static void render_glyph(byte cell, byte char_idx)
{
	if (char_idx >= FONT_COUNT)
		char_idx = CHAR_RESERVED;  // blank for unknown glyphs

	char *dst = BM_BASE + (unsigned)cell * BYTES_PER_CELL;
	const char *src = font[char_idx];
	for (unsigned row = 0; row < BYTES_PER_CELL; row++) {
		unsigned nibble = ((unsigned)src[row] >> 4) & 0x0F;
		dst[row] = expand4[nibble];
	}
	CRAM_BASE[cell] = 1;  // "11" pixels = white
	SMEM_BASE[cell] = 0;  // "01"/"10" pixels = black (unused)
}

// String length helper.  NUL-terminated ASCII, max 40 chars (the
// width of one row in cells).  Longer strings are truncated.
static byte strnlen40(const char *s)
{
	byte n = 0;
	while (n < 40 && s[n] != 0)
		n++;
	return n;
}

void banner_render(const char *text, byte row)
{
	// Truncate to 40 chars (the row width).
	byte len = strnlen40(text);
	if (len == 0)
		return;  // empty string: leave the row untouched

	// Clear the row first so leftover pixels from the screen art
	// (or a previous banner) don't show through between glyphs.
	banner_clear(row);

	// Center the text: start_cell = (40 - len) / 2.
	byte start = (40 - len) >> 1;
	unsigned row_offset = (unsigned)row * CELLS_PER_ROW;

	for (byte i = 0; i < len; i++) {
		byte idx = font_lookup(text[i]);
		render_glyph(row_offset + start + i, idx);
	}
}
