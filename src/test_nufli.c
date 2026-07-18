#include "memmap.h"
#include "nufli.h"
#include <c64/vic.h>

int main(void)
{
    memmap_setup();
    
    // Just try to access the NUFLI data
    const unsigned char *data = nufli_get_screen(0);
    
    // Infinite loop
    for (;;)
        ;
    
    return 0;
}
