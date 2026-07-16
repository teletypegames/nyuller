# Programming the Commodore 64 — A Low-Level Overview

Notes from studying the downloaded C64 reference docs in `./docs/c64/`
(C64-Wiki, Christian Bauer's VIC-II article, the Commodore 64 Programmer's
Reference Guide, and the Oscar64 runtime headers). This is the practical
"how do I actually program the thing" companion to `OSCAR64.md`.

## 0. The mental model in one paragraph

The C64 is a 6510 (6502 plus an 8-bit I/O port) running at ~1 MHz, sharing
the bus with a 6567/6569 VIC-II graphics chip and a 6581 SID sound chip.
The VIC and CPU alternate bus cycles automatically; the VIC can "stun" the
CPU when it needs extra cycles. Two 6526 CIAs handle I/O and timers. The
whole 64 KB address space is overlaid with RAM, ROM, I/O, and Color RAM in
a swizzled fashion controlled by the 6510's $01 port plus the PLA. The
KERNAL ROM at $E000-$FFFF is always present at reset and provides 39
service routines via a jump table at $FF81-$FFF3. There is no memory
protection. To make the chip set do interesting things, you write to its
registers — for the VIC that's $D000-$D3FF, for the SID $D400-$D7FF, for
the CIAs $DC00-$DCFF and $DD00-$DDFF. Everything visible on screen is
driven by writes to these registers plus writes to the 1 KB of "screen
memory" at $0400-$07FF and the 1/2 KB of "color RAM" at $D800-$DBFF.

## 1. The hardware

```
                +-------+   +-------+   +-------+    +--------+
                | BASIC |   |KERNAL |   | CHAR  |    |  8x8K  |
                |  ROM  |   |  ROM  |   |  ROM  |    |  RAM   |
                +--++---+   +--++---+   +--++---+    +--++---+
                   ||          ||         ||            ||
            +------+||+++++++++||+++++++++||++++++++++++||+++++
            |  PLA |+>------------>------------>----------- --->|  (chip selects)
            +--+---+                                       |
               |                                          |
        +------+------+                              +-----+----+
        |   6510     |                              |   VIC-II  |
        |   CPU      |<--data/addr----+--------------+   6569   |
        +-----+------+              |  |             +-----+----+
              |                     |  |                   |
              |                     |  |  +--------+        |
              |                     |  +--+ 6581   |        |
              |                     |  |  |  SID   |        |
              |                     |  |  +--------+        |
              |                     |  |                   |
        +-----+-----+         +-----+--+----+        +-----+-----+
        |  6526 CIA1|         | 6526 CIA2  |        |  4-bit    |
        |  (IRQ)    |         |  (NMI)     |        |  Color RAM|
        +-----+-----+         +-----+-------+        +-----------+
              |                     |
   +----------+----------+          |
   |  keyboard, joystick |    +-----+----+
   |  paddles, datasette |    | IEC bus  |
   +---------------------+    |  RS-232  |
                              |  userport|
                              +----------+
```

The 6510's port at `$01` plus the PLA determine which of BASIC, KERNAL, CHAR
ROM, and I/O is visible at $A000-$BFFF, $E000-$FFFF, and $D000-$DFFF
respectively. CIA 2 port A bits 0-1 pick one of four 16 KB "VIC banks" by
extending the VIC's 14-bit address bus to a full 16 bits.

## 2. The bus and the "stun" mechanism

This is the bit that explains every weird timing thing on the C64.

The system clock is one period of ϕ2 (phi2). PAL: ~985 kHz, NTSC: ~1.023 MHz.
The VIC generates the pixel clock (8 pixels per ϕ2 cycle) and divides by 8
to make ϕ0, which the 6510 delays by ~30-40 ns to make ϕ2. Within each ϕ2
cycle, the first half (ϕ2 low) is the VIC's turn and the second half (ϕ2
high) is the CPU's turn. So they normally share the bus cleanly with no
contention.

The VIC needs extra cycles for two things:
1. **C-accesses** — 40 character-pointer reads at the start of each text
   row. These happen on "badlines" (every 8th raster line when the display
   is enabled and `(RASTER & 7) == YSCROLL` for raster in the range $30-$F7).
2. **S-accesses** — 2 sprite data reads per sprite per scanline, when the
   sprite's Y position matches the current raster line.

For these, the VIC pulls BA (Bus Available) low 3 cycles early. BA is wired
to the 6510's RDY line. The 6510 can only be paused on a *read* (writes
can't be paused — that's why the VIC waits up to 3 cycles: 3 is the
maximum run of consecutive writes the 6510 can do). When AEC is then held
low for the second half of a cycle too, the VIC owns the bus exclusively.

The 6510's actual data/address timing from the datasheet, relative to ϕ2:
- Address valid: 100-300 ns after falling edge of ϕ2.
- Write data valid: 150-200 ns after rising edge of ϕ2.
- Read data latched on falling edge of ϕ0.

In practice the C64 is a bit slower than this because the VIC holds AEC
late, so address is valid about 60-75 ns after ϕ2 rising edge and data
another ~120 ns later.

**Why this matters:** any code that depends on cycle-accurate timing
(raster IRQ tricks, floppy timing, raster bars) has to count cycles
*including* badline stalls. The "raster stable" raster line (line 311 on
PAL) is the line right after vertical blank where no badline penalties
have happened recently, so cycle counts on that line are exact.

The VIC also performs 5 DRAM refresh accesses per raster line — that
means you don't have to refresh the 64 KB of main RAM yourself; the VIC
does it. (Pretty unusual for a graphics chip.)

## 3. The 6510

- 6502 plus a 6-/8-bit on-chip I/O port.
- 16-bit address bus, 8-bit data bus.
- Two external interrupts: **IRQ** (maskable via the I flag in P; SEI/CLI)
  and **NMI** (non-maskable; fires on RESTORE key or CIA 2 /FLAG).
- Three vectors in the top six bytes of the address space (KERNAL ROM
  always present at reset):
  - `$FFFA-$FFFB` NMI → `$FE43`
  - `$FFFC-$FFFD` RESET → `$FCE2`
  - `$FFFE-$FFFF` IRQ / BRK → `$FF48`

The 6510 is *object-code-compatible* with the 6502. Same instruction set,
same addressing modes, same cycle counts. The only thing the 6510 adds is
the port.

The port lives at `$00` (data direction, 1=output) and `$01` (data). `$01`
is also the CPU's bank-switch latch. The bit meanings:

| Bit | Name | Effect |
|-----|------|--------|
| 0 | LORAM | 0=RAM at $A000-$BFFF, 1=BASIC ROM |
| 1 | HIRAM | 0=RAM at $E000-$FFFF, 1=KERNAL ROM |
| 2 | CHAREN | 0=CHAR ROM at $D000-$DFFF, 1=I/O (default) |
| 3 | Cassette Data Out |
| 4 | Cassette Switch Sense |
| 5 | Cassette Motor (0=on) |
| 6-7 | Unused |

Default after reset is `$37` (%00110111) — everything visible, cassette
motor off.

To run with no ROMs (for a self-contained .prg that owns all memory), set
`$01` to `$34` (or thereabouts depending on whether you need CHAR ROM).
The Oscar64 runtime does exactly this kind of bank switching in its crt.c
when `-rt=` is given, and the oscar64 `mmap_set` API exposes it.

There is no "address 0/1 in RAM". The 6510 port occupies those two
addresses. To read the underlying RAM, you have to use a VIC trick
(corrupting the datassette buffer at $02 and using the VIC's read
prefetch), but you almost never need to.

## 4. The memory map (after reset, no cartridge)

| Address | Contents |
|---------|----------|
| $0000-$00FF | Zeropage (see §5) |
| $0100-$01FF | Hardware stack (and a few KERNAL scratch bytes at the bottom) |
| $0200-$02FF | BASIC / KERNAL pointers |
| $0300-$03FF | More KERNAL/BASIC pointers; **$0314-$0315 = IRQ vector, $0316-$0317 = BRK vector, $0318-$0319 = NMI vector** |
| $0400-$07FF | Screen RAM (1000 bytes for the 40×25 text screen) |
| $0800-$9FFF | Free BASIC program storage (38911 bytes) |
| $A000-$BFFF | BASIC ROM (8 KB, visible iff LORAM=1) |
| $C000-$CFFF | Free for ML programs |
| $D000-$D3FF | VIC-II registers (47 of them, mirrored every 64 bytes) |
| $D400-$D7FF | SID registers (mirrored every 32 bytes; not on C128) |
| $D800-$DBFF | Color RAM (1000 nibbles, 4-bit wide) |
| $DC00-$DCFF | CIA 1 |
| $DD00-$DDFF | CIA 2 |
| $DE00-$DFFF | I/O expansion / open bus |
| $E000-$FFFF | KERNAL ROM (8 KB, visible iff HIRAM=1) |

Two important properties:

- **Write-through to RAM under ROM.** If a write targets an address where
  ROM is currently visible, the read still goes to the ROM but the data
  is written to the underlying RAM. This is how a .prg loaded at $0801
  can be "installed" — the kernal loads it at the standard BASIC start
  address $0801, writes a 0 to $01 to hide ROMs, then JMPs to the entry
  point. The RAM at $A000-$BFFF and $E000-$FFFF is still 8 KB of usable
  space once you bank it in. (But you have to write a custom ISR vector
  because the KERNAL ISR is in $EA31, which is in KERNAL ROM.)

- **Color RAM upper nibble is open bus.** Color RAM is a 4-bit-wide
  2114 SRAM, so reads of $D800-$DBFF return the low nibble as a valid
  color, but the high nibble is "random" (often it reflects the last
  byte the VIC read, due to bus capacitance).

For full detail see `docs/c64/memory/memory_map.md`.

## 5. Zero page

Every 6502 has 256 bytes of "page 0" with a special addressing mode that
is 1 byte shorter and 1 cycle faster than the equivalent absolute mode.
The C64 puts the 6510 port at $00/$01, the floating-point accumulators at
$61-$6E, the BASIC interpreter state at $2B-$8F, the keyboard buffer
state at $C5-$D7, and a ton of other shared state through the rest of the
page. The hardware stack (which the 6502 grows downward) lives in
$0100-$01FF, so $013F-$01FF is the actual usable stack.

For ML work the "free" scratch locations are very few. A common pattern:
a few variables at $FB-$FE (which the KERNAL doesn't touch except for
$FF in the float-to-ASCII routine), and the area $C0-$C4 is free when
no cassette or serial I/O is in progress.

For the full table of every zero-page address, see
`docs/c64/memory/zeropage.md`.

The Oscar64 runtime uses its own scheme (it dedicates a zero-page window
to its software stack, e.g. $F7-$FF on a C64, and shifts it around for
other targets — see `Compiler.cpp` lines 57-105 for the ZP register
allocation logic).

## 6. The VIC-II: how the screen actually gets drawn

The VIC has:
- A 14-bit address bus (16 KB of address space). The high 2 bits come
  from CIA 2 port A, picking one of 4 banks.
- A 12-bit data bus (8 normal + 4 directly to color RAM).
- A 320×200 pixel display generated 8 pixels at a time (1 character cell).
- 16 colors, fixed palette (see below).
- 8 sprites (MOBs), 24×21 each (12×21 in multicolor).
- 5 DRAM refresh accesses per line.
- A raster counter you can read at $D012 (low 8 bits; the 9th bit is
  bit 7 of $D011).

### 6.1 The display window and border

The VIC paints within a fixed "display window" surrounded by a border in
the color stored in $D020. The window dimensions are selected by
RSEL (bit 3 of $D011) and CSEL (bit 3 of $D016):

| RSEL | Window height | First line | Last line |
|------|---------------|------------|-----------|
| 0    | 24 rows (192 px) | 55 ($37) | 246 ($F6) |
| 1    | 25 rows (200 px) | 51 ($33) | 250 ($FA) |

| CSEL | Window width | First X | Last X |
|------|--------------|---------|--------|
| 0    | 38 cols (304 px) | 31 ($1F) | 334 ($14E) |
| 1    | 40 cols (320 px) | 24 ($18) | 343 ($157) |

The YSCROLL (bits 0-2 of $D011) and XSCROLL (bits 0-2 of $D016) shift the
*content* of the window in 1-pixel increments, which is how smooth
scrolling works. (Set RSEL=1, CSEL=1, and XSCROLL=YSCROLL=0 to get the
canonical 40×25 layout, or XSCROLL=YSCROLL=7 for 38×24, etc.)

DEN (bit 4 of $D011) is the "display enable" master switch. If DEN=0 the
VIC still runs but doesn't actually display anything.

### 6.2 Badlines and bus stalls

A "badline" is any line on which the VIC has to do the 40-character
pointer read. By default that happens on the first raster line of each
text row, i.e. every 8th line. The exact definition (from Christian
Bauer's article):

> A Bad Line Condition is given at any arbitrary clock cycle if, at the
> negative edge of ϕ0 at the beginning of the cycle, RASTER >= $30 and
> RASTER <= $f7 and the lower three bits of RASTER are equal to YSCROLL,
> and if the DEN bit was set during an arbitrary cycle of raster line $30.

That means you can trigger a badline manually by changing YSCROLL mid-line
— this is the basis of FLI/AFLI/NUFLI, the demo scene hacks that get
more colors per cell.

When a badline fires, the VIC "stuns" the CPU for 40 cycles (plus the
3-cycle setup), then does 40 c-accesses to read the video matrix. That's
the 40-cycle-per-text-row overhead that the CPU pays for graphics.

### 6.3 The 16 colors

The 16-color palette is hard-wired in the VIC (the VIC generates color
from the phase and amplitude of a signal derived from a 14.3/17.7 MHz
color clock):

| Code | Color    | Code | Color       |
|------|----------|------|-------------|
| 0    | Black    | 8    | Orange      |
| 1    | White    | 9    | Brown       |
| 2    | Red      | 10   | Light red   |
| 3    | Cyan     | 11   | Dark gray   |
| 4    | Pink     | 12   | Medium gray |
| 5    | Green    | 13   | Light green |
| 6    | Blue     | 14   | Light blue  |
| 7    | Yellow   | 15   | Light gray  |

### 6.4 The 5 legal graphics modes

The VIC is set into one of 5 legal "modes" by three bits:

- **ECM** (Extended Color Mode) — bit 6 of $D011
- **BMM** (Bitmap Mode) — bit 5 of $D011
- **MCM** (Multicolor Mode) — bit 4 of $D016

| Mode | ECM BMM MCM | Result |
|------|-------------|--------|
| 0    | 0   0   0   | Standard Character Mode (40×25 text) |
| 1    | 0   0   1   | Multicolor Character Mode |
| 2    | 0   1   0   | Standard Bitmap Mode (hires, 320×200) |
| 3    | 0   1   1   | Multicolor Bitmap Mode (160×200) |
| 4    | 1   0   0   | Extended Background Color Mode |

Modes 5-7 are technically possible but produce no visible output.

In Standard Character Mode:
- Screen memory at $0400-$07FF (40×25 = 1000 bytes) holds character codes
  (PETSCII values 0-255, but the character generator only has 256 chars so
  effectively 0-255).
- Color RAM at $D800-$DBE7 (1000 nibbles, low 4 bits) holds the color of
  each character cell, drawn from the 16-color palette.
- One global background color in $D021.
- Character patterns in the char generator (default 4 KB at $D000-$DFFF
  in banks 0/2; can be relocated by changing $D018 to point to RAM at
  $1000-aligned addresses).

In Bitmap Mode:
- 8000 bytes of bitmap at the address pointed to by bits VM13-VM10 of
  $D018 (in 8 KB steps). Each bit is one pixel.
- 1000 bytes of "screen memory" at the address pointed to by bits VM13-VM10
  in 1 KB steps. The screen memory holds color information: the low 4
  bits are the cell's foreground color, the high 4 bits are the cell's
  background color *index* (0-3, picking one of $D021-$D024). When MCM=1,
  the 2 bits per pixel pick from $D021/$D022/$D023/color RAM.

### 6.5 Sprites

8 sprites (MOBs), each:
- 24×21 pixels (12×21 in multicolor).
- Data: 63 bytes + 1 unused pad = 64 bytes, must be 64-byte aligned in the
  current VIC bank.
- Position: X (9 bits — low 8 in $D000/$D002/.../$D00E, bit 8 in $D010
  one bit per sprite) and Y (8 bits in $D001/$D003/.../$D00F).
- Color: $D027-$D02E (one per sprite); shared multicolor colors at
  $D025 (MC0), $D026 (MC1).
- Pointer: at $07F8-$07FF (one byte per sprite; contains the pattern
  address divided by 64).
- Enabled: $D015 (one bit per sprite).
- Mode (hires or multicolor): $D01C (one bit per sprite).
- X/Y expansion: $D01D (X) and $D017 (Y), one bit per sprite.
- Priority vs background: $D01B (one bit per sprite, 1 = behind
  background).
- Sprite-sprite priority is hardwired: lower-numbered sprite is in front.
- Collision detection: $D01E (sprite-sprite, read clears) and $D01F
  (sprite-data, read clears). Bits 1 and 2 of $D019 / $D01A are the
  collision IRQ flag and enable.

The "trick" of showing more than 8 sprites is to use raster IRQs to
re-load the sprite pointers and Y positions mid-frame, presenting a
different set of up to 8 sprites on each part of the screen. With careful
timing you can show 24+ sprites on a single line.

### 6.6 Raster interrupt

The raster IRQ is the heart of all the C64 graphics tricks. The hardware
fires IRQ whenever the 9-bit raster counter (RASTER in $D012 plus bit 7
of $D011) equals the 9-bit value last written to those same bits.

The standard setup is:

```asm
Init    SEI
        LDA #%01111111
        STA $DC0D               ; mask CIA 1 IRQs

        AND $D011               ; clear bit 7 of $D011 (RST8)
        STA $D011

        STA $DC0D               ; ack CIA 1
        STA $DD0D               ; ack CIA 2

        LDA #<raster_line       ; raster line low
        STA $D012
        LDA #<Irq
        STA $0314               ; IRQ vector
        LDA #>Irq
        STA $0315

        LDA #%00000001
        STA $D01A               ; enable raster IRQ in VIC

        CLI
        RTS

Irq     ; do stuff
        ASL $D019               ; ack: clear bit 0 of $D019
        JMP $EA31               ; chain into KERNAL ISR (or $EA81 to skip)
```

The 8-bit "raster line 311" trick (writing a specific YSCROLL value
during certain ranges of the raster counter to force extra badlines) is
the basis of FLI/AFLI/NUFLI.

Stable raster: raster line 311 on PAL is the one that gives the cleanest,
most predictable cycle counts because no recent badline penalty is in
effect. Writing to $D012 on this line will trigger the IRQ exactly
63 cycles later.

For the rest of the line the timing is stable too; Christian Bauer's
article has the full per-cycle timing table.

## 7. The SID

The 6581 (early C64s) or 8580 (C64C) is a 3-voice analog synthesizer
chip designed by Bob Yannes. The 8580 is "cleaner" but lacks the bugs
of the 6581 that people used for tricks like 4-bit sample playback.

Each voice has:
- 16-bit frequency ($D400/$D401, $D407/$D408, $D40E/$D410).
- 12-bit pulse width ($D402/$D403, $D409/$D40A, $D410/$D411).
- Control register $D404 / $D40B / $D412: gate, sync, ring mod, test,
  triangle, saw, pulse, noise.
- ADSR envelope in $D405/$D406 etc. (4-bit attack, 4-bit decay, 4-bit
  sustain, 4-bit release).

Shared:
- Filter cutoff at $D415/$D416 (low 3 bits + 8 bits).
- Filter routing/resonance at $D417 (high 4 bits = resonance, low 4 bits
  = enable ext/v3/v2/v1).
- Filter mode and master volume at $D418 (bit 6 = mute V3, bit 5 = HP,
  bit 4 = BP, bit 3 = LP, bits 0-3 = volume 0-15).
- Read-only: $D419 paddle X, $D41A paddle Y, $D41B oscillator V3, $D41C
  envelope V3.

Classic 4-bit sample playback on the 6581: rapidly write the high nibble
of $D418 to output 4-bit sample values. The 8580 fixed this "feature" so
samples are very quiet; restoring it requires a 470kΩ-1MΩ resistor on
EXT IN to GND.

## 8. The CIAs

Two 6526 CIAs. CIA 1 drives the keyboard, joysticks, paddles, and
datasette. CIA 2 drives the serial (IEC) bus, RS-232, the userport, and
provides the VIC bank bits (lower 2 of port A).

Each CIA has:
- Two 16-bit timers (A and B). Timer A is normally used by the KERNAL
  IRQ for the jiffy clock.
- 24-hour TOD clock with alarm (B.C.D format).
- 8-bit serial shift register.
- 16 GPIO lines (PA0-PA7, PB0-PB7), with separate DDR.
- Interrupt control register (ICR) with sources from timers, TOD, serial,
  FLAG pin.

The CIA is at $DC00 (CIA 1) and $DD00 (CIA 2); each is 16 bytes, mirrored
every 16 bytes in its 256-byte page.

### 8.1 Joystick reading

The joystick switches are wired in parallel with the keyboard matrix.
Reading $DC00 (PRA, port 2 / left) and $DC01 (PRB, port 1 / right) gives
a byte where the low 5 bits are active-low direction + fire. The
high 3 bits are keyboard column bits. Rest position, no buttons, is $7F
(127) on port 2 and $FF (255) on port 1.

Important: in the C64, the CIA 1 IRQ line is the IRQ for the *whole CPU*.
If you don't want CIA 1 timer A to fire IRQ (which is the default and
triggers the jiffy-clock-and-keyboard-scan KERNAL handler), mask it via
`$DC0D` (the ICR) at the start of your raster-IRQ setup.

## 9. The KERNAL

39 service routines, jump table at $FF81-$FFF3. The most useful ones for
low-level work:

| Use | Call |
|-----|------|
| Print a character | `JSR $FFD2` (CHROUT, A = char) |
| Read a character | `JSR $FFCF` (CHRIN) — A = char on return |
| Test for STOP key | `JSR $FFE1` (STOP) — carry set if STOP pressed |
| Open a file | `JSR $FFC0` (OPEN) — A=LA, X=FA, Y=SA |
| Close a file | `JSR $FFC3` (CLOSE) — A=LA |
| Set filename | `JSR $FFBD` (SETNAM) — A=length, X/Y=ptr |
| Set logical file | `JSR $FFBA` (SETLFS) — A=LA, X=FA, Y=SA |
| Load | `JSR $FFD5` (LOAD) — .A=0 load, .A=1 verify |
| Save | `JSR $FFD8` (SAVE) — A=zpage pointer, X/Y=end ptr |
| Read clock | `JSR $FFDE` (RDTIM) — A/X/Y = jiffy/seconds/min |
| Set clock | `JSR $FFDB` (SETTIM) — A/X/Y = same |
| Plot (get/set cursor) | `JSR $FFF0` (PLOT) — carry=set, X=row, Y=col |

Oscar64 wraps these in its runtime. `printf("hello")` ultimately calls
CHROUT in a loop; `getch()` calls CHRIN; `oscar_kernalio.h` provides the
serial-bus file routines.

## 10. The interrupt model

### 10.1 Hardware vectors

In the top 6 bytes of KERNAL ROM (always mapped at reset, in the top of
the 64 KB address space):

- `$FFFA-$FFFB` NMI
- `$FFFC-$FFFD` RESET
- `$FFFE-$FFFF` IRQ / BRK

The KERNAL entry points then read indirect RAM vectors:

- `$0314-$0315` IRQ (default → $EA31, the KERNAL standard IRQ)
- `$0316-$0317` BRK (used for the BASIC `BRK` instruction in monitor-style use)
- `$0318-$0319` NMI (default → $FE47)

### 10.2 The KERNAL IRQ handler

When an IRQ fires, the 6510 hardware pushes P and PC and jumps via
($FFFE) to $FF48. $FF48 pushes A, X, Y onto the stack and then indirect-
jumps via $0314 (for IRQ) or $0316 (for BRK). The default $0314 → $EA31
runs the standard KERNAL ISR which:
1. Reads CIA 1 ICR to find the source.
2. If timer A: increments the jiffy clock at $A0-$A2.
3. Scans the keyboard.
4. Handles the cursor blink.
5. Checks for RUN/STOP.
6. Exits via $EA81 which pulls A/X/Y and `RTI`.

The stack at the start of a user IRQ handler is (top to bottom):
PC_hi, PC_lo, P, A, X, Y.

The fastest way to do an IRQ that does work but still gets the KERNAL
service: end with `JMP $EA31`. The fastest way to do an IRQ that needs
no KERNAL service at all (e.g. a music player): end with `JMP $EA81`.

### 10.3 NMI

NMI is wired to CIA 2's FLAG pin, which is the RESTORE key. The KERNAL's
NMI handler at $FE47 is also the "soft reset" entry: if RUN/STOP is held
when RESTORE is pressed, the system returns to BASIC without losing the
program in memory. To disable the soft-reset, mask CIA 2 interrupts
($DD0D).

## 11. The character set and PETSCII

PETSCII is the C64's character encoding — not ASCII. Printable range is
$20-$7E and $A0-$FE (with some overlap differences). CR ($0D) is the
line terminator, not LF. There's an upper/lowercase font switch:
$00-$1F are graphics characters in uppercase mode; $00-$1F are
uppercase letters in lowercase mode (the "shifted" font).

The two font variants:
- "Uppercase" / graphics mode: PETSCII $00-$1F → graphics chars
- "Lowercase" mode: PETSCII $00-$1F → uppercase letters

Switch with `iocharmap(IOCHM_PETSCII_2)` in Oscar64. In assembly: KERNAL
call `CHROUT` with $0E to set uppercase+graphics, $8E for lowercase.

In Oscar64 you can mark string literals with `p""` for PETSCII or `s""`
for screen code (the byte values used directly in screen memory, which
are the same as the character set ROM offsets).

## 12. Putting it all together: a tiny "hires clear" example

In C, with Oscar64:

```c
#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>

char Color[1000];
char Hires[8000];
Bitmap Screen;

int main(void) {
    mmap_trampoline();
    mmap_set(MMAP_RAM);              // bank out BASIC+KERNAL

    memset(Color, 0x01, 1000);       // white-on-black
    memset(Hires, 0x00, 8000);       // clear bitmap

    mmap_set(MMAP_NO_ROM);           // also bank out the I/O CHAR ROM
                                     //  (so hires memory is visible)
    vic_setmode(VICM_HIRES, Color, Hires);
    vic.color_border = VCOL_WHITE;
    bm_init(&Screen, Hires, 40, 25);

    getch();

    mmap_set(MMAP_ROM);              // put ROMs back
    vic_setmode(VICM_TEXT, (char*)0x0400, (char*)0x1000);
    return 0;
}
```

What this exercises:
- Bank switching: `mmap_set` writes to $01 (and triggers the trampoline
  for IRQs).
- Absolute addressing: `char *Hires = (char*)0xe000` is the
  straightforward way to reach video memory, made possible by
  `mmap_set(MMAP_RAM)` which gives us $E000-$FFFF as 8 KB of RAM (with
  the CHAR ROM not at $D000 anymore, because CHAREN=0 maps CHAR ROM
  there).
- VIC control: `vic_setmode` writes ECM/BMM/MCM, $D018 (video matrix
  and bitmap pointers), $D016 (CSEL=1, XSCROLL=0), and the screen
  memory pointer (set via CIA 2 PRA low 2 bits, plus $DD00 writes).

## 13. Things to remember

- The VIC and CPU share the bus. Badlines cost 40 cycles every 8 lines
  in text mode. Cycle-counting code on other lines gives you one cycle
  per line, but the badline penalty can blow your timing.
- Writes to ROM addresses go to underlying RAM. This is how .prg files
  install themselves and how you can put code at $A000 without banking
  out BASIC (just write to the right places, but BASIC will still run if
  you don't bank it out).
- The 6510 I/O port at $00/$01 is the master bank switch. Three bits
  (LORAM, HIRAM, CHAREN) plus the PLA. The Oscar64 `mmap_set` API
  exposes this as `MMAP_NO_BASIC`, `MMAP_RAM`, `MMAP_NO_ROM`, etc.
- The VIC raster counter is 9 bits. Always write to $D011 bit 7 (RST8)
  and $D012 together to set the IRQ trigger line. Reading them also
  gives you 9 bits of current raster.
- The KERNAL ISR at $EA31 does useful work (jiffy clock, keyboard scan,
  cursor blink) but you can call $EA81 to skip it. For a music player
  running in IRQ, the trick is to keep $EA31 alive by patching $0314 to
  your routine, ending with `JMP $EA31`.
- PETSCII != ASCII. Use `p""` or `s""` prefixes in Oscar64, or `-psci`
  on the command line. Use the KERNAL `CHROUT` ($FFD2) and `CHRIN` ($FFCF)
  for character I/O so the translation happens.
- The Color RAM upper nibble is meaningless. Only the low 4 bits are
  connected.
- Sprites need to be 64-byte aligned in the current VIC bank. The
  pointers at $07F8-$07FF are byte values, pattern address / 64.
- The SID's 6581 has bugs that the 8580 doesn't. If you want 4-bit
  sample playback, you need a 6581 (or simulate the bug).
- The CIAs' FLAG pins are NMI/IRQ sources. The RESTORE key triggers an
  NMI through CIA 2. If you want RUN/STOP+RESTORE to be a no-op, mask
  CIA 2 NMI in $DD0D.

## 14. Where to look for more

- `docs/c64/memory/memory_map.md` — every address
- `docs/c64/memory/zeropage.md` — every zero-page address
- `docs/c64/memory/hardware_internals.md` — block diagram and bus
- `docs/c64/vic/vic_registers.md` — every VIC register
- `docs/c64/vic/graphics_modes.md` — the 5 official modes
- `docs/c64/vic/cebix-vic-article.txt` — Christian Bauer's VIC paper
  (the canonical timing reference, 80 pages, dense)
- `docs/c64/sprites/sprites_overview.md` — sprite programming
- `docs/c64/interrupts/raster_interrupt.md` — how to set up a raster IRQ
- `docs/c64/interrupts/interrupts_overview.md` — IRQ/NMI/BRK flow
- `docs/c64/cia/cia_overview.md` — CIA 1 + CIA 2 register maps
- `docs/c64/sid/sid_overview.md` — SID register map, ADSR, filter
- `docs/c64/kernal/kernal_jumptable.md` — every KERNAL entry point
- `docs/c64/kernal/c64_programmers_reference_guide.txt` — the full
  500-page official Commodore 64 Programmer's Reference Guide (1982)

The PRG is dense and a bit old-fashioned (it's 1982) but it's the
definitive reference and includes the full instruction set with cycle
counts. Christian Bauer's VIC article is the modern, exhaustive
treatment of the VIC-II — section 3.14 ("Effects and applications") is
where the FLI/AFLI/NUFLI/etc. tricks are explained.
