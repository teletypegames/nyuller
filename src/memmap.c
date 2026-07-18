#include "memmap.h"

void memmap_setup(void)
{
	mmap_trampoline();
	mmap_set(MMAP_NO_ROM);
}

// memmap_restore() — intentionally unused. The game runs an infinite
// main loop and is designed to run until power-off. If a graceful exit
// path is ever added (e.g. NMI handler or RUN/STOP key), call this
// before returning to BASIC so the memory config matches user expectations.
void memmap_restore(void)
{
	mmap_set(MMAP_ROM);
}
