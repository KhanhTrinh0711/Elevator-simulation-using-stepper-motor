#include "bsp_i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static I2C_HandleTypeDef hi2c1;
static SemaphoreHandle_t i2c1_mutex;
static uint8_t initialized;

static int context_valid(void)
{
    return __get_IPSR() == 0U && __get_PRIMASK() == 0U &&
           __get_BASEPRI() == 0U;
}

HAL_StatusTypeDef BSP_I2C1_Init(void)
{
    GPIO_InitTypeDef pins = {0};
    HAL_StatusTypeDef status;

    if (!context_valid() ||
        xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return HAL_ERROR;
    }
    if (initialized) {
        return HAL_OK;
    }
    if (i2c1_mutex == NULL) {
        i2c1_mutex = xSemaphoreCreateMutex();
        if (i2c1_mutex == NULL) {
            return HAL_ERROR;
        }
    }

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_AFIO_REMAP_I2C1_DISABLE();
    pins.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    pins.Mode = GPIO_MODE_AF_OD;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &pins);

    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000U;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0U;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0U;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    status = HAL_I2C_Init(&hi2c1);
    initialized = (status == HAL_OK);
    return status;
}

static HAL_StatusTypeDef lock_bus(uint8_t address, const uint8_t *data,
                                 uint16_t size, BaseType_t *locked)
{
    *locked = pdFALSE;
    if (!context_valid() || !initialized || data == NULL || size == 0U ||
        size > BSP_I2C1_MAX_TRANSFER_SIZE || address < 0x08U || address > 0x77U) {
        return HAL_ERROR;
    }
    BaseType_t state = xTaskGetSchedulerState();
    if (state == taskSCHEDULER_SUSPENDED) {
        return HAL_ERROR;
    }
    if (state == taskSCHEDULER_RUNNING) {
        if (xSemaphoreTake(i2c1_mutex, pdMS_TO_TICKS(BSP_I2C1_TIMEOUT_MS)) != pdTRUE) {
            return HAL_TIMEOUT;
        }
        *locked = pdTRUE;
    }
    return HAL_OK;
}

HAL_StatusTypeDef BSP_I2C1_Write(uint8_t address, const uint8_t *data,
                               uint16_t size)
{
    BaseType_t locked;
    HAL_StatusTypeDef status = lock_bus(address, data, size, &locked);
    if (status != HAL_OK) {
        return status;
    }
    /* STM32 HAL takes a mutable pointer but does not modify transmit data. */
    status = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)address << 1,
                                    (uint8_t *)data, size, BSP_I2C1_TIMEOUT_MS);
    if (locked) {
        xSemaphoreGive(i2c1_mutex);
    }
    return status;
}

HAL_StatusTypeDef BSP_I2C1_ReadRegister(uint8_t address, uint8_t reg,
                                      uint8_t *data, uint16_t size)
{
    BaseType_t locked;
    HAL_StatusTypeDef status = lock_bus(address, data, size, &locked);
    if (status != HAL_OK) {
        return status;
    }
    status = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)address << 1, reg,
                             I2C_MEMADD_SIZE_8BIT, data, size,
                             BSP_I2C1_TIMEOUT_MS);
    if (locked) {
        xSemaphoreGive(i2c1_mutex);
    }
    return status;
}
