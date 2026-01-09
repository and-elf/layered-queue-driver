/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * AVR Platform Configuration
 * 
 * Reduced configuration for 8-bit AVR microcontrollers with limited RAM.
 * Arduino Uno: 2KB RAM, Mega: 8KB RAM
 * 
 * This configuration reduces array sizes to fit in AVR's constrained memory.
 * Features are still available but with reduced capacity.
 */

#ifndef LQ_CONFIG_AVR_H_
#define LQ_CONFIG_AVR_H_

#ifdef __AVR__

/* Core engine limits - reduced for AVR RAM constraints */
#define LQ_MAX_SIGNALS 8            /**< 8 signals (vs 32 default) */
#define LQ_MAX_CYCLIC_OUTPUTS 4     /**< 4 cyclic outputs (vs 16) */
#define LQ_MAX_OUTPUT_EVENTS 16     /**< 16 events (vs 64) */

/* Driver limits */
#define LQ_MAX_MERGES 2             /**< 2 merges (vs 8) */
#define LQ_MAX_FAULT_MONITORS 2     /**< 2 fault monitors (vs 8) */
#define LQ_MAX_REMAPS 4             /**< 4 remaps (vs 16) */
#define LQ_MAX_SCALES 4             /**< 4 scales (vs 16) */
#define LQ_MAX_VERIFIED_OUTPUTS 2   /**< 2 verified outputs (vs 16) */
#define LQ_MAX_PIDS 2               /**< 2 PIDs (vs 8) */

/* Disable heavy features on AVR */
#define LQ_MAX_DTCS 4               /**< 4 DTCs (vs 16) - minimal diagnostics */

/* Queue sizes */
#define LQ_MAX_QUEUE_SIZE 16        /**< 16 entries (vs 64) */

/* Estimated memory usage with these settings:
 * - lq_engine struct: ~1.2KB (vs ~5KB default)
 * - Stack/heap overhead: ~400B
 * - Arduino runtime: ~200B
 * - Total: ~1.8KB, leaving ~200B free on Uno
 * 
 * For Arduino Mega (8KB RAM), you can increase these limits as needed.
 */

#endif /* __AVR__ */

#endif /* LQ_CONFIG_AVR_H_ */
