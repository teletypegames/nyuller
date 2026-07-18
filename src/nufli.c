// nufli.c — NUFLI image display for static screens.
//
// Displays NUFLI format images using delta encoding:
// - A shared base (23040 bytes) is stored once
// - Per-screen deltas (bitmask + values) reconstruct each screen
// - Delta is applied to $2000, then NUFLI displayer is called

#include "nufli.h"
#include <string.h>
#include <c64/vic.h>
#include <c64/cia.h>

// NUFLI data size
#define NUFLI_DATA_SIZE 23040
#define BITMASK_SIZE 2880  // 23040 / 8

// Place NUFLI data in custom sections:
// - Base (23KB) at $1000-$7FFF
// - Title delta (7KB) at $A000-$BFFF (BASIC ROM area)
// Other deltas loaded from disk at runtime (TODO)
#pragma section( nufli_data, 0)
#pragma region( nufli_data, 0x1000, 0x8000, , , {nufli_data} )
#pragma data(nufli_data)

// Shared base data (consensus across all screens)
const unsigned char nufli_base[] = {
	#embed NUFLI_DATA_SIZE 0 "data/nufli/nufli_base.base"
};

#pragma section( nufli_delta, 0)
#pragma region( nufli_delta, 0xA000, 0xC000, , , {nufli_delta} )
#pragma data(nufli_delta)

// Per-screen delta data (bitmask + values)
// Only title delta fits in memory; others need disk loading
const unsigned char delta_title[] = {
	#embed "data/nufli/screen_title.delta"
};

#pragma data(data)

// Screen delta pointers
// Only title delta is embedded; others use title as fallback for now
static const unsigned char *nufli_deltas[] = {
	delta_title,
	delta_title,  // win_hare - TODO: load from disk
	delta_title,  // win_scoot - TODO: load from disk
	delta_title,  // gameover - reuses title
};

// VIC-II registers for restoration after NUFLI
static unsigned char saved_d011;
static unsigned char saved_d016;
static unsigned char saved_d018;
static unsigned char saved_dd00;

// Apply delta to base at $2000
// Delta format: 2880 bytes bitmask + N bytes values
// Each set bit in bitmask indicates a byte that differs from base
static void apply_delta(const unsigned char *delta)
{
	const unsigned char *bitmask = delta;
	const unsigned char *values = delta + BITMASK_SIZE;
	unsigned char *target = (unsigned char *)0x2000;
	unsigned int value_idx = 0;
	
	for (unsigned int byte_idx = 0; byte_idx < NUFLI_DATA_SIZE; byte_idx++) {
		unsigned char mask_byte = bitmask[byte_idx >> 3];
		unsigned char bit = 1 << (byte_idx & 7);
		
		if (mask_byte & bit) {
			target[byte_idx] = values[value_idx++];
		}
	}
}

void nufli_show(int screen_id)
{
	// Save current VIC-II state
	saved_d011 = vic.ctrl1;
	saved_d016 = vic.ctrl2;
	saved_d018 = vic.memptr;
	saved_dd00 = cia2.pra;

	// Copy base to $2000
	memcpy((unsigned char *)0x2000, nufli_base, NUFLI_DATA_SIZE);
	
	// Apply delta to reconstruct this screen
	if (screen_id >= 0 && screen_id < 4) {
		apply_delta(nufli_deltas[screen_id]);
	}

	// Switch to bank 3 and call NUFLI displayer
	__asm {
		sei
		lda $dd00
		and #$fc
		sta $dd00
		lda #$00
		sta $d015
		jsr $3000
	}
}

void nufli_exit(void)
{
	// Restore VIC-II state
	__asm {
		sei
		lda #$00
		sta $d015
		sta $d01d
		sta $d017
		lda $dd00
		ora #$03
		sta $dd00
		cli
	}
	
	// Restore saved VIC-II registers
	vic.ctrl1 = saved_d011;
	vic.ctrl2 = saved_d016;
	vic.memptr = saved_d018;
	cia2.pra = saved_dd00;
}
