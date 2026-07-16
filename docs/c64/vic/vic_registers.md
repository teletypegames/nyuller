# VIC-II Register Map ($D000–$D3FF)

Source: https://www.c64-wiki.com/wiki/Page_208-211

Page 208-211 covers the memory locations 53248-54271 ($D000-D3FF). This area is wholly reserved for the VIC-II Registers. The 47 registers are mirrored every 64 bytes in this area.

## Register Table

| Hex  | Dec  | Type     | Bit7 | Bit6 | Bit5 | Bit4 | Bit3 | Bit2 | Bit1 | Bit0 | Contents |
|------|------|----------|------|------|------|------|------|------|------|------|----------|
| $D000 | 53248 | Register |      |      |      |      |      | M0X  |      |      | X Coordinate Sprite 0 |
| $D001 | 53249 | Register |      |      |      |      |      | M0Y  |      |      | Y Coordinate Sprite 0 |
| $D002 | 53250 | Register |      |      |      |      |      | M1X  |      |      | X Coordinate Sprite 1 |
| $D003 | 53251 | Register |      |      |      |      |      | M1Y  |      |      | Y Coordinate Sprite 1 |
| $D004 | 53252 | Register |      |      |      |      |      | M2X  |      |      | X Coordinate Sprite 2 |
| $D005 | 53253 | Register |      |      |      |      |      | M2Y  |      |      | Y Coordinate Sprite 2 |
| $D006 | 53254 | Register |      |      |      |      |      | M3X  |      |      | X Coordinate Sprite 3 |
| $D007 | 53255 | Register |      |      |      |      |      | M3Y  |      |      | Y Coordinate Sprite 3 |
| $D008 | 53256 | Register |      |      |      |      |      | M4X  |      |      | X Coordinate Sprite 4 |
| $D009 | 53257 | Register |      |      |      |      |      | M4Y  |      |      | Y Coordinate Sprite 4 |
| $D00A | 53258 | Register |      |      |      |      |      | M5X  |      |      | X Coordinate Sprite 5 |
| $D00B | 53259 | Register |      |      |      |      |      | M5Y  |      |      | Y Coordinate Sprite 5 |
| $D00C | 53260 | Register |      |      |      |      |      | M6X  |      |      | X Coordinate Sprite 6 |
| $D00D | 53261 | Register |      |      |      |      |      | M6Y  |      |      | Y Coordinate Sprite 6 |
| $D00E | 53262 | Register |      |      |      |      |      | M7X  |      |      | X Coordinate Sprite 7 |
| $D00F | 53263 | Register |      |      |      |      |      | M7Y  |      |      | Y Coordinate Sprite 7 |
| $D010 | 53264 | Register | M7X8 | M6X8 | M5X8 | M4X8 | M3X8 | M2X8 | M1X8 | M0X8 | MSBs of X coordinates |
| $D011 | 53265 | Register | RST8 | ECM  | BMM  | DEN  | RSEL | YSCROLL |   |      | Control register 1 |
| $D012 | 53266 | Register |      | RASTER       |      |      |      |      |      |      | Raster counter |
| $D013 | 53267 | Register |      | LPX          |      |      |      |      |      |      | Light pen X |
| $D014 | 53268 | Register |      | LPY          |      |      |      |      |      |      | Light pen Y |
| $D015 | 53269 | Register | M7E  | M6E  | M5E  | M4E  | M3E  | M2E  | M1E  | M0E  | Sprite enabled |
| $D016 | 53270 | Register | -    | -    | RES  | MCM  | CSEL | XSCROLL |  |      | Control register 2 |
| $D017 | 53271 | Register | M7YE | M6YE | M5YE | M4YE | M3YE | M2YE | M1YE | M0YE | Sprite Y expansion |
| $D018 | 53272 | Register | VM13 | VM12 | VM11 | VM10 | CB13 | CB12 | CB11 | -    | Memory pointers |
| $D019 | 53273 | Register | IRQ  | -    | -    | -    | ILP  | IMMC | IMBC | IRST | Interrupt register |
| $D01A | 53274 | Register | IRQ  | -    | -    | -    | ELP  | EMMC | EMBC | ERST | Interrupt enabled |
| $D01B | 53275 | Register | M7DP | M6DP | M5DP | M4DP | M3DP | M2DP | M1DP | M0DP | Sprite data priority |
| $D01C | 53276 | Register | M7MC | M6MC | M5MC | M4MC | M3MC | M2MC | M1MC | M0MC | Sprite multicolour |
| $D01D | 53277 | Register | M7XE | M6XE | M5XE | M4XE | M3XE | M2XE | M1XE | M0XE | Sprite X expansion |
| $D01E | 53278 | Register | M7M  | M6M  | M5M  | M4M  | M3M  | M2M  | M1M  | M0M  | Sprite-sprite collision |
| $D01F | 53279 | Register | M7D  | M6D  | M5D  | M4D  | M3D  | M2D  | M1D  | M0D  | Sprite-data collision |
| $D020 | 53280 | Register | -    | -    | -    | -    | EC        |      |      |      | Border colour |
| $D021 | 53281 | Register | -    | -    | -    | -    | B0C       |      |      |      | Background colour 0 |
| $D022 | 53282 | Register | -    | -    | -    | -    | B1C       |      |      |      | Background colour 1 |
| $D023 | 53283 | Register | -    | -    | -    | -    | B2C       |      |      |      | Background colour 2 |
| $D024 | 53284 | Register | -    | -    | -    | -    | B3C       |      |      |      | Background colour 3 |
| $D025 | 53285 | Register | -    | -    | -    | -    | MM0       |      |      |      | Sprite multicolour 0 |
| $D026 | 53286 | Register | -    | -    | -    | -    | MM1       |      |      |      | Sprite multicolour 1 |
| $D027 | 53287 | Register | -    | -    | -    | -    | M0C       |      |      |      | Sprite 0 colour |
| $D028 | 53288 | Register | -    | -    | -    | -    | M1C       |      |      |      | Sprite 1 colour |
| $D029 | 53289 | Register | -    | -    | -    | -    | M2C       |      |      |      | Sprite 2 colour |
| $D02A | 53290 | Register | -    | -    | -    | -    | M3C       |      |      |      | Sprite 3 colour |
| $D02B | 53291 | Register | -    | -    | -    | -    | M4C       |      |      |      | Sprite 4 colour |
| $D02C | 53292 | Register | -    | -    | -    | -    | M5C       |      |      |      | Sprite 5 colour |
| $D02D | 53293 | Register | -    | -    | -    | -    | M6C       |      |      |      | Sprite 6 colour |
| $D02E | 53294 | Register | -    | -    | -    | -    | M7C       |      |      |      | Sprite 7 colour |
| $D02F-$D03F | 53295-53311 | Unused | ($FF on read, ignored on write) |   |      |      |      |      |      |      | |
| $D040-$D3FF | 53312-54271 | Same as $D000-$D03F (mirror every 64 bytes) | | | | | | | | | |

## Notes

- The bits marked with '-' are not connected and give "1" on reading.
- The registers $D01E and $D01F cannot be written and are automatically cleared on reading.
- The RES bit (bit 5) of register $D016 has no function on the VIC 6567/6569. On the 6566, this bit is used to stop the VIC.
- Bit 7 in register $D011 (RST8) is bit 8 of register $D012. Together they are called "RASTER". A write access to these bits sets the comparison line for the raster interrupt.
</content>
</invoke>