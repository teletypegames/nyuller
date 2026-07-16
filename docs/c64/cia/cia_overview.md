# CIA — Complex Interface Adapter (6526)

Source: https://www.c64-wiki.com/wiki/CIA

## Description

The CIA is an interface chip used in the Commodore home computers. It controls most of the I/O processes and contains as well the internal timer (clock). The CIA was developed by MOS Technology. Inside the C64 there are two CIA 6526 chips used; later C64-versions may also use the 6526A or the 8521.

## Features (6526)

- 16 single programmable In- and Output lines
  - The lines are lead through open collector with internal pullups.
  - In the C64 some of these lines are used to monitor/control the I/O devices (keyboard, joystick, iec, etc.), some are connected with the VIC to define which area of memory it can address, others are for free disposition and are available at the userport.
- 8- or 16-Bit data transport (reading or writing) with handshaking.
- 2 independent 16-bit interval timers
  - each timer consists of a 16 bit-latch (start value) and the actual 16 bit-timer
  - the latch can be set directly, the timer can only be read indirectly via the latch.
- 24-h-timeclock (AM/PM) with programmable alarm
- 8-bit shift register for serial In- and Output.
- 2 TTL-inputs
- CMOS-compatible
- Pulsing: 1 MHz (6526) or 2 MHz (6526A)

## CIA 1 (at $DC00-$DCFF, 56320-56575)

Tasks: Keyboard, Joystick, Paddles, Datasette, IRQ control

| Addr Hex | Addr Dec | Reg | Function |
|----------|----------|-----|----------|
| $DC00 | 56320 | 0 PRA | Data Port A. Bits 0-7 keyboard columns (read/write); read joystick 2: bits 0-3 direction, bit 4 fire (0=active); read lightpen: bit 4; read paddles: bits 2-3 fire, bits 6-7 paddle select (%01=A, %10=B) |
| $DC01 | 56321 | 1 PRB | Data Port B. Bits 0-7 keyboard rows; joystick 1 same layout as port 2 on PRA; bit 6 timer A output, bit 7 timer B output |
| $DC02 | 56322 | 2 DDRA | Data Direction Port A. Bit=0 input, 1 output |
| $DC03 | 56323 | 3 DDRB | Data Direction Port B |
| $DC04 | 56324 | 4 TA LO | Timer A low byte |
| $DC05 | 56325 | 5 TA HI | Timer A high byte |
| $DC06 | 56326 | 6 TB LO | Timer B low byte |
| $DC07 | 56327 | 7 TB HI | Timer B high byte |
| $DC08 | 56328 | 8 TOD 10THS | BCD 1/10 seconds |
| $DC09 | 56329 | 9 TOD SEC | BCD seconds |
| $DC0A | 56330 | 10 TOD MIN | BCD minutes |
| $DC0B | 56331 | 11 TOD HR | BCD hours (AM/PM bit 7) |
| $DC0C | 56332 | 12 SDR | Serial shift register |
| $DC0D | 56333 | 13 ICR | Interrupt control and status |
| $DC0E | 56334 | 14 CRA | Control Timer A |
| $DC0F | 56335 | 15 CRB | Control Timer B |
| $DC10-$DCFF | 56336-56575 | - | Mirror of $DC00-$DC0F every 16 bytes |

## CIA 2 (at $DD00-$DDFF, 56576-56831)

Tasks: Serial bus, RS-232, VIC memory bank, NMI control

| Addr Hex | Addr Dec | Reg | Function |
|----------|----------|-----|----------|
| $DD00 | 56576 | 0 PRA | Bits 0-1: VIC bank select (%00=bank 3 $C000-$FFFF, %01=bank 2 $8000-$BFFF, %10=bank 1 $4000-$7FFF, %11=bank 0 $0000-$3FFF default). Bit 2: RS-232 TXD, userport PA2. Bits 3-5: serial bus ATN/CLOCK/DATA OUT. Bits 6-7: serial bus CLOCK/DATA IN. |
| $DD01 | 56577 | 1 PRB | Userport PB0-7 |
| $DD02 | 56578 | 2 DDRA | |
| $DD03 | 56579 | 3 DDRB | |
| $DD04-$DD07 | 56580-56583 | 4-7 | Timer A, Timer B |
| $DD08-$DD0B | 56584-56587 | 8-11 | TOD (Time of Day) |
| $DD0C | 56588 | 12 SDR | Serial shift register |
| $DD0D | 56589 | 13 ICR | Bit 4: NMI on FLAG pin (RS-232), bit 7: NMI occurred |
| $DD0E | 56590 | 14 CRA | Control Timer A |
| $DD0F | 56591 | 15 CRB | Control Timer B |

## Timer A Control ($DC0E / $DD0E)

- Bit 0: 0 = Stop timer, 1 = Start timer
- Bit 1: 1 = Timer A underflow output on port B bit 6
- Bit 2: 0 = pulse output, 1 = toggle output
- Bit 3: 0 = restart after underflow, 1 = stop after underflow
- Bit 4: 1 = Load latch into timer
- Bit 5: 0 = count system cycles, 1 = count positive slope at CNT pin
- Bit 6: 0 = SP input, 1 = SP output (serial shift register direction)
- Bit 7: 0 = 60 Hz TOD, 1 = 50 Hz TOD

## Timer B Control ($DC0F / $DD0F)

Same as A but bits 5-6:
- %00 = system cycles
- %01 = positive slope on CNT
- %10 = timer A underflows
- %11 = timer A underflows with CNT high

Bit 7: 0 = TOD register sets time, 1 = TOD sets alarm time.

## Interrupt Control/Status Register ($DC0D / $DD0D)

CIA 1 is connected to IRQ. CIA 2 to NMI.

Read: bits 0-4 are interrupt source flags (cleared on read!), bit 7 is "any IRQ" flag.
- Bit 0: Timer A underflow
- Bit 1: Timer B underflow
- Bit 2: TOD = alarm
- Bit 3: Serial register full/empty
- Bit 4: FLAG pin negative edge (CIA 1: cassette input/serial SRQ; CIA 2: RS-232 RX / NMI)
- Bit 5-6: always 0
- Bit 7: 1 = any enabled interrupt occurred

Write: bits 0-4 set/clear mask depending on bit 7 (1=set, 0=clear).

## Pinout (6526)

- Vss: Ground
- PA0-PA7: 8-bit Port A
- PB0-PB7: 8-bit Port B
- /PC: Handshake output, low pulse after read/write on port B
- TOD: Time Of Day input (50/60 Hz)
- Vcc: +5V
- /IRQ: Interrupt request to CPU
- R/W: Read/Write
- /CS: Chip select
- /FLAG: Negative edge IRQ input / handshake
- /phi2: Processor Φ2 clock
- DB0-DB7: Data bus
- /RES: Reset
- RS0-RS3: Register select
- SP: Serial port
- CNT: Count (timer input)

## Failure Symptoms

CIA 1: Startup screen normal but no cursor. No keyboard or control port access. May overheat if shorted.
CIA 2: Startup screen normal. No serial or user port access. "File not found" error on drive access.
