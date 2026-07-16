# SID — Sound Interface Device (6581 / 8580)

Source: https://www.c64-wiki.com/wiki/SID

## Overview

SID is the name of the sound chip used in the VC 10, C64 and C128. The SID was developed by Bob Yannes, an employee of MOS Technology. Bob (Robert) Yannes knew a lot about music. His intention was to implement a real subtractive synthesis chip, totally different from all other home computer sound devices of its time. The chip combines analogue and digital circuitry that cannot be emulated with 100% fidelity even today.

The C64 and C128 (plastic case) use the 6581. The C64-II and C128DCR (metal case) use the newer 8580. Newer chip replacements emulate the SID: SIDKick pico, SwinSID, etc.

## Properties

- 3 tone generators (voices), frequency 0-4 kHz (16 bit precision)
- 4 waveforms: sawtooth, triangle, rectangle (pulse width modulation), noise
- 3 amplitude modulators, up to 48 dB
- 3 envelope generators (ADSR)
- Synchronization of the oscillators
- Ring modulation
- Programmable filters: low pass, band pass, high pass
- Master volume in 16 steps
- 2 A/D converters (8 bit, low frequency, used for reading paddle input)
- Random generator
- Audio input (cannot be used for sampling, but signal can be routed through the SID filter)

## Chip Variations

| Chip | Production | Notes |
|------|------------|-------|
| 6581 | 21/1982 - 30/1985 | NMOS, pin 28 = 12V, 470pF on filter caps |
| 6581R3 | 42/1985 - 07/1986 | |
| 6581R4 | 16/1986 - 30/1986 | |
| 6581R4AR | 22/1986 - 06/1987 | |
| 8580R5 | 06/1987 - 19/1992 | HMOS-II, pin 28 = 9V, 22nF on filter caps; quiet digisound without "digifix" |

## Memory Addresses (SID at $D400-$D41C, mirrored through $D7FF except on C128)

| Address Hex | Address Dec | Function |
|-------------|-------------|----------|
| $D400 | 54272 | Frequency voice 1 low byte |
| $D401 | 54273 | Frequency voice 1 high byte |
| $D402 | 54274 | Pulse wave duty cycle voice 1 low byte (low nibble) |
| $D403 | 54275 | Pulse wave duty cycle voice 1 high byte (low nibble) |
| $D404 | 54276 | Control register voice 1 |
| $D405 | 54277 | Attack duration / Decay duration voice 1 |
| $D406 | 54278 | Sustain level / Release duration voice 1 |
| $D407-$D40D | 54279-54285 | Same for voice 2 |
| $D40E-$D414 | 54286-54292 | Same for voice 3 |
| $D415 | 54293 | Filter cutoff frequency low byte (low 3 bits) |
| $D416 | 54294 | Filter cutoff frequency high byte (8 bits) |
| $D417 | 54295 | Filter resonance and routing (high nibble=res, low 4 bits=ext/3/2/1 enable) |
| $D418 | 54296 | Filter mode and main volume (bits 6-4: mute V3 / HP / BP / LP, bits 3-0: volume 0-15) |
| $D419 | 54297 | Paddle X value (read only) |
| $D41A | 54298 | Paddle Y value (read only) |
| $D41B | 54299 | Oscillator voice 3 (read only) |
| $D41C | 54300 | Envelope voice 3 (read only) |

## Voice Control Register ($D404 / $D40B / $D412)

Bit 0: Gate (1 = start envelope, 0 = release)
Bit 1: Synchronize with voice 3 (for V1) / voice 1 (V2) / voice 2 (V3)
Bit 2: Ring modulation with voice 3 (V1) / voice 1 (V2) / voice 2 (V3)
Bit 3: Test (reset oscillator)
Bit 4: Triangle wave
Bit 5: Sawtooth wave
Bit 6: Pulse (rectangle) wave
Bit 7: Noise

## ADSR ($D405-$D406 per voice)

$D405 high nibble: Attack (0-15)
$D405 low nibble: Decay (0-15)
$D406 high nibble: Sustain (0-15)
$D406 low nibble: Release (0-15)

## Pinout

- CAP1A, CAP1B / CAP2A, CAP2B: filter capacitors (6581: 470pF, 8580: 20nF)
- /RES: Reset
- phi2: system clock
- R/W
- /CS
- A0-A4
- GND
- Vdd: 12V (6581) or 9V (8580)
- AUDIO OUT
- EXT IN: external audio in (8580 needs ~330kOhm to GND to fix old digisound)
- Vcc: 5V
- POT X, POT Y: paddle inputs
- D0-D7: data bus

## Trivia

Bob Yannes later founded Ensoniq (the ESQ-1 is reportedly the synth he wanted the SID to be). The 6581's volume register design flaw was famously used to play 4-bit samples by rapidly changing the volume. The 8580 fixed this; sample playback can be restored with a resistor on EXT IN.
