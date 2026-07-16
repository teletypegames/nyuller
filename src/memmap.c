#include "memmap.h"

void memmap_setup(void)
{
	mmap_trampoline();
	mmap_set(MMAP_RAM);
	mmap_set(MMAP_NO_ROM);
}

void memmap_restore(void)
{
	mmap_set(MMAP_ROM);
}
