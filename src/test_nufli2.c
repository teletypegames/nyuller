#include "memmap.h"
#include "nufli.h"
#include <c64/vic.h>

int main(void)
{
    memmap_setup();
    
    // Try to display NUFLI image
    const unsigned char *data = nufli_get_screen(0);
    nufli_show(data);
    
    // Infinite loop
    for (;;)
        ;
    
    return 0;
}
