
#include "hal_stepper.h"
#include "bsp_gpio.h"

/* 4 chân điều khiển motor */
#define STEPPER_GPIO_PORT       GPIOB

#define STEPPER_GPIO_PINS       (GPIO_PIN_10 | \
                                 GPIO_PIN_11 | \
                                 GPIO_PIN_12 | \
                                 GPIO_PIN_13)

/* TIM2 ngắt mỗi 1 ms.
   Cứ 10 lần ngắt thì chuyển pha một lần. */
#define STEPPER_STEP_INTERVAL   10U

/* Chuỗi điều khiển half-step */
static const uint16_t step_sequence[8] =
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

/* Biến trạng thái motor */
static volatile uint8_t step_index = 0;
static volatile uint8_t step_busy = 0;
static volatile uint8_t direction = STEPPER_DIR_CW;

static volatile uint32_t remaining_steps = 0;
static volatile uint16_t timer_count = 0;

/* Ghi trạng thái pha ra GPIO */
static void Stepper_WritePhase(uint8_t index)
{
    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      STEPPER_GPIO_PINS,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      step_sequence[index],
                      GPIO_PIN_SET);
}

void Stepper_Init(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    step_index = 0;
    step_busy = 0;
    direction = STEPPER_DIR_CW;
    remaining_steps = 0;
    timer_count = 0;

    __set_PRIMASK(primask);

    HAL_GPIO_WritePin(STEPPER_GPIO_PORT,
                      STEPPER_GPIO_PINS,
                      GPIO_PIN_RESET);
}

HAL_StatusTypeDef Stepper_Start(uint32_t steps,
                                Stepper_Direction_t dir)
{
    if (steps == 0U)
    {
        return HAL_ERROR;
    }

    if ((dir != STEPPER_DIR_CW) &&
        (dir != STEPPER_DIR_CCW))
    {
        return HAL_ERROR;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    /* Không nhận lệnh mới khi motor đang chạy */
    if (step_busy != 0U)
    {
        __set_PRIMASK(primask);
        return HAL_BUSY;
    }

    step_index = 0;
    direction = (uint8_t)dir;
    remaining_steps = steps;
    timer_count = 0;
    step_busy = 1;

    /* Bật pha đầu tiên ngay khi bắt đầu */
    Stepper_WritePhase(step_index);

    __set_PRIMASK(primask);

    return HAL_OK;
}

void Stepper_Stop(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    step_busy = 0;
    remaining_steps = 0;
    timer_count = 0;

    __set_PRIMASK(primask);

    /*
     * Giữ nguyên pha hiện tại để giữ mô-men.
     * Không tắt GPIO ở đây.
     */
}

uint8_t Stepper_IsBusy(void)
{
    return step_busy;
}

void Stepper_OnTimerElapsed(void)
{
    if (step_busy == 0U)
    {
        return;
    }

    timer_count++;

    if (timer_count < STEPPER_STEP_INTERVAL)
    {
        return;
    }

    timer_count = 0;

    /* Chuyển pha theo chiều quay */
    if (direction == STEPPER_DIR_CW)
    {
        step_index++;

        if (step_index >= 8U)
        {
            step_index = 0;
        }
    }
    else
    {
        if (step_index == 0U)
        {
            step_index = 7U;
        }
        else
        {
            step_index--;
        }
    }

    Stepper_WritePhase(step_index);

    remaining_steps--;

    if (remaining_steps == 0U)
    {
        step_busy = 0;
    }
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        Stepper_OnTimerElapsed();
    }
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        /* Emergency: dừng động cơ */
        Stepper_Stop();
    }
    else if (GPIO_Pin == GPIO_PIN_1)
    {
        /* Limit switch: chỉ dừng nếu motor đang chạy */
        if (Stepper_IsBusy())
        {
            Stepper_Stop();
        }
    }
}
