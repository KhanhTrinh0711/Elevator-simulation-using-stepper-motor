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
/* Elevator owns expiry, measured from AUTH event timestamp. */
#define APP_AUTH_WINDOW_MS           (3000u)
#define APP_POSITION_PERIOD_MS       (5u)
#define APP_DISPLAY_PERIOD_MS        (200u)

/* The active destination is stored separately from the two waiting floors. */
#define APP_INPUT_QUEUE_LENGTH       (6u)
#define APP_FLOOR_QUEUE_LENGTH       (2u)
#define APP_MOTOR_CMD_QUEUE_LENGTH   (1u)
#define APP_MOTOR_RPT_QUEUE_LENGTH   (4u)
#define APP_POSITION_QUEUE_LENGTH    (1u)
#define APP_DISPLAY_QUEUE_LENGTH     (1u)
/* Motor position and reference supervision. */
#define APP_FLOOR_COUNT                  (3u)
#define APP_POSITION_MAX_GAP_MS          \
    (APP_POSITION_PERIOD_MS * 2u)

#define APP_MOTOR_MAGNET_CHECK_PERIOD_MS (50u)
#define APP_MOTOR_HOME_TIMEOUT_MS        (2000u)
#define APP_MOTOR_HOME_STABLE_SAMPLES    (10u)
#define APP_MOTOR_ARRIVAL_TOLERANCE_COUNTS (0u)

/*
 * Keep MOVE disabled until the mechanical ratio and direction
 * have been measured on the actual model.
 *
 * APP_MOTOR_POSITIVE_DIR_CW:
 *   1u = positive encoder counts correspond to STEPPER_DIR_CW.
 *   0u = positive encoder counts correspond to STEPPER_DIR_CCW.
 *   2u = not measured yet.
 */
#define APP_MOTOR_MECHANICS_CALIBRATED   (0u)
#define APP_MOTOR_STEPS_PER_ENCODER_REV  (0u)
#define APP_MOTOR_POSITIVE_DIR_CW         (2u)
#define APP_MOTOR_MOVE_TIMEOUT_MS         (0u)
#endif /* APP_CFG_H */

/*** end of file ***/
