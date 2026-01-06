/* SPDX-License-Identifier: Apache-2.0 */
#include "lq_verified_output.h"
#include "lq_engine.h"
#include "layered_queue_core.h"

/**
 * @brief Check if command matches verification within tolerance
 */
static bool is_verified(int32_t command, int32_t verification, int32_t tolerance)
{
    int32_t error = command - verification;
    if (error < 0) {
        error = -error; /* abs() */
    }
    return error <= tolerance;
}

/**
 * @brief Process a single verified output
 */
static void process_single_verified_output(
    struct lq_verified_output_ctx *ctx,
    struct lq_engine *engine,
    uint64_t now)
{
    if (!ctx->enabled) {
        return;
    }

    /* Validate signal indices */
    if (ctx->command_signal >= engine->num_signals ||
        ctx->verification_signal >= engine->num_signals ||
        ctx->output_signal >= engine->num_signals) {
        return;
    }

    struct lq_signal *cmd_sig = &engine->signals[ctx->command_signal];
    struct lq_signal *verify_sig = &engine->signals[ctx->verification_signal];
    struct lq_signal *out_sig = &engine->signals[ctx->output_signal];

    /* Detect command changes */
    if (cmd_sig->value != ctx->last_command) {
        ctx->last_command = cmd_sig->value;
        ctx->command_timestamp = now;
        ctx->waiting_for_verify = (ctx->verify_timeout_us > 0);
    }

    /* Determine if we should verify now */
    bool should_verify = false;
    
    if (ctx->continuous_verify) {
        /* Continuous mode: always verify after timeout expires */
        if (ctx->waiting_for_verify) {
            uint64_t elapsed = now - ctx->command_timestamp;
            if (elapsed >= ctx->verify_timeout_us) {
                should_verify = true;
                ctx->waiting_for_verify = false;
            }
        } else {
            /* Already past timeout, keep verifying */
            should_verify = true;
        }
    } else {
        /* One-shot mode: verify once after timeout */
        if (ctx->waiting_for_verify) {
            uint64_t elapsed = now - ctx->command_timestamp;
            if (elapsed >= ctx->verify_timeout_us) {
                should_verify = true;
                ctx->waiting_for_verify = false;
            }
        }
    }

    /* Perform verification check */
    if (should_verify) {
        bool verified = is_verified(cmd_sig->value, verify_sig->value, ctx->tolerance);
        
        /* Update output signal */
        out_sig->value = verify_sig->value; /* Actual measured value */
        out_sig->status = verified ? LQ_EVENT_OK : LQ_EVENT_ERROR;
        out_sig->timestamp = now;
        out_sig->updated = true;
    } else {
        /* Waiting for timeout - output is pending */
        out_sig->value = cmd_sig->value; /* Show commanded value */
        out_sig->status = LQ_EVENT_OK; /* Not yet verified, but not faulted */
        out_sig->timestamp = now;
        out_sig->updated = false;
    }
}

void lq_process_verified_outputs(
    struct lq_engine *engine,
    struct lq_verified_output_ctx *outputs,
    uint8_t num_outputs,
    uint64_t now)
{
    for (uint8_t i = 0; i < num_outputs; i++) {
        process_single_verified_output(&outputs[i], engine, now);
    }
}
