#include "input.h"

static const unsigned short cia_port_base[2] = { 0xDC00, 0xDC01 };

char input_fire(int port)
{
	volatile char *p = (volatile char *)cia_port_base[port & 1];
	return ((*p) & 0x10) == 0;
}
