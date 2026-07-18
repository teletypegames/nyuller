#ifndef NYULLER_TICK_H
#define NYULLER_TICK_H

// tick.h — 50 Hz raster IRQ setup for the game tick.
//
// A single RIRQ is installed at raster line 311 (PAL stable line, right
// after vertical blank, before any badlines).  The IRQ handler runs
// once per frame and does two things:
//
//   1. Increments the global 16-bit frame_count (50 Hz).
//   2. Calls game_step() — the state machine + per-state step actions
//      read both joysticks, update the score bar, and run the
//      flashing-text animations.
//
// All game timing is now driven by frame_count comparisons.  No more
// busy-wait.  The main() loop is an empty `while (1) {}`.
//
// CIA 1 and CIA 2 IRQs are masked at setup so the jiffy-clock handler
// doesn't fire nested inside the raster IRQ.  We bank out the KERNAL
// at startup (memmap_setup) so we use rirq_init(false) — the hardware
// IRQ vector, no KERNAL continuation.
//
// (We can't name this file "rasterirq.h" because the oscar64
// library's rasterirq.c does `#include "rasterirq.h"` to find its
// own rasterirq.h, and that include would resolve to our header
// instead — see the comment in tick.c for details.)

void rasterirq_setup(void);

#pragma compile("tick.c")

#endif
