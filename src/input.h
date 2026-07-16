#ifndef WHACK_HARE_INPUT_H
#define WHACK_HARE_INPUT_H

// input_fire(port) — returns 1 if the fire button on the given joystick
// port is currently pressed, 0 otherwise.
//
// port = 0  →  CIA1 PRA at $DC00  (left joystick, "Scoot", player 2)
// port = 1  →  CIA1 PRB at $DC01  (right joystick, "Hare", player 1)
//
// Active low: the button is "pressed" when bit 4 of the CIA port is 0.
// The direction bits (0..3) are read but masked off — only the fire
// bit is examined.  Bits 5..7 of the CIA ports are timer A/B outputs
// and the paddles, which we ignore here.
//
// No debounce — this is a raw poll.  Call it once per frame from a
// raster IRQ or game loop.

char input_fire(int port);

#pragma compile("input.c")

#endif
