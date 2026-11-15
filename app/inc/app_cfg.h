/** @file app_cfg.h
 * @brief Initial resource budget for the five application tasks.
 */
#ifndef APP_CFG_H
#define APP_CFG_H

/* Priorities exclude the kernel idle and software timer tasks. */
#define APP_EMERGENCY_PRIORITY       (5u)
#define APP_ELEVATOR_PRIORITY        (4u)
#define APP_INPUT_PRIORITY           (3u)
#define APP_MOTOR_PRIORITY_IDLE      (3u)
#define APP_MOTOR_PRIORITY_ACTIVE    (4u)
#define APP_DISPLAY_PRIORITY         (1u)

/* Initial budgets in StackType_t words, not bytes; measure on hardware. */
#define APP_EMERGENCY_STACK_WORDS    (256u)
#define APP_ELEVATOR_STACK_WORDS     (384u)
#define APP_INPUT_STACK_WORDS        (384u)
#define APP_MOTOR_STACK_WORDS        (384u)
#define APP_DISPLAY_STACK_WORDS      (256u)

/* Sampling intervals in milliseconds; tasks convert using pdMS_TO_TICKS. */
#define APP_INPUT_PERIOD_MS          (10u)
#define APP_RFID_PERIOD_MS           (50u)
#define APP_POSITION_PERIOD_MS       (5u)
#define APP_DISPLAY_PERIOD_MS        (200u)

/* The active destination is stored separately from the two waiting floors. */
#define APP_INPUT_QUEUE_LENGTH       (6u)
#define APP_FLOOR_QUEUE_LENGTH       (2u)
#define APP_MOTOR_CMD_QUEUE_LENGTH   (1u)
#define APP_MOTOR_RPT_QUEUE_LENGTH   (4u)
#define APP_POSITION_QUEUE_LENGTH    (1u)
#define APP_DISPLAY_QUEUE_LENGTH     (1u)

#endif /* APP_CFG_H */

/*** end of file ***/
