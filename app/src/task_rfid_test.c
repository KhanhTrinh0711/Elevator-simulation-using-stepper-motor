/** @file task_rfid_test.c
 * @brief Poll a single RFID card and publish test results without a queue.
 */
#include "task_rfid_test.h"
#include <stdbool.h>
#include "app_cfg.h"
#include "bsp_gpio.h"
#include "task.h"

_Static_assert(APP_INPUT_PRIORITY < configMAX_PRIORITIES,
               "RFID test priority exceeds the RTOS range");
_Static_assert(0u < pdMS_TO_TICKS(APP_RFID_PERIOD_MS),
               "RFID polling must yield for at least one tick");

static TaskHandle_t        gp_rfid_task = NULL;
/* Accessed only inside critical sections; volatile is not synchronization. */
static task_rfid_snapshot_t g_snapshot = {.status = HAL_RC522_NOT_READY};

static bool rfid_context_valid (void);
static void rfid_test_task (void * p_argument);
static void rfid_publish (const task_rfid_snapshot_t * p_snapshot);

/** @brief Prepare only the test LEDs and create the polling task. */
BaseType_t
task_rfid_test_create (void)
{
    if ((!rfid_context_valid()) || (NULL != gp_rfid_task) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN | LED_RED_PIN, GPIO_PIN_RESET);
    GPIO_InitTypeDef pins = {0};

    pins.Pin   = LED_GREEN_PIN | LED_RED_PIN;
    pins.Mode  = GPIO_MODE_OUTPUT_PP;
    pins.Pull  = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &pins);

    return xTaskCreate(rfid_test_task, "RfidTest", APP_INPUT_STACK_WORDS,
                       NULL, APP_INPUT_PRIORITY, &gp_rfid_task);
}

/** @brief Return a coherent result, even when a higher-priority reader runs. */
BaseType_t
task_rfid_test_snapshot (task_rfid_snapshot_t * p_snapshot)
{
    if ((NULL == p_snapshot) || (!rfid_context_valid()) ||
        (NULL == gp_rfid_task) ||
        (taskSCHEDULER_RUNNING != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    taskENTER_CRITICAL();
    *p_snapshot = g_snapshot;
    taskEXIT_CRITICAL();

    return pdPASS;
}

/** @brief Disallow ISR and masked-interrupt calls to task APIs. */
static bool
rfid_context_valid (void)
{
    return ((0u == __get_IPSR()) && (0u == __get_PRIMASK()) &&
            (0u == __get_BASEPRI()));
}

/** @brief Reset on faults; normal absence of a card simply resumes polling. */
static void
rfid_test_task (void * p_argument)
{
    /* Required RTOS signature; this task has no argument to consume. */
    (void)p_argument;

    bool b_ready = false;
    task_rfid_snapshot_t snapshot = {.status = HAL_RC522_NOT_READY};

    vTaskDelay(pdMS_TO_TICKS(TASK_RFID_POWER_WAIT_MS));

    for (;;)
    {
        const hal_rc522_uid_t empty_uid = {0};
        snapshot.uid = empty_uid;

        if (!b_ready)
        {
            snapshot.status = hal_rc522_init(&snapshot.version);
            b_ready = (HAL_RC522_OK == snapshot.status);
        }

        if (b_ready)
        {
            snapshot.status = hal_rc522_read_uid(&snapshot.uid);
        }

        if (HAL_RC522_OK == snapshot.status)
        {
            snapshot.successful_reads++;
        }

        snapshot.timestamp_ticks = xTaskGetTickCount();
        rfid_publish(&snapshot);

        if ((HAL_RC522_OK != snapshot.status) &&
            (HAL_RC522_NO_CARD != snapshot.status))
        {
            b_ready = false;
            vTaskDelay(pdMS_TO_TICKS(TASK_RFID_RETRY_MS));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(APP_RFID_PERIOD_MS));
        }
    }
}

/** @brief Publish before reflecting status on the two test-owned LEDs. */
static void
rfid_publish (const task_rfid_snapshot_t * p_snapshot)
{
    taskENTER_CRITICAL();
    g_snapshot = *p_snapshot;
    taskEXIT_CRITICAL();

    const bool b_card = (HAL_RC522_OK == p_snapshot->status);
    const bool b_error = ((!b_card) &&
                         (HAL_RC522_NO_CARD != p_snapshot->status));

    HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN,
                      b_card ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED_RED_PIN,
                      b_error ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/*** end of file ***/
