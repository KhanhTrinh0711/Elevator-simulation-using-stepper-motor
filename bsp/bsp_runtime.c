/** @file bsp_runtime.c
 * @brief Replace generic CRT startup, retaining Newlib constructor support.
 */
#include "bsp_runtime.h"

void
_init (void)
{
    /* ST Reset_Handler initializes data/BSS and calls __libc_init_array. */
}

/*** end of file ***/
