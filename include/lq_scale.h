/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Scale Driver - Applies linear transformations to signals
 */

#ifndef LQ_SCALE_H
#define LQ_SCALE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct lq_engine;

/**
 * @brief Scale context structure
 * 
 * Applies linear transformation: output = (input * scale / 1000) + offset
 * with optional min/max clamping.
 */
struct lq_scale_ctx {
    uint8_t input_signal;      /**< Input signal ID */
    uint8_t output_signal;     /**< Output signal ID (scaled result) */
    int32_t scale_factor;      /**< Scale multiplier (1000 = 1.0x) */
    int32_t offset;            /**< Offset to add after scaling */
    int32_t clamp_min;         /**< Minimum output value */
    int32_t clamp_max;         /**< Maximum output value */
    bool has_clamp_min;        /**< Minimum clamping enabled */
    bool has_clamp_max;        /**< Maximum clamping enabled */
    bool enabled;              /**< Processing enabled */
};

/**
 * @brief Process scale transformations
 * 
 * Applies scale operations to input signals and produces output signals.
 * 
 * @param engine Engine context
 * @param scales Array of scale contexts
 * @param num_scales Number of scales to process
 * @param now Current timestamp in microseconds
 */
void lq_process_scales(
    struct lq_engine *engine,
    const struct lq_scale_ctx *scales,
    uint8_t num_scales,
    uint64_t now);

#endif /* LQ_SCALE_H */
