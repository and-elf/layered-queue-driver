# FreeRTOS Integration Guide

The Layered Queue Driver provides full FreeRTOS support with thread-safe operations, task-based cyclic outputs, and proper RTOS primitives.

## Features

✅ **Thread-safe queue operations** with FreeRTOS mutexes  
✅ **ISR-safe signaling** with `FromISR` variants  
✅ **Cyclic output tasks** with configurable priorities  
✅ **Event-driven processing** with semaphores  
✅ **FreeRTOS heap integration** for dynamic allocation  

## Quick Start

### 1. Add FreeRTOS Platform to Build

**CMakeLists.txt:**
```cmake
add_library(layered_queue
    src/lq_queue_core.c
    src/lq_engine.c
    src/lq_hw_input.c
    src/lq_j1939.c
    src/platform/lq_platform_freertos.c  # Add this
)

target_compile_definitions(layered_queue PUBLIC
    LQ_PLATFORM_FREERTOS=1
)

# Link FreeRTOS
target_link_libraries(layered_queue PUBLIC freertos_kernel)
```

### 2. Initialize in main.c

```c
#include "FreeRTOS.h"
#include "task.h"
#include "lq_generated.h"
#include "lq_platform.h"

void lq_processing_task(void *pvParameters)
{
    while (1) {
        /* Wait for new data (blocks on semaphore) */
        if (lq_platform_wait_event(100000) == 0) {  /* 100ms timeout */
            /* Process hardware inputs */
            lq_hw_input_process();
            
            /* Process merge layer */
            lq_merge_process();
        }
    }
}

int main(void)
{
    /* Hardware init */
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize FreeRTOS platform */
    lq_platform_init();
    
    /* Initialize layered queue system */
    lq_generated_init();
    
    /* Create processing task */
    xTaskCreate(
        lq_processing_task,
        "lq_proc",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );
    
    /* Cyclic output tasks are created automatically */
    
    /* Start FreeRTOS scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1);
}
```

### 3. Push Data from ISRs

```c
/* ADC conversion complete callback (STM32 HAL) */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        
        /* Push from ISR - thread-safe */
        lq_hw_push(0, (uint32_t)value);
        
        /* Signal processing task - uses FromISR variant */
        lq_platform_signal_event_isr();
    }
}

/* CAN receive callback */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
        uint32_t pgn = (rx_header.ExtId >> 8) & 0x3FFFF;
        
        if (pgn == 65265) {  /* EEC1 */
            int32_t rpm = (rx_data[3] << 24) | (rx_data[2] << 16) |
                          (rx_data[1] << 8) | rx_data[0];
            lq_hw_push(10, rpm);
            lq_platform_signal_event_isr();  /* Wake processing task */
        }
    }
}
```

## Task Architecture

### Processing Task (Your Responsibility)

Create a task that processes incoming data:

```c
void lq_processing_task(void *pvParameters)
{
    while (1) {
        /* Block waiting for new data */
        lq_platform_wait_event(portMAX_DELAY);
        
        /* Process inputs */
        lq_hw_input_process();
        
        /* Process merges/voting */
        lq_merge_process();
        
        /* Error detection could run here */
    }
}
```

**Recommended priority:** `tskIDLE_PRIORITY + 2` to `+ 4`

### Cyclic Output Tasks (Automatic)

The system automatically creates tasks for cyclic outputs defined in DTS:

```dts
eec1_output: engine-controller-1 {
    compatible = "lq,cyclic-output";
    source_signal_id = <20>;
    period_us = <100000>;  /* 10Hz → vTaskDelayUntil(100ms) */
    priority = <3>;        /* Mapped to FreeRTOS priority */
};
```

**Priority mapping:**
- DTS priority 0 (highest) → FreeRTOS `configMAX_PRIORITIES - 1`
- DTS priority 7 (lowest) → FreeRTOS lower priority
- Ensures higher-priority outputs run first

### ISR Context

ISRs (ADC, CAN, SPI, etc.) push data and signal the processing task:

```c
/* From ISR - uses lock-free queue or critical sections */
lq_hw_push(signal_id, value);

/* Wake processing task - uses xSemaphoreGiveFromISR */
lq_platform_signal_event_isr();
```

## Memory Management

### Heap Configuration

FreeRTOS heap is used for dynamic allocation:

```c
/* FreeRTOSConfig.h */
#define configTOTAL_HEAP_SIZE    ((size_t)(32 * 1024))  /* 32KB */
#define configMINIMAL_STACK_SIZE ((unsigned short)128)   /* Words */
```

**Typical usage:**
- Queue buffers: ~2-4 KB
- Each cyclic task: ~512 bytes stack
- Total for 10 signals + 3 outputs: ~8-12 KB

### Stack Sizes

```c
/* Processing task - handles merges, voting */
xTaskCreate(..., configMINIMAL_STACK_SIZE * 4, ...);  /* 512 bytes on ARM */

/* Cyclic output tasks - minimal work */
xTaskCreate(..., configMINIMAL_STACK_SIZE * 2, ...);  /* 256 bytes */
```

## Thread Safety

### Mutex Protection

Queue operations are protected:

```c
void lq_hw_push(uint8_t signal_id, int32_t value)
{
    lq_platform_lock();      /* xSemaphoreTake(mutex) */
    /* ... modify queue ... */
    lq_platform_unlock();    /* xSemaphoreGive(mutex) */
}
```

### ISR Safety

ISRs **do not take mutexes** (would crash):

```c
void lq_platform_signal_event_isr(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(lq_event_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  /* Context switch if needed */
}
```

## Example: Complete FreeRTOS Application

### main.c

```c
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"
#include "lq_generated.h"
#include "lq_platform.h"

/* Processing task */
void lq_processing_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        /* Wait for sensor data (100ms max) */
        if (lq_platform_wait_event(100000) == 0) {
            /* New data arrived */
            lq_hw_input_process();
            lq_merge_process();
        }
        
        /* Run at least every 100ms even if no events */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* Diagnostics task */
void lq_diagnostics_task(void *pvParameters)
{
    while (1) {
        /* Check for errors and send DM1 */
        lq_diagnostics_check();
        
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 1Hz */
    }
}

int main(void)
{
    /* STM32 HAL init */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_CAN1_Init();
    MX_ADC1_Init();
    
    /* FreeRTOS platform */
    lq_platform_init();
    
    /* Layered queue system */
    lq_generated_init();
    
    /* Platform-specific peripherals (starts ADC DMA, CAN, etc.) */
    lq_platform_peripherals_init();
    
    /* Create tasks */
    xTaskCreate(lq_processing_task, "lq_proc", 512, NULL, 3, NULL);
    xTaskCreate(lq_diagnostics_task, "lq_diag", 256, NULL, 2, NULL);
    
    /* Cyclic outputs are automatically created as tasks */
    
    /* Start scheduler */
    vTaskStartScheduler();
    
    /* Never reached */
    while (1);
}
```

### FreeRTOSConfig.h (Example)

```c
#define configUSE_PREEMPTION              1
#define configUSE_IDLE_HOOK               0
#define configUSE_TICK_HOOK               0
#define configCPU_CLOCK_HZ                168000000UL  /* STM32F4 @ 168MHz */
#define configTICK_RATE_HZ                1000          /* 1ms tick */
#define configMAX_PRIORITIES              8
#define configMINIMAL_STACK_SIZE          128
#define configTOTAL_HEAP_SIZE             32768         /* 32KB */
#define configMAX_TASK_NAME_LEN           16
#define configUSE_16_BIT_TICKS            0
#define configIDLE_SHOULD_YIELD           1
#define configUSE_MUTEXES                 1
#define configUSE_COUNTING_SEMAPHORES     1
#define configUSE_TIMERS                  0
#define configUSE_MALLOC_FAILED_HOOK      1

/* Cortex-M specific */
#define configPRIO_BITS                   4  /* STM32F4 has 4 priority bits */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << (8 - configPRIO_BITS))
```

## Performance Considerations

### Task Priorities

Recommended priority assignment:

| Task | Priority | Reason |
|------|----------|--------|
| ISR handlers | Highest (hardware) | Time-critical sensor data |
| Cyclic output (10Hz) | `tskIDLE_PRIORITY + 5` | Real-time J1939 transmission |
| Processing task | `tskIDLE_PRIORITY + 3` | Data fusion, voting |
| Diagnostics (DM1) | `tskIDLE_PRIORITY + 2` | Non-critical monitoring |
| Idle task | `tskIDLE_PRIORITY` | Background work |

### CPU Usage

With 10 signals and 3 cyclic outputs @ 168MHz STM32F4:

- Processing task: ~2-5% CPU
- Cyclic outputs: ~1% CPU each
- ISRs: <1% CPU
- **Total: ~10-15% CPU usage**

Plenty of headroom for application logic!

## Platform Detection

The build system automatically defines:

```c
#ifdef LQ_PLATFORM_FREERTOS
    /* FreeRTOS-specific code */
    #include "FreeRTOS.h"
    #include "task.h"
#endif
```

## Common Issues

### Semaphore Not Given from ISR

**Problem:** Using `lq_platform_signal_event()` instead of `lq_platform_signal_event_isr()`

**Solution:**
```c
/* ❌ Wrong - will crash */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    lq_platform_signal_event();  /* Uses xSemaphoreGive, not allowed in ISR */
}

/* ✅ Correct */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    lq_platform_signal_event_isr();  /* Uses xSemaphoreGiveFromISR */
}
```

### Stack Overflow

**Symptom:** Hard fault, random crashes

**Solution:** Increase task stack size or enable stack overflow detection:

```c
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_MALLOC_FAILED_HOOK      1

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* Task name in pcTaskName */
    while(1);  /* Debug break here */
}
```

### Priority Inversion

**Symptom:** High-priority task blocked by low-priority task

**Solution:** Enable priority inheritance:

```c
#define configUSE_MUTEXES                 1  /* Required */
```

The system automatically uses mutexes, which support priority inheritance in FreeRTOS.

## Best Practices

1. **Always use `_isr()` variants from interrupt context**
2. **Set appropriate task stack sizes** (monitor with `uxTaskGetStackHighWaterMark()`)
3. **Configure heap size** to accommodate all tasks and queues
4. **Use event-driven processing** instead of polling (saves power)
5. **Assign priorities carefully** to ensure real-time guarantees
6. **Enable stack overflow detection** during development
7. **Test with different loads** to verify timing requirements

## Next Steps

- See [samples/freertos/](../samples/freertos/) for complete examples
- Review [docs/platform-adaptors.md](platform-adaptors.md) for platform-specific code generation
- Check [docs/can-j1939-guide.md](can-j1939-guide.md) for CAN integration

## References

- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [FreeRTOS API Reference](https://www.freertos.org/a00106.html)
- [STM32 + FreeRTOS](https://www.st.com/content/st_com/en/ecosystems/stm32-open-development-environment/freertos.html)
