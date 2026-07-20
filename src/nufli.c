// nufli.c — NUFLI image display for static screens.
//
// Displays the Mufflon-generated NUFLI image by:
//   1. Saving the $1000-$1FFF scratch area (the Mufflon displayer at $3000
//      generates self-modifying speedcode there).
//   2. Saving the hardware IRQ vector at $0314-$0315 (the displayer installs
//      its own stabilising IRQ handler).
//   3. Patching the displayer's infinite WaitLoop at $310D to RTS.
//   4. Calling the displayer entry point at $3000 (it sets up the VIC,
//      then returns because of the patch).
//   5. Restoring the hardware IRQ vector and the $1000-$1FFF scratch area.
//
// The NUFLI image data is embedded directly at its runtime location
// ($2000-$7A00) from the .nuf file.  The displayer is allowed to set up
// the VIC but is forced to return so the raster IRQ continues to drive
// the game state machine.  Normal multicolor mode is restored later by
// nufli_exit() when the state machine transitions to a dynamic screen.

#include "nufli.h"
#include <string.h>
#include <c64/vic.h>
#include <c64/cia.h>

// Mufflon displayer addresses
#define NUFLI_ENTRY       0x3000
#define NUFLI_WAITLOOP    0x310D

// The title .nuf file is embedded directly at its runtime location.
// The .nuf file starts with a 2-byte load address ($00 $20); skip it.
#pragma section( nufli_runtime, 0)
#pragma region( nufli_runtime, 0x2000, 0x7A00, , , {nufli_runtime}, 0 )
#pragma data(nufli_runtime)

const unsigned char nufli_image[23040] = {
	#embed 23040 2 "data/nufli/screen_title.nuf"
};

#pragma data(data)

// VIC-II/CIA/IRQ state saved on entry so nufli_exit() can restore them.
static unsigned char saved_d011;
static unsigned char saved_d012;
static unsigned char saved_d016;
static unsigned char saved_d018;
static unsigned char saved_d01a;
static unsigned char saved_dd00;
static unsigned char saved_irqvec_lo;
static unsigned char saved_irqvec_hi;

void nufli_show(int screen_id)
{
	(void)screen_id;  // Only the title screen is embedded for now.

	// Save current VIC-II/CIA state.
	saved_d011 = vic.ctrl1;
	saved_d012 = vic.raster;
	saved_d016 = vic.ctrl2;
	saved_d018 = vic.memptr;
	saved_d01a = *(volatile unsigned char *)0xD01A;
	saved_dd00 = cia2.pra;

	// Save the hardware IRQ vector; the displayer overwrites it.
	saved_irqvec_lo = *(volatile unsigned char *)0x0314;
	saved_irqvec_hi = *(volatile unsigned char *)0x0315;

	// The Mufflon displayer at $3000 writes self-modifying speedcode into
	// $1000-$1FFF while it runs.  Save that area so game code/data there is
	// preserved; the bitmap area $E000-$EFFF will be redrawn by the next
	// show_screen() anyway.
	memcpy((char *)0xE000, (char *)0x1000, 0x1000);

	// Patch the displayer's infinite WaitLoop ($310D) to RTS so the entry
	// point at $3000 returns after setting up the VIC.  The WaitLoop is
	// normally `jmp $310D`; we replace it with `rts` ($60).
	*(volatile unsigned char *)NUFLI_WAITLOOP = 0x60;

	// Reference the embedded image so the linker keeps it.  The image is
	// already at $2000-$7A00, which is the displayer's runtime location.
	(void)nufli_image[0];

	// Switch to bank 3, disable sprites, and call the displayer setup.
	__asm {
		sei
		lda $dd00
		and #$fc
		sta $dd00
		lda #$00
		sta $d015
		jsr NUFLI_ENTRY
		cli
	}

	// Restore the hardware IRQ vector immediately so the game's raster IRQ
	// keeps firing.  The displayer's IRQ handler is no longer reachable.
	*(volatile unsigned char *)0x0314 = saved_irqvec_lo;
	*(volatile unsigned char *)0x0315 = saved_irqvec_hi;

	// Restore the $1000-$1FFF scratch area that the displayer overwrote.
	memcpy((char *)0x1000, (char *)0xE000, 0x1000);
}

void nufli_exit(void)
{
	// Disable interrupts briefly while restoring VIC-II state.
	__asm {
		sei
	}

	// Disable all sprites.
	*(unsigned char *)0xD015 = 0;
	*(unsigned char *)0xD01D = 0;
	*(unsigned char *)0xD017 = 0;

	// Restore bank 0 (default C64 VIC bank with screen/bitmap at $C400/$E000).
	cia2.pra = (cia2.pra & 0xFC) | 0x03;

	// Restore saved VIC-II registers, including the raster line and the
	// raster-interrupt enable bit so the game's 50 Hz IRQ keeps firing.
	vic.ctrl1 = saved_d011;
	vic.raster = saved_d012;
	vic.ctrl2 = saved_d016;
	vic.memptr = saved_d018;
	*(volatile unsigned char *)0xD01A = saved_d01a;

	// Acknowledge any pending VIC interrupt.
	*(volatile unsigned char *)0xD019 = 0xFF;

	__asm {
		cli
	}
}
