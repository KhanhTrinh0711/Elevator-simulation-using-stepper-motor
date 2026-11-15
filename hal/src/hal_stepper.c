
/**
 * @file hal_stepper.c
 * @brief Stepper motor driver.
 */
#include <stdint.h>

#include "hal_stepper.h"
#include "bsp_gpio.h"

#define STEPPER_GPIO_PORT       GPIOB

#define STEPPER_GPIO_PINS       (GPIO_PIN_10 | \
                                 GPIO_PIN_11 | \
                                 GPIO_PIN_12 | \
                                 GPIO_PIN_13)

#define STEPPER_STEP_INTERVAL   (10u)
#define STEPPER_PHASE_COUNT     (8u)

static const uint16_t step_sequence[STEPPER_PHASE_COUNT] =
{
    GPIO_PIN_10,
    GPIO_PIN_10 | GPIO_PIN_11,
    GPIO_PIN_11,
    GPIO_PIN_11 | GPIO_PIN_12,
    GPIO_PIN_12,
    GPIO_PIN_12 | GPIO_PIN_13,
    GPIO_PIN_13,
    GPIO_PIN_13 | GPIO_PIN_10
};

static volatile uint8_t step_index = 0u;
static volatile uint8_t step_busy = 0u;
static volatile uint8_t direction = STEPPER_DIR_CW;
static volatile uint8_t emergency_latched = 0u;
static volatile uint8_t limit_event_pending = 0u;

static volatile uint32_t remaining_steps = 0u;
static volatile uint16_t timer_count = 0u;

/**
 * @brief Apply one half-step phase to the motor outputs.
 * @param index Phase index.
 */
static void
Stepper_WritePhase (uint8_t index)
{
    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      STEPPER_GPIO_PINS,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      step_sequence[index],
                      GPIO_PIN_SET);
}

/**
 * @brief Initialize the stepper driver.
 */
void
Stepper_Init (void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    step_index = 0u;
    step_busy = 0u;
    direction = STEPPER_DIR_CW;
    emergency_latched = 0u;
    limit_event_pending = 0u;
    remaining_steps = 0u;
    timer_count = 0u;

    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      STEPPER_GPIO_PINS,
                      GPIO_PIN_RESET);

    __set_PRIMASK(primask);
}

/**
 * @brief Start the motor without resetting its current phase.
 */
HAL_StatusTypeDef
Stepper_Start (uint32_t steps, Stepper_Direction_t dir)
{
    uint32_t primask = __get_PRIMASK();
    HAL_StatusTypeDef result = HAL_OK;

    if ((0u == steps) ||
        ((STEPPER_DIR_CW != dir) &&
         (STEPPER_DIR_CCW != dir)))
    {
        return HAL_ERROR;
    }

    __disable_irq();

    if (0u != emergency_latched)
    {
        result = HAL_BUSY;
    }
    else if (0u != step_busy)
    {
        result = HAL_BUSY;
    }
    else
    {
        direction = (uint8_t)dir;
        remaining_steps = steps;
        timer_count = 0u;
        step_busy = 1u;

        /* Re-energize the current phase without advancing it. */
        Stepper_WritePhase(step_index);
    }

    __set_PRIMASK(primask);

    return result;
}

/**
 * @brief Stop the motor and retain the current phase.
 */
void
Stepper_Stop (void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    step_busy = 0u;
    remaining_steps = 0u;
    timer_count = 0u;

    __set_PRIMASK(primask);
}

/**
 * @brief Stop the motor and latch the emergency state.
 */
void
Stepper_EmergencyStop (void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    emergency_latched = 1u;
    step_busy = 0u;
    remaining_steps = 0u;
    timer_count = 0u;

    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      STEPPER_GPIO_PINS,
                      GPIO_PIN_RESET);

    __set_PRIMASK(primask);
}

/**
 * @brief Recover after the emergency input has been released.
 */
HAL_StatusTypeDef
Stepper_EmergencyRecover (void)
{
    uint32_t primask;
    HAL_StatusTypeDef result = HAL_OK;

    if (GPIO_PIN_SET != HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0))
    {
        return HAL_BUSY;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0))
    {
        result = HAL_BUSY;
    }
    else
    {
        emergency_latched = 0u;
    }

    __set_PRIMASK(primask);

    return result;
}

/**
 * @brief Return the emergency latch state.
 */
uint8_t
Stepper_IsEmergencyLatched (void)
{
    return emergency_latched;
}

/**
 * @brief Return the motor busy state.
 */
uint8_t
Stepper_IsBusy (void)
{
    return step_busy;
}

/**
 * @brief Return the pending limit-switch event state.
 */
uint8_t
Stepper_LimitEventPending (void)
{
    return limit_event_pending;
}

/**
 * @brief Clear the pending limit-switch event.
 */
void
Stepper_ClearLimitEvent (void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    limit_event_pending = 0u;
    __set_PRIMASK(primask);
}

/**
 * @brief Advance the motor by one phase when the interval expires.
 */
void
Stepper_OnTimerElapsed (void)
{
    if ((0u != emergency_latched) || (0u == step_busy))
    {
        return;
    }

    timer_count++;

    if (timer_count < STEPPER_STEP_INTERVAL)
    {
        return;
    }

    timer_count = 0u;

    if (STEPPER_DIR_CW == direction)
    {
        step_index++;

        if (step_index >= STEPPER_PHASE_COUNT)
        {
            step_index = 0u;
        }
    }
    else
    {
        if (0u == step_index)
        {
            step_index = STEPPER_PHASE_COUNT - 1u;
        }
        else
        {
            step_index--;
        }
    }

    Stepper_WritePhase(step_index);

    remaining_steps--;

    if (0u == remaining_steps)
    {
        step_busy = 0u;
    }
}

/**
 * @brief HAL timer callback.
 */
void
HAL_TIM_PeriodElapsedCallback (TIM_HandleTypeDef *htim)
{
    if (TIM2 == htim->Instance)
    {
        Stepper_OnTimerElapsed();
    }
}

/**
 * @brief HAL external interrupt callback.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_PIN_0 == GPIO_Pin)
    {
        Stepper_EmergencyStop();
    }
    else if (GPIO_PIN_1 == GPIO_Pin)
    {
        limit_event_pending = 1u;
    }
    else
    {
        /* Ignore unused EXTI pins. */
    }
}

/*** end of file ***/
