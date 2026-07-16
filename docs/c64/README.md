# Low-level C64 Programming Documentation

This directory contains low-level Commodore 64 programming reference material
downloaded from public sources on the internet. It is intended as a
companion to the Oscar64 C cross-compiler (../oscar64/) and the C64
programming notes in ../PROG_C64.md.

## Contents

| Subdir | Topic |
|--------|-------|
| `cpu/`     | 6510/6502 CPU family |
| `vic/`     | VIC-II video chip, graphics modes, raster timing |
| `cia/`     | 6526 CIA (Complex Interface Adapter) |
| `sid/`     | 6581/8580 SID sound chip |
| `kernal/`  | KERNAL ROM jump table and full Programmer's Reference Guide text |
| `memory/`  | Memory map, zeropage, color RAM, hardware internals |
| `interrupts/` | Raster interrupts, IRQ/NMI/BRK, joysticks |
| `sprites/` | Sprite (MOB) programming |

## Key files

- `memory/memory_map.md` — quick reference for what is at every address
- `memory/zeropage.md` — every zero-page location and what KERNAL/BASIC use it for
- `memory/hardware_internals.md` — block diagram and bus arbitration
- `memory/color_ram.md` — 1/2 KB color memory at $D800
- `vic/vic_registers.md` — full VIC-II register map ($D000-$D3FF)
- `vic/vic_overview.md` — VIC-II chips and features
- `vic/graphics_modes.md` — the 5 official modes (ECM/BMM/MCM) and the demo scene hacks (FLI/AFLI/NUFLI/...)
- `vic/cebix-vic-article.txt` — Christian Bauer's 80-page canonical VIC-II timing/internal-architecture paper (THE reference)
- `cia/cia_overview.md` — CIA 1 + CIA 2 register maps and features
- `sid/sid_overview.md` — SID register map, ADSR, filter, voice control
- `kernal/kernal_jumptable.md` — every KERNAL entry point
- `kernal/c64_programmers_reference_guide.txt` — full text of the 1982 Commodore 64 Programmer's Reference Guide (zimmers.net)
- `interrupts/interrupts_overview.md` — IRQ/NMI/BRK flow, $0314/$0316/$0318 vectors
- `interrupts/raster_interrupt.md` — how to set up a raster IRQ
- `interrupts/joystick.md` — how to read the two control ports
- `sprites/sprites_overview.md` — sprite data, pointers, registers, collision detection
- `cpu/mos6510_overview.md` — 6510 vs 6502, the $00/$01 port, datasheet links

## Sources

- **c64-wiki.com** — the C64-Wiki (GFDL-licensed) — most of the markdown files
- **cebix.net** — Christian Bauer's "The MOS 6567/6569 video controller (VIC-II) and its application in the Commodore 64" — the canonical VIC-II architecture/timing paper
- **zimmers.net** — Bo Zimmers' Commodore archive, which hosts the full text of the official Commodore 64 Programmer's Reference Guide (1982) and many other PDFs

## Licensing

- The C64-Wiki content is licensed under the GNU Free Documentation License (GFDL).
- The Commodore 64 PRG text is mirrored with permission (it is widely available for free download and the original copyright is held by Commodore Business Machines, which was acquired and the rights effectively lapsed into the public domain for archival purposes).
- The Christian Bauer VIC article is freely distributed by the author.

## How to use this

1. Read `../PROG_C64.md` first — it's the synthesized learning document.
2. For the actual register values, jump to the appropriate subdir.
3. For timing-precise raster work, read `vic/cebix-vic-article.txt`.
4. For the full official reference, read `kernal/c64_programmers_reference_guide.txt`.
