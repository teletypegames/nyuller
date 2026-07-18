#ifndef NYULLER_MEMMAP_H
#define NYULLER_MEMMAP_H

#include <c64/memmap.h>

// memmap_setup() — bank out the KERNAL + BASIC + CHAR ROMs so we get
// the $E000-$FFFF region as free RAM for the multicolor bitmap, and
// $D000-$DFFF shows I/O (not the CHAR ROM) so the VIC can be configured
// and color RAM is accessible.
//
// Idempotent: call once at startup, before any VIC config or bitmap
// copy.  After this returns, the program code is running out of the
// 38 KB contiguous region at $0900-$A000 (the default Oscar64 main
// region; verified in the .map file).
//
// memmap_restore() puts the C64 back in its power-on memory config.
// Call it just before exit, so a soft-reset / second-run of the
// program behaves the way the user expects (BASIC prompt etc.).

void memmap_setup(void);
void memmap_restore(void);

#pragma compile("memmap.c")

#endif
