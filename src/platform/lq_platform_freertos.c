/*
 * FreeRTOS Platform Implementation for Layered Queue Driver
 * 
 * Provides thread-safe operations using FreeRTOS primitives:
 * - Mutexes for queue protection
 * - Semaphores for event signaling
 * - Tasks for cyclic outputs
 * - Timer API for timestamps
 */

#include "lq_platform.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* Forward declarations */
extern void lq_cyclic_output_execute(struct lq_cyclic_ctx *ctx);

/* FreeRTOS synchronization primitives */
static SemaphoreHandle_t lq_queue_mutex = NULL;
static SemaphoreHandle_t lq_event_sem = NULL;

/* Cyclic output task handles */
#define LQ_MAX_CYCLIC_TASKS 16
static TaskHandle_t lq_cyclic_task_handles[LQ_MAX_CYCLIC_TASKS];
static uint8_t lq_cyclic_task_count = 0;

/* Initialize platform-specific resources */
int lq_platform_init(void)
{
    /* Create mutex for queue protection */
    lq_queue_mutex = xSemaphoreCreateMutex();
    if (lq_queue_mutex == NULL) {
        return -1;
    }
    
    /* Create binary semaphore for event signaling */
    lq_event_sem = xSemaphoreCreateBinary();
    if (lq_event_sem == NULL) {
        vSemaphoreDelete(lq_queue_mutex);
        return -1;
    }
    
    return 0;
}

/* Cleanup platform resources */
void lq_platform_cleanup(void)
{
    /* Delete all cyclic tasks */
    for (uint8_t i = 0; i < lq_cyclic_task_count; i++) {
        if (lq_cyclic_task_handles[i] != NULL) {
            vTaskDelete(lq_cyclic_task_handles[i]);
            lq_cyclic_task_handles[i] = NULL;
        }
    }
    lq_cyclic_task_count = 0;
    
    /* Delete synchronization primitives */
    if (lq_event_sem != NULL) {
        vSemaphoreDelete(lq_event_sem);
        lq_event_sem = NULL;
    }
    
    if (lq_queue_mutex != NULL) {
        vSemaphoreDelete(lq_queue_mutex);
        lq_queue_mutex = NULL;
    }
}

/* Lock queue for thread-safe access */
void lq_platform_lock(void)
{
    if (lq_queue_mutex != NULL) {
        xSemaphoreTake(lq_queue_mutex, portMAX_DELAY);
    }
}

/* Unlock queue */
void lq_platform_unlock(void)
{
    if (lq_queue_mutex != NULL) {
        xSemaphoreGive(lq_queue_mutex);
    }
}

/* Lock from ISR context */
void lq_platform_lock_isr(void)
{
    /* Cannot take mutex from ISR - caller must handle this differently
     * or use fromISR variants of FreeRTOS functions */
}

/* Unlock from ISR context */
void lq_platform_unlock_isr(void)
{
    /* Cannot give mutex from ISR */
}

/* Signal event (e.g., new data available) */
void lq_platform_signal_event(void)
{
    if (lq_event_sem != NULL) {
        xSemaphoreGive(lq_event_sem);
    }
}

/* Signal event from ISR */
void lq_platform_signal_event_isr(void)
{
    if (lq_event_sem != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(lq_event_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* Wait for event with timeout */
int lq_platform_wait_event(uint32_t timeout_us)
{
    if (lq_event_sem == NULL) {
        return -1;
    }
    
    TickType_t timeout_ticks;
    if (timeout_us == UINT32_MAX) {
        timeout_ticks = portMAX_DELAY;
    } else {
        timeout_ticks = pdMS_TO_TICKS(timeout_us / 1000);
        if (timeout_ticks == 0 && timeout_us > 0) {
            timeout_ticks = 1;  /* Ensure non-zero timeout */
        }
    }
    
    if (xSemaphoreTake(lq_event_sem, timeout_ticks) == pdTRUE) {
        return 0;
    }
    
    return -1;  /* Timeout */
}

/* Get microsecond timestamp */
uint64_t lq_platform_get_time_us(void)
{
    /* FreeRTOS tick count converted to microseconds
     * Note: This may overflow, use only for relative timing */
    TickType_t ticks = xTaskGetTickCount();
    return (uint64_t)ticks * (1000000ULL / configTICK_RATE_HZ);
}

/* Sleep for microseconds */
void lq_platform_sleep_us(uint32_t us)
{
    TickType_t ticks = pdMS_TO_TICKS(us / 1000);
    if (ticks == 0 && us > 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
}

/* Cyclic task function */
static void lq_cyclic_task(void *pvParameters)
{
    struct lq_cyclic_ctx *ctx = (struct lq_cyclic_ctx *)pvParameters;
    
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t period_ticks = pdMS_TO_TICKS(ctx->period_us / 1000);
    
    if (period_ticks == 0) {
        period_ticks = 1;
    }
    
    while (1) {
        /* Execute cyclic output */
        lq_cyclic_output_execute(ctx);
        
        /* Wait for next period */
        vTaskDelayUntil(&last_wake_time, period_ticks);
    }
}

/* Create cyclic output task */
int lq_platform_create_cyclic_task(struct lq_cyclic_ctx *ctx, uint8_t priority)
{
    if (lq_cyclic_task_count >= LQ_MAX_CYCLIC_TASKS) {
        return -1;
    }
    
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "lq_cyc_%d", ctx->signal_id);
    
    /* Map priority (0-7 in our system) to FreeRTOS priority (0-configMAX_PRIORITIES)
     * Higher number = higher priority in FreeRTOS */
    UBaseType_t freertos_priority = (configMAX_PRIORITIES - 1) - priority;
    if (freertos_priority >= configMAX_PRIORITIES) {
        freertos_priority = configMAX_PRIORITIES - 1;
    }
    
    BaseType_t result = xTaskCreate(
        lq_cyclic_task,
        task_name,
        configMINIMAL_STACK_SIZE * 2,  /* Stack size */
        ctx,                            /* Task parameter */
        freertos_priority,
        &lq_cyclic_task_handles[lq_cyclic_task_count]
    );
    
    if (result != pdPASS) {
        return -1;
    }
    
    lq_cyclic_task_count++;
    return 0;
}

/* Allocate memory (uses FreeRTOS heap) */
void *lq_platform_malloc(size_t size)
{
    return pvPortMalloc(size);
}

/* Free memory */
void lq_platform_free(void *ptr)
{
    vPortFree(ptr);
}

/* Thread-safe print (optional, for debugging) */
void lq_platform_printf(const char *format, ...)
{
    /* In FreeRTOS, printf might not be thread-safe depending on newlib configuration
     * Use a mutex or implement a thread-safe print queue */
    taskENTER_CRITICAL();
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    taskEXIT_CRITICAL();
}
