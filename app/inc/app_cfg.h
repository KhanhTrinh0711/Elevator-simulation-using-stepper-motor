
/** @file app_cfg.h
 * @brief Initial resource budget and configuration for application tasks.
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

/* Sampling intervals in milliseconds; convert using pdMS_TO_TICKS. */
#define APP_INPUT_PERIOD_MS          (10u)
#define APP_RFID_PERIOD_MS           (50u)
#define APP_RFID_RETRY_MS            (1000u)
#define APP_POSITION_PERIOD_MS       (5u)
#define APP_DISPLAY_PERIOD_MS        (200u)

/* Authorization is measured from the AUTH event timestamp. */
#define APP_AUTH_WINDOW_MS           (3000u)

/* Input and RFID handling. */
#define APP_INPUT_DEBOUNCE_SAMPLES   (3u)
#define APP_RFID_NO_CARD_SAMPLES     (3u)
#define APP_INPUT_UID_CAPACITY       (10u)

/* Queue lengths shared by application tasks. */
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
#define APP_EMERGENCY_WAIT_MS         (10000u)
#define APP_EMERGENCY_RELEASE_MS      (30u)
#define APP_CONTROL_PERIOD_MS        (10u)
#define APP_SENSOR_RETRY_MS          (100u)
#define APP_HOME_SETTLE_MS           (50u)
#define APP_HOME_VERIFY_TIMEOUT_MS   (1000u)

/* Board measurements: zero means UNCONFIGURED, never guess these values.
 * Positive encoder counts must mean upward cabin travel. If counts increase
 * downward, correct the sensor mounting/direction before enabling motion.
 * Times are ms, positions are encoder counts, intervals are TIM2 1 ms ticks.
 */
#ifndef APP_MOTOR_MECHANICS_CALIBRATED
#define APP_MOTOR_MECHANICS_CALIBRATED  (0u)
#endif
#ifndef APP_MOTOR_STEPS_PER_ENCODER_REV
#define APP_MOTOR_STEPS_PER_ENCODER_REV (0u)
#endif
#ifndef APP_MOTOR_POSITIVE_DIR_CW
#define APP_MOTOR_POSITIVE_DIR_CW       (2u)
#endif
#ifndef APP_MOTOR_MOVE_TIMEOUT_MS
#define APP_MOTOR_MOVE_TIMEOUT_MS       (0u)
#endif
#ifndef APP_MOTOR_STEP_INTERVAL_MS
#define APP_MOTOR_STEP_INTERVAL_MS      (0u)
#endif
#ifndef APP_HOME_STEP_INTERVAL_MS
#define APP_HOME_STEP_INTERVAL_MS       (0u)
#endif
#ifndef APP_HOME_SEARCH_STEPS
#define APP_HOME_SEARCH_STEPS           (0u)
#endif
#ifndef APP_HOME_RELEASE_STEPS
#define APP_HOME_RELEASE_STEPS          (0u)
#endif
#ifndef APP_HOME_SEARCH_TIMEOUT_MS
#define APP_HOME_SEARCH_TIMEOUT_MS      (0u)
#endif
#ifndef APP_HOME_RELEASE_TIMEOUT_MS
#define APP_HOME_RELEASE_TIMEOUT_MS     (0u)
#endif
#ifndef APP_MOTOR_STALL_TIMEOUT_MS
#define APP_MOTOR_STALL_TIMEOUT_MS      (0u)
#endif
#ifndef APP_FLOOR_TWO_COUNTS
#define APP_FLOOR_TWO_COUNTS            (0)
#endif
#ifndef APP_FLOOR_THREE_COUNTS
#define APP_FLOOR_THREE_COUNTS          (0)
#endif
#ifndef APP_MOTOR_ARRIVAL_TOLERANCE_COUNTS
#define APP_MOTOR_ARRIVAL_TOLERANCE_COUNTS (0u)
#endif

#endif /* APP_CFG_H */

/*** end of file ***/
