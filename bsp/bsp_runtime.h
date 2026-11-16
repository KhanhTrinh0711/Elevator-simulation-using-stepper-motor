/** @file bsp_runtime.h
 * @brief Newlib initialization hook for the ST-owned reset sequence.
 */
#ifndef BSP_RUNTIME_H
#define BSP_RUNTIME_H

/** @brief Framework-required name called by __libc_init_array. */
void _init (void);

#endif /* BSP_RUNTIME_H */

/*** end of file ***/
