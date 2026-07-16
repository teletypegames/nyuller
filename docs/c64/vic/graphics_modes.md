# Graphics Modes

Source: https://www.c64-wiki.com/wiki/Graphics_Modes and https://www.c64-wiki.com/wiki/Standard_Character_Mode

The C64 supports 5 "official" graphics modes selected by 3 bits in the two VIC-II control registers:
- **ECM** (Extended Color Mode) — bit 6 of $D011
- **BMM** (Bitmap Mode) — bit 5 of $D011
- **MCM** (Multicolor Mode) — bit 4 of $D016

The other bits of those registers are YSCROLL (0-2 of $D011), DEN (display enable, bit 4 of $D011), RSEL (bit 3 of $D011), CSEL (bit 3 of $D016), XSCROLL (0-2 of $D016).

## The 8 (5 legal) modes

| Mode | ECM | BMM | MCM | Result |
|------|-----|-----|-----|--------|
| 0 | 0 | 0 | 0 | Standard Character Mode |
| 1 | 0 | 0 | 1 | Multicolor Character Mode |
| 2 | 0 | 1 | 0 | Standard Bitmap Mode |
| 3 | 0 | 1 | 1 | Multicolor Bitmap Mode |
| 4 | 1 | 0 | 0 | Extended Background Color Mode |
| 5 | 1 | 0 | 1 | Invalid |
| 6 | 1 | 1 | 0 | Invalid |
| 7 | 1 | 1 | 1 | Invalid |

Modes 5-7 are "technically feasible but produce no visible output."

## Standard Character Mode (Mode 0)

- 40×25 character cells of 8×8 pixels.
- Each cell can have one of 16 colors (from color RAM nibble at $D800+cell_index).
- A single background color ($D021) applies to the whole screen.
- Screen memory is 1 KB ($0400-$07FF); Color RAM is 1000 nibbles ($D800-$DBE7).
- Character patterns are 8 bytes per character, fetched from the character generator (default is the 4 KB character ROM at $D000-$DFFF, but can be relocated to RAM).
- Soft scrolling is "easier" in character mode than in bitmap mode (just change XSCROLL/YSCROLL).

## Multicolor Character Mode (Mode 1)

- Each cell still 8×8, but only 4×8 in effective pixel resolution (each pair of bits in the character pattern is one "wide pixel").
- Up to 4 colors per cell: the two global background colors ($D021 and $D022) plus the cell's color RAM color and the multicolor-1 shared color $D026.
- "Color" bits %00/%01/%10/%11 map to: background 0 ($D021), background 1 ($D022), color RAM, background 2 ($D023). Pattern bit pairs.

## Standard Bitmap Mode (Mode 2) — "hires"

- 320×200 pixels, 1 bit per pixel.
- Bitmap is 8000 bytes. Location selected by bits VM13-VM10 of $D018 (in 8 KB steps within the 16 KB VIC bank). Video matrix (screen memory) is 1 KB at $D018's VM bits (in 1 KB steps).
- Color: each 8×8 cell is monochrome (foreground from color RAM nibble, background from $D021).
- BM=1: the video matrix contains *color* information instead of character codes, so color RAM is doubled up to provide per-cell background and foreground colors (the color RAM holds the cell's foreground; the cell's background is one of the 4 global "background color" registers $D021-$D024 in the 4 most-significant bits of the cell).

## Multicolor Bitmap Mode (Mode 3)

- 160×200 pixels, 2 bits per pixel (4 colors per 4×8 cell).
- Each pair of bits selects from: background 0 ($D021), background 1 ($D022), background 2 ($D023), color RAM cell color.
- Same memory layout as Standard Bitmap Mode (8000 bytes bitmap + 1 KB matrix).

## Extended Background Color Mode (Mode 4)

- Character mode (not bitmap) with 4 background colors instead of 1.
- Each character cell can select one of the 4 background colors ($D021-$D024) via 2 bits in its screen memory code.
- Useful for colored PETSCII art.

## Unofficial "modes" (demo scene techniques)

There are dozens of techniques that go beyond the official modes by exploiting undocumented VIC behavior or combining tricks like raster IRQ, sprites, and interlacing. The most famous families:

- **FLI** (Flexible Line Interpretation, July 1989) — forces a badline every line, allowing per-line color attributes. The first "demo scene" graphic hack.
- **IFLI** (Interlaced FLI, 1991) — combines FLI with 2-frame interlace.
- **AFLI** (Advanced FLI, 1990), **NUFLI** (2009), **MUFLI** (2006), **UFLI** (1996), **XFLI** (2002), **SHFLI** (1996), etc.
- **AFLI** and **UFLI** give 8+ colors per 8×8 cell, **NUFLI** adds even more.
- **SHI** (Super HiRes Interlace, 1991) — interlaced 320×400 mode.
- **Hyperscreen** — stable display using all 16 KB VIC bank by abusing badline tricks.
- **FLD** (Flexible Line Distance) — variation.
- **Megatext** (2004) — large character set mode.

(See `vic/cebix-vic-article.txt` for the technical underpinnings of these tricks, especially sections 3.14 "Effects and applications" and 3.5 "Bad Lines".)
