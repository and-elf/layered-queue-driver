/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Core layered queue API (platform-independent)
 */

#ifndef LAYERED_QUEUE_CORE_H_
#define LAYERED_QUEUE_CORE_H_

#include "lq_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Queue configuration
 */
struct lq_queue_config {
    /** Queue capacity */
    uint32_t capacity;
    /** Drop policy */
    enum lq_drop_policy drop_policy;
    /** Priority (for future use) */
    int priority;
};

/**
 * @brief Queue runtime data
 */
struct lq_queue_data {
    /** Ring buffer for queue items */
    struct lq_item *items;
    /** Read index */
    uint32_t read_idx;
    /** Write index */
    uint32_t write_idx;
    /** Current number of items */
    lq_atomic_t count;
    /** Statistics */
    struct lq_stats stats;
    /** Semaphore for blocking read operations */
    lq_sem_t sem_read;
    /** Semaphore for blocking write operations */
    lq_sem_t sem_write;
    /** Mutex for thread safety */
    lq_mutex_t mutex;
    /** Sequence counter */
    uint32_t sequence;
};

/**
 * @brief Queue handle
 */
typedef struct lq_queue {
    const struct lq_queue_config *config;
    struct lq_queue_data *data;
} lq_queue_t;

/**
 * @brief Initialize a queue
 * @param queue Queue handle
 * @param config Queue configuration
 * @param data Queue runtime data
 * @param items_buffer Buffer for queue items (must have config->capacity elements)
 * @return 0 on success, negative errno on failure
 */
int lq_queue_init(lq_queue_t *queue,
                  const struct lq_queue_config *config,
                  struct lq_queue_data *data,
                  struct lq_item *items_buffer);

/**
 * @brief Destroy a queue
 * @param queue Queue handle
 */
void lq_queue_destroy(lq_queue_t *queue);

/**
 * @brief Push an item to a queue
 * @param queue Queue handle
 * @param item Item to push
 * @param timeout_ms Timeout in milliseconds (0=no wait, UINT32_MAX=forever)
 * @return 0 on success, negative errno on failure
 */
int lq_queue_push(lq_queue_t *queue,
                  const struct lq_item *item,
                  uint32_t timeout_ms);

/**
 * @brief Pop an item from a queue
 * @param queue Queue handle
 * @param item Pointer to store popped item
 * @param timeout_ms Timeout in milliseconds (0=no wait, UINT32_MAX=forever)
 * @return 0 on success, negative errno on failure
 */
int lq_queue_pop(lq_queue_t *queue,
                 struct lq_item *item,
                 uint32_t timeout_ms);

/**
 * @brief Peek at the newest item without removing it
 * @param queue Queue handle
 * @param item Pointer to store peeked item
 * @return 0 on success, negative errno on failure
 */
int lq_queue_peek(lq_queue_t *queue,
                  struct lq_item *item);

/**
 * @brief Get queue statistics
 * @param queue Queue handle
 * @param stats Pointer to store statistics
 * @return 0 on success, negative errno on failure
 */
int lq_queue_get_stats(lq_queue_t *queue,
                       struct lq_stats *stats);

/**
 * @brief Get current queue count
 * @param queue Queue handle
 * @return Current number of items in queue
 */
uint32_t lq_queue_count(lq_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* LAYERED_QUEUE_CORE_H_ */
