/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform Implementation Bridge for Arduino
 * 
 * This file includes the appropriate platform-specific implementation
 * from the parent repository's src/platform/ directory based on the
 * Arduino board being compiled for.
 * 
 * This avoids duplicating platform code and ensures the Arduino library
 * stays in sync with the main repository.
 */

#include "platform_includes.h"

#ifdef ARDUINO

/* Include platform-specific implementation from parent src/platform/ */
#if defined(__SAMD21__) || defined(__SAMD51__)
    /* Compile SAMD TCC-based implementation */
    #include "../../src/platform/lq_platform_samd.c"
    
#elif defined(ESP32)
    /* Compile ESP32 MCPWM-based implementation */
    #include "../../src/platform/lq_platform_esp32.c"
    
#elif defined(ARDUINO_ARCH_STM32) || defined(STM32F4) || defined(STM32F7)
    /* Compile STM32 TIM-based implementation */
    #include "../../src/platform/lq_platform_stm32.c"
    
#else
    /* No platform-specific implementation available */
    #warning "No BLDC platform implementation for this Arduino board"
    
    /* Provide stubs so code compiles but won't actually control motors */
    #include "lq_bldc.h"
    
    int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config) {
        (void)motor_id; (void)config;
        return -1;  /* Not supported */
    }
    
    int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase, uint16_t duty_cycle) {
        (void)motor_id; (void)phase; (void)duty_cycle;
        return -1;
    }
    
    int lq_bldc_platform_enable(uint8_t motor_id, bool enable) {
        (void)motor_id; (void)enable;
        return -1;
    }
    
    int lq_bldc_platform_brake(uint8_t motor_id) {
        (void)motor_id;
        return -1;
    }
#endif

#endif /* ARDUINO */
