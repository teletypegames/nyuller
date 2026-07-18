// main.c — Nyuller entry point (Phase 5: raster IRQ + idle loop).
//
// Flow:
//   1. memmap_setup()      — bank out KERNAL/BASIC/CHAR ROM.
//   2. audio_init()        — set SID master volume, no filter, silence
//                            all 3 voices.  Phase 6 addition.
//   3. score_init()        — zero both player scores.
//   4. game_init()         — enter the TITLE state (which also loads
//                            the title screen, renders the score bar,
//                            and calls audio_state_enter(STATE_TITLE)).
//   5. rasterirq_setup()   — install the single RIRQ at line 311.
//                            The IRQ handler increments frame_count
//                            and calls game_step() once per frame.
//   6. while (1) {}         — idle.  All per-frame work happens in
//                            the IRQ handler.
//
// In Phase 4 step 6 was `while (1) game_step();` — a busy-wait that
// called game_step as fast as the CPU could, so the "frame counter"
// was CPU-bound.  In Phase 5 the busy-wait is gone: game_step is
// called from the raster IRQ at exactly 50 Hz, so state durations
// are wall-clock-bound (60 frames = 1.2 sec, etc.) regardless of
// what the CPU is doing between IRQs.

#include "memmap.h"
#include "audio.h"
#include "game.h"
#include "score.h"
#include "tick.h"

// We don't malloc, so the heap is unused.  Setting it to 0 frees the
// space for the screen data in the main region.
//
// The stack is set to 0x400 (1 KB) — this is the oscar64 default,
// pinned explicitly so the layout is predictable across `-O1` /
// `-O3` builds.  With `-O3` the code section is larger, so the
// data section's spillover into the default stack/heap gap region
// leaves only ~0x400 for both.  Anything larger (0x600+) makes
// `-O3` fail to link with "Cannot place stack section".  1 KB is
// plenty because the oscar64 software stack lives in zero-page
// (0xF7-0xFF, 9 bytes per the -O3 default); the spillover area
// only needs to hold the few locals/params that don't fit in ZP.
#pragma heapsize(0)
#pragma stacksize(0x400)

int main(void)
{
	memmap_setup();
	audio_init();
	score_init();
	game_init();
	rasterirq_setup();

	// The raster IRQ does all the per-frame work.  The main loop
	// is a deliberate spin: nothing to do between IRQs.
	for (;;)
		;

	return 0;
}
