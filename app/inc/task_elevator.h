/** @file task_elevator.h
 * @brief Sole owner of FSM, FIFO, authorization and display snapshots.
 */
#ifndef TASK_ELEVATOR_H
#define TASK_ELEVATOR_H

#include "FreeRTOS.h"

/** @brief Create after IPC/peripherals, before scheduler start. */
BaseType_t task_elevator_create (void);

#endif /* TASK_ELEVATOR_H */

/*** end of file ***/
