/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform abstraction for logging
 */

#ifndef LQ_LOG_H_
#define LQ_LOG_H_

#include "lq_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform-specific logging backends
 * ========================================================================== */

#if defined(CONFIG_ZEPHYR_RTOS)
/* Zephyr logging */
#include <zephyr/logging/log.h>

#define LQ_LOG_MODULE_REGISTER(name, level) LOG_MODULE_REGISTER(name, level)
#define LQ_LOG_ERR(...)  LOG_ERR(__VA_ARGS__)
#define LQ_LOG_WRN(...)  LOG_WRN(__VA_ARGS__)
#define LQ_LOG_INF(...)  LOG_INF(__VA_ARGS__)
#define LQ_LOG_DBG(...)  LOG_DBG(__VA_ARGS__)

#elif defined(__linux__) || defined(__APPLE__)
/* Native POSIX - printf-based logging */
#include <stdio.h>

#define LQ_LOG_MODULE_REGISTER(name, level) /* No-op */
#define LQ_LOG_ERR(fmt, ...)  printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#define LQ_LOG_WRN(fmt, ...)  printf("[WRN] " fmt "\n", ##__VA_ARGS__)
#define LQ_LOG_INF(fmt, ...)  printf("[INF] " fmt "\n", ##__VA_ARGS__)
#define LQ_LOG_DBG(fmt, ...)  printf("[DBG] " fmt "\n", ##__VA_ARGS__)

#else
/* Bare-metal - no logging */
#define LQ_LOG_MODULE_REGISTER(name, level) /* No-op */
#define LQ_LOG_ERR(...)  /* No-op */
#define LQ_LOG_WRN(...)  /* No-op */
#define LQ_LOG_INF(...)  /* No-op */
#define LQ_LOG_DBG(...)  /* No-op */

#endif

/* ============================================================================
 * Log levels (for future use)
 * ========================================================================== */

enum lq_log_level {
    LQ_LOG_LEVEL_NONE = 0,
    LQ_LOG_LEVEL_ERR = 1,
    LQ_LOG_LEVEL_WRN = 2,
    LQ_LOG_LEVEL_INF = 3,
    LQ_LOG_LEVEL_DBG = 4,
};

#ifdef __cplusplus
}
#endif

#endif /* LQ_LOG_H_ */
