// main.c — entry point and memory layout for Nyuller.
//
// Memory layout chosen for the NUFLI displayer:
//   $0000-$0088: zero page
//   $0100-$01FF: hardware stack
//   $0200-$02FF: buffers
//   $0300-$03FF: system variables
//   $0900-$2000: program code
//   $2000-$7A00: NUFLI runtime image
//   $7A00-$8000: small stack (512 bytes)
//   $8000-$D000: compressed waiting bitmaps and screen attributes
//   $D000-$DFFF: C64 I/O registers
//   $E000-$FFFF: game bitmap (RAM with KERNAL banked out)
//
// The program is split into three address ranges so the NUFLI runtime
// image ($2000-$7A00) is never overwritten by the linker.

#include "game.h"
#include "audio.h"
#include "nufli.h"
#include "memmap.h"
#include "score.h"
#include "tick.h"

#pragma heapsize(0)
#pragma stacksize(0x200)

#pragma section( screens, 0)

// Region layout.  The stack lives in the small gap between the NUFLI
// image ($2000-$7A00) and the high data region ($8000-$D000).  Code and
// data are placed in whichever region fits.
#pragma region( region_low,  0x0900, 0x2000, , , { code, bss, heap, screens } )
#pragma region( region_stack, 0x7A00, 0x8000, , , { stack } )
#pragma region( region_high, 0x8000, 0xD000, , , { code, data, bss, heap, screens } )

int main(void)
{
	memmap_setup();
	audio_init();
	score_init();
	game_init();
	rasterirq_setup();

	for (;;) {
		;
	}

	return 0;
}
