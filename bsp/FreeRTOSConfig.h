/** @file FreeRTOSConfig.h
 * @brief Project FreeRTOS configuration; names are the kernel contract.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f1xx.h"

#define configUSE_PREEMPTION                     1u
#define configCPU_CLOCK_HZ                       SystemCoreClock
#define configTICK_RATE_HZ                       1000u
#define configMAX_PRIORITIES                     5u
#define configMINIMAL_STACK_SIZE                 128u
#define configTOTAL_HEAP_SIZE                    (8u * 1024u)
#define configMAX_TASK_NAME_LEN                  16u
#define configUSE_16_BIT_TICKS                    0u
#define configIDLE_SHOULD_YIELD                   1u
#define configSUPPORT_DYNAMIC_ALLOCATION         1u
#define configSUPPORT_STATIC_ALLOCATION          0u
#define configUSE_MUTEXES                        1u
#define configUSE_RECURSIVE_MUTEXES               1u
#define configUSE_COUNTING_SEMAPHORES             1u
#define configUSE_CO_ROUTINES                    0u
#define configUSE_IDLE_HOOK                      0u
#define configUSE_TICK_HOOK                      0u
#define configCHECK_FOR_STACK_OVERFLOW           2u
#define configUSE_MALLOC_FAILED_HOOK             1u
#define configUSE_TIMERS                         1u
#define configTIMER_TASK_PRIORITY                2u
#define configTIMER_QUEUE_LENGTH                 8u
#define configTIMER_TASK_STACK_DEPTH              256u

#define INCLUDE_vTaskDelay                       1u
#define INCLUDE_vTaskDelayUntil                  1u
#define INCLUDE_vTaskDelete                      1u
#define INCLUDE_vTaskSuspend                     1u
#define INCLUDE_xTaskGetSchedulerState           1u
#define INCLUDE_uxTaskGetStackHighWaterMark       1u
#define INCLUDE_xTimerPendFunctionCall           1u

#define configPRIO_BITS                          __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

/* Fail-stop macro required by FreeRTOS; evaluate the condition once. */
#define configASSERT(condition)             \
    do                                      \
    {                                       \
        if (!(condition))                   \
        {                                   \
            __disable_irq();                \
            for (;;)                        \
            {                               \
                /* Wait for the debugger. */\
            }                               \
        }                                   \
    } while (0)

#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* FREERTOS_CONFIG_H */

/*** end of file ***/
