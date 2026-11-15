/**
 * @file task_motor.h
 * @brief MotorTask interface.
 */

#ifndef TASK_MOTOR_H
#define TASK_MOTOR_H

#include "FreeRTOS.h"

/**
 * @brief Create the motor control task.
 *
 * @return pdPASS if the task was created successfully.
 *         An error code otherwise.
 */
BaseType_t task_motor_create(void);

#endif /* TASK_MOTOR_H */

/*** end of file ***/
