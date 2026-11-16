/** @file bsp_fault.h
 * @brief Allocation-free shutdown usable from fatal error hooks.
 */
#ifndef BSP_FAULT_H
#define BSP_FAULT_H

/** @brief Disable interrupts, stop TIM2, clear motor outputs, then halt. */
void bsp_fault_stop (void);

#endif /* BSP_FAULT_H */

/*** end of file ***/
