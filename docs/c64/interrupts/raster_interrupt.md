# Raster Interrupts

Source: https://www.c64-wiki.com/wiki/Raster_interrupt

A raster interrupt is an interrupt trigger signal that the VIC-II can supply, if desired, to the CPU whenever the raster in the VIC's video signal reaches a specific line. With machine code programming, this mechanism can be exploited to perform many kinds of VIC "trickery" — having both text and high-res graphics on screen simultaneously, displaying more than eight hardware-supported sprites at once. This is heavily used on the C64/C128 for computer games and demos.

The Atari 800, MSX, and Amstrad CPC also support raster interrupts. The 80-column mode on the C128/VDC cannot.

## Setting up a raster interrupt

By default, the system is set up to receive timer-based signals from CIA-1's Timer A. To use the VIC raster interrupt:

```asm
Init    SEI                     ; disable IRQ
        LDA #%01111111
        STA $DC0D               ; switch off interrupt signals from CIA-1

        AND $D011               ; clear MSB of VIC raster
        STA $D011

        STA $DC0D               ; acknowledge pending CIA-1 IRQs
        STA $DD0D               ; acknowledge pending CIA-2 NMIs

        LDA #210                ; set raster line where interrupt shall occur
        STA $D012

        LDA #<Irq
        STA $0314               ; set IRQ vector
        LDA #>Irq
        STA $0315

        LDA #%00000001
        STA $D01A               ; enable raster interrupt

        CLI                     ; re-enable IRQ
        RTS
```

Note: enabling raster interrupts from the VIC takes place *after* setting up everything else. The routine starts with `SEI` because if the interrupt is enabled before e.g. the vector is re-directed, an interrupt may occur while the vector is being altered, sending the CPU to a "random" address and crashing the system.

## Single ISR example (wiggle border)

```asm
Irq     LDA #$07
        STA $D020               ; border = yellow

        LDX #$90                ; delay ~half a millisecond
Pause:  DEX
        BNE Pause

        LDA #$00
        STA $D020               ; border = black

        ASL $D019               ; acknowledge raster IRQ
        JMP $EA31               ; into KERNAL standard ISR (handles cursor blink, etc.)
```

The yellow stripe across the border is exactly 8 raster lines wide, set by the pause loop length.

## Multiple raster ISRs (split-screen: hires top, text bottom)

The "concatenated" pattern — two routines handling two different raster lines, each setting up the next:

```asm
Irq     LDA $D011
        AND #%11011111
        STA $D011               ; switch to text mode

        LDA #<Irq2
        STA $0314
        LDA #>Irq2
        STA $0315

        LDA #$0
        STA $D012               ; next IRQ at line 0

        ASL $D019               ; acknowledge
        JMP $EA31               ; into KERNAL ISR

Irq2    LDA $D011
        ORA #%00100000
        STA $D011               ; switch to bitmap mode

        LDA #<Irq
        STA $0314
        LDA #>Irq
        STA $0315

        LDA #210
        STA $D012               ; next IRQ at line 210

        ASL $D019
        JMP $EA81               ; shorter ROM routine — just restore regs and RTI
```

Only one of the chained routines needs to jump to the full KERNAL ISR; the others can exit through $EA81 (the register-restore stub) for speed.
