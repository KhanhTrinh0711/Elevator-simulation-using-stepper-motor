/** @file bsp_gpio.h
 * @brief Board peripheral API.
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stm32f1xx_hal.h"

/* Stepper motor */
#define MOTOR_IN1_PORT GPIOB
#define MOTOR_IN1_PIN  GPIO_PIN_10

#define MOTOR_IN2_PORT GPIOB
#define MOTOR_IN2_PIN  GPIO_PIN_11

#define MOTOR_IN3_PORT GPIOB
#define MOTOR_IN3_PIN  GPIO_PIN_12

#define MOTOR_IN4_PORT GPIOB
#define MOTOR_IN4_PIN  GPIO_PIN_13

/* Floor buttons */
#define BUTTON_FLOOR_1_PIN GPIO_PIN_2
#define BUTTON_FLOOR_2_PIN GPIO_PIN_3
#define BUTTON_FLOOR_3_PIN GPIO_PIN_4

/* Emergency button */
#define BUTTON_EMERGENCY_PIN GPIO_PIN_0

/* Limit switch */
#define LIMIT_SWITCH_PIN GPIO_PIN_1

/* Status LEDs */
#define LED_GREEN_PIN GPIO_PIN_14
#define LED_RED_PIN   GPIO_PIN_15

/** @brief Configure motor outputs, button inputs and status LEDs. */
void BSP_GPIO_Init (void);

#endif /* BSP_GPIO_H */

/*** end of file ***/
