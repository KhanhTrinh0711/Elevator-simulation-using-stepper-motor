
#ifndef HAL_STEPPER_H
#define HAL_STEPPER_H

#include "stm32f1xx_hal.h"

typedef enum
{
    STEPPER_DIR_CW = 0,
    STEPPER_DIR_CCW
} Stepper_Direction_t;

/* Khởi tạo driver */
void Stepper_Init(void);

/* Bắt đầu quay với số bước và chiều quay */
HAL_StatusTypeDef Stepper_Start(uint32_t steps,
                                Stepper_Direction_t direction);

/* Dừng động cơ */
void Stepper_Stop(void);

/* Kiểm tra động cơ đang quay */
uint8_t Stepper_IsBusy(void);

/* Được gọi từ callback ngắt TIM2 */
void Stepper_OnTimerElapsed(void);

#endif
