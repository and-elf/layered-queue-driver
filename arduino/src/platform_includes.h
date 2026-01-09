/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform-specific includes for Arduino builds
 * 
 * This file includes the appropriate platform implementation
 * based on the Arduino board selected.
 */

#ifndef PLATFORM_INCLUDES_H_
#define PLATFORM_INCLUDES_H_

#ifdef ARDUINO

/* Include platform implementations from parent src/platform/ directory */
#if defined(__SAMD21__) || defined(__SAMD51__)
    /* SAMD platform implementation will be compiled */
    #define PLATFORM_SAMD 1
#elif defined(ESP32)
    /* ESP32 platform implementation will be compiled */
    #define PLATFORM_ESP32 1
#elif defined(ARDUINO_ARCH_STM32) || defined(STM32F4) || defined(STM32F7)
    /* STM32 platform implementation will be compiled */
    #define PLATFORM_STM32 1
#else
    #warning "Unknown Arduino platform - motor control may not work"
#endif

#endif /* ARDUINO */

#endif /* PLATFORM_INCLUDES_H_ */
