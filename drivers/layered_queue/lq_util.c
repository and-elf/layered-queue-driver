/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr wrapper for utility functions (just includes core implementation)
 */

#include "lq_util.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lq_util, CONFIG_LQ_LOG_LEVEL);
