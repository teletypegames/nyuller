# VIC-II (Video Interface Chip II)

Source: https://www.c64-wiki.com/wiki/VIC

## Identification

VIC-II is the video chip used in the C64 / C128 family. There are two VIC-II types in the C64: the 6567 in NTSC machines and the 6569 in PAL machines. Newer C64 versions use the functionally equivalent 8562 (NTSC) and 8565 (PAL) chips.

## Features

- 16 kB address space for screen, character and sprite memory
- 320 × 200 pixels video resolution (160 × 200 in multi-color mode)
- 40 × 25 characters text resolution
- Three character display modes and two bitmap modes
- 16 colors
- Concurrent handling of 8 sprites per scanline, each of 24 × 21 pixels (12 × 21 multicolor)
- Raster interrupt
- Smooth scrolling
- Independent dynamic RAM refresh (an unusual feature for a graphics processor)
- Bus mastering for a 6502-style system bus; CPU and VIC-II accessing the bus during alternating half-clock cycles (the VIC-II will halt the CPU when it needs extra cycles)

## Memory Addresses of the VIC-II

| Hex Address | Dec Address | Page | Contents |
|-------------|-------------|------|----------|
| $D000-$D3FF | 53248-54271 | Page 208-211 | VIC-II registers |
| $D800-$DBE7 | 55296-56295 | Page 216-219 | Color RAM |

- Sprites lie at address MEM(Start of screen mem + $03F8 + sprite number)*64
- The start of the Screen RAM is set by $DD00 (the VIC bank, see CIA 2) and $D018.

## Technical Notes (from Christian Bauer's article)

The operation of the VIC-II is thoroughly described in the document
"The MOS 6567/6569 video controller (VIC-II) and its application in the Commodore 64"
by Christian Bauer. See `vic/cebix-vic-article.txt` for the full text.

Key facts:
- The cycle count starts at 1; for example, VICE starts counting at 0.
- Writing to $D011/$D012 can immediately trigger a raster IRQ, provided no raster IRQ has yet been triggered in the current line.
- The vertical "expansion flip-flop" is somewhat misleadingly named; its state actually indicates whether a line in the sprite data should currently be skipped or not.
- VCBASE and MCBASE are used to reset VC and MC to their initial values.
- VMLI is not a pointer but rather a 40-bit shift register that controls the 40 enable bits of the VIC-II's internal "40×12 bit video matrix/color line."

## Known Variants

- MOS 6560 / 6561: VIC-I (VIC-20)
- MOS 6566: VIC-II for MAX Machine
- MOS 6567: VIC-II for (NTSC) C64
- MOS 6569: VIC-II for (PAL) C64 (R1/R3/R4/R5)
- MOS 6572/6573: VIC-II (PAL-N/PAL-M)
- MOS 8562: VIC-II (NTSC) for later C64 / C128
- MOS 8564/8565/8566/8569: VIC-II variants for C64 / C128
- CSG 4567: VIC-III (C65/C64DX)

The NMOS variants 6566/67/69 require +12V on Pin 13/Vdd; the HMOS-II variants (8562/65) require only +5V DC. The 856x variants exhibit the "Grey Dots" problem.
