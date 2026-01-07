# Protocol Drivers Guide

## Overview

Protocol drivers in the Layered Queue Driver provide unified bidirectional protocol support **across any transport** (CAN, UART, SPI, GPIO, etc.), combining:
- **Decode (RX)**: Parse incoming messages into internal signals (mid-level driver)
- **Encode (TX)**: Format internal signals into outgoing messages (output driver)
- **Shared state**: Protocol context shared between RX and TX paths

This design is **transport-agnostic** - the same pattern works for CAN networks, serial protocols, SPI devices, GPIO patterns, or custom transports.

## Architecture

```
Transport RX → Protocol.decode() → Events → Engine → Protocol.encode() → Transport TX
   (Any)        ↑ (mid-level)                        ↓ (output)             (Any)
                └────────── Shared Protocol State ──────────┘
                     (addressing, sessions, counters)
```

### Supported Transports

- **CAN**: J1939, CANopen, custom CAN protocols
- **UART/Serial**: Modbus RTU, custom ASCII/binary protocols, configuration interfaces
- **SPI**: Sensor protocols, peripheral communication
- **GPIO**: PWM encoding, parallel data, bit-banged protocols
- **Custom**: Any user-defined transport

### Data Flow

1. **RX Path**: Hardware → Ringbuffer → `protocol.decode()` → Events → Engine
2. **TX Path**: Engine → Events → `protocol.update_signal()` → `protocol.get_cyclic()` → Hardware

### Why Unified Protocol Drivers?

**Before** (separate RX/TX):
- ❌ Duplicate state management
- ❌ No coordination between RX/TX
- ❌ Complex multi-address/session protocols difficult

**After** (unified):
- ✅ Single source of truth for protocol state
- ✅ Coordinated request/response handling
- ✅ Natural support for stateful protocols
- ✅ Simplified configuration

## Built-in Protocols

### J1939 (SAE J1939)

Heavy-duty vehicle CAN protocol with PGN-based messaging.

**Features**:
- Standard PGN support (EEC1, ET1, DM1, etc.)
- Configurable cyclic transmission
- Diagnostic message support
- Priority and addressing

**Example Configuration**:
```dts
j1939: j1939-protocol {
    compatible = "lq,protocol-j1939";
    node-address = <0x25>;  /* ECU address */
    priority = <6>;
    
    /* Decode EEC1 (Engine Controller) */
    eec1-rx {
        pgn = <65265>;  /* EEC1 */
        direction = "rx";
        signal-ids = <&engine_rpm &engine_torque>;
        timeout-ms = <100>;
    };
    
    /* Encode EEC1 for transmission */
    eec1-tx {
        pgn = <65265>;
        direction = "tx";
        signal-ids = <&my_rpm &my_torque>;
        period-ms = <50>;  /* 20 Hz cyclic */
        on-change;
    };
    
    /* Diagnostic messages */
    dm1-tx {
        pgn = <65226>;  /* DM1 - Active DTCs */
        direction = "tx";
        signal-ids = <&dtc_status>;
        on-change;
    };
};

&can0 {
    protocol = <&j1939>;  /* Link to CAN hardware */
};
```

**Usage in Code**:
```c
#include "lq_j1939.h"

struct lq_protocol_driver j1939_proto;
struct lq_protocol_config config = {
    .node_address = 0x25,
    /* ... configure decode/encode maps ... */
};

/* Create protocol driver */
lq_j1939_protocol_create(&j1939_proto, &config);

/* Protocol automatically handles RX/TX through engine */
```

### CANopen (DS-301)

Industrial automation CAN protocol with PDO, SDO, and NMT.

**Features**:
- PDO (Process Data Objects) for real-time data
- NMT (Network Management) state machine
- Heartbeat monitoring
- Configurable SYNC-based or event-driven transmission

**Example Configuration**:
```dts
canopen: canopen-protocol {
    compatible = "lq,protocol-canopen";
    node-id = <5>;
    heartbeat-period-ms = <1000>;
    nmt-startup-state = "pre-operational";
    
    /* TPDO1 - Transmit sensor data */
    tpdo1 {
        pdo-type = "tpdo1";
        cob-id = <0x185>;  /* 0x180 + node-id */
        transmission-type = <254>;  /* Event-driven */
        event-timer-ms = <100>;
        
        /* Map 2 signals into PDO */
        map-speed {
            signal-id = <&motor_speed>;
            length = <16>;
        };
        map-position {
            signal-id = <&motor_position>;
            length = <32>;
        };
    };
    
    /* RPDO1 - Receive commands */
    rpdo1 {
        pdo-type = "rpdo1";
        cob-id = <0x205>;  /* 0x200 + node-id */
        
        map-cmd {
            signal-id = <&motor_command>;
            length = <16>;
        };
    };
};

&can1 {
    protocol = <&canopen>;
};
```

**Usage in Code**:
```c
#include "lq_canopen.h"

struct lq_protocol_driver canopen_proto;
struct lq_protocol_config config = {
    .node_address = 5,
    /* ... configure PDO maps ... */
};

lq_canopen_protocol_create(&canopen_proto, &config);

/* Send emergency message */
lq_canopen_send_emergency(&canopen_proto, 
                          CANOPEN_EMCY_TEMPERATURE,
                          0x01, 
                          NULL);

/* Change NMT state */
lq_canopen_set_nmt_state(&canopen_proto, CANOPEN_NMT_OPERATIONAL);
```

## Creating Custom Protocols

Users can implement custom protocols by providing the `lq_protocol_vtbl` interface. This guide shows both **platform-independent implementation** (the core protocol logic) and **Zephyr integration** (device tree wrapper).

### Protocol Implementation Checklist

1. ✅ Define protocol context structure (state, buffers, counters)
2. ✅ Implement vtable functions (init, decode, encode, get_cyclic, update_signal)
3. ✅ Create header with public API and vtable export
4. ✅ For Zephyr: Create device tree binding (YAML)
5. ✅ For Zephyr: Create DT wrapper with MTU-sized buffers
6. ✅ For Zephyr: Add Kconfig option and CMakeLists integration

### Implementation Guide

This guide walks through creating a **UART configuration protocol** that supports:
- ASCII command/response format
- SET/GET commands for signal values
- Periodic status broadcasts
- Transport-agnostic design (works on any serial interface)

---

### Message Buffer Management

Protocol messages use **externally allocated buffers** to enable per-instance MTU configuration from device tree:

```c
/* Protocol message with external buffer */
struct lq_protocol_msg {
    uint32_t address;      /* Transport-specific address */
    uint8_t *data;         /* Points to static buffer */
    size_t len;            /* Actual data length */
    size_t capacity;       /* Buffer size (from DT mtu property) */
    uint64_t timestamp;
    uint32_t flags;
};
```

**Device Tree MTU Configuration**:
```dts
j1939: j1939-protocol {
    compatible = "lq,protocol-j1939";
    mtu = <8>;    /* Standard CAN: 8 bytes */
    /* ... */
};

canfd_proto: j1939-canfd {
    compatible = "lq,protocol-j1939";
    mtu = <64>;   /* CAN-FD: up to 64 bytes */
    /* ... */
};

uart_proto: uart-protocol {
    compatible = "my,uart-protocol";
    mtu = <256>;  /* UART: larger frames */
    /* ... */
};
```

**Benefits**:
- ✅ **Per-instance MTU**: Each protocol instance has optimal buffer size
- ✅ **No dynamic allocation**: Buffers are static arrays generated from device tree
- ✅ **Compile-time safety**: Buffer sizes known at build time
- ✅ **Memory efficiency**: No global maximum, each protocol uses exactly what it needs

**Implementation** (Zephyr code generator creates):
```c
/* Generated from device tree */
static uint8_t j1939_rx_buf[8];      /* mtu=<8> */
static uint8_t canfd_rx_buf[64];     /* mtu=<64> */
static uint8_t uart_rx_buf[256];     /* mtu=<256> */

/* Usage */
struct lq_protocol_msg msg = {
    .data = j1939_rx_buf,
    .capacity = sizeof(j1939_rx_buf),
    /* ... */
};
```

Common MTU values:
- **8 bytes**: CAN 2.0 (J1939, CANopen)
- **64 bytes**: CAN FD
- **256 bytes**: UART/serial protocols
- **512+ bytes**: TCP/IP, large payloads

### Step 1: Define Protocol Context

Create the context structure that holds all protocol state.

**File: `include/lq_uart_config.h`**

```c
/*
 * UART Configuration Protocol
 * Simple ASCII protocol for runtime configuration
 */

#ifndef LQ_UART_CONFIG_H_
#define LQ_UART_CONFIG_H_

#include "lq_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol context - holds all stateful data */
struct lq_uart_config_ctx {
    uint8_t device_id;           /* Device address from config */
    uint32_t update_rate_ms;     /* Status broadcast period */
    uint64_t last_update_time;   /* Last status transmission */
    
    /* Cached TX signals (for status broadcasts) */
    struct {
        uint32_t signal_id;
        int32_t value;
        uint64_t timestamp;
    } signals[32];
    size_t num_signals;
    
    /* RX parsing state */
    char rx_buffer[256];         /* Accumulate partial messages */
    size_t rx_pos;              /* Current position in buffer */
};

/**
 * @brief UART Config Protocol Driver vtable
 * Export for Zephyr/other integrations
 */
extern const struct lq_protocol_vtbl lq_uart_config_vtbl;

/**
 * @brief Create UART config protocol driver
 * 
 * @param proto Protocol driver to initialize
 * @param config Protocol configuration
 * @return 0 on success, -1 on failure
 */
int lq_uart_config_create(struct lq_protocol_driver *proto,
                           const struct lq_protocol_config *config);

#ifdef __cplusplus
}
#endif

#endif /* LQ_UART_CONFIG_H_ */
```

**Design Notes**:
- Keep context structure private to implementation (only declare in .c file if preferred)
- Store all mutable state here - protocols are stateful
- Use fixed-size arrays to avoid malloc
- Export vtable and create function for external use

---

### Step 2: Implement Protocol Functions

Implement each function in the vtable.

**File: `src/drivers/lq_uart_config.c`**

```c
#include "lq_uart_config.h"
#include <string.h>
#include <stdio.h>

/* ===== INIT ===== */
static int uart_config_init(struct lq_protocol_driver *proto,
                             const struct lq_protocol_config *config)
{
    struct lq_uart_config_ctx *ctx = proto->ctx;
    
    if (!ctx || !config) {
        return -1;
    }
    
    /* Initialize context from config */
    memset(ctx, 0, sizeof(*ctx));
    ctx->device_id = config->node_address;
    ctx->update_rate_ms = 100;  /* Default 100ms status updates */
    ctx->rx_pos = 0;
    
    /* Initialize signal cache from encode maps */
    ctx->num_signals = 0;
    for (size_t i = 0; i < config->num_encode_maps && i < 32; i++) {
        const struct lq_protocol_encode_map *map = &config->encode_maps[i];
        for (size_t j = 0; j < map->num_signals && ctx->num_signals < 32; j++) {
            ctx->signals[ctx->num_signals].signal_id = map->signal_ids[j];
            ctx->signals[ctx->num_signals].value = 0;
            ctx->num_signals++;
        }
    }
    
    return 0;
}

/* ===== DECODE (RX) ===== */
static size_t uart_config_decode(struct lq_protocol_driver *proto,
                                   uint64_t now,
                                   const struct lq_protocol_msg *msg,
                                   struct lq_event *out_events,
                                   size_t max_events)
{
    struct lq_uart_config_ctx *ctx = proto->ctx;
    
    if (!msg->data || msg->len == 0) {
        return 0;
    }
    
    /* Protocol format: "#CMD:ARG1:ARG2\r\n"
     * Examples:
     *   #GET:1000\r\n          - Request signal 1000 value
     *   #SET:1000:1234\r\n     - Set signal 1000 to 1234
     *   #CFG:RATE:100\r\n      - Set update rate to 100ms
     */
    
    size_t num_events = 0;
    
    /* Accumulate data in RX buffer (handle fragmented messages) */
    for (size_t i = 0; i < msg->len && ctx->rx_pos < sizeof(ctx->rx_buffer) - 1; i++) {
        ctx->rx_buffer[ctx->rx_pos++] = msg->data[i];
        
        /* Check for complete message (ends with \n) */
        if (msg->data[i] == '\n') {
            ctx->rx_buffer[ctx->rx_pos] = '\0';
            
            /* Parse complete command */
            char cmd[16] = {0};
            uint32_t arg1 = 0, arg2 = 0;
            
            int parsed = sscanf(ctx->rx_buffer, "#%[^:]:%u:%u", cmd, &arg1, &arg2);
            
            if (parsed >= 2) {
                if (strcmp(cmd, "SET") == 0 && num_events < max_events) {
                    /* Generate event for SET command */
                    out_events[num_events].source_id = arg1;  /* signal_id */
                    out_events[num_events].value = (int32_t)arg2;
                    out_events[num_events].status = LQ_EVENT_OK;
                    out_events[num_events].timestamp = now;
                    num_events++;
                }
                else if (strcmp(cmd, "CFG") == 0 && strcmp((char*)&arg1, "RATE") == 0) {
                    /* Configuration: update broadcast rate */
                    ctx->update_rate_ms = arg2;
                }
                /* GET command would be handled by sending response in get_cyclic() */
            }
            
            /* Reset buffer for next message */
            ctx->rx_pos = 0;
        }
    }
    
    return num_events;
}

/* ===== GET_CYCLIC (TX periodic) ===== */
static size_t uart_config_get_cyclic(struct lq_protocol_driver *proto,
                                       uint64_t now,
                                       struct lq_protocol_msg *out_msgs,
                                       size_t max_msgs)
{
    struct lq_uart_config_ctx *ctx = proto->ctx;
    
    if (max_msgs == 0) {
        return 0;
    }
    
    /* Check if periodic update is due */
    uint64_t elapsed_ms = (now - ctx->last_update_time) / 1000;
    if (elapsed_ms < ctx->update_rate_ms) {
        return 0;  /* Not time yet */
    }
    
    /* Build status message - uses external buffer (capacity from DT mtu) */
    if (!out_msgs[0].data || out_msgs[0].capacity == 0) {
        return 0;
    }
    
    /* Format: "#STATUS:ID=VALUE,ID=VALUE,...\r\n" */
    int len = snprintf((char *)out_msgs[0].data, out_msgs[0].capacity, 
                       "#STATUS:%u:", ctx->device_id);
    
    /* Append all cached signal values */
    for (size_t i = 0; i < ctx->num_signals && len < (int)out_msgs[0].capacity - 20; i++) {
        len += snprintf((char *)out_msgs[0].data + len, 
                       out_msgs[0].capacity - len,
                       "%u=%d%s",
                       ctx->signals[i].signal_id,
                       ctx->signals[i].value,
                       (i < ctx->num_signals - 1) ? "," : "");
    }
    
    len += snprintf((char *)out_msgs[0].data + len, 
                   out_msgs[0].capacity - len, "\r\n");
    
    /* Fill message structure */
    out_msgs[0].address = 0;  /* Broadcast (transport-specific) */
    out_msgs[0].len = len;
    out_msgs[0].timestamp = now;
    out_msgs[0].flags = 0;
    
    ctx->last_update_time = now;
    
    return 1;  /* One message to transmit */
}

/* ===== UPDATE_SIGNAL (cache values) ===== */
static void uart_config_update_signal(struct lq_protocol_driver *proto,
                                        uint32_t signal_id,
                                        int32_t value,
                                        uint64_t timestamp)
{
    struct lq_uart_config_ctx *ctx = proto->ctx;
    
    /* Find and update existing signal */
    for (size_t i = 0; i < ctx->num_signals; i++) {
        if (ctx->signals[i].signal_id == signal_id) {
            ctx->signals[i].value = value;
            ctx->signals[i].timestamp = timestamp;
            return;
        }
    }
    
    /* Add new signal if space available */
    if (ctx->num_signals < 32) {
        ctx->signals[ctx->num_signals].signal_id = signal_id;
        ctx->signals[ctx->num_signals].value = value;
        ctx->signals[ctx->num_signals].timestamp = timestamp;
        ctx->num_signals++;
    }
}

/* ===== ENCODE (on-demand TX) ===== */
static int uart_config_encode(struct lq_protocol_driver *proto,
                                const struct lq_event *events,
                                size_t num_events,
                                struct lq_protocol_msg *out_msg)
{
    /* Most protocols use get_cyclic() for transmission
     * encode() is for event-driven responses (optional)
     */
    return -1;  /* Not implemented for this protocol */
}

/* ===== VTABLE ===== */
const struct lq_protocol_vtbl lq_uart_config_vtbl = {
    .init = uart_config_init,
    .decode = uart_config_decode,
    .encode = uart_config_encode,
    .get_cyclic = uart_config_get_cyclic,
    .update_signal = uart_config_update_signal,
};

/* ===== PUBLIC API ===== */
int lq_uart_config_create(struct lq_protocol_driver *proto,
                           const struct lq_protocol_config *config)
{
    if (!proto) {
        return -1;
    }
    
    proto->vtbl = &lq_uart_config_vtbl;
    /* ctx is set by caller or Zephyr wrapper */
    
    if (proto->ctx) {
        return proto->vtbl->init(proto, config);
    }
    
    return 0;
}
```

**Key Implementation Points**:

1. **init()**: Initialize context from config, set defaults, cache signal IDs
2. **decode()**: Parse incoming messages, generate events for engine
3. **get_cyclic()**: Called periodically, generate TX messages when due
4. **update_signal()**: Cache signal values from engine for later transmission
5. **encode()**: Optional, for event-driven responses (many protocols don't need it)

**Zero Malloc**: All buffers are fixed-size arrays in context structure.

---

### Step 3: Implement Vtable and Create Function

```c
/* my_protocol.h */
#include "lq_protocol.h"

struct my_protocol_ctx {
    uint8_t device_address;
    
    /* TX state */
    struct {
        uint32_t signal_id;
        int32_t value;
    } tx_signals[16];
    size_t num_tx_signals;
    
    /* RX state */
    uint32_t last_rx_sequence;
    
    /* Protocol-specific state */
    bool handshake_complete;
};
```

### Step 2: Implement Protocol Functions

```c
/* my_protocol.c */
#include "my_protocol.h"

static int my_protocol_init(struct lq_protocol_driver *proto,
                            const struct lq_protocol_config *config)
{
    struct my_protocol_ctx *ctx = proto->ctx;
    
    ctx->device_address = config->node_address;
    ctx->handshake_complete = false;
    
    return 0;
}

static size_t my_protocol_decode(struct lq_protocol_driver *proto,
                                  uint64_t now,
                                  const struct lq_protocol_msg *msg,
                                  struct lq_event *out_events,
                                  size_t max_events)
{
    struct my_protocol_ctx *ctx = proto->ctx;
    size_t num_events = 0;
    
    /* Parse protocol-specific message format */
    uint8_t msg_type = msg->data[0];
    
    switch (msg_type) {
        case 0x01:  /* Sensor data message */
            if (max_events > 0) {
                uint16_t sensor_value = msg->data[1] | (msg->data[2] << 8);
                
                out_events[0].source_id = 1000;  /* Or from config */
                out_events[0].value = sensor_value;
                out_events[0].status = LQ_EVENT_OK;
                out_events[0].timestamp = now;
                num_events = 1;
            }
            break;
            
        case 0x02:  /* Handshake message */
            ctx->handshake_complete = true;
            break;
    }
    
    return num_events;
}

static size_t my_protocol_get_cyclic(struct lq_protocol_driver *proto,
                                      uint64_t now,
                                      struct lq_protocol_msg *out_msgs,
                                      size_t max_msgs)
{
    struct my_protocol_ctx *ctx = proto->ctx;
    
    if (!ctx->handshake_complete || max_msgs == 0) {
        return 0;
    }
    
    /* Build periodic status message */
    out_msgs[0].id = 0x100 + ctx->device_address;
    out_msgs[0].len = 8;
    out_msgs[0].data[0] = 0x10;  /* Status message type */
    
    /* Pack cached signal values */
    for (size_t i = 0; i < ctx->num_tx_signals && i < 3; i++) {
        uint16_t value = (uint16_t)ctx->tx_signals[i].value;
        out_msgs[0].data[1 + i*2] = value & 0xFF;
        out_msgs[0].data[2 + i*2] = (value >> 8) & 0xFF;
    }
    
    return 1;
}

static void my_protocol_update_signal(struct lq_protocol_driver *proto,
                                       uint32_t signal_id,
                                       int32_t value,
                                       uint64_t timestamp)
{
    struct my_protocol_ctx *ctx = proto->ctx;
    
    /* Cache signal for next transmission */
    for (size_t i = 0; i < ctx->num_tx_signals; i++) {
        if (ctx->tx_signals[i].signal_id == signal_id) {
            ctx->tx_signals[i].value = value;
            return;
        }
    }
    
    /* Add new signal */
    if (ctx->num_tx_signals < 16) {
        ctx->tx_signals[ctx->num_tx_signals].signal_id = signal_id;
        ctx->tx_signals[ctx->num_tx_signals].value = value;
        ctx->num_tx_signals++;
    }
}

static int my_protocol_encode(struct lq_protocol_driver *proto,
                               const struct lq_event *events,
                               size_t num_events,
                               struct lq_protocol_msg *out_msg)
{
    /* Most protocols use get_cyclic() instead */
    return -1;
}
```

**Key Implementation Points**:

1. **init()**: Initialize context from config, set defaults, cache signal IDs
2. **decode()**: Parse incoming messages, generate events for engine
3. **get_cyclic()**: Called periodically, generate TX messages when due
4. **update_signal()**: Cache signal values from engine for later transmission
5. **encode()**: Optional, for event-driven responses (many protocols don't need it)

**Zero Malloc**: All buffers are fixed-size arrays in context structure.

---

### Step 3: Platform-Independent Usage

For non-Zephyr platforms, use the protocol directly:

```c
#include "lq_uart_config.h"

/* Allocate context and driver */
static struct lq_uart_config_ctx uart_ctx;
static struct lq_protocol_driver uart_proto;

/* Configure protocol */
struct lq_protocol_config config = {
    .node_address = 42,  /* Device ID */
    .num_decode_maps = 0,
    .num_encode_maps = 0,
};

/* Create protocol instance */
uart_proto.ctx = &uart_ctx;
lq_uart_config_create(&uart_proto, &config);

/* Use in RX path */
struct lq_protocol_msg rx_msg = {
    .data = uart_rx_buffer,
    .capacity = sizeof(uart_rx_buffer),
    .len = bytes_received,
    .address = 0,
};

struct lq_event events[10];
size_t num_events = lq_protocol_decode(&uart_proto, now_us, &rx_msg, 
                                        events, 10);

/* Use in TX path */
struct lq_protocol_msg tx_msgs[4];
tx_msgs[0].data = uart_tx_buffer;
tx_msgs[0].capacity = sizeof(uart_tx_buffer);

size_t num_msgs = lq_protocol_get_cyclic(&uart_proto, now_us, tx_msgs, 4);
for (size_t i = 0; i < num_msgs; i++) {
    uart_transmit(tx_msgs[i].data, tx_msgs[i].len);
}
```

---

### Step 4: Zephyr Device Tree Binding (Optional)

For Zephyr integration, create a device tree binding.

**File: `dts/bindings/layered-queue/lq,protocol-uart-config.yaml`**

```yaml
# Copyright (c) 2026 Layered Queue Driver
# SPDX-License-Identifier: Apache-2.0

description: |
  UART Configuration Protocol Driver
  
  Simple ASCII protocol for runtime configuration and monitoring.
  Supports SET/GET commands and periodic status broadcasts.

compatible: "lq,protocol-uart-config"

properties:
  device-id:
    type: int
    required: true
    description: |
      Device identifier (0-255).
      Included in status broadcasts.

  mtu:
    type: int
    default: 256
    description: |
      Maximum Transmission Unit in bytes.
      UART typically uses larger frames (256-512 bytes).

  update-rate-ms:
    type: int
    default: 100
    description: |
      Default status broadcast period in milliseconds.
      Can be changed at runtime via CFG command.

child-binding:
  description: Signal mappings for status broadcasts

  properties:
    signal-ids:
      type: array
      required: true
      description: |
        Array of signal IDs to include in status broadcasts.
```

---

### Step 5: Zephyr Device Tree Wrapper (Optional)

Create Zephyr wrapper that instantiates protocol from device tree.

**File: `drivers/layered_queue/lq_protocol_uart_config.c`**

```c
/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Tree Wrapper for UART Config Protocol
 */

#define DT_DRV_COMPAT lq_protocol_uart_config

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "lq_uart_config.h"

LOG_MODULE_REGISTER(lq_protocol_uart_config, CONFIG_LQ_LOG_LEVEL);

/* Device data */
struct lq_uart_config_dt_data {
    struct lq_uart_config_ctx ctx;
    uint8_t *rx_buffer;
    uint8_t *tx_buffer;
};

/* Device config */
struct lq_uart_config_dt_config {
    struct lq_protocol_driver driver;
    struct lq_protocol_config protocol_config;
    size_t mtu;
    uint32_t update_rate_ms;
};

static const struct device_api lq_uart_config_api = {
    /* Protocol accessed via proto->vtbl, not device API */
};

static int lq_uart_config_dt_init(const struct device *dev)
{
    struct lq_uart_config_dt_data *data = dev->data;
    const struct lq_uart_config_dt_config *config = dev->config;
    
    /* Wire up context */
    struct lq_protocol_driver *proto = 
        (struct lq_protocol_driver *)&config->driver;
    proto->ctx = &data->ctx;
    
    /* Initialize */
    int ret = lq_protocol_init(proto, &config->protocol_config);
    if (ret != 0) {
        LOG_ERR("UART config protocol init failed: %d", ret);
        return ret;
    }
    
    /* Apply DT configuration */
    data->ctx.update_rate_ms = config->update_rate_ms;
    
    LOG_INF("UART config protocol initialized: device_id=%u, mtu=%zu",
            config->protocol_config.node_address, config->mtu);
    
    return 0;
}

/* Macro to generate signal array from child nodes */
#define UART_CONFIG_SIGNAL_IDS_INIT(child) \
    DT_FOREACH_PROP_ELEM_SEP(child, signal_ids, DT_PROP_BY_IDX, (,))

/* Device instantiation macro */
#define LQ_PROTOCOL_UART_CONFIG_DEFINE(inst) \
    /* Static RX/TX buffers sized from DT mtu */ \
    static uint8_t lq_uart_config_rx_buf_##inst[DT_INST_PROP(inst, mtu)]; \
    static uint8_t lq_uart_config_tx_buf_##inst[DT_INST_PROP(inst, mtu)]; \
    \
    /* Encode map for status broadcasts */ \
    static const uint32_t lq_uart_config_signal_ids_##inst[] = { \
        DT_INST_FOREACH_CHILD_STATUS_OKAY_VARGS(inst, \
            UART_CONFIG_SIGNAL_IDS_INIT) \
    }; \
    \
    static const struct lq_protocol_encode_map \
        lq_uart_config_encode_map_##inst = { \
        .protocol_id = 0, /* Not used for UART */ \
        .signal_ids = (uint32_t *)lq_uart_config_signal_ids_##inst, \
        .num_signals = ARRAY_SIZE(lq_uart_config_signal_ids_##inst), \
        .period_ms = DT_INST_PROP(inst, update_rate_ms), \
        .on_change = false, \
    }; \
    \
    /* Device data */ \
    static struct lq_uart_config_dt_data lq_uart_config_data_##inst = { \
        .rx_buffer = lq_uart_config_rx_buf_##inst, \
        .tx_buffer = lq_uart_config_tx_buf_##inst, \
    }; \
    \
    /* Device config */ \
    static const struct lq_uart_config_dt_config \
        lq_uart_config_config_##inst = { \
        .driver = { \
            .vtbl = &lq_uart_config_vtbl, \
            .ctx = NULL, /* Set during init */ \
        }, \
        .protocol_config = { \
            .node_address = DT_INST_PROP(inst, device_id), \
            .decode_maps = NULL, \
            .num_decode_maps = 0, \
            .encode_maps = &lq_uart_config_encode_map_##inst, \
            .num_encode_maps = 1, \
        }, \
        .mtu = DT_INST_PROP(inst, mtu), \
        .update_rate_ms = DT_INST_PROP_OR(inst, update_rate_ms, 100), \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_uart_config_dt_init, \
                          NULL, \
                          &lq_uart_config_data_##inst, \
                          &lq_uart_config_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_LQ_INIT_PRIORITY, \
                          &lq_uart_config_api);

/* Instantiate all instances from device tree */
DT_INST_FOREACH_STATUS_OKAY(LQ_PROTOCOL_UART_CONFIG_DEFINE)
```

---

### Step 6: Zephyr Build Integration (Optional)

Add Kconfig and CMakeLists entries.

**File: `drivers/layered_queue/Kconfig`** (append):

```kconfig
config LQ_PROTOCOL_UART_CONFIG
    bool "UART Configuration Protocol Driver"
    help
      Enable UART configuration protocol for ASCII command/response.
      Supports SET/GET commands and periodic status broadcasts.
```

**File: `drivers/layered_queue/CMakeLists.txt`** (append):

```cmake
# Protocol drivers (platform-independent)
zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_UART_CONFIG 
                              ../../src/drivers/lq_uart_config.c)

# Zephyr device tree wrappers
zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_UART_CONFIG 
                              lq_protocol_uart_config.c)
```

---

### Step 7: Device Tree Usage Example

**File: `my_device.dts`**

```dts
/ {
    model = "UART Config Example";
    
    signals {
        temperature: signal@1000 {
            signal-id = <1000>;
        };
        pressure: signal@1001 {
            signal-id = <1001>;
        };
        flow_rate: signal@1002 {
            signal-id = <1002>;
        };
    };
    
    protocols {
        uart_config: uart-config-protocol {
            compatible = "lq,protocol-uart-config";
            device-id = <42>;
            mtu = <256>;
            update-rate-ms = <1000>;  /* 1Hz status */
            
            /* Signals to broadcast in status */
            status-broadcast {
                signal-ids = <&temperature &pressure &flow_rate>;
            };
        };
    };
    
    &uart0 {
        status = "okay";
        protocol = <&uart_config>;
    };
};
```

**Generated buffers**:
```c
static uint8_t lq_uart_config_rx_buf_0[256];  /* mtu=<256> */
static uint8_t lq_uart_config_tx_buf_0[256];
```

---

## Complete Implementation Checklist

### Core Protocol (Platform-Independent)

- [ ] **Header file** (`lq_my_protocol.h`)
  - [ ] Include `lq_protocol.h`
  - [ ] Define context structure
  - [ ] Export vtable: `extern const struct lq_protocol_vtbl lq_my_protocol_vtbl;`
  - [ ] Declare create function
  
- [ ] **Implementation file** (`lq_my_protocol.c`)
  - [ ] Implement `init()` - initialize context from config
  - [ ] Implement `decode()` - parse RX messages → generate events
  - [ ] Implement `get_cyclic()` - periodic TX message generation
  - [ ] Implement `update_signal()` - cache signal values for TX
  - [ ] Implement `encode()` - optional, on-demand TX
  - [ ] Define vtable with all function pointers
  - [ ] Implement create function

- [ ] **Testing**
  - [ ] Unit test decode() with sample messages
  - [ ] Unit test get_cyclic() timing
  - [ ] Verify signal caching in update_signal()

### Zephyr Integration (Optional)

- [ ] **Device tree binding** (`lq,protocol-my-protocol.yaml`)
  - [ ] Define `compatible` string
  - [ ] Add `mtu` property (default appropriate for transport)
  - [ ] Add protocol-specific properties
  - [ ] Define child-binding for signal mappings
  
- [ ] **Zephyr wrapper** (`lq_protocol_my_protocol.c`)
  - [ ] Define `#define DT_DRV_COMPAT lq_protocol_my_protocol`
  - [ ] Create static buffers: `uint8_t rx_buf[DT_INST_PROP(inst, mtu)]`
  - [ ] Define device data and config structures
  - [ ] Implement init function that calls protocol vtable->init()
  - [ ] Create instantiation macro with `DT_INST_FOREACH_STATUS_OKAY`
  
- [ ] **Build integration**
  - [ ] Add `CONFIG_LQ_PROTOCOL_MY_PROTOCOL` to Kconfig
  - [ ] Add core .c to CMakeLists with `zephyr_library_sources_ifdef`
  - [ ] Add wrapper .c to CMakeLists with `zephyr_library_sources_ifdef`

### Documentation

- [ ] Add protocol description to this document
- [ ] Create example device tree configuration
- [ ] Document message format and protocol behavior
- [ ] Add to protocol comparison table

---

## Protocol Comparison

| Protocol | Transport | MTU | Cyclic TX | State Machine | Complexity |
|----------|-----------|-----|-----------|---------------|------------|
| **J1939** | CAN 2.0 | 8 | Yes (PGNs) | No | Medium |
| **CANopen** | CAN 2.0/FD | 8-64 | Yes (PDOs) | Yes (NMT) | High |
| **UART Config** | UART/Serial | 256+ | Yes (Status) | No | Low |
| **Your Protocol** | Any | Custom | Custom | Custom | ? |

---

## Advanced Topics

### Handling Multi-Frame Messages

For protocols that span multiple frames (e.g., J1939 transport protocol):

```c
struct my_ctx {
    /* Multi-frame reassembly */
    struct {
        uint8_t buffer[1785];  /* Max J1939 TP size */
        size_t received;
        size_t total;
        uint32_t sequence;
        bool in_progress;
    } multiframe;
};

static size_t decode(...)  {
    /* Detect multi-frame start */
    if (is_tp_cm(msg)) {
        ctx->multiframe.total = extract_size(msg);
        ctx->multiframe.received = 0;
        ctx->multiframe.in_progress = true;
        return 0;  /* No events yet */
    }
    
    /* Accumulate data frames */
    if (ctx->multiframe.in_progress && is_tp_dt(msg)) {
        memcpy(&ctx->multiframe.buffer[ctx->multiframe.received],
               msg->data, msg->len);
        ctx->multiframe.received += msg->len;
        
        /* Complete? */
        if (ctx->multiframe.received >= ctx->multiframe.total) {
            /* Parse complete message */
            return parse_multiframe(&ctx->multiframe, out_events, max_events);
        }
    }
    
    return 0;
}
```

### Request/Response Handling

For protocols with query/response patterns:

```c
static size_t decode(...) {
    if (is_request(msg)) {
        /* Store request for response in get_cyclic() */
        ctx->pending_request.signal_id = extract_signal_id(msg);
        ctx->pending_request.requester = msg->address;
        ctx->pending_request.pending = true;
    }
    return 0;
}

static size_t get_cyclic(...) {
    if (ctx->pending_request.pending) {
        /* Build response */
        build_response(out_msgs, ctx->pending_request.signal_id);
        ctx->pending_request.pending = false;
        return 1;
    }
    return 0;
}
```

### Error Handling and Diagnostics

```c
struct my_ctx {
    struct {
        uint32_t rx_errors;
        uint32_t tx_errors;
        uint32_t parse_errors;
        uint32_t timeout_count;
    } diagnostics;
};

static size_t decode(...) {
    if (!validate_checksum(msg)) {
        ctx->diagnostics.rx_errors++;
        /* Optionally generate error event */
        out_events[0].status = LQ_EVENT_INVALID;
        return 1;
    }
    /* ... */
}
```

---

## Best Practices Summary

### ✅ DO

1. **Use external buffers** - MTU from device tree, no malloc
2. **Cache signal values** - Store in update_signal() for get_cyclic()
3. **Handle partial messages** - UART/serial may fragment
4. **Validate all inputs** - Check msg->data, msg->len, capacity
5. **Use get_cyclic() for TX** - Most protocols transmit periodically
6. **Keep context structure clean** - Only mutable state
7. **Export vtable** - Allow external instantiation
8. **Document message format** - Critical for integration

### ❌ DON'T

1. **Don't malloc** - Use fixed buffers in context
2. **Don't assume MTU** - Use msg->capacity, not hardcoded size
3. **Don't share context** - One context per protocol instance
4. **Don't block** - All functions must be non-blocking
5. **Don't forget init()** - Initialize all context fields
6. **Don't ignore errors** - Return -1, set error status
7. **Don't hardcode signal IDs** - Use config->decode_maps/encode_maps
8. **Don't use static locals** - State goes in context structure

---

## See Also

- [J1939 Implementation](../src/drivers/lq_j1939.c) - Production example
- [CANopen Implementation](../src/drivers/lq_canopen.c) - Complex state machine
- [Protocol Interface](../include/lq_protocol.h) - Core types and vtable
- [Zephyr J1939 Wrapper](../drivers/layered_queue/lq_protocol_j1939.c) - DT integration pattern

```c
/* Validate this struct exists in your protocol */
const struct lq_protocol_vtbl my_protocol_vtbl = {
    .init = my_init,
    .decode = my_decode,
    .encode = my_encode,
    .get_cyclic = my_get_cyclic,
    .update_signal = my_update_signal,
};
```

Now you have a complete, production-ready protocol driver! 🚀

```c
const struct lq_protocol_vtbl my_protocol_vtbl = {
    .init = my_protocol_init,
    .decode = my_protocol_decode,
    .encode = my_protocol_encode,
    .get_cyclic = my_protocol_get_cyclic,
    .update_signal = my_protocol_update_signal,
};

int my_protocol_create(struct lq_protocol_driver *proto,
                       const struct lq_protocol_config *config)
{
    struct my_protocol_ctx *ctx = malloc(sizeof(*ctx));
    if (!ctx) return -1;
    
    proto->vtbl = &my_protocol_vtbl;
    proto->ctx = ctx;
    
    return proto->vtbl->init(proto, config);
}
```

### Step 4: Use Custom Protocol

```c
#include "my_protocol.h"

struct lq_protocol_driver my_proto;
struct lq_protocol_config config = {
    .node_address = 42,
    /* ... */
};

my_protocol_create(&my_proto, &config);

/* Integrate with engine */
// Protocol driver automatically called during engine processing
```

## Device Tree Integration

Protocol drivers link hardware interfaces to signal mappings:

```dts
/ {
    /* Define signals */
    signals {
        engine_rpm: signal@1000 {
            signal-id = <1000>;
        };
        engine_temp: signal@1001 {
            signal-id = <1001>;
        };
    };
    
    /* Define protocol */
    protocols {
        my_j1939: j1939@0 {
            compatible = "lq,protocol-j1939";
            node-address = <0x25>;
            
            eec1 {
                pgn = <65265>;
                direction = "both";
                signal-ids = <&engine_rpm>;
                period-ms = <50>;
            };
        };
    };
    
    /* Link to hardware */
    &can0 {
        status = "okay";
        protocol = <&my_j1939>;
    };
};
```

## Best Practices

### 1. Separate RX and TX Mappings

Even though they share state, define clear RX vs TX message configurations:

```dts
/* Good: Clear separation */
rx-speed {
    direction = "rx";
    /* ... */
}
tx-speed {
    direction = "tx";
    /* ... */
}

/* Avoid: Ambiguous "both" unless truly bidirectional */
```

### 2. Cache Signal Values

Protocol drivers should cache signal values for encoding:

```c
static void update_signal(struct lq_protocol_driver *proto,
                          uint32_t signal_id, int32_t value, ...)
{
    /* ALWAYS cache for later encoding */
    ctx->signals[idx].value = value;
}
```

### 3. Use get_cyclic() for TX

Most protocols should transmit via `get_cyclic()` rather than `encode()`:

```c
/* get_cyclic() called periodically by engine */
static size_t get_cyclic(struct lq_protocol_driver *proto, ...)
{
    /* Check timing, build messages, return them */
}
```

### 4. Handle State Machines

Protocols with states (CANopen NMT, handshakes) manage them in context:

```c
struct my_ctx {
    enum protocol_state {
        STATE_INIT,
        STATE_HANDSHAKE,
        STATE_OPERATIONAL
    } state;
};

/* Update state based on RX/TX */
```

## Protocol Driver Checklist

- [ ] Define protocol context structure
- [ ] Implement `init()` to setup initial state
- [ ] Implement `decode()` to parse RX messages
- [ ] Implement `update_signal()` to cache TX values
- [ ] Implement `get_cyclic()` for periodic transmission
- [ ] Implement `encode()` if needed for event-driven TX
- [ ] Create vtable and constructor function
- [ ] Document protocol-specific message formats
- [ ] Create devicetree bindings (YAML)
- [ ] Add example configurations

## Advanced Topics

### Multi-Master Protocols

For protocols with address arbitration:

```c
struct ctx {
    uint8_t claimed_address;
    bool address_claim_pending;
};

/* Handle address claim in decode() */
```

### Transport Protocols

For multi-frame messages (J1939 TP, ISO-TP):

```c
struct ctx {
    struct {
        uint8_t frames[256];
        size_t total_len;
        size_t received;
    } rx_session;
};

/* Reassemble in decode(), segment in encode() */
```

### Request/Response

Coordinate request/response pairs:

```c
struct ctx {
    struct {
        uint32_t request_id;
        uint64_t request_time;
        bool pending;
    } rr_state;
};

/* Track requests in encode(), match in decode() */
```

## Example: UART Configuration Protocol

Here's a complete example of a custom UART-based configuration protocol:

### Protocol Specification

```
Message Format (ASCII):
  #CMD:ADDR:DATA\r\n
  
Commands:
  #GET:1000\r\n          -> Read signal 1000
  #SET:1000:1234\r\n     -> Write signal 1000 = 1234
  #CFG:RATE:50\r\n       -> Configure update rate (50ms)
```

### Implementation

```c
/* uart_config_protocol.h */
#include "lq_protocol.h"

#define UART_MSG_MAX_LEN 128

struct uart_config_ctx {
    uint8_t device_id;
    uint32_t update_rate_ms;
    
    /* RX parsing state */
    uint8_t rx_buffer[UART_MSG_MAX_LEN];
    size_t rx_pos;
    
    /* TX state */
    uint64_t last_update_time;
    
    /* Cached signals for periodic transmission */
    struct {
        uint32_t signal_id;
        int32_t value;
        uint64_t timestamp;
    } signals[32];
    size_t num_signals;
};

/* uart_config_protocol.c */
static int uart_config_init(struct lq_protocol_driver *proto,
                            const struct lq_protocol_config *config)
{
    struct uart_config_ctx *ctx = proto->ctx;
    
    ctx->device_id = config->node_address;
    ctx->update_rate_ms = 100;  /* Default 100ms */
    ctx->rx_pos = 0;
    
    return 0;
}

static size_t uart_config_decode(struct lq_protocol_driver *proto,
                                  uint64_t now,
                                  const struct lq_protocol_msg *msg,
                                  struct lq_event *out_events,
                                  size_t max_events)
{
    struct uart_config_ctx *ctx = proto->ctx;
    
    if (!msg->data || msg->len == 0) {
        return 0;
    }
    
    /* Parse ASCII command: #GET:1000\r\n or #SET:1000:1234\r\n */
    char cmd[16] = {0};
    uint32_t signal_id = 0;
    int32_t value = 0;
    
    /* Simple ASCII parsing */
    if (sscanf((char *)msg->data, "#%[^:]:%u:%d", cmd, &signal_id, &value) >= 2) {
        
        if (strcmp(cmd, "SET") == 0 && max_events > 0) {
            /* Generate event for SET command */
            out_events[0].source_id = signal_id;
            out_events[0].value = value;
            out_events[0].status = LQ_EVENT_OK;
            out_events[0].timestamp = now;
            return 1;
        }
        
        if (strcmp(cmd, "CFG") == 0) {
            /* Configuration command */
            if (signal_id == 0) {  /* Update rate config */
                ctx->update_rate_ms = value;
            }
        }
    }
    
    return 0;
}

static size_t uart_config_get_cyclic(struct lq_protocol_driver *proto,
                                      uint64_t now,
                                      struct lq_protocol_msg *out_msgs,
                                      size_t max_msgs)
{
    struct uart_config_ctx *ctx = proto->ctx;
    
    if (max_msgs == 0) {
        return 0;
    }
    
    /* Check if update is due */
    uint64_t elapsed_ms = (now - ctx->last_update_time) / 1000;
    if (elapsed_ms < ctx->update_rate_ms) {
        return 0;
    }
    
    /* Build status message - uses external buffer (capacity from DT mtu) */
    if (!out_msgs[0].data || out_msgs[0].capacity == 0) {
        return 0;
    }
    
    int len = snprintf((char *)out_msgs[0].data, out_msgs[0].capacity, "#STATUS:");
    
    /* Add all cached signal values */
    for (size_t i = 0; i < ctx->num_signals && len < (int)out_msgs[0].capacity - 20; i++) {
        len += snprintf((char *)out_msgs[0].data + len, out_msgs[0].capacity - len, 
                       "%u=%d,", 
                       ctx->signals[i].signal_id,
                       ctx->signals[i].value);
    }
    
    len += snprintf((char *)out_msgs[0].data + len, out_msgs[0].capacity - len, "\r\n");
    
    out_msgs[0].address = 0;  /* Broadcast on UART */
    out_msgs[0].len = len;
    out_msgs[0].timestamp = now;
    
    ctx->last_update_time = now;
    
    return 1;
}

static void uart_config_update_signal(struct lq_protocol_driver *proto,
                                       uint32_t signal_id,
                                       int32_t value,
                                       uint64_t timestamp)
{
    struct uart_config_ctx *ctx = proto->ctx;
    
    /* Update or add signal */
    for (size_t i = 0; i < ctx->num_signals; i++) {
        if (ctx->signals[i].signal_id == signal_id) {
            ctx->signals[i].value = value;
            ctx->signals[i].timestamp = timestamp;
            return;
        }
    }
    
    if (ctx->num_signals < 32) {
        ctx->signals[ctx->num_signals].signal_id = signal_id;
        ctx->signals[ctx->num_signals].value = value;
        ctx->signals[ctx->num_signals].timestamp = timestamp;
        ctx->num_signals++;
    }
}

const struct lq_protocol_vtbl uart_config_vtbl = {
    .init = uart_config_init,
    .decode = uart_config_decode,
    .encode = NULL,  /* Use get_cyclic instead */
    .get_cyclic = uart_config_get_cyclic,
    .update_signal = uart_config_update_signal,
};

/* Create UART config protocol */
int uart_config_protocol_create(struct lq_protocol_driver *proto,
                                 const struct lq_protocol_config *config)
{
    struct uart_config_ctx *ctx = malloc(sizeof(*ctx));
    if (!ctx) return -1;
    
    proto->vtbl = &uart_config_vtbl;
    proto->ctx = ctx;
    
    return proto->vtbl->init(proto, config);
}
```

### Device Tree Configuration

```dts
uart_config: uart-config-protocol {
    compatible = "custom,uart-config";
    node-address = <1>;  /* Device ID */
    
    /* Signals to expose over UART */
    monitored-signals {
        signal-ids = <&motor_speed &motor_temp &battery_voltage>;
    };
    
    /* Update rate configuration */
    update-rate-ms = <100>;
};

&uart0 {
    status = "okay";
    baudrate = <115200>;
    protocol = <&uart_config>;
};
```

### Usage

```bash
# Read motor speed
echo "#GET:2000" > /dev/ttyUSB0

# Set motor command
echo "#SET:2010:500" > /dev/ttyUSB0

# Configure update rate to 50ms
echo "#CFG:RATE:50" > /dev/ttyUSB0

# Receive periodic status
cat /dev/ttyUSB0
#STATUS:2000=3000,2001=75,2002=48,
```

## See Also

- [J1939 Guide](can-j1939-guide.md) - Detailed J1939 usage
- [Architecture](architecture.md) - Overall system architecture
- [Mid-Level Drivers](platform-adaptors.md) - Related driver patterns
