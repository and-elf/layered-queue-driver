/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Protocol Driver Interface
 * 
 * Unified interface for bidirectional network protocols (J1939, CANopen, etc.)
 * Combines mid-level driver (decode RX) and output driver (encode TX) with shared state.
 */

#ifndef LQ_PROTOCOL_H_
#define LQ_PROTOCOL_H_

#include "lq_mid_driver.h"
#include "lq_event.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Protocol message structure
 * 
 * Generic container for network messages (CAN frames, etc.)
 */
struct lq_protocol_msg {
    uint32_t id;                  /**< Message ID (CAN ID, COB-ID, etc) */
    uint8_t data[8];              /**< Message payload */
    uint8_t len;                  /**< Payload length */
    uint64_t timestamp;           /**< Reception timestamp */
    uint32_t flags;               /**< Protocol-specific flags */
};

/**
 * @brief Protocol decode mapping
 * 
 * Maps incoming protocol messages to signal IDs
 */
struct lq_protocol_decode_map {
    uint32_t protocol_id;         /**< Protocol ID (PGN, COB-ID, etc) */
    uint32_t *signal_ids;         /**< Array of signal IDs extracted from this message */
    size_t num_signals;           /**< Number of signals */
};

/**
 * @brief Protocol encode mapping
 * 
 * Maps signal IDs to outgoing protocol messages
 */
struct lq_protocol_encode_map {
    uint32_t protocol_id;         /**< Protocol ID (PGN, COB-ID, etc) */
    uint32_t *signal_ids;         /**< Array of signal IDs to encode in this message */
    size_t num_signals;           /**< Number of signals */
    uint32_t period_ms;           /**< Cyclic period (0 = on-change only) */
    bool on_change;               /**< Transmit on signal change */
};

/**
 * @brief Protocol driver configuration
 */
struct lq_protocol_config {
    uint8_t node_address;         /**< Node address on the bus */
    
    /* Decode configuration */
    const struct lq_protocol_decode_map *decode_maps;
    size_t num_decode_maps;
    
    /* Encode configuration */
    const struct lq_protocol_encode_map *encode_maps;
    size_t num_encode_maps;
    
    /* Protocol-specific settings */
    void *protocol_settings;
};

/* Forward declaration */
struct lq_protocol_driver;

/**
 * @brief Protocol driver virtual table
 * 
 * Implements both decode (RX) and encode (TX) operations.
 */
struct lq_protocol_vtbl {
    /**
     * @brief Initialize protocol driver
     * 
     * @param proto Protocol driver instance
     * @param config Protocol configuration
     * @return 0 on success, negative errno on failure
     */
    int (*init)(struct lq_protocol_driver *proto, 
                const struct lq_protocol_config *config);
    
    /**
     * @brief Decode incoming protocol message to events
     * 
     * Pure function - parses protocol message and generates events.
     * Used as mid-level driver for RX path.
     * 
     * @param proto Protocol driver instance
     * @param now Current timestamp (microseconds)
     * @param msg Incoming protocol message
     * @param out_events Output buffer for generated events
     * @param max_events Maximum number of events
     * @return Number of events generated (0 to max_events)
     */
    size_t (*decode)(
        struct lq_protocol_driver *proto,
        uint64_t now,
        const struct lq_protocol_msg *msg,
        struct lq_event *out_events,
        size_t max_events);
    
    /**
     * @brief Encode events to protocol message
     * 
     * Takes events and formats them into protocol messages.
     * Used as output driver for TX path.
     * 
     * @param proto Protocol driver instance
     * @param events Array of events to encode
     * @param num_events Number of events
     * @param out_msg Output protocol message
     * @return 0 on success, negative errno on failure
     */
    int (*encode)(
        struct lq_protocol_driver *proto,
        const struct lq_event *events,
        size_t num_events,
        struct lq_protocol_msg *out_msg);
    
    /**
     * @brief Get cyclic messages that need transmission
     * 
     * Called periodically to check for cyclic transmission requirements.
     * 
     * @param proto Protocol driver instance
     * @param now Current timestamp (microseconds)
     * @param out_msgs Output buffer for messages to transmit
     * @param max_msgs Maximum number of messages
     * @return Number of messages to transmit
     */
    size_t (*get_cyclic)(
        struct lq_protocol_driver *proto,
        uint64_t now,
        struct lq_protocol_msg *out_msgs,
        size_t max_msgs);
    
    /**
     * @brief Update signal value for encoding
     * 
     * Called when engine generates new signal values.
     * Protocol driver caches these for encoding.
     * 
     * @param proto Protocol driver instance
     * @param signal_id Signal identifier
     * @param value Signal value
     * @param timestamp Signal timestamp
     */
    void (*update_signal)(
        struct lq_protocol_driver *proto,
        uint32_t signal_id,
        int32_t value,
        uint64_t timestamp);
};

/**
 * @brief Protocol driver instance
 * 
 * Unified driver that handles both RX decode and TX encode
 * with shared protocol state.
 */
struct lq_protocol_driver {
    const struct lq_protocol_vtbl *vtbl;   /**< Virtual table */
    void *ctx;                              /**< Protocol-specific context */
    
    /* Configuration */
    struct lq_protocol_config config;
    
    /* Mid-level driver wrapper for RX path */
    struct lq_mid_driver mid_driver;
    
    /* Output driver wrapper for TX path */
    struct lq_output_driver output_driver;
};

/**
 * @brief Initialize protocol driver
 * 
 * @param proto Protocol driver instance
 * @param config Protocol configuration
 * @return 0 on success, negative errno on failure
 */
static inline int lq_protocol_init(struct lq_protocol_driver *proto,
                                   const struct lq_protocol_config *config)
{
    if (!proto || !proto->vtbl || !proto->vtbl->init) {
        return -1;
    }
    return proto->vtbl->init(proto, config);
}

/**
 * @brief Decode protocol message
 * 
 * @param proto Protocol driver instance
 * @param now Current timestamp
 * @param msg Protocol message
 * @param out_events Output event buffer
 * @param max_events Maximum events
 * @return Number of events generated
 */
static inline size_t lq_protocol_decode(struct lq_protocol_driver *proto,
                                        uint64_t now,
                                        const struct lq_protocol_msg *msg,
                                        struct lq_event *out_events,
                                        size_t max_events)
{
    if (!proto || !proto->vtbl || !proto->vtbl->decode) {
        return 0;
    }
    return proto->vtbl->decode(proto, now, msg, out_events, max_events);
}

/**
 * @brief Encode events to protocol message
 * 
 * @param proto Protocol driver instance
 * @param events Events to encode
 * @param num_events Number of events
 * @param out_msg Output message
 * @return 0 on success, negative errno on failure
 */
static inline int lq_protocol_encode(struct lq_protocol_driver *proto,
                                     const struct lq_event *events,
                                     size_t num_events,
                                     struct lq_protocol_msg *out_msg)
{
    if (!proto || !proto->vtbl || !proto->vtbl->encode) {
        return -1;
    }
    return proto->vtbl->encode(proto, events, num_events, out_msg);
}

/**
 * @brief Update signal value
 * 
 * @param proto Protocol driver instance
 * @param signal_id Signal ID
 * @param value Signal value
 * @param timestamp Timestamp
 */
static inline void lq_protocol_update_signal(struct lq_protocol_driver *proto,
                                             uint32_t signal_id,
                                             int32_t value,
                                             uint64_t timestamp)
{
    if (proto && proto->vtbl && proto->vtbl->update_signal) {
        proto->vtbl->update_signal(proto, signal_id, value, timestamp);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* LQ_PROTOCOL_H_ */
