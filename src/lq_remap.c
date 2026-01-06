/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Remap Driver Implementation
 */

#include "lq_remap.h"
#include "lq_engine.h"

void lq_process_remaps(
    struct lq_engine *engine,
    const struct lq_remap_ctx *remaps,
    uint8_t num_remaps,
    uint64_t now)
{
    if (!engine || !remaps) {
        return;
    }
    
    for (uint8_t i = 0; i < num_remaps; i++) {
        const struct lq_remap_ctx *remap = &remaps[i];
        
        if (!remap->enabled) {
            continue;
        }
        
        /* Validate signal indices */
        if (remap->input_signal >= engine->num_signals ||
            remap->output_signal >= engine->num_signals) {
            continue;
        }
        
        const struct lq_signal *input = &engine->signals[remap->input_signal];
        struct lq_signal *output = &engine->signals[remap->output_signal];
        
        /* Skip if input has error status */
        if (input->status != LQ_EVENT_OK) {
            output->status = input->status;
            output->timestamp = now;
            output->updated = true;
            continue;
        }
        
        int32_t value = input->value;
        
        /* Apply deadzone */
        if (remap->deadzone > 0) {
            if (value >= -remap->deadzone && value <= remap->deadzone) {
                value = 0;
            }
        }
        
        /* Apply inversion */
        if (remap->invert) {
            value = -value;
        }
        
        /* Update output signal */
        output->value = value;
        output->status = LQ_EVENT_OK;
        output->timestamp = now;
        output->updated = true;
    }
}
