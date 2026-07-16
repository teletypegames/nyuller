# Interrupts (IRQ / NMI / BRK)

Source: https://www.c64-wiki.com/wiki/Interrupt

The C64 with its 6510 CPU supports two types of interrupt: **IRQ** (Interrupt Request, maskable) and **NMI** (Non-Maskable Interrupt). The CPU has the option of ignoring IRQ (via the I flag in the status register, set with `SEI`, cleared with `CLI`), but must respond to NMI.

The first thing the CPU does for either is push the program counter and status register onto the stack. Then it does an indirect JMP through a vector in the very last six bytes of KERNAL ROM:

- $FFFA-$FFFB — NMI vector → $FE43
- $FFFC-$FFFD — Cold start (RESET) vector → $FCE2
- $FFFE-$FFFF — IRQ / BRK vector → $FF48

The KERNAL routines in turn jump through a RAM vector, which can be redirected to a user-supplied ISR.

## IRQ flow

1. CPU pushes PC and P, jumps via ($FFFE) to $FF48.
2. $FF48 pushes A, X, Y onto the stack:

```asm
.C:ff48  48         PHA       ; push A
.C:ff49  8A         TXA
.C:ff4a  48         PHA       ; push X
.C:ff4b  98         TYA
.C:ff4c  48         PHA       ; push Y
; Stack now (top to bottom): PC_hi, PC_lo, P, A, X, Y
.C:ff4d  BA         TSX
.C:ff4e  BD 04 01   LDA $0104,X  ; load saved P
.C:ff51  29 10      AND #$10    ; test BREAK flag
.C:ff53  F0 03      BEQ $FF58
.C:ff55  6C 16 03   JMP ($0316) ; BRK vector
.C:ff58  6C 14 03   JMP ($0314) ; IRQ vector
```

3. Default IRQ vector at $0314-$0315 = $EA31 (KERNAL IRQ routine: maintains jiffy clock, scans keyboard for RUN/STOP, blinks cursor).
4. KERNAL exits via $EA81 (pops A/X/Y and RTI).

So a custom IRQ routine has the stack laid out as: PC_hi, PC_lo, P, A, X, Y (top to bottom). Your ISR ends with `PLA : TAY : PLA : TAX : PLA : RTI` (or just `JMP $EA81` if you want the KERNAL to handle the standard jobs first).

## NMI flow

1. CPU pushes PC and P, jumps via ($FFFA) to $FE43.
2. NMI routine sets the I flag (masking further IRQs), saves A/X/Y, then jumps via $0318-$0319 (NMI RAM vector).
3. Default NMI vector at $0318-$0319 = $FE47.
4. If a cartridge is present, NMI is handed to it via vector at $8002-$8003.
5. RUN/STOP+RESTORE triggers NMI which is treated as a soft reset (BASIC warm start).

## Interrupt sources on the C64

- **CIA 1** (IRQ): Timer A/B underflow, TOD=alarm, serial byte complete, FLAG pin (cassette)
- **CIA 2** (NMI): mostly RS-232 via FLAG pin, RESTORE key
- **VIC-II** (IRQ): raster match, sprite-sprite collision, sprite-data collision, light pen

## Tricks

- Disable interrupts in BASIC (kills keyboard): `POKE 56334, PEEK(56334) AND 254`
- Re-enable: `POKE 56334, PEEK(56334) OR 1`
- RUN/STOP+RESTORE: triggers NMI → soft reset
