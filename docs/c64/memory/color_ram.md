# Color RAM

Source: https://www.c64-wiki.com/wiki/Color_RAM

## Description

The **color RAM** (or color memory) of the C64 starts at 55296 ($D800) and ends at 56295 ($DBE7). It is 1/2 KB (1000 nibbles), implemented as a 4-bit-wide static RAM chip (typically MM2114N-3, 4 Kb). Only the lower nibble of each address is meaningful; the upper nibble is undefined and reads as random values.

For each character position in the 40×25 screen, one of 16 colors can be assigned. Color values 0-15 are stored in the lower nibble.

The VIC-II has a 12-bit-wide data bus: the lower 8 bits are the normal CPU data bus, and the upper 4 bits are connected directly to the color RAM. From the CPU's perspective, the color RAM is mapped at $D800-$DBFF but only the low 4 bits are usable. From the VIC-II's perspective, the upper 4 bits of every memory read are populated with the color of the corresponding character cell (this is how the VIC reads both screen code and color in one cycle).

## Addresses

| Hex | Dec | Purpose |
|-----|-----|---------|
| $D800-$DBE7 | 55296-56295 | 1/2 KB color memory |
| $DBE8-$DBFF | 56296-56319 | Unused |

## PETSCII colors (16)

From the C64-Wiki: black, white, red, cyan, pink/purple, green, blue, yellow, orange, brown, light red, dark grey, medium grey, light green, light blue, light grey.

## Usage

```basic
POKE 55296, 1   ; sets upper-left character block to white
```

## See also

- VIC-II: 16-color palette generation (analog TV phase/amplitude from ϕCOLOR)
- The standard 16 PETSCII colors are stored as constants in the Oscar64 header `c64/vic.h` (`VCOL_BLACK` etc.) and the color registers accept these values.
