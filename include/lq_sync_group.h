/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Synchronized output groups
 * 
 * Coordinates multiple output drivers to ensure atomic,
 * time-synchronized updates across different hardware interfaces.
 */

#ifndef LQ_SYNC_GROUP_H_
#define LQ_SYNC_GROUP_H_

#include "lq_event.h"
#include "lq_engine.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Synchronized output group
 * 
 * Groups multiple output drivers together so they can be updated
 * atomically at a specific period. Useful for ensuring consistent
 * snapshots across CAN, GPIO, and other outputs.
 * 
 * Example: Update CAN frame and warning LED simultaneously every 100ms
 */
struct lq_sync_group {
    const char *name;                      /**< Group name for debugging */
    struct lq_output_driver **members;     /**< Array of output drivers */
    size_t num_members;                    /**< Number of members */
    uint64_t period_us;                    /**< Update period (microseconds) */
    uint64_t last_update_ts;               /**< Last update timestamp */
    bool use_staging;                      /**< Use stage/commit pattern */
};

/**
 * @brief Initialize synchronized output group
 * 
 * @param group Group instance
 * @return 0 on success, negative errno on failure
 */
int lq_sync_group_init(struct lq_sync_group *group);

/**
 * @brief Update synchronized group
 * 
 * Evaluates all member outputs and updates them atomically if:
 * 1. Sufficient time has elapsed (based on period_us)
 * 2. At least one source has new data
 * 
 * If use_staging is true, uses stage/commit pattern for atomicity.
 * Otherwise, writes directly to all outputs.
 * 
 * @param group Group instance
 * @param now Current timestamp (microseconds)
 * @param events Array of events to consider
 * @param num_events Number of events
 * @return 0 on success, negative errno on failure
 */
int lq_sync_group_update(struct lq_sync_group *group,
                         uint64_t now,
                         const struct lq_event *events,
                         size_t num_events);

/**
 * @brief Check if group is due for update
 * 
 * @param group Group instance
 * @param now Current timestamp
 * @return true if update period has elapsed
 */
static inline bool lq_sync_group_is_due(const struct lq_sync_group *group,
                                         uint64_t now)
{
    if (!group || group->period_us == 0) {
        return false;
    }
    return (now - group->last_update_ts) >= group->period_us;
}

#ifdef __cplusplus
}
#endif

#endif /* LQ_SYNC_GROUP_H_ */
