# Zeropage (Page 0) and Page 1

Source: https://www.c64-wiki.com/wiki/Zeropage and https://www.c64-wiki.com/wiki/Page_1

The first 256 bytes ($0000-$00FF) of the C64 memory map are called **zeropage** (or "Page 0"). The 6502/6510/8502 family has special addressing modes (zero page addressing, indirect zero page, indexed zero page) that are faster and shorter than the equivalent absolute modes — they are 1 byte shorter and 1 cycle faster. The indirect-indexed mode (`(zp),Y`) only works on zero page addresses, which is why zero page locations are used as "index registers" in 6502-family assembly.

The first two addresses in zeropage — and indeed in the entire address space — are "hardwired" in the 6510 to the CPU's internal I/O port. The data direction register is at $00 and the data register is at $01.

## The 6510 I/O Port

| Addr | Purpose |
|------|---------|
| $00 (0) | Data direction for $01 (0=input, 1=output) |
| $01 (1) | CPU data port (also controls memory banking) |

### $01 — CPU data port (and bank bits)

| Bit | Name | Purpose |
|-----|------|---------|
| 0 | LORAM | 0 = RAM at $A000-$BFFF visible; 1 = BASIC ROM visible |
| 1 | HIRAM | 0 = RAM at $E000-$FFFF visible; 1 = KERNAL ROM visible |
| 2 | CHAREN | 0 = Char ROM visible at $D000-$DFFF; 1 = I/O registers visible (default) |
| 3 | Cassette Data Output (Datasette) |
| 4 | Cassette Switch Sense (1 = switch closed) |
| 5 | Cassette Motor (0 = on) |
| 6 | (Undefined on 6510) |
| 7 | (Undefined on 6510) |

The default value after reset is `$37 = %00110111` — BASIC + KERNAL + I/O visible, cassette motor off.

Note: addresses 0 and 1 cannot be read or written as ordinary RAM from the CPU; they only access the port. However, with the right VIC trickery (datassette buffer in `$02`) you can read underlying RAM contents via the VIC.

## KERNAL/BASIC zero page usage

Most of the zero page is in use by KERNAL and BASIC ROMs. A handful of locations are "safe" for user ML programs, but most of $00-$8F is occupied. The page is dominated by:

- $00-$02: CPU port, unused, float-to-int / int-to-float ROM pointers
- $03-$06: ROM helper pointers
- $07-$0F: BASIC flags and various state
- $10-$13: BASIC bookkeeping
- $14-$15: pointer for ON/GOTO/GOSUB/LIST/PEEK/POKE/SYS target
- $16-$18: temporary string stack
- $19-$21: temporary string descriptors
- $22-$25: utility pointer area
- $26-$2A: float multiply/divide result
- $2B-$37: BASIC program/symbol table pointers (TXTTAB, VARTAB, ARYTAB, STREND, FRETOP, MEMSIZ)
- $39-$3E: current/previous BASIC line numbers, CONT target
- $3F-$42: DATA line/item for READ
- $43-$44: INPUT storage
- $45-$48: variable name lookup
- $49-$4A: FOR/NEXT index
- $4B-$4C: math temp / TXTPTR for READ/GET/INPUT
- $4D: mask for <, >, = evaluation
- $4E-$4F: temp for FN or float
- $50-$52: strings
- $53: string length for garbage collection
- $54: constant `$4C` (JMP opcode)
- $55-$56: pointer for function eval
- $57-$6E: floating point accumulators (FAC#3, #4, #1, #2)
- $6F: result of signed comparison
- $70-$72: FAC#2 round / temp series pointer
- $73-$8A: CHRGET (fetch next BASIC char) routine
- $8B-$8F: RND seed
- $90: KERNAL I/O status (bit 6 = EOF)
- $91: STOP/C=/SPACE/CTRL flag (127/223/239/251/255)
- $92: cassette timing constant
- $93: LOAD/VERIFY flag
- $94-$9B: serial bus / cassette / output device
- $9C-$9F: cassette status
- $A0-$A2: software jiffy clock (updated by KERNAL IRQ every 1/60 sec)
- $A3-$A4: serial/cassette bit counter
- $A5-$A6: cassette sync / buffer size
- $A7-$AC: RS-232 / cassette byte
- $AC-$AF: LOAD/VERIFY/SAVE start/end
- $B0-$B1: cassette timing constants (default $92 = 146)
- $B2-$B3: cassette buffer start
- $B4-$B6: RS-232 output
- $B7-$BA: current logical file / secondary / device
- $BB-$BC: current file name pointer
- $BD: RS-232 parity / cassette R/W register
- $BE: cassette dup block counter
- $BF: cassette byte register
- $C0: cassette motor (0=off)
- $C1-$C2: LOAD/SAVE start address
- $C3-$C4: LOAD/SAVE end address
- $C5: last key matrix coord (64 = none)
- $C6: keyboard buffer count
- $C7: reverse print flag
- $C8: last column of current line during INPUT
- $C9-$CA: cursor X/Y
- $CB: index into keyboard decoding table
- $CC: flash cursor flag
- $CD: cursor flash counter
- $CE: char at cursor
- $CF: cursor flash phase
- $D0: input from keyboard/screen
- $D1-$D2: pointer to current screen line
- $D3: cursor column in logical line
- $D4: quote mode flag
- $D5: max column (39 or 79)
- $D6: current physical line
- $D7: ASCII of last printed char
- $D8: insert mode flag
- $D9-$F2: screen line link table (26 bytes)
- $F3-$F4: color RAM pointer for current line
- $F5-$F6: keyboard decoding table pointer
- $F7-$F8: RS-232 input buffer pointer
- $F9-$FA: RS-232 output buffer pointer
- $FB-$FC: unused
- $FD: unused
- $FE: unused
- $FF: temp for BASIC float to ASCII

## Page 1 ($0100-$01FF)

Page 1 (also called "Extended Zeropage") is mostly used by the **hardware stack** (which grows downward from $01FF). The bottom of the page is reserved for the BASIC/Datasette scratch area:

- $0100-$010A: floating point to string conversion work area
- $0100-$013E: Datasette input error log
- $013F-$01FF: the 6502 hardware stack (default top = $FF, so stack is at $0100-$01FF)

In an Oscar64 (or typical) machine-language program, the hardware stack is in this area. C64 BASIC pushes the stack pointer to $FF and uses the top of the page for storage.
