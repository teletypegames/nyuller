# Hardware Internals of the C64

Source: https://www.c64-wiki.com/wiki/Hardware_internals_of_the_C64

The original C64 ("breadbox") mainboard (KU-14194HB) has these major components:

- MOS 6510 CPU
- MOS 6567 (NTSC) or 6569 (PAL) VIC-II video chip
- MOS 6581 SID sound chip
- Two MOS 6526 CIA I/O chips
- 64 KB dynamic RAM (8× 64K×1 chips) for main memory
- 0.5 KB static RAM (1K×4, typically 2114) for color RAM
- 16 KB ROM (BASIC + KERNAL)
- 4 KB ROM character generator
- A PLA (Programmable Logic Array) — Signetics 82S100 in early boards, then mask-programmed NMOS (906114-01), then SuperPLA (251715-01), then integrated "Memory Controller" (252535-01)
- 74-series glue logic and discrete transistors for video / cassette / power

## Bus architecture

The CPU has 16-bit address bus, 8-bit data bus. The VIC has 14-bit address bus, 12-bit data bus (8 normal + 4 to color RAM). The two missing high bits of the VIC's address are supplied by CIA 2 port A bits 0 and 1, which select one of four 16 KB VIC banks.

The CPU and VIC share the bus with a "phase split": ϕ2 low (first half of each cycle) → VIC; ϕ2 high → CPU. The VIC and CPU alternate automatically.

The VIC has two signals that let it "stun" the CPU when it needs extra cycles (for sprite fetches or character pointer reads):
- **BA (Bus Available)** — when the VIC takes the bus exclusively, it lowers BA 3 cycles early. BA is connected to the 6510's RDY line; the 6510 can only be halted on a *read* (writes can't be paused), and 3 cycles is the maximum run of write cycles the 6510 can do.
- **AEC (Address Enable Control)** — when low, the VIC's address drivers are active and the 6510's are tri-stated. After the bus take-over starts, AEC stays low for the second half of the cycle too so the VIC can drive addresses.

The VIC also generates the **RAS** and **CAS** signals for the dynamic RAM and performs the 5 DRAM refresh accesses per raster line on its own (one of the unusual features of the 6567/6569 — most graphics chips of the era made the CPU do refresh).

## Clock generation

- Y1 crystal: **17.734472 MHz** (PAL) color clock.
- The VIC contains a PLL (U32) that derives an ~7.88 MHz pixel clock (PAL) from the color clock. NTSC ratio is 7:4 instead of 9:4.
- The VIC divides the pixel clock by 8 to make **ϕ0** (~1 MHz, 0.985 MHz PAL / 1.023 MHz NTSC). ϕ0 is an output of the VIC.
- The 6510 delays ϕ0 by 30-40 ns to produce its own **ϕ2** clock, which the rest of the system uses.
- ϕ2=0 → VIC accesses; ϕ2=1 → CPU accesses.
- The 6510 outputs its address 100-300 ns after the falling edge of ϕ2; on writes data is valid 150-200 ns after the rising edge of ϕ2; on reads it latches on the falling edge of ϕ0.
- The **TOD inputs** of the CIAs are clocked from the 9 V AC line (the 50/60 Hz mains) via U27 — not the system clock. (Except on the SX-64, which uses an internal oscillator.)

## PLA

The PLA is the "glue logic" that decides which chip is enabled for any given address access. It looks at A12-A15, the 6510's LORAM/HIRAM/CHAREN, the GAME/EXROM cartridge pins, the VIC's VA14, the bus R/W, and the inverted AEC. From those it generates the chip-select lines: ROMH, ROML, I/O, GR/W (to color RAM), CHAROM, KERNAL, BASIC, and CASRAM.

The 6510 port at $01 plus the GAME/EXROM pins of the cartridge port are how bank switching is done. The full banking matrix is in `PLA - The C64 PLA Dissected` (skoe.de).

## PLA failure

The original bipolar 82S100 PLA and the early NMOS 906114-01 are notorious for failure. Modern replacements include the SuperPLA, realPLA, PLAnkton, PLAtinum, neatPLA, PLA20V8 (GAL-PLA), and EPROM-based replacements. Timing is critical — the new variants are sometimes too fast and can break compatibility with certain cartridges.

## Memory access patterns (normal and badline)

The VIC's "normal" pattern in a raster line, when not a badline and no sprites, is:

```
cycle: 1..14   idle (VIC reads, CPU reads/writes alternate)
       15      start of display?  (depends on RC, VC, DEN)
       16+     g-accesses (character generator reads)
       ...
       58      last g-access
       59..    more idle / sprite accesses
```

On a **badline** (every 8th raster line within the display window when in text/bitmap mode and YSCROLL matches the lower 3 bits of RASTER), the VIC does the additional 40 c-accesses (video matrix reads) which forces the take-over of the bus — that's why the CPU is paused for 40 cycles. Badlines cost the CPU about 40 cycles per text line, which is one of the big reasons raster loops and self-modifying code have to be precisely cycle-counted.
