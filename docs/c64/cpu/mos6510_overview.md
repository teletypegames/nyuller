# MOS 6510 CPU

Source: https://www.c64-wiki.com/wiki/MOS_Technology_6510

## Identification

- Chip Name: MOS 6510
- Clock Speed: 0.985 MHz (PAL), 1.023 MHz (NTSC)
- Manufacturer: MOS Technologies
- Designer: Commodore Semiconductor Group
- Board: Commodore 64, Commodore 128
- Socket: C64 U7, C64G U6, C128 U6
- Released: 1982, Discontinued: 1994
- Package: DIP-40
- Datasheet: http://archive.6502.org/datasheets/mos_6510_mpu.pdf

## Description

The MOS 6510 works on the mainboard of a C64 as the CPU. The 6510 is a modified version of the MOS 6502, distinguished primarily by the addition of an 8-bit general-purpose I/O port. In the most common implementation, six of these I/O pins are available. The chip also introduces support for tri-state operation on the address bus and allows the CPU to be halted cleanly.

## C64 MOS 6510 Failure Symptoms

- Blank screen, no border
- Cartridge doesn't work

## Reference Documents

- [MOS 6510 MPU data sheet (PDF)](http://archive.6502.org/datasheets/mos_6510_mpu.pdf)
- [Documentation for the NMOS 65xx/85xx Instruction Set (viceteam.org)](http://viceteam.org/plain/64doc.txt)

## See also

For full C64 register map, the zeropage I/O port ($00/$01) and the complete KERNAL jump table, see the other documents in this `docs/c64/` directory.
