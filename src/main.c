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

#include "memmap.h"
#include "audio.h"
#include "game.h"
#include "score.h"
#include "tick.h"

#pragma heapsize(0)
#pragma stacksize(0x400)

int main(void)
{
	memmap_setup();
	audio_init();
	score_init();
	game_init();
	rasterirq_setup();

	// The raster IRQ does all the per-frame work.
	for (;;)
		;

	return 0;
}
