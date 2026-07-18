// score.c — score bar rendering for the top 8 rows of the multicolor
// bitmap.  See score.h for the layout.
//
// In multicolor bitmap mode the bitmap is 160 px wide × 200 px tall
// (2 bpp) = 8000 bytes.  One row of pixels = 40 bytes (160 px × 2 bpp
// / 8 bits).  The top 8 rows = 320 bytes at $E000..$E13F.
//
// We draw three kinds of things into those 320 bytes:
//   1. 4×8 label characters (one cell = 8 bytes of bitmap).
//   2. 8×8 pips (two cells = 16 bytes of bitmap).
//   3. Background fill (00 pixel = $D021 black) for the cells outside
//      the labels and pips (the margins).
//
// All visible pixels are either "00" (black, from $D021) or "11" (white,
// from color RAM).  We rewrite the top 40 cells' attributes
// (screen memory → 0, color RAM → 1) so the cell palette is exactly
// 2 colors and we don't have to think about 01/10 mismatches across
// the 4 cells of the bar.

#include "score.h"

// Player scores.  0..5 (clamped at the render side).  Stored in zero-page-
// adjacent BSS, default 0.  Phase 4 will mutate these on WIN_P1/WIN_P2.
byte score_p1 = 0;
byte score_p2 = 0;

// --- custom 4×8 font ---------------------------------------------------
//
// 16 characters, each 4 px wide × 8 px tall = 8 bytes (one byte per
// row, top 4 bits are the 4 pixels of that row, MSB = leftmost pixel).
// 128 bytes total.  Bottom 4 bits of every byte are 0.
//
// Index (also see CHAR_* macros in score.h):
//   0=H, 1=A, 2=R, 3=E, 4=S, 5=C, 6=O, 7=T  (score bar labels)
//   8=P, 9=F, 10=W, 11=I, 12=N, 13=!, 14=' ', 15=reserved
static const unsigned char font[FONT_COUNT][8] = {
	// 0 H  (1 0 0 1) × 3, (1 1 1 1), (1 0 0 1) × 4
	{ 0x90, 0x90, 0x90, 0xF0, 0x90, 0x90, 0x90, 0x90 },
	// 1 A  (0 1 1 0), (1 0 0 1) × 2, (1 1 1 1), (1 0 0 1) × 4
	{ 0x60, 0x90, 0x90, 0xF0, 0x90, 0x90, 0x90, 0x90 },
	// 2 R  (1 1 1 0), (1 0 0 1) × 2, (1 1 1 0), (1 0 1 0),
	//      (1 0 0 1) × 3
	{ 0xE0, 0x90, 0x90, 0xE0, 0xA0, 0x90, 0x90, 0x90 },
	// 3 E  (1 1 1 1), (1 0 0 0) × 2, (1 1 1 0), (1 0 0 0) × 3, (1 1 1 1)
	{ 0xF0, 0x80, 0x80, 0xE0, 0x80, 0x80, 0x80, 0xF0 },
	// 4 S  (0 1 1 1), (1 0 0 0) × 2, (0 1 1 0), (0 0 0 1) × 3, (1 1 1 0)
	{ 0x70, 0x80, 0x80, 0x60, 0x10, 0x10, 0x10, 0xE0 },
	// 5 C  (0 1 1 0), (1 0 0 1), (1 0 0 0) × 4, (1 0 0 1), (0 1 1 0)
	{ 0x60, 0x90, 0x80, 0x80, 0x80, 0x80, 0x90, 0x60 },
	// 6 O  (0 1 1 0), (1 0 0 1) × 5, (0 1 1 0)
	{ 0x60, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x60 },
	// 7 T  (1 1 1 1), (0 0 1 0) × 7
	{ 0xF0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 },
	// 8 P  (1 1 1 0), (1 0 0 1) × 2, (1 1 1 1), (1 0 0 0) × 4
	{ 0xE0, 0x90, 0x90, 0xF0, 0x80, 0x80, 0x80, 0x80 },
	// 9 F  (1 1 1 1), (1 0 0 0) × 2, (1 1 1 0), (1 0 0 0) × 4
	{ 0xF0, 0x80, 0x80, 0xE0, 0x80, 0x80, 0x80, 0x80 },
	// 10 W  (1 0 0 1) × 4, (1 1 1 1) × 2, (0 1 1 0), (0 0 0 0)
	{ 0x90, 0x90, 0x90, 0x90, 0xF0, 0xF0, 0x60, 0x00 },
	// 11 I  (1 1 1 1), (0 0 1 0) × 6, (1 1 1 1)
	{ 0xF0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xF0 },
	// 12 N  (1 0 0 0), (1 1 0 0), (1 1 0 0), (1 0 1 0), (1 0 1 0),
	//      (1 0 0 1), (1 0 0 1), (0 0 0 0)
	{ 0x80, 0xC0, 0xC0, 0xA0, 0xA0, 0x90, 0x90, 0x00 },
	// 13 !  (0 0 1 0) × 5, (0 0 0 0), (0 0 1 0)
	{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x20 },
	// 14 ' ' (space) - 8 empty rows
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	// 15 reserved - blank for now
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

// --- pixel-pair expansion table ----------------------------------------
//
// For each 4-bit pixel pattern (1 = on, 0 = off), the corresponding
// multicolor bitmap byte.  Each "1" becomes "11" (white, from color RAM);
// each "0" becomes "00" (background, from $D021 = black).
//
//   bit pattern  | multicolor byte
//      0000      |   00 00 00 00   = 0x00
//      0001      |   00 00 00 11   = 0x03
//      0010      |   00 00 11 00   = 0x0C
//      0011      |   00 00 11 11   = 0x0F
//      0100      |   00 11 00 00   = 0x30
//      0101      |   00 11 00 11   = 0x33
//      0110      |   00 11 11 00   = 0x3C
//      0111      |   00 11 11 11   = 0x3F
//      1000      |   11 00 00 00   = 0xC0
//      1001      |   11 00 00 11   = 0xC3
//      1010      |   11 00 11 00   = 0xCC
//      1011      |   11 00 11 11   = 0xCF
//      1100      |   11 11 00 00   = 0xF0
//      1101      |   11 11 00 11   = 0xF3
//      1110      |   11 11 11 00   = 0xFC
//      1111      |   11 11 11 11   = 0xFF
static const char expand4[16] = {
	0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
	0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF
};

// --- pip bitmaps -------------------------------------------------------
//
// 8×8 pixels = 2 cells wide × 1 cell tall = 16 bytes of bitmap
// (8 rows × 2 bytes/row, 4 pixels per byte in 2 bpp).
//
// Filled pip: 1-pixel white border around a 6×6 black block.
// Empty pip: 8×8 white square (the "border only" with no fill).
//
// Row 0 = top border:           8 white pixels  = $FF $FF
// Rows 1..6: 1 white + 6 black + 1 white:
//   left byte  = 11 00 00 00   = 0xC0
//   right byte = 00 00 00 11   = 0x03
// Row 7 = bottom border:        $FF $FF
static const char pip_filled[16] = {
	0xFF, 0xFF, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03,
	0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xFF, 0xFF
};

static const char pip_empty[16] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// --- cell layout constants --------------------------------------------
//
// The score bar fits in the top 40 cells (one character row).  Each
// cell is 4 px wide × 8 px tall = 8 bytes of bitmap.  The whole top
// row = 40 × 8 = 320 bytes at $E000..$E13F.

#define SCORE_BAR_BYTES		320
#define CELLS_PER_ROW		40
#define BYTES_PER_CELL		8
#define SCORE_BITMAP_BASE	((char *)0xE000)
#define SCORE_SCREEN_BASE	((char *)0xC400)
#define SCORE_CRAM_BASE		((char *)0xD800)

// Cell indices for the labels and pip groups (see score.h for diagram).
// Pips are 2 cells wide; the left pips start at cell 7, the right pips
// at cell 22.  Stride between adjacent pips is 2 cells (no gap; the
// 1-px borders of adjacent pips merge into a 2-px white vertical line).
#define HARE_FIRST_CELL		2
#define LEFT_PIPS_FIRST_CELL	7
#define RIGHT_PIPS_FIRST_CELL	22
#define SCOOT_FIRST_CELL	33

// --- helpers -----------------------------------------------------------

static void clear_score_bar(void)
{
	for (unsigned i = 0; i < SCORE_BAR_BYTES; i++)
		SCORE_BITMAP_BASE[i] = 0;
}

static void setup_score_bar_cells(void)
{
	// Top 40 cells: screen memory = 0, color RAM = 1.
	// Screen memory 0 means both "01" and "10" pixel codes map to black
	// (we don't use either code in the score bar).  Color RAM 1 means
	// the "11" pixel code maps to white — that's the color of the
	// pip borders and the label characters.
	for (unsigned i = 0; i < CELLS_PER_ROW; i++) {
		SCORE_SCREEN_BASE[i] = 0;
		SCORE_CRAM_BASE[i]  = 1;
	}
}

static void render_char(unsigned cell_idx, unsigned char_idx)
{
	char *dst = SCORE_BITMAP_BASE + cell_idx * BYTES_PER_CELL;
	const char *src = font[char_idx];
	for (unsigned row = 0; row < BYTES_PER_CELL; row++) {
		unsigned nibble = ((unsigned)src[row] >> 4) & 0x0F;
		dst[row] = expand4[nibble];
	}
}

static void render_pip(unsigned cell_idx, char filled)
{
	char *dst = SCORE_BITMAP_BASE + cell_idx * BYTES_PER_CELL;
	const char *src = filled ? pip_filled : pip_empty;
	for (unsigned i = 0; i < 16; i++)
		dst[i] = src[i];
}

// --- public API --------------------------------------------------------

void score_init(void)
{
	score_p1 = 0;
	score_p2 = 0;
}

void score_render(void)
{
	// Clamp to 0..5 so a future bug in Phase 4 (e.g. score == 6 on a
	// double-tap) doesn't write past the pip array.
	byte p1 = score_p1;
	if (p1 > 5) p1 = 5;
	byte p2 = score_p2;
	if (p2 > 5) p2 = 5;

	setup_score_bar_cells();
	clear_score_bar();

	// HARE label: 4 cells, 4 unique chars.
	render_char(HARE_FIRST_CELL + 0, 0);  // H
	render_char(HARE_FIRST_CELL + 1, 1);  // A
	render_char(HARE_FIRST_CELL + 2, 2);  // R
	render_char(HARE_FIRST_CELL + 3, 3);  // E

	// 5 pips for player 1 (left side).  2 cells per pip, no gap.
	for (unsigned p = 0; p < 5; p++)
		render_pip(LEFT_PIPS_FIRST_CELL + p * 2, (byte)p < p1);

	// 5 pips for player 2 (right side).
	for (unsigned p = 0; p < 5; p++)
		render_pip(RIGHT_PIPS_FIRST_CELL + p * 2, (byte)p < p2);

	// SCOOT label: 5 cells, 4 unique chars (O repeats).
	render_char(SCOOT_FIRST_CELL + 0, 4);  // S
	render_char(SCOOT_FIRST_CELL + 1, 5);  // C
	render_char(SCOOT_FIRST_CELL + 2, 6);  // O
	render_char(SCOOT_FIRST_CELL + 3, 6);  // O
	render_char(SCOOT_FIRST_CELL + 4, 7);  // T
}

byte font_lookup(char c)
{
	// Maps ASCII to font index.  Returns CHAR_RESERVED for any
	// character not in our 15-char font.  Used by the banner
	// renderer.
	switch (c) {
		case 'H': return CHAR_H;
		case 'A': return CHAR_A;
		case 'R': return CHAR_R;
		case 'E': return CHAR_E;
		case 'S': return CHAR_S;
		case 'C': return CHAR_C;
		case 'O': return CHAR_O;
		case 'T': return CHAR_T;
		case 'P': return CHAR_P;
		case 'F': return CHAR_F;
		case 'W': return CHAR_W;
		case 'I': return CHAR_I;
		case 'N': return CHAR_N;
		case '!': return CHAR_EXCL;
		case ' ': return CHAR_SPACE;
		default:  return CHAR_RESERVED;
	}
}
