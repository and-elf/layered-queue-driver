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

Users can implement custom protocols by providing the `lq_protocol_vtbl` interface.

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

### Step 3: Define Vtable

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
