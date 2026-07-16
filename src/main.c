// main.c — Whack Hare! entry point (Phase 3: score bar).
//
// Flow:
//   1. memmap_setup()      — bank out KERNAL/BASIC/CHAR ROM.
//   2. show_screen(TITLE)  — copy the title bitmap into $E000 and
//                            configure the VIC for multicolor bitmap
//                            mode.
//   3. score_init()        — zero both player scores.
//   4. score_render()      — overlay the score bar on the top 8 rows.
//   5. Loop: poll joystick port 1 for fire.  When pressed, restore
//      the C64's default memory config and return from main() (which
//      ends the program).
//
// Build:  ./build.sh        (compile to src/build/whack_hare.prg)
// Run:    ./build.sh -e     (headless, in oscar64's built-in emulator)
//         ./build.sh -v     (VICE x64, interactive, needs a display)

#include "memmap.h"
#include "screens.h"
#include "input.h"
#include "score.h"

int main(void)
{
	memmap_setup();
	show_screen(SCREEN_TITLE);

	score_init();
	score_render();

	// Phase 3: just poll for fire on port 1 (the Hare / right joystick).
	// In Phase 4 this becomes a 50 Hz raster IRQ handler that drives the
	// full state machine.
	while (!input_fire(1))
		;

	memmap_restore();
	return 0;
}
