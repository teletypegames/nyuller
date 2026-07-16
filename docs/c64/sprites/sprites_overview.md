# Sprites (MOBs — Movable Object Blocks)

Source: https://www.c64-wiki.com/wiki/Sprite

A sprite is a piece of graphics that can move and be assigned attributes independently of other graphics or text. The VIC-II supports up to 8 sprites, but with raster interrupt programming you can display more than eight simultaneously.

## Sprite data

- A sprite pattern is 24×21 pixels (12×21 in multicolor).
- 63 bytes of data + 1 unused "pad" byte = 64 bytes per sprite (must be 64-byte aligned in the current VIC bank).
- In theory a VIC bank holds 256 patterns; with a text screen and charset, practically ~208.

## Hi-res sprite

- Bit = 0 → transparent
- Bit = 1 → sprite's individual color

## Multicolor sprite

Each 2 bits = 1 pixel (2× wider):
- %00 → transparent
- %01 → color from $D025 (multicolor 0, common)
- %10 → color from $D027-$D02E (this sprite's color)
- %11 → color from $D026 (multicolor 1, common)

## Sprite pointers (in screen RAM area)

If text screen starts at address S, sprite pointers are at S+1016 through S+1023. With the default screen at $0400, that's $07F8-$07FF. Pointers contain the pattern address / 64.

| Sprite | Pointer |
|--------|---------|
| #0 | $07F8 (2040) |
| #1 | $07F9 (2041) |
| #2 | $07FA (2042) |
| #3 | $07FB (2043) |
| #4 | $07FC (2044) |
| #5 | $07FD (2045) |
| #6 | $07FE (2046) |
| #7 | $07FF (2047) |

## Sprite position registers

| Sprite | X reg | Y reg |
|--------|-------|-------|
| #0 | $D000 (53248) | $D001 (53249) |
| #1 | $D002 (53250) | $D003 (53251) |
| #2 | $D004 (53252) | $D005 (53253) |
| #3 | $D006 (53254) | $D007 (53255) |
| #4 | $D008 (53256) | $D009 (53257) |
| #5 | $D00A (53258) | $D00B (53259) |
| #6 | $D00C (53260) | $D00D (53261) |
| #7 | $D00E (53262) | $D00F (53263) |

X coordinates need 9 bits; the MSBs are in $D010 (one bit per sprite).

## Sprite color registers

| Sprite | Color |
|--------|-------|
| #0 | $D027 |
| #1 | $D028 |
| #2 | $D029 |
| #3 | $D02A |
| #4 | $D02B |
| #5 | $D02C |
| #6 | $D02D |
| #7 | $D02E |

Multicolor shared colors: $D025, $D026.

## Sprite enable ($D015)

Each bit is a switch for that sprite. 1 = enabled, 0 = hidden.
Bit 0 = sprite #0, bit 7 = sprite #7.

## Sprite multicolor mode ($D01C)

Each bit selects multicolor (1) or hires (0) for that sprite.

## X expansion ($D01D), Y expansion ($D017)

Each bit doubles that sprite's size in the corresponding direction.

## Sprite priority ($D01B)

Each bit, when set, puts the sprite *behind* the background graphics (text/bitmap). When 0, sprite is in front. Sprite-sprite priority is hardwired: lower-numbered sprites always appear in front of higher-numbered ones.

## Collision detection

$D01E: sprite-sprite collision (1 bit per sprite). Read clears.
$D01F: sprite-data (background) collision. Read clears.
$D019 bit 2 = sprite-sprite IRQ flag, bit 1 = sprite-data IRQ flag.
$D01A bits enable these IRQs.

## Example: enable sprite #0 with white color

```basic
POKE 2040,13: REM pattern at 13*64=832
POKE 53248,255: REM X=255
POKE 53249,100: REM Y=100
POKE 53287,1: REM color=1 (white)
POKE 53269,1: REM enable sprite #0
```
