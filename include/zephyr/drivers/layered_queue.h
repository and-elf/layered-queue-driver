/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_LAYERED_QUEUE_H_
#define ZEPHYR_INCLUDE_DRIVERS_LAYERED_QUEUE_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Layered Queue Driver API
 * @defgroup layered_queue_interface Layered Queue Interface
 * @ingroup io_interfaces
 * @{
 */

/**
 * @brief Status codes for layered queue items
 */
enum lq_status {
    /** Value is within normal operating range */
    LQ_OK = 0,
    /** Value is degraded but still functional */
    LQ_DEGRADED = 1,
    /** Value is out of acceptable range */
    LQ_OUT_OF_RANGE = 2,
    /** Hardware or communication error */
    LQ_ERROR = 3,
    /** Timeout waiting for data */
    LQ_TIMEOUT = 4,
    /** Inconsistent data from redundant sources */
    LQ_INCONSISTENT = 5,
};

/**
 * @brief Queue item containing value and status
 */
struct lq_item {
    /** Timestamp in milliseconds since boot */
    uint32_t timestamp;
    /** Value read from source or computed by merge */
    int32_t value;
    /** Status/health indicator */
    enum lq_status status;
    /** Source identifier (optional, for debugging) */
    uint8_t source_id;
    /** Sequence number for tracking updates */
    uint32_t sequence;
};

/**
 * @brief Queue drop policy
 */
enum lq_drop_policy {
    /** Drop oldest item when queue is full */
    LQ_DROP_OLDEST = 0,
    /** Drop newest (incoming) item when full */
    LQ_DROP_NEWEST = 1,
    /** Block writer until space is available */
    LQ_BLOCK = 2,
};

/**
 * @brief Voting/merge method for redundant inputs
 */
enum lq_voting_method {
    /** Use median value (good for outlier rejection) */
    LQ_VOTE_MEDIAN = 0,
    /** Average all input values */
    LQ_VOTE_AVERAGE = 1,
    /** Use minimum value */
    LQ_VOTE_MIN = 2,
    /** Use maximum value */
    LQ_VOTE_MAX = 3,
    /** Require majority consensus within tolerance */
    LQ_VOTE_MAJORITY = 4,
};

/**
 * @brief Expected range definition for validation
 */
struct lq_range {
    /** Minimum value (inclusive) */
    int32_t min;
    /** Maximum value (inclusive) */
    int32_t max;
    /** Status to assign if value is in this range */
    enum lq_status status;
};

/**
 * @brief Expected value definition for discrete state validation
 */
struct lq_expected_value {
    /** Expected value */
    int32_t value;
    /** Status to assign when this value is read */
    enum lq_status status;
};

/**
 * @brief Queue statistics
 */
struct lq_stats {
    /** Total items written to queue */
    uint32_t items_written;
    /** Total items read from queue */
    uint32_t items_read;
    /** Number of items dropped due to overflow */
    uint32_t items_dropped;
    /** Number of items currently in queue */
    uint32_t items_current;
    /** Peak queue utilization */
    uint32_t peak_usage;
};

/**
 * @brief Callback function for queue events
 *
 * @param dev Queue device
 * @param item Item that triggered the event
 * @param user_data User-provided data
 */
typedef void (*lq_callback_t)(const struct device *dev,
                               const struct lq_item *item,
                               void *user_data);

/**
 * @brief Push an item to a queue
 *
 * @param dev Queue device
 * @param item Item to push
 * @param timeout Timeout for blocking operations
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_push_t)(const struct device *dev,
                         const struct lq_item *item,
                         k_timeout_t timeout);

/**
 * @brief Pop an item from a queue
 *
 * @param dev Queue device
 * @param item Pointer to store popped item
 * @param timeout Timeout to wait for data
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_pop_t)(const struct device *dev,
                        struct lq_item *item,
                        k_timeout_t timeout);

/**
 * @brief Peek at the newest item without removing it
 *
 * @param dev Queue device
 * @param item Pointer to store peeked item
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_peek_t)(const struct device *dev,
                         struct lq_item *item);

/**
 * @brief Get queue statistics
 *
 * @param dev Queue device
 * @param stats Pointer to store statistics
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_get_stats_t)(const struct device *dev,
                              struct lq_stats *stats);

/**
 * @brief Register a callback for queue events
 *
 * @param dev Queue device
 * @param callback Callback function
 * @param user_data User data to pass to callback
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_register_callback_t)(const struct device *dev,
                                       lq_callback_t callback,
                                       void *user_data);

/**
 * @brief Layered Queue API structure
 */
__subsystem struct lq_driver_api {
    lq_push_t push;
    lq_pop_t pop;
    lq_peek_t peek;
    lq_get_stats_t get_stats;
    lq_register_callback_t register_callback;
};

/**
 * @brief Push an item to a queue
 *
 * @param dev Queue device
 * @param item Item to push
 * @param timeout Timeout for blocking operations
 * @return 0 on success, negative errno on failure
 */
static inline int lq_push(const struct device *dev,
                          const struct lq_item *item,
                          k_timeout_t timeout)
{
    const struct lq_driver_api *api =
        (const struct lq_driver_api *)dev->api;

    if (!api || !api->push) {
        return -ENOTSUP;
    }

    return api->push(dev, item, timeout);
}

/**
 * @brief Pop an item from a queue
 *
 * @param dev Queue device
 * @param item Pointer to store popped item
 * @param timeout Timeout to wait for data
 * @return 0 on success, negative errno on failure
 */
static inline int lq_pop(const struct device *dev,
                         struct lq_item *item,
                         k_timeout_t timeout)
{
    const struct lq_driver_api *api =
        (const struct lq_driver_api *)dev->api;

    if (!api || !api->pop) {
        return -ENOTSUP;
    }

    return api->pop(dev, item, timeout);
}

/**
 * @brief Peek at the newest item without removing it
 *
 * @param dev Queue device
 * @param item Pointer to store peeked item
 * @return 0 on success, negative errno on failure
 */
static inline int lq_peek(const struct device *dev,
                          struct lq_item *item)
{
    const struct lq_driver_api *api =
        (const struct lq_driver_api *)dev->api;

    if (!api || !api->peek) {
        return -ENOTSUP;
    }

    return api->peek(dev, item);
}

/**
 * @brief Get queue statistics
 *
 * @param dev Queue device
 * @param stats Pointer to store statistics
 * @return 0 on success, negative errno on failure
 */
static inline int lq_get_stats(const struct device *dev,
                               struct lq_stats *stats)
{
    const struct lq_driver_api *api =
        (const struct lq_driver_api *)dev->api;

    if (!api || !api->get_stats) {
        return -ENOTSUP;
    }

    return api->get_stats(dev, stats);
}

/**
 * @brief Register a callback for queue events
 *
 * @param dev Queue device
 * @param callback Callback function
 * @param user_data User data to pass to callback
 * @return 0 on success, negative errno on failure
 */
static inline int lq_register_callback(const struct device *dev,
                                       lq_callback_t callback,
                                       void *user_data)
{
    const struct lq_driver_api *api =
        (const struct lq_driver_api *)dev->api;

    if (!api || !api->register_callback) {
        return -ENOTSUP;
    }

    return api->register_callback(dev, callback, user_data);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_LAYERED_QUEUE_H_ */
