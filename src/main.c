// main.c — Whack Hare! entry point (Phase 5: raster IRQ + idle loop).
//
// Flow:
//   1. memmap_setup()      — bank out KERNAL/BASIC/CHAR ROM.
//   2. score_init()        — zero both player scores.
//   3. game_init()         — enter the TITLE state (which also loads
//                            the title screen and renders the score
//                            bar).
//   4. rasterirq_setup()   — install the single RIRQ at line 311.
//                            The IRQ handler increments frame_count
//                            and calls game_step() once per frame.
//   5. while (1) {}         — idle.  All per-frame work happens in
//                            the IRQ handler.
//
// In Phase 4 step 5 was `while (1) game_step();` — a busy-wait that
// called game_step as fast as the CPU could, so the "frame counter"
// was CPU-bound.  In Phase 5 the busy-wait is gone: game_step is
// called from the raster IRQ at exactly 50 Hz, so state durations
// are wall-clock-bound (60 frames = 1.2 sec, etc.) regardless of
// what the CPU is doing between IRQs.

#include "memmap.h"
#include "game.h"
#include "score.h"
#include "tick.h"

// We don't malloc, so the heap is unused.  Setting it to 0 frees the
// space for the screen data in the main region.
#pragma heapsize(0)

int main(void)
{
	memmap_setup();
	score_init();
	game_init();
	rasterirq_setup();

	// The raster IRQ does all the per-frame work.  The main loop
	// is a deliberate spin: nothing to do between IRQs.
	for (;;)
		;

	return 0;
}
