/** @file bsp_i2c.c
 * @brief Serialized I2C1 transport for OLED and encoder.
 */
#include "bsp_i2c.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* Init publishes these before scheduler start; mutex guards transactions. */
static I2C_HandleTypeDef gh_i2c         = {0};
static SemaphoreHandle_t gp_bus_mutex   = NULL;
static bool              gb_initialized = false;

static bool i2c_context_valid (void);
static HAL_StatusTypeDef i2c_lock_bus (uint8_t address,
    const uint8_t * p_data, uint16_t size, bool * p_locked);

/** @brief Initialize the bus before starting the scheduler. */
HAL_StatusTypeDef
BSP_I2C1_Init (void)
{
    GPIO_InitTypeDef  pins   = {0};
    HAL_StatusTypeDef status = HAL_ERROR;

    if ((!i2c_context_valid()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return HAL_ERROR;
    }

    if (gb_initialized)
    {
        return HAL_OK;
    }

    if (NULL == gp_bus_mutex)
    {
        gp_bus_mutex = xSemaphoreCreateMutex();

        if (NULL == gp_bus_mutex)
        {
            return HAL_ERROR;
        }
    }

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_AFIO_REMAP_I2C1_DISABLE();
    pins.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    pins.Mode  = GPIO_MODE_AF_OD;
    pins.Pull  = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &pins);

    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();
    gh_i2c.Instance             = I2C1;
    gh_i2c.Init.ClockSpeed      = 100000u;
    gh_i2c.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    gh_i2c.Init.OwnAddress1     = 0u;
    gh_i2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    gh_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    gh_i2c.Init.OwnAddress2     = 0u;
    gh_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    gh_i2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    status                      = HAL_I2C_Init(&gh_i2c);
    gb_initialized              = (HAL_OK == status);

    return status;
}

/** @brief Write one bounded transaction under the bus mutex. */
HAL_StatusTypeDef
BSP_I2C1_Write (uint8_t address, const uint8_t * p_data,
                uint16_t size)
{
    bool              b_locked = false;
    HAL_StatusTypeDef status   = i2c_lock_bus(address, p_data, size, &b_locked);

    if (HAL_OK != status)
    {
        return status;
    }

    /* HAL never writes TX data; removing const is safe for this call. */
    /* Widening the validated 7-bit address makes the shift unsigned. */
    status = HAL_I2C_Master_Transmit(&gh_i2c, ((uint32_t)address << 1u),
                                   (uint8_t *)p_data, size,
                                   BSP_I2C1_TIMEOUT_MS);

    if (b_locked)
    {
        xSemaphoreGive(gp_bus_mutex);
    }

    return status;
}

/** @brief Read registers with one repeated-START transaction. */
HAL_StatusTypeDef
BSP_I2C1_ReadRegister (uint8_t address, uint8_t reg,
                       uint8_t * p_data, uint16_t size)
{
    bool              b_locked = false;
    HAL_StatusTypeDef status   = i2c_lock_bus(address, p_data, size, &b_locked);

    if (HAL_OK != status)
    {
        return status;
    }

    /* Widening the validated 7-bit address makes the shift unsigned. */
    status = HAL_I2C_Mem_Read(&gh_i2c, ((uint32_t)address << 1u), reg,
                             I2C_MEMADD_SIZE_8BIT, p_data, size,
                             BSP_I2C1_TIMEOUT_MS);

    if (b_locked)
    {
        xSemaphoreGive(gp_bus_mutex);
    }

    return status;
}

/** @brief Check that task APIs and the HAL tick can be used. */
static bool
i2c_context_valid (void)
{
    return ((0u == __get_IPSR()) && (0u == __get_PRIMASK()) &&
            (0u == __get_BASEPRI()));
}

/** @brief Validate arguments and serialize task callers. */
static HAL_StatusTypeDef
i2c_lock_bus (uint8_t address, const uint8_t * p_data,
              uint16_t size, bool * p_locked)
{
    *p_locked = false;

    if ((!i2c_context_valid()) || (!gb_initialized) || (NULL == p_data) ||
        (0u == size) || (size > BSP_I2C1_MAX_TRANSFER_SIZE) ||
        (address < 0x08u) || (address > 0x77u))
    {
        return HAL_ERROR;
    }

    BaseType_t state = xTaskGetSchedulerState();

    if (taskSCHEDULER_SUSPENDED == state)
    {
        return HAL_ERROR;
    }

    if (taskSCHEDULER_RUNNING == state)
    {
        if (pdTRUE != xSemaphoreTake(
                gp_bus_mutex, pdMS_TO_TICKS(BSP_I2C1_TIMEOUT_MS)))
        {
            return HAL_TIMEOUT;
        }

        *p_locked = true;
    }

    return HAL_OK;
}

/*** end of file ***/
