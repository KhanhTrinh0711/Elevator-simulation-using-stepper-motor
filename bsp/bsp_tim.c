
/**
 * @file bsp_tim.c
 * @brief Board TIM2 initialization and control.
 */

#include <stdint.h>

#include "bsp_tim.h"

#define BSP_TIM2_COUNTER_HZ       (1000000u)
#define BSP_TIM2_INTERRUPT_HZ     (1000u)
#define BSP_TIM2_PERIOD           (999u)

TIM_HandleTypeDef htim2;

/**
 * @brief Calculate the actual TIM2 input clock.
 * @return TIM2 input clock frequency in Hz.
 */
static uint32_t
BSP_TIM2_GetClock (void)
{
    RCC_ClkInitTypeDef clock_config = {0};
    uint32_t flash_latency = 0u;
    uint32_t timer_clock = 0u;

    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);

    timer_clock = HAL_RCC_GetPCLK1Freq();

    /*
     * STM32F1:
     * APB1 prescaler = 1: TIM2 clock = PCLK1.
     * APB1 prescaler > 1: TIM2 clock = 2 * PCLK1.
     */
    if (RCC_HCLK_DIV1 != clock_config.APB1CLKDivider)
    {
        timer_clock *= 2u;
    }

    return timer_clock;
}

/**
 * @brief Initialize TIM2 for a 1 ms periodic interrupt.
 * @return HAL_OK if successful, otherwise HAL_ERROR.
 */
HAL_StatusTypeDef
BSP_TIM2_Init (void)
{
    TIM_ClockConfigTypeDef clock_source = {0};
    TIM_MasterConfigTypeDef master_config = {0};

    uint32_t timer_clock = 0u;
    uint32_t prescaler = 0u;
    uint32_t flash_latency = 0u;

    /* Enable TIM2 peripheral clock. */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Calculate the actual TIM2 input clock. */
    timer_clock = BSP_TIM2_GetClock();

    /*
     * Configure the timer counter to 1 MHz.
     * The prescaler must fit in the 16-bit TIM2 PSC register.
     */
    if ((timer_clock < BSP_TIM2_COUNTER_HZ) ||
        (0u != (timer_clock % BSP_TIM2_COUNTER_HZ)))
    {
        return HAL_ERROR;
    }

    prescaler = (timer_clock / BSP_TIM2_COUNTER_HZ) - 1u;

    if (prescaler > 65535u)
    {
        return HAL_ERROR;
    }

    /* Configure TIM2 base parameters. */
    htim2.Instance = TIM2;

    htim2.Init.Prescaler = prescaler;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = BSP_TIM2_PERIOD;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Select the internal timer clock. */
    clock_source.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

    if (HAL_TIM_ConfigClockSource(
            &htim2, &clock_source) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Configure timer synchronization. */
    master_config.MasterOutputTrigger = TIM_TRGO_RESET;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(
            &htim2, &master_config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Configure TIM2 interrupt priority. */
    HAL_NVIC_SetPriority(TIM2_IRQn, 6u, 0u);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    return HAL_OK;
}

/**
 * @brief Start TIM2 in interrupt mode.
 * @return HAL status.
 */
HAL_StatusTypeDef
BSP_TIM2_Start (void)
{
    return HAL_TIM_Base_Start_IT(&htim2);
}

/**
 * @brief Stop TIM2 interrupt generation.
 * @return HAL status.
 */
HAL_StatusTypeDef
BSP_TIM2_Stop (void)
{
    return HAL_TIM_Base_Stop_IT(&htim2);
}

/*** end of file ***/
