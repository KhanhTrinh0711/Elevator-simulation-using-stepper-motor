#include "bsp_spi.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SPI_HandleTypeDef hspi1;
static SemaphoreHandle_t spi1_mutex;
static uint8_t initialized;

static int context_valid(void)
{
    return __get_IPSR() == 0U && __get_PRIMASK() == 0U &&
           __get_BASEPRI() == 0U;
}

HAL_StatusTypeDef BSP_SPI1_Init(void)
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
    if (spi1_mutex == NULL) {
        spi1_mutex = xSemaphoreCreateMutex();
        if (spi1_mutex == NULL) {
            return HAL_ERROR;
        }
    }

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_AFIO_REMAP_SPI1_DISABLE();

    /* Preload inactive NSS and released reset before enabling outputs. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
    pins.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &pins);

    pins.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    pins.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOA, &pins);
    pins.Pin = GPIO_PIN_6;
    pins.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &pins);

    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_SPI1_FORCE_RESET();
    __HAL_RCC_SPI1_RELEASE_RESET();
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7U;
    status = HAL_SPI_Init(&hspi1);
    initialized = (status == HAL_OK);
    return status;
}

HAL_StatusTypeDef BSP_SPI1_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t size)
{
    BaseType_t locked = pdFALSE;
    HAL_StatusTypeDef status;

    if (!context_valid() || !initialized || tx == NULL || rx == NULL ||
        size == 0U || size > BSP_SPI1_MAX_TRANSFER_SIZE) {
        return HAL_ERROR;
    }
    uintptr_t tx_addr = (uintptr_t)tx;
    uintptr_t rx_addr = (uintptr_t)rx;
    if ((tx_addr <= rx_addr ? rx_addr - tx_addr : tx_addr - rx_addr) < size) {
        return HAL_ERROR;
    }
    BaseType_t state = xTaskGetSchedulerState();
    if (state == taskSCHEDULER_SUSPENDED) {
        return HAL_ERROR;
    }
    if (state == taskSCHEDULER_RUNNING) {
        if (xSemaphoreTake(spi1_mutex, pdMS_TO_TICKS(BSP_SPI1_TIMEOUT_MS)) != pdTRUE) {
            return HAL_TIMEOUT;
        }
        locked = pdTRUE;
    }

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, size, BSP_SPI1_TIMEOUT_MS);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    if (locked) {
        xSemaphoreGive(spi1_mutex);
    }
    return status;
}
