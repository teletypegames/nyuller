#include "memmap.h"
#include <c64/vic.h>

int main(void)
{
    memmap_setup();
    
    // Set border color to red
    vic.color_border = 2;
    
    // Infinite loop
    for (;;)
        ;
    
    return 0;
}
