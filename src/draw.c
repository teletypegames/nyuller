// draw.c — DRAW screen counter rendering (Phase 7).
//
// 3-digit 7-segment counter, each digit 3 cells wide × 8 cells tall.
// Layout of segments within a digit (col 0..2 left-to-right, row
// 0..7 top-to-bottom):
//
//   row 0:     [ ] [a] [ ]
//   row 1:     [f] [ ] [b]
//   row 2:     [f] [ ] [b]
//   row 3:     [f] [g] [b]
//   row 4:     [e] [g] [c]
//   row 5:     [e] [ ] [c]
//   row 6:     [e] [ ] [c]
//   row 7:     [ ] [d] [ ]
//
// "On" segments: bitmap = 0xFF (all "11" pixels) + color RAM = 0
// (black).  "Off" segments: bitmap = 0x00 (all "00" pixels) +
// color RAM = 0 (no-op for "00" pixels, which use $D021 = white).
// On the white background, the result is: black digits on white.
//
// The middle segment (g) is 2 cells tall to match the 3-cell-tall
// vertical segments visually.  16 of 24 cells are filled when all
// 7 segments are on (the "8"); only 6 of 24 are filled for the "1".

#include "draw.h"
#include <c64/types.h>

// 7-segment pattern table.  Bit 0 = a, bit 1 = b, bit 2 = c,
// bit 3 = d, bit 4 = e, bit 5 = f, bit 6 = g.
static const char seg7[10] = {
	0x3F,  // 0: a b c d e f
	0x06,  // 1: b c
	0x5B,  // 2: a b d e g
	0x4F,  // 3: a b c d g
	0x66,  // 4: b c f g
	0x6D,  // 5: a c d f g
	0x7D,  // 6: a c d e f g
	0x07,  // 7: a b c
	0x7F,  // 8: a b c d e f g
	0x6F,  // 9: a b c d f g
};

// Bitmap at $E000, color RAM at $D800.  Each cell = 8 bitmap bytes.
#define BM_BASE		((char *)0xE000)
#define CRAM_BASE	((char *)0xD800)

#define DIGIT_START_ROW		8
#define DIGIT_START_COL		15
#define DIGIT_CELLS_WIDE	3
#define DIGIT_CELLS_TALL	8
#define BYTES_PER_CELL		8
#define CELLS_PER_ROW		40

// Returns 1 if the cell at (row, col) within a 3x8 digit is "on"
// for the given 7-segment bit pattern.
static char cell_is_on(byte pattern, byte row, byte col)
{
	if (col == 0) {
		if (row >= 1 && row <= 3) return (pattern & 0x20) != 0;  // f
		if (row >= 4 && row <= 6) return (pattern & 0x10) != 0;  // e
	} else if (col == 2) {
		if (row >= 1 && row <= 3) return (pattern & 0x02) != 0;  // b
		if (row >= 4 && row <= 6) return (pattern & 0x04) != 0;  // c
	} else {
		if (row == 0) return (pattern & 0x01) != 0;              // a
		if (row == 3 || row == 4) return (pattern & 0x40) != 0;  // g
		if (row == 7) return (pattern & 0x08) != 0;              // d
	}
	return 0;
}

// Render a single digit at the given top-left cell.  Writes to the
// bitmap and color RAM for the 24 cells of the digit.  Color RAM is
// always zeroed (black) so any "11" pixel reads as black; "00"
// pixels (off segments) read $D021 = white regardless.
static void render_digit(unsigned start_cell, byte digit)
{
	byte pattern = seg7[digit];
	for (byte row = 0; row < DIGIT_CELLS_TALL; row++) {
		for (byte col = 0; col < DIGIT_CELLS_WIDE; col++) {
			unsigned cell = start_cell + (unsigned)row * CELLS_PER_ROW + col;
			char *bm = BM_BASE + cell * BYTES_PER_CELL;
			char fill = cell_is_on(pattern, row, col) ? (char)0xFF : (char)0x00;
			for (byte r = 0; r < BYTES_PER_CELL; r++)
				bm[r] = fill;
			CRAM_BASE[cell] = 0;
		}
	}
}

void draw_render_counter(int value)
{
	if (value < 0) value = 0;
	if (value > 999) value = 999;

	byte d0 = (byte)(value / 100);
	byte d1 = (byte)((value / 10) % 10);
	byte d2 = (byte)(value % 10);

	unsigned base = (unsigned)DIGIT_START_ROW * CELLS_PER_ROW + DIGIT_START_COL;
	render_digit(base + 0, d0);
	render_digit(base + 3, d1);
	render_digit(base + 6, d2);
}
