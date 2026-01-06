/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Remap Driver - Maps input signals to logical functions
 */

#ifndef LQ_REMAP_H
#define LQ_REMAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct lq_engine;

/**
 * @brief Remap context structure
 * 
 * Remaps an input signal to an output signal, optionally applying
 * inversion and deadzone filtering.
 */
struct lq_remap_ctx {
    uint8_t input_signal;      /**< Input signal ID */
    uint8_t output_signal;     /**< Output signal ID (remapped function) */
    bool invert;               /**< Invert signal (multiply by -1) */
    int32_t deadzone;          /**< Deadzone threshold (±deadzone -> 0) */
    bool enabled;              /**< Processing enabled */
};

/**
 * @brief Process remap transformations
 * 
 * Applies remap operations to input signals and produces output signals.
 * 
 * @param engine Engine context
 * @param remaps Array of remap contexts
 * @param num_remaps Number of remaps to process
 * @param now Current timestamp in microseconds
 */
void lq_process_remaps(
    struct lq_engine *engine,
    const struct lq_remap_ctx *remaps,
    uint8_t num_remaps,
    uint64_t now);

#ifdef __cplusplus
}
#endif

#endif /* LQ_REMAP_H */
