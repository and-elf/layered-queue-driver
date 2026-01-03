/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Engine implementation
 */

#include "lq_engine.h"
#include "lq_hw_input.h"
#include "lq_mid_driver.h"
#include "lq_platform.h"
#include "lq_util.h"
#include <string.h>

/* ============================================================================
 * Engine initialization
 * ============================================================================ */

int lq_engine_init(struct lq_engine *engine)
{
    if (!engine) {
        return -22; /* -EINVAL */
    }
    
    /* Reset runtime state only, preserving static configuration */
    engine->out_event_count = 0;
    
    /* Reset signal runtime state */
    for (int i = 0; i < LQ_MAX_SIGNALS; i++) {
        engine->signals[i].updated = false;
    }
    
    return 0;
}

/* ============================================================================
 * Phase 1: Ingest events into signals
 * ============================================================================ */

void lq_ingest_events(
    struct lq_engine *e,
    const struct lq_event *events,
    size_t n_events)
{
    for (size_t i = 0; i < n_events; i++) {
        const struct lq_event *evt = &events[i];
        
        if (evt->source_id >= LQ_MAX_SIGNALS) {
            continue; /* Invalid signal ID */
        }
        
        struct lq_signal *sig = &e->signals[evt->source_id];
        
        /* Update signal value and status */
        bool value_changed = (sig->value != evt->value);
        sig->value = evt->value;
        sig->status = evt->status;
        sig->timestamp = evt->timestamp;
        sig->updated = value_changed;
    }
}

/* ============================================================================
 * Phase 2: Apply staleness detection
 * ============================================================================ */

void lq_apply_input_staleness(
    struct lq_engine *e,
    uint64_t now)
{
    for (uint8_t i = 0; i < e->num_signals; i++) {
        struct lq_signal *sig = &e->signals[i];
        
        /* Skip if no staleness check configured */
        if (sig->stale_us == 0) {
            continue;
        }
        
        /* Check if signal is stale */
        uint64_t age = now - sig->timestamp;
        if (age > sig->stale_us) {
            sig->status = LQ_EVENT_TIMEOUT;
            sig->updated = true;
        }
    }
}

/* ============================================================================
 * Phase 3: Process merge/voter logic
 * ============================================================================ */

void lq_process_merges(
    struct lq_engine *e,
    uint64_t now)
{
    for (uint8_t i = 0; i < e->num_merges; i++) {
        struct lq_merge_ctx *merge = &e->merges[i];
        
        if (!merge->enabled) {
            continue;
        }
        
        /* Collect input values */
        int32_t values[8];
        uint8_t valid_count = 0;
        
        for (uint8_t j = 0; j < merge->num_inputs; j++) {
            uint8_t sig_idx = merge->input_signals[j];
            if (sig_idx >= e->num_signals) {
                continue;
            }
            
            struct lq_signal *sig = &e->signals[sig_idx];
            
            /* Only include OK signals */
            if (sig->status == LQ_EVENT_OK) {
                values[valid_count++] = sig->value;
            }
        }
        
        /* Need at least one valid input */
        if (valid_count == 0) {
            e->signals[merge->output_signal].status = LQ_EVENT_ERROR;
            continue;
        }
        
        /* Apply voting algorithm */
        int32_t result;
        enum lq_status vote_status = LQ_OK;
        
        int rc = lq_vote(values, valid_count, merge->voting_method, merge->tolerance, &result, &vote_status);
        
        /* Convert lq_status to lq_event_status */
        enum lq_event_status result_status = LQ_EVENT_OK;
        if (rc != 0 || vote_status == LQ_ERROR || vote_status == LQ_TIMEOUT) {
            result_status = LQ_EVENT_ERROR;
        } else if (vote_status == LQ_OUT_OF_RANGE) {
            result_status = LQ_EVENT_OUT_OF_RANGE;
        } else if (vote_status == LQ_DEGRADED) {
            result_status = LQ_EVENT_DEGRADED;
        } else if (vote_status == LQ_INCONSISTENT) {
            result_status = LQ_EVENT_INCONSISTENT;
        }
        
        /* Check tolerance if multiple inputs */
        if (valid_count > 1 && merge->tolerance > 0) {
            int32_t min_val = result;
            int32_t max_val = result;
            
            for (uint8_t j = 0; j < valid_count; j++) {
                if (values[j] < min_val) min_val = values[j];
                if (values[j] > max_val) max_val = values[j];
            }
            
            uint32_t spread = (uint32_t)(max_val - min_val);
            if (spread > merge->tolerance) {
                result_status = LQ_EVENT_INCONSISTENT;
            }
        }
        /* Update output signal */
        struct lq_signal *out = &e->signals[merge->output_signal];
        bool changed = (out->value != result);
        out->value = result;
        out->status = result_status;
        out->timestamp = now;
        out->updated = changed;
    }
}

/* ============================================================================
 * Phase 4: Process on-change outputs
 * ============================================================================ */

void lq_process_outputs(struct lq_engine *e)
{
    /* On-change outputs would be implemented here */
    /* For now, this is a placeholder for future on-change logic */
    (void)e;
}

/* ============================================================================
 * Phase 5: Process cyclic outputs
 * ============================================================================ */

void lq_process_cyclic_outputs(
    struct lq_engine *e,
    uint64_t now)
{
    for (uint8_t i = 0; i < e->num_cyclic_outputs; i++) {
        struct lq_cyclic_ctx *ctx = &e->cyclic_outputs[i];
        lq_cyclic_process(e, ctx, now);
    }
}

/* ============================================================================
 * Main engine step
 * ============================================================================ */

void lq_engine_step(
    struct lq_engine *engine,
    uint64_t now,
    const struct lq_event *events,
    size_t n_events)
{
    if (!engine) {
        return;
    }
    
    /* Reset output event count */
    engine->out_event_count = 0;
    
    /* Execute processing phases */
    lq_ingest_events(engine, events, n_events);
    lq_apply_input_staleness(engine, now);
    lq_process_merges(engine, now);
    lq_process_outputs(engine);
    lq_process_cyclic_outputs(engine, now);
}

/* ============================================================================
 * Engine task implementations
 * ============================================================================ */

#ifdef LQ_PLATFORM_NATIVE

#include <pthread.h>
#include <unistd.h>

/* Global engine instance */
static struct lq_engine g_engine;

static void *engine_thread_func(void *arg)
{
    (void)arg;
    
    struct lq_event events[LQ_MAX_OUTPUT_EVENTS];
    
    while (1) {
        uint64_t now = lq_platform_uptime_get() * 1000; /* Convert ms to us */
        size_t num_events = 0;
        
        /* Collect events from hardware layer */
        struct lq_hw_sample sample;
        while (num_events < LQ_MAX_OUTPUT_EVENTS && lq_hw_pop(&sample) == 0) {
            /* Convert sample to event */
            /* In real implementation, route through mid-level drivers */
            events[num_events].source_id = sample.src;
            events[num_events].value = sample.value;
            events[num_events].status = LQ_EVENT_OK;
            events[num_events].timestamp = sample.timestamp;
            num_events++;
        }
        
        /* Run engine step */
        lq_engine_step(&g_engine, now, events, num_events);
        
        /* Transmit output events */
        for (uint8_t i = 0; i < g_engine.out_event_count; i++) {
            /* Platform-specific transmission would go here */
            /* For now, just a placeholder */
        }
        
        /* Sleep for cycle time */
        usleep(100); /* 100us = 10kHz */
    }
    
    return NULL;
}

#ifndef CONFIG_LQ_ENGINE_TASK
int lq_engine_task_start(void)
{
    pthread_t thread;
    int ret;
    
    /* Initialize hardware input */
    ret = lq_hw_input_init(128);
    if (ret != 0) {
        return ret;
    }
    
    /* Initialize engine */
    ret = lq_engine_init(&g_engine);
    if (ret != 0) {
        return ret;
    }
    
    /* Create thread */
    ret = pthread_create(&thread, NULL, engine_thread_func, NULL);
    if (ret != 0) {
        return -ret;
    }
    
    pthread_detach(thread);
    
    return 0;
}

void lq_engine_task_run(void)
{
    /* For bare metal - just run the loop directly */
    engine_thread_func(NULL);
}
#endif

struct lq_engine *lq_engine_get_instance(void)
{
    return &g_engine;
}

#endif /* LQ_PLATFORM_NATIVE */

#ifdef __ZEPHYR__

#include <zephyr/kernel.h>

/* Global engine instance */
static struct lq_engine g_engine;

#ifdef CONFIG_LQ_ENGINE_TASK_ZEPHYR

#ifndef CONFIG_LQ_ENGINE_TASK_STACK_SIZE
#define CONFIG_LQ_ENGINE_TASK_STACK_SIZE 2048
#endif

#ifndef CONFIG_LQ_ENGINE_TASK_PRIORITY
#define CONFIG_LQ_ENGINE_TASK_PRIORITY 5
#endif

#ifndef CONFIG_LQ_ENGINE_CYCLE_TIME_US
#define CONFIG_LQ_ENGINE_CYCLE_TIME_US 100
#endif

K_THREAD_STACK_DEFINE(engine_stack, CONFIG_LQ_ENGINE_TASK_STACK_SIZE);
static struct k_thread engine_thread;

static void engine_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    struct lq_event events[LQ_MAX_OUTPUT_EVENTS];
    
    while (1) {
        uint64_t now = k_uptime_get() * 1000; /* Convert ms to us */
        size_t num_events = 0;
        
        /* Collect events from hardware layer */
        struct lq_hw_sample sample;
        while (num_events < LQ_MAX_OUTPUT_EVENTS && lq_hw_pop(&sample) == 0) {
            /* Convert sample to event */
            events[num_events].source_id = sample.src;
            events[num_events].value = sample.value;
            events[num_events].status = LQ_EVENT_OK;
            events[num_events].timestamp = sample.timestamp;
            num_events++;
        }
        
        /* Run engine step */
        lq_engine_step(&g_engine, now, events, num_events);
        
        /* Transmit output events */
        for (uint8_t i = 0; i < g_engine.out_event_count; i++) {
            /* Platform-specific transmission would go here */
        }
        
        /* Sleep for cycle time */
        k_usleep(CONFIG_LQ_ENGINE_CYCLE_TIME_US);
    }
}

static int lq_engine_task_init(void)
{
    int ret;
    
    /* Initialize hardware input */
    ret = lq_hw_input_init(CONFIG_LQ_HW_RINGBUFFER_SIZE);
    if (ret != 0) {
        return ret;
    }
    
    /* Initialize engine */
    ret = lq_engine_init(&g_engine);
    if (ret != 0) {
        return ret;
    }
    
    /* Create and start thread */
    k_thread_create(&engine_thread, engine_stack,
                    K_THREAD_STACK_SIZEOF(engine_stack),
                    engine_thread_entry,
                    NULL, NULL, NULL,
                    CONFIG_LQ_ENGINE_TASK_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_name_set(&engine_thread, "lq_engine");
    
    return 0;
}

SYS_INIT(lq_engine_task_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_LQ_ENGINE_TASK_ZEPHYR */

struct lq_engine *lq_engine_get_instance(void)
{
    return &g_engine;
}

#endif /* __ZEPHYR__ */
