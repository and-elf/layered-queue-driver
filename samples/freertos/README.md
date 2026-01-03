# FreeRTOS Sample Application

This sample demonstrates integrating the Layered Queue Driver with FreeRTOS on an STM32F4 microcontroller.

## Features

✅ **FreeRTOS task-based architecture**  
✅ **ISR-safe sensor data acquisition**  
✅ **Event-driven processing** with semaphores  
✅ **Automatic cyclic output tasks** for J1939 transmission  
✅ **Real-time diagnostics** with DM1 messages  
✅ **Complete automotive ECU example**

## Hardware Requirements

- **STM32F4 Discovery** or similar (STM32F407VG recommended)
- **CAN transceiver** (TJA1050, MCP2551, or SN65HVD230)
- **Sensors** (or test with potentiometers):
  - 3x analog inputs for ADC (RPM, oil pressure, coolant temp)
  - 1x CAN bus connection

## Software Requirements

- **STM32CubeIDE** or **STM32CubeMX** + toolchain
- **FreeRTOS** (included in STM32Cube)
- **HAL drivers** for STM32F4

## Quick Start

### 1. Generate STM32CubeMX Project

Configure peripherals:

**ADC1** (for sensors):
- Enable channels 0, 1, 2, 3
- DMA mode: Circular
- Conversion trigger: Timer or software
- Enable interrupts

**CAN1** (for J1939):
- Bit rate: 250 kbps
- Mode: Normal
- Extended ID: Enabled
- Enable RX FIFO0 interrupt

**FreeRTOS**:
- CMSIS_V2 interface
- Heap: heap_4 (recommended)
- Total heap size: 32768 bytes
- Minimal stack size: 128 words

### 2. Generate Platform-Specific Code

```bash
# Create DTS for your application (or use automotive example)
python3 scripts/dts_gen.py samples/j1939/automotive_can_system.dts Inc/ --platform=stm32
```

This generates:
- `lq_generated.h` - System declarations
- `lq_generated.c` - Engine configuration
- `lq_platform_hw.c` - STM32 HAL ISRs

### 3. Add Files to STM32CubeIDE Project

**Copy to project:**
```
Core/
  Inc/
    lq_generated.h        (generated)
    lq_platform.h         (from include/)
    lq_j1939.h            (from include/)
  Src/
    lq_generated.c        (generated)
    lq_platform_hw.c      (generated)
    lq_platform_freertos.c (from src/platform/)
    lq_j1939.c            (from src/)
    lq_engine.c           (from src/)
    lq_hw_input.c         (from src/)
    lq_queue_core.c       (from src/)
    main.c                (from samples/freertos/main.c)
```

### 4. Build Configuration

**Preprocessor defines:**
```
LQ_PLATFORM_FREERTOS=1
USE_HAL_DRIVER
STM32F407xx
```

**Include paths:**
```
Core/Inc
Middlewares/Third_Party/FreeRTOS/Source/include
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
```

### 5. Configure FreeRTOS

**FreeRTOSConfig.h** (adjust as needed):
```c
#define configUSE_PREEMPTION              1
#define configCPU_CLOCK_HZ                168000000UL
#define configTICK_RATE_HZ                1000
#define configMAX_PRIORITIES              8
#define configMINIMAL_STACK_SIZE          128
#define configTOTAL_HEAP_SIZE             32768
#define configUSE_MUTEXES                 1
#define configUSE_COUNTING_SEMAPHORES     1
```

### 6. Build and Flash

```bash
# In STM32CubeIDE:
Project → Build All
Run → Debug (or Flash)

# Or with command line:
make -j8
st-flash write build/firmware.bin 0x8000000
```

### 7. Test

**Connect CAN analyzer** (PCAN-USB, Kvaser, etc.) at 250 kbps:

Expected J1939 messages:
```
ID: 18FEF128  [8]  <RPM data>     # EEC1 @ 10Hz
ID: 18FEEE28  [8]  <Temp data>    # ET1 @ 1Hz
ID: 18FECA28  [8]  <DTC data>     # DM1 @ 1Hz (if faults)
```

## Task Architecture

The application creates these tasks:

| Task | Priority | Stack | Period | Function |
|------|----------|-------|--------|----------|
| `lq_proc` | 3 | 512 words | 50ms | Sensor fusion, voting |
| `lq_diag` | 2 | 256 words | 1s | DM1 diagnostics |
| `lq_cyc_20` | 5 | 256 words | 100ms | EEC1 output (auto-created) |
| `lq_cyc_3` | 4 | 256 words | 1s | ET1 output (auto-created) |

**CPU usage:** ~10-15% @ 168MHz

## Customization

### Add Custom Sensor

**1. Update DTS:**
```dts
my_sensor: my-custom-sensor {
    compatible = "lq,hw-adc-input";
    signal_id = <50>;
    hw_instance = <1>;
    hw_channel = <5>;
    stale_us = <100000>;
};
```

**2. Regenerate code:**
```bash
python3 scripts/dts_gen.py my_app.dts Inc/ --platform=stm32
```

**3. Rebuild and flash**

### Add Custom Task

```c
void my_custom_task(void *pvParameters)
{
    while (1) {
        /* Your application logic */
        struct lq_signal *sig = lq_get_signal(50);
        
        if (sig && sig->status == LQ_STATUS_OK) {
            /* Process signal */
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* In main(), after create_tasks(): */
xTaskCreate(my_custom_task, "my_task", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
```

## Debugging

### Enable Stack Overflow Detection

```c
#define configCHECK_FOR_STACK_OVERFLOW    2

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* Set breakpoint here */
    __BKPT(0);
}
```

### Monitor Task Statistics

```c
#define configUSE_TRACE_FACILITY          1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

/* In a debug task: */
char buffer[512];
vTaskList(buffer);
printf("%s\n", buffer);
```

### Check Heap Usage

```c
size_t free_heap = xPortGetFreeHeapSize();
printf("Free heap: %u bytes\n", free_heap);
```

## Performance Tuning

### Reduce CPU Usage

1. **Lower processing rate:**
   ```c
   const TickType_t period = pdMS_TO_TICKS(100);  /* 10Hz instead of 20Hz */
   ```

2. **Optimize voting algorithm:**
   - Use faster median calculation
   - Skip unnecessary checks

3. **Batch CAN transmissions:**
   - Send multiple PGNs together
   - Use CAN TX FIFO efficiently

### Increase Responsiveness

1. **Raise task priorities:**
   ```c
   tskIDLE_PRIORITY + 4  /* Higher priority for processing */
   ```

2. **Use DMA for all peripherals:**
   - ADC with DMA
   - CAN with hardware filters

3. **Enable interrupt preemption:**
   - Set proper NVIC priorities
   - Use configMAX_SYSCALL_INTERRUPT_PRIORITY

## Troubleshooting

### Hard Fault on Startup

- ✅ Check stack sizes (increase if needed)
- ✅ Verify heap size is sufficient
- ✅ Enable stack overflow detection
- ✅ Check ISR priorities (must be ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY)

### Tasks Not Running

- ✅ Call `vTaskStartScheduler()` in main
- ✅ Verify FreeRTOS tick interrupt is running
- ✅ Check SysTick configuration (1ms tick)

### CAN Messages Not Received

- ✅ Verify CAN filters are configured
- ✅ Check bit rate (250 kbps for J1939)
- ✅ Enable CAN RX interrupts
- ✅ Check bus termination (120Ω)

### Memory Allocation Fails

- ✅ Increase `configTOTAL_HEAP_SIZE`
- ✅ Use heap_4 (allows free)
- ✅ Monitor with `xPortGetFreeHeapSize()`

## Next Steps

- See [../../docs/freertos-integration.md](../../docs/freertos-integration.md) for detailed guide
- Review [../../docs/can-j1939-guide.md](../../docs/can-j1939-guide.md) for CAN details
- Check [../../docs/platform-adaptors.md](../../docs/platform-adaptors.md) for code generation

## License

Same as parent project.
