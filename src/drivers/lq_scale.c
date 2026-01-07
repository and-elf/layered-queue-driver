/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Scale Driver Implementation
 */

#include "lq_scale.h"
#include "lq_engine.h"

void lq_process_scales(
    struct lq_engine *engine,
    const struct lq_scale_ctx *scales,
    uint8_t num_scales,
    uint64_t now)
{
    if (!engine || !scales) {
        return;
    }
    
    for (uint8_t i = 0; i < num_scales; i++) {
        const struct lq_scale_ctx *scale = &scales[i];
        
        if (!scale->enabled) {
            continue;
        }
        
        /* Validate signal indices */
        if (scale->input_signal >= engine->num_signals ||
            scale->output_signal >= engine->num_signals) {
            continue;
        }
        
        const struct lq_signal *input = &engine->signals[scale->input_signal];
        struct lq_signal *output = &engine->signals[scale->output_signal];
        
        /* Skip if input has error status */
        if (input->status != LQ_EVENT_OK) {
            output->status = input->status;
            output->timestamp = now;
            output->updated = true;
            continue;
        }
        
        /* Apply scaling: output = (input * scale / 1000) + offset */
        int64_t temp = (int64_t)input->value * scale->scale_factor;
        temp /= 1000;
        temp += scale->offset;
        
        /* Convert back to int32 with saturation */
        int32_t value;
        if (temp > INT32_MAX) {
            value = INT32_MAX;
        } else if (temp < INT32_MIN) {
            value = INT32_MIN;
        } else {
            value = (int32_t)temp;
        }
        
        /* Apply clamping if enabled */
        if (scale->has_clamp_min && value < scale->clamp_min) {
            value = scale->clamp_min;
        }
        if (scale->has_clamp_max && value > scale->clamp_max) {
            value = scale->clamp_max;
        }
        
        /* Update output signal */
        output->value = value;
        output->status = LQ_EVENT_OK;
        output->timestamp = now;
        output->updated = true;
    }
}
