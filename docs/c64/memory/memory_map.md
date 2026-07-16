# C64 Memory Map

Source: https://www.c64-wiki.com/wiki/Memory_Map

The following article shows a short overview of the C64 memory map (pages and memory addresses) as seen by its CPU. The address space may look different from the view of other chips such as the VIC.

This overview shows the status after power on of the C64 in the standard memory configuration ($37/55 in memory address $01, no cartridge).

Detailed descriptions of every memory area can be found in the associated articles. The memory management is implemented mostly by the C64 PLA.

## RAM Table

| Hex Address | Dec Address | Page | Contents |
|-------------|-------------|------|----------|
| $0000-$00FF | 0-255 | Page 0 | Zeropage addressing |
| $0100-$01FF | 256-511 | Page 1 | Enhanced Zeropage contains the stack |
| $0200-$02FF | 512-767 | Page 2 | Operating System and BASIC pointers |
| $0300-$03FF | 768-1023 | Page 3 | Operating System and BASIC pointers |
| $0400-$07FF | 1024-2047 | Page 4-7 | Screen Memory |
| $0800-$9FFF | 2048-40959 | Page 8-159 | Free BASIC program storage area (38911 bytes) |
| $A000-$BFFF | 40960-49151 | Page 160-191 | Free machine language program storage area (when switched-out with ROM) |
| $C000-$CFFF | 49152-53247 | Page 192-207 | Free machine language program storage area |
| $D000-$D3FF | 53248-54271 | Page 208-211 | VIC-II registers |
| $D400-$D7FF | 54272-54527 | Page 212-215 | SID registers |
| $D800-$DBFF | 55296-56319 | Page 216-219 | Color RAM |
| $DC00-$DCFF | 56320-56575 | Page 220 | CIA 1 |
| $DD00-$DDFF | 56576-56831 | Page 221 | CIA 2 |
| $DE00-$DFFF | 56832-57343 | Page 222-223 | Reserved for interface extensions |
| $E000-$FFFF | 57344-65535 | Page 224-255 | Free machine language program storage area (when switched-out with ROM) |

## ROM Table

| Hex Address | Dec Address | Page | Contents |
|-------------|-------------|------|----------|
| $8000-$9FFF | 32768-40959 | Page 128-159 | Cartridge ROM (low) |
| $A000-$BFFF | 40960-49151 | Page 160-191 | BASIC interpretor ROM or cartridge ROM (high) |
| $D000-$DFFF | 53248-57343 | Page 208-223 | Character generator ROM |
| $E000-$FFFF | 57344-65535 | Page 224-255 | KERNAL ROM or cartridge ROM (high) |

## I/O Table

| Hex Address | Dec Address | Page | Contents |
|-------------|-------------|------|----------|
| $0000-$0001 | 0-1 | - | CPU I/O port - see Zeropage |
| $D000-$D3FF | 53248-54271 | Page 208-211 | VIC-II registers |
| $D400-$D7FF | 54272-55295 | Page 212-215 | SID registers |
| $D800-$DBFF | 55296-56319 | Page 216-219 | Color Memory |
| $DC00-$DCFF | 56320-56575 | Page 220 | CIA 1 |
| $DD00-$DDFF | 56576-56831 | Page 221 | CIA 2 |
| $DE00-$DEFF | 56832-57087 | Page 222 | I/O 1 |
| $DF00-$DFFF | 57088-57343 | Page 223 | I/O 2 |

## Notes

- The default configuration is for KERNAL ROM, I/O, BASIC ROM and the remaining RAM banks to be visible to the CPU. All configurations depend upon the state of latch bits set in the Programmable Logic Unit (PLA). The 7 distinct RAM banks are the smallest zones which can be bank switched.
- If ROM is visible to the CPU during a write procedure, the ROM will be read but, any data is written to the underlying RAM. This principle is particularly significant to understanding how the I/O registers are addressed.
- If cartridge ROM is present it can be located in up to three addressable locations. However, only two 8 kByte banks can be seen by the CPU at any time.
- The BASIC program storage space crosses the boundaries of RAM zones, sitting between $0800-$9FFF (38911 BASIC bytes).
