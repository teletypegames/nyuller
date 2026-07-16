// main.c — Whack Hare! entry point (Phase 4: state machine skeleton).
//
// Flow:
//   1. memmap_setup()      — bank out KERNAL/BASIC/CHAR ROM.
//   2. score_init()        — zero both player scores.
//   3. game_init()         — enter the TITLE state (which also loads
//                            the title screen and renders the score
//                            bar).
//   4. while (1) game_step() — the state machine runs forever.
//
// In Phase 4 this is a busy-wait loop: game_step() is called in a
// tight loop with no real timing.  The "frame counter" inside game.c
// is just a count of how many times game_step() has been called, so
// the state durations are CPU-bound, not wall-clock-bound.
//
// In Phase 5 the busy-wait body of the main loop becomes a raster
// IRQ handler that runs at 50 Hz, and game_step() is called from
// the IRQ.  The function signature doesn't change — only the call
// site does — so the game logic is the same in both phases.

#include "memmap.h"
#include "game.h"
#include "score.h"

// We don't malloc, so the heap is unused.  Setting it to 0 frees the
// space for the screen data in the main region.
#pragma heapsize(0)

int main(void)
{
	memmap_setup();
	score_init();
	game_init();

	while (1)
		game_step();

	return 0;
}
