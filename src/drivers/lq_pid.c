/* SPDX-License-Identifier: Apache-2.0 */
#include "lq_pid.h"
#include "lq_engine.h"
#include "layered_queue_core.h"
#include <limits.h>

/**
 * @brief Clamp value to range
 */
static int32_t clamp(int64_t value, int32_t min, int32_t max)
{
    if (value < min) return min;
    if (value > max) return max;
    return (int32_t)value;
}

/**
 * @brief Process a single PID controller
 */
static void process_single_pid(
    struct lq_pid_ctx *ctx,
    struct lq_engine *engine,
    uint64_t now)
{
    if (!ctx->enabled) {
        return;
    }

    /* Validate signal indices */
    if (ctx->setpoint_signal >= engine->num_signals ||
        ctx->measurement_signal >= engine->num_signals ||
        ctx->output_signal >= engine->num_signals) {
        return;
    }

    struct lq_signal *setpoint_sig = &engine->signals[ctx->setpoint_signal];
    struct lq_signal *measurement_sig = &engine->signals[ctx->measurement_signal];
    struct lq_signal *output_sig = &engine->signals[ctx->output_signal];

    /* Initialize on first run */
    if (ctx->first_run) {
        ctx->integral = 0;
        ctx->last_error = 0;
        ctx->last_setpoint = setpoint_sig->value;
        ctx->last_time = now;
        ctx->first_run = false;
        return; /* Skip first calculation, need dt */
    }

    /* Check for setpoint change */
    if (ctx->reset_on_setpoint_change && setpoint_sig->value != ctx->last_setpoint) {
        ctx->integral = 0; /* Reset integral on setpoint change */
        ctx->last_setpoint = setpoint_sig->value;
    }

    /* Calculate error */
    int32_t error = setpoint_sig->value - measurement_sig->value;

    /* Calculate time delta */
    uint64_t dt_us;
    if (ctx->sample_time_us > 0) {
        dt_us = ctx->sample_time_us; /* Fixed sample time */
    } else {
        dt_us = now - ctx->last_time; /* Variable sample time */
        if (dt_us == 0) {
            return; /* Avoid division by zero */
        }
    }

    /* Proportional term: P = Kp * error */
    int64_t p_term = ((int64_t)ctx->kp * error) / 1000;

    /* Integral term: I = Ki * sum(error * dt) */
    int64_t i_term = 0;
    if (ctx->ki != 0) {
        /* Only accumulate if outside deadband */
        int32_t abs_error = error < 0 ? -error : error;
        if (abs_error > ctx->deadband) {
            /* Accumulate error * dt */
            /* Scale: error is raw units, dt is microseconds, ki is scaled by 1000 */
            /* integral += error * dt_us / 1000000 (convert us to seconds) */
            /* Then i_term = ki * integral / 1000 */
            /* Combined: integral += error * dt_us * 1000 / 1000000 = error * dt_us / 1000 */
            ctx->integral += ((int64_t)error * dt_us) / 1000000; /* Convert to seconds */
            
            /* Apply integral anti-windup */
            if (ctx->integral > ctx->integral_max) {
                ctx->integral = ctx->integral_max;
            } else if (ctx->integral < ctx->integral_min) {
                ctx->integral = ctx->integral_min;
            }
        }
        
        i_term = ((int64_t)ctx->ki * ctx->integral) / 1000;
    }

    /* Derivative term: D = Kd * d(error)/dt */
    int64_t d_term = 0;
    if (ctx->kd != 0) {
        /* d(error)/dt = (error - last_error) / dt */
        /* Scale to per-second: multiply by 1000000 (us to s) */
        int32_t error_delta = error - ctx->last_error;
        d_term = ((int64_t)ctx->kd * error_delta * 1000000) / (dt_us * 1000);
    }

    /* Calculate total output */
    int64_t output = p_term + i_term + d_term;

    /* Clamp output to limits */
    int32_t clamped_output = clamp(output, ctx->output_min, ctx->output_max);

    /* Update output signal */
    output_sig->value = clamped_output;
    output_sig->status = LQ_EVENT_OK;
    output_sig->timestamp = now;
    output_sig->updated = true;

    /* Save state for next iteration */
    ctx->last_error = error;
    ctx->last_time = now;
}

void lq_process_pids(
    struct lq_engine *engine,
    struct lq_pid_ctx *pids,
    uint8_t num_pids,
    uint64_t now)
{
    for (uint8_t i = 0; i < num_pids; i++) {
        process_single_pid(&pids[i], engine, now);
    }
}
