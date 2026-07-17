// tick.c — install a single raster IRQ at line 311 (PAL stable
// line) that calls the per-frame game tick handler.
//
// The Oscar64 rasterirq library provides the boilerplate: ISR stub,
// table management, sort, and start.  We add the two C64-specific
// bits the library doesn't do for us when running with KERNAL banked
// out:
//
//   1. Mask CIA 1 and CIA 2 IRQs (write 0x7F to $DC0D/$DD0D).  This
//      stops the jiffy-clock Timer A from latching an IRQ that would
//      fire the moment we RTI out of the raster IRQ.  (The rirq
//      library only does this for the *_kernal() init paths, which
//      call into the KERNAL ISR at $EA31 to acknowledge; we can't
//      because KERNAL is banked out.)
//
//   2. Use rirq_init(false), which installs the rirq_isr_ram_io
//      handler at the hardware IRQ vector ($FFFE/$FFFF).  This
//      handler does NOT call into KERNAL — it acks the raster IRQ
//      (asl $d019) and returns directly via rti.  Combined with the
//      CIA mask, this means the only IRQ we ever service is the
//      raster IRQ.
//
// The RIRQ code is a single "wait for line, then JSR frame_tick_handler,
// RTS" stub.  We use rirq_call() to install the JSR.
//
// **PAL line 311 and the 9-bit raster counter.**  PAL frames are 312
// lines (0..311), so the stable line 311 is outside the 8-bit
// $D012 range.  The VIC's raster register is 9 bits: the high bit
// is bit 7 of $D011 (VIC_CTRL1_RST8), and the low 8 bits are $D012.
// The oscar64 rirq library's `rirq_set(n, row, code)` takes a `byte
// row` (0..255) and writes (row - 1) to $D012 internally, so it
// cannot directly address line 311.  The workaround is:
//
//   - Pass row = 56 to rirq_set (i.e. 311 - 256 + 1, where the +1
//     accounts for the library's "one line below" convention).
//   - Set VIC_CTRL1_RST8 = 1 after rirq_sort and BEFORE enabling
//     CPU IRQ, so the raster comparison becomes (1 << 8) | 55 = 311.
//
// We can't use rirq_start() because it clears RST8 and overwrites
// $D012 with 100 (a "kick start" line that lets the first IRQ fire
// quickly).  Instead we do the equivalent of rirq_start inline, with
// RST8 left at 1 and $D012 left at the rirq_sort value.
//
// **File naming.**  This file is named tick.c (not rasterirq.c)
// because the oscar64 library's rasterirq.c does `#include
// "rasterirq.h"` to find its own rasterirq.h, and that include
// resolves relative to the compile CWD.  If our header were also
// named rasterirq.h, the library would pick up ours and the
// NUM_IRQS / RIRQCode defines would be missing.  The naming is
// purely a workaround for the library's include style.

#include "tick.h"
#include "game.h"
#include <c64/vic.h>
#include <c64/cia.h>
#include <c64/rasterirq.h>

// The single RIRQ code slot.  One IRQ = one wait + one JSR + one RTS.
static RIRQCode frame_tick;

// The per-frame handler, called from the raster IRQ at line 311.
//
// Marked __interrupt so the compiler saves/restores any zero-page
// registers the function (or game_step) uses.  This matches the
// autocrawler.c pattern in the oscar64 samples.  Note that this
// function is NOT the 6502 ISR — the rirq_isr_ram_io stub installed
// by rirq_init is the actual ISR.  This function is called via JSR
// from the rirq_isr, and returns with RTS.  A/X/Y are saved by the
// rirq_isr, so we can clobber them freely.
//
// Also marked __noinline to prevent `-O3` from inlining the entire
// state machine (game_step + audio_state_step + all per-state step
// functions) into the IRQ handler.  Without __noinline, the
// optimizer's aggressive inlining makes the handler 6000+ bytes
// long, which would overrun its raster-line budget and break the
// 50 Hz timing.  game_step is large but called only from the IRQ,
// so the call/return overhead is negligible.
__interrupt __noinline void frame_tick_handler(void)
{
	frame_count++;
	game_step();
}

void rasterirq_setup(void)
{
	// 1. Mask all CIA 1 and CIA 2 interrupt sources.  The ICR at
	//    $DC0D/$DD0D is a set/clear register: bit 7 = 0 means
	//    "clear", bits 0-4 = 0x1F means "clear all source mask
	//    bits".  Writing 0x7F disables every source.  A second
	//    write acknowledges any latched IRQ; reading would do the
	//    same but writing is fine.
	cia1.icr = 0x7f;
	cia2.icr = 0x7f;
	cia1.icr = 0x7f;
	cia2.icr = 0x7f;

	// 2. Install the raster IRQ system.  false = use the hardware
	//    IRQ vector at $FFFE, no KERNAL continuation (KERNAL is
	//    banked out by memmap_setup).
	rirq_init(false);

	// 3. Build the RIRQ code: a single call to frame_tick_handler.
	//    size=1 = one op slot.  rirq_call at index 0 replaces the
	//    STY $xxxx stub at offset 9 with a JSR frame_tick_handler.
	//    The resulting code is: wait + JSR handler + RTS.
	rirq_build(&frame_tick, 1);
	rirq_call(&frame_tick, 0, frame_tick_handler);

	// 4. Place this RIRQ at line 56 in the rirq library's 8-bit
	//    view.  Combined with RST8=1 (set in step 6 below) the
	//    actual raster comparison becomes 256 + (56 - 1) = 311.
	rirq_set(0, 56, &frame_tick);

	// 5. Sort the RIRQ list.  This also writes $D012 = 56 - 1 = 55
	//    (the low byte of the 9-bit row 311) and sets nextIRQ = 0.
	rirq_sort();

	// 6. Set the high bit of the 9-bit raster register.  This
	//    makes the VIC compare the raster counter against 256 + 55
	//    = 311 instead of just 55.  Must happen before we enable
	//    CPU IRQ (CLI), otherwise the first IRQ might fire at the
	//    wrong line.
	vic.ctrl1 |= VIC_CTRL1_RST8;

	// 7. Start the raster IRQ.  We can't use rirq_start() because
	//    it would clear RST8 and overwrite $D012 with 100.  Instead
	//    we do the same thing minus those two writes: acknowledge
	//    any pending VIC IRQ, then enable CPU IRQ.
	__asm {
		asl $d019
		cli
	}
}
