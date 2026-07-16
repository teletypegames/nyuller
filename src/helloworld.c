// helloworld.c — minimal C64 program in C with Oscar64
//
// Builds a .prg that prints "Hello World" on the C64 text screen and exits.
//
// Build:  ../scripts/build.sh
// Run:    x64 helloworld.prg   (or use ../scripts/build.sh -e to run in
//                              the built-in oscar64 emulator)

#include <stdio.h>

int main(void)
{
	// CHR$(147) is PETSCII "clear screen" — same as printf("\f") in C but
	// emitted as the right byte for the C64's PETSCII character set.
	putchar(147);

	// The p"" prefix tells Oscar64 to emit the string as PETSCII, not ASCII.
	// Without it the C64's character ROM would render garbage because the
	// character generator maps 'H' to PETSCII 8, not ASCII 72.
	printf(p"Hello World\n");

	return 0;
}
