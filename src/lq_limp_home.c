/* SPDX-License-Identifier: Apache-2.0 */
#include "lq_limp_home.h"
#include "lq_engine.h"
#include "lq_scale.h"
#include "layered_queue_core.h"
#include "lq_platform.h"
#include <limits.h>

/**
 * @brief Process a single limp-home mode controller
 * 
 * Checks fault signal and transitions between normal and limp modes.
 * When entering limp mode, saves current scale parameters and applies
 * degraded parameters. When exiting (with delay), restores saved parameters.
 */
static void process_single_limp_home(struct lq_limp_home_ctx *ctx,
                                     struct lq_engine *engine,
                                     uint64_t current_time_ms)
{
    if (!ctx->enabled) {
        return;
    }

    /* Validate target scale index */
    if (ctx->target_scale_id >= engine->num_scales) {
        return;
    }

    struct lq_scale_ctx *scale = &engine->scales[ctx->target_scale_id];

    /* Get fault signal value */
    if (ctx->fault_signal_id >= engine->num_signals) {
        return;
    }

    struct lq_signal *fault_signal = &engine->signals[ctx->fault_signal_id];
    bool fault_active = (fault_signal->value >= ctx->fault_threshold);

    /* State machine: Normal <-> Limp */
    if (fault_active) {
        if (!ctx->is_limp_mode_active) {
            /* Entering limp mode - save current scale parameters */
            ctx->saved_scale_factor = scale->scale_factor;
            ctx->saved_clamp_max = scale->clamp_max;
            ctx->saved_clamp_min = scale->clamp_min;
            ctx->saved_has_clamp = scale->has_clamp_min || scale->has_clamp_max;

            /* Apply limp mode parameters */
            if (ctx->limp_scale_factor != INT32_MIN) {
                scale->scale_factor = ctx->limp_scale_factor;
            }
            if (ctx->limp_clamp_max != INT32_MIN) {
                scale->clamp_max = ctx->limp_clamp_max;
                scale->has_clamp_max = true;
            }
            if (ctx->limp_clamp_min != INT32_MIN) {
                scale->clamp_min = ctx->limp_clamp_min;
                scale->has_clamp_min = true;
            }

            ctx->is_limp_mode_active = true;
        }
        /* Reset clear timestamp while fault is active */
        ctx->fault_clear_timestamp_ms = current_time_ms;
    } else {
        if (ctx->is_limp_mode_active) {
            /* Fault cleared - check if delay has elapsed */
            uint64_t elapsed_ms = current_time_ms - ctx->fault_clear_timestamp_ms;
            
            if (elapsed_ms >= ctx->restore_delay_ms) {
                /* Restore normal mode parameters */
                scale->scale_factor = ctx->saved_scale_factor;
                scale->clamp_max = ctx->saved_clamp_max;
                scale->clamp_min = ctx->saved_clamp_min;
                scale->has_clamp_min = ctx->saved_has_clamp;
                scale->has_clamp_max = ctx->saved_has_clamp;

                ctx->is_limp_mode_active = false;
            }
        } else {
            /* Normal mode, no fault - update timestamp */
            ctx->fault_clear_timestamp_ms = current_time_ms;
        }
    }
}

void lq_process_limp_home(struct lq_engine *engine)
{
    uint64_t current_time_ms = lq_platform_uptime_get();

    for (int i = 0; i < engine->num_limp_homes; i++) {
        process_single_limp_home(&engine->limp_homes[i], engine, current_time_ms);
    }
}
