# Joysticks (Control Ports)

Source: https://www.c64-wiki.com/wiki/Joystick

A joystick is a gaming control device. The C64 uses the standard first seen on the Atari 2600 — eight directions and one fire button. The stick mechanically activates four switches (up/down/left/right); pushing diagonally activates two. Some joysticks have two fire buttons but they appear identical to software.

The switches and button connect to the CIA #1 ports A and B (in parallel with the keyboard matrix), which is why a joystick — especially on port 1 — can cause the machine to "type" characters when you operate it.

## Reading a joystick

- Port #1 (right port): read via $DC01 (CIA 1 PRB)
- Port #2 (left port): read via $DC00 (CIA 1 PRA)

In each byte, the bits are active-low (0 = pressed):

- Bit 0 (1) — Up
- Bit 1 (2) — Down
- Bit 2 (4) — Left
- Bit 3 (8) — Right
- Bit 4 (16) — Fire

## Typical values (rest position, no buttons)

| Position | Port 1 ($DC01) | Port 2 ($DC00) | +Fire (Port 1) | +Fire (Port 2) |
|----------|----------------|----------------|----------------|----------------|
| middle   | 255            | 127            | 239            | 111            |
| up       | 254            | 126            | 238            | 110            |
| down     | 253            | 125            | 237            | 109            |
| left     | 251            | 123            | 235            | 107            |
| right    | 247            | 119            | 231            | 103            |
| up+left  | 250            | 122            | 234            | 106            |
| up+right | 246            | 118            | 230            | 102            |
| down+left| 249            | 121            | 233            | 105            |
| down+right|245            | 117            | 229            | 101            |

## Keyboard collision

Keyboard scanning uses the same CIA port bits, so reading the joystick will "see" pressed keys. To disable the keyboard while polling: `POKE 56322, 224` (write %11100000 to CIA 1 DDRA so PRA pins are inputs, leaving only the rows set as output — wait, that is the opposite of "disable keyboard". The actual recipe to disable keyboard scanning is to disable CIA 1 interrupts or to set all keyboard columns to inputs).

The simpler approach is to mask out the keyboard bits and only test the low 5 bits of the joystick.

## Analog inputs

The control ports also provide +5V and two analog lines (designed for paddles) — the SID reads these as 8-bit values via $D419 (X) and $D41A (Y).

## Sample BASIC polling

```basic
10 J = NOT PEEK(56321)
20 PRINT CHR$(147);"JOYSTICKTEST"
30 IF (J AND 1)  THEN PRINT "1-U ";
35 IF (J AND 2)  THEN PRINT "1-D ";
40 IF (J AND 4)  THEN PRINT "1-L ";
45 IF (J AND 8)  THEN PRINT "1-R ";
50 IF (J AND 16) THEN PRINT "1-F ";
55 GOTO 10
```
