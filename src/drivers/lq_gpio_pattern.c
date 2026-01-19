/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO Pattern Driver Implementation
 */

#include "lq_gpio_pattern.h"
#include "lq_engine.h"
#include "lq_platform.h"

void lq_gpio_pattern_init(
    struct lq_gpio_pattern_ctx *ctx,
    uint8_t gpio_pin,
    enum lq_gpio_pattern_type type,
    uint32_t period_us)
{
    if (!ctx) {
        return;
    }

    ctx->gpio_pin = gpio_pin;
    ctx->control_signal = 0xFF; /* No control signal by default */
    ctx->type = type;
    ctx->period_us = period_us;
    ctx->on_time_us = period_us / 2; /* 50% duty cycle default */
    ctx->pattern_bits = 0;
    ctx->pattern_length = 0;
    ctx->last_update_us = UINT64_MAX; /* Sentinel for "not initialized" */
    ctx->phase_us = 0;
    ctx->pattern_index = 0;
    ctx->current_state = false;
    ctx->enabled = true;
    ctx->inverted = false;
}

void lq_gpio_pattern_set_duty(
    struct lq_gpio_pattern_ctx *ctx,
    uint8_t duty_percent)
{
    if (!ctx || duty_percent > 100) {
        return;
    }

    ctx->on_time_us = (ctx->period_us * duty_percent) / 100;
}

void lq_gpio_pattern_set_custom(
    struct lq_gpio_pattern_ctx *ctx,
    uint32_t pattern,
    uint8_t length)
{
    if (!ctx || length == 0 || length > 32) {
        return;
    }

    ctx->type = LQ_GPIO_PATTERN_CUSTOM;
    ctx->pattern_bits = pattern;
    ctx->pattern_length = length;
    ctx->pattern_index = 0;
}

void lq_gpio_pattern_enable(
    struct lq_gpio_pattern_ctx *ctx,
    bool enabled)
{
    if (!ctx) {
        return;
    }

    ctx->enabled = enabled;

    /* Turn off GPIO when disabled */
    if (!enabled) {
        lq_gpio_set(ctx->gpio_pin, ctx->inverted ? true : false);
        ctx->current_state = false;
    }
}

static bool lq_gpio_pattern_calculate_state(
    const struct lq_gpio_pattern_ctx *ctx,
    uint32_t phase_us)
{
    bool state = false;

    switch (ctx->type) {
    case LQ_GPIO_PATTERN_STATIC:
        /* Static high */
        state = true;
        break;

    case LQ_GPIO_PATTERN_BLINK:
    case LQ_GPIO_PATTERN_PWM:
        /* High for on_time_us, then low */
        state = (phase_us < ctx->on_time_us);
        break;

    case LQ_GPIO_PATTERN_CUSTOM:
        /* Check bit at current pattern index */
        if (ctx->pattern_length > 0) {
            uint8_t bit_index = ctx->pattern_index % ctx->pattern_length;
            state = (ctx->pattern_bits & (1U << bit_index)) != 0;
        }
        break;
    }

    return state;
}

void lq_process_gpio_patterns(
    struct lq_engine *engine,
    struct lq_gpio_pattern_ctx *patterns,
    uint8_t num_patterns,
    uint64_t now)
{
    if (!patterns) {
        return;
    }

    for (uint8_t i = 0; i < num_patterns; i++) {
        struct lq_gpio_pattern_ctx *ctx = &patterns[i];

        /* Check if pattern is enabled */
        if (!ctx->enabled) {
            continue;
        }

        /* Check control signal if configured */
        if (engine && ctx->control_signal != 0xFF) {
            if (ctx->control_signal >= engine->num_signals) {
                continue;
            }

            const struct lq_signal *sig = &engine->signals[ctx->control_signal];

            /* Use signal value to control pattern:
             * 0 = disabled
             * non-zero = enabled
             * Can also use signal value to control frequency/duty cycle in future
             */
            if (sig->value == 0) {
                if (ctx->current_state) {
                    lq_gpio_set(ctx->gpio_pin, ctx->inverted ? true : false);
                    ctx->current_state = false;
                }
                continue;
            }
        }

        /* Initialize timing on first run
         * Use UINT64_MAX as sentinel for "not initialized"
         * This allows us to handle first call at time 0 correctly
         */
        bool is_first_run = (ctx->last_update_us == UINT64_MAX);

        if (is_first_run) {
            /* First run - initialize and output initial state */
            ctx->last_update_us = now;
            ctx->phase_us = 0;

            /* Calculate and set initial state */
            bool initial_state = lq_gpio_pattern_calculate_state(ctx, 0);
            if (ctx->inverted) {
                initial_state = !initial_state;
            }
            lq_gpio_set(ctx->gpio_pin, initial_state);
            ctx->current_state = initial_state;
            continue; /* Skip rest of processing on first run */
        }

        /* Calculate elapsed time and update phase */
        uint64_t elapsed = now - ctx->last_update_us;
        ctx->last_update_us = now;
        ctx->phase_us += (uint32_t)elapsed;

        /* Handle period wrap-around */
        if (ctx->period_us > 0) {
            while (ctx->phase_us >= ctx->period_us) {
                ctx->phase_us -= ctx->period_us;

                /* Advance custom pattern index */
                if (ctx->type == LQ_GPIO_PATTERN_CUSTOM && ctx->pattern_length > 0) {
                    ctx->pattern_index++;
                    if (ctx->pattern_index >= ctx->pattern_length) {
                        ctx->pattern_index = 0;
                    }
                }
            }
        }

        /* Calculate desired state */
        bool desired_state = lq_gpio_pattern_calculate_state(ctx, ctx->phase_us);

        /* Apply inversion if configured */
        if (ctx->inverted) {
            desired_state = !desired_state;
        }

        /* Update GPIO if state changed */
        if (desired_state != ctx->current_state) {
            lq_gpio_set(ctx->gpio_pin, desired_state);
            ctx->current_state = desired_state;
        }
    }
}
