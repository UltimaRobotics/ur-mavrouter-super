# Message Routing System

## Overview

The MAVLink Router implements a sophisticated message routing system that intelligently forwards MAVLink messages between multiple communication endpoints based on system ID, component ID, and configurable filtering rules.

## Message Structure

### MAVLink Protocol Support
The router supports both MAVLink v1.0 and v2.0 protocols:

#### MAVLink v2.0 Header
```cpp
struct mavlink_router_mavlink2_header {
    uint8_t magic;              // 0xFD
    uint8_t payload_len;        // Payload length
    uint8_t incompat_flags;     // Incompatibility flags
    uint8_t compat_flags;       // Compatibility flags
    uint8_t seq;                // Sequence number
    uint8_t sysid;              // System ID
    uint8_t compid;             // Component ID
    uint32_t msgid : 24;        // Message ID (24-bit)
};
```

#### MAVLink v1.0 Header
```cpp
struct mavlink_router_mavlink1_header {
    uint8_t magic;              // 0xFE
    uint8_t payload_len;        // Payload length
    uint8_t seq;                // Sequence number
    uint8_t sysid;              // System ID
    uint8_t compid;             // Component ID
    uint8_t msgid;              // Message ID (8-bit)
};
```

## Routing Algorithm

### 1. Message Reception and Parsing

#### Buffer Structure
```cpp
struct buffer {
    unsigned int len;
    uint8_t *data;
    
    struct {
        uint32_t msg_id;
        int target_sysid;
        int target_compid;
        uint8_t src_sysid;
        uint8_t src_compid;
        uint8_t payload_len;
        uint8_t *payload;
    } curr;
};
```

### 2. Routing Decision Process

```cpp
void Mainloop::route_msg(struct buffer *buf)
{
    bool unknown = true;
    
    for (const auto &e : this->g_endpoints) {
        auto acceptState = e->accept_msg(buf);
        
        switch (acceptState) {
        case Endpoint::AcceptState::Accepted:
            if (write_msg(e, buf) == -EPIPE) {
                should_process_tcp_hangups = true;
            }
            unknown = false;
            break;
        case Endpoint::AcceptState::Filtered:
            unknown = false;
            break;
        case Endpoint::AcceptState::Rejected:
            break;
        }
    }
    
    if (unknown) {
        _errors_aggregate.msg_to_unknown++;
    }
}
```

### 3. Accept States

#### Message Accept Logic
Each endpoint evaluates incoming messages and returns one of three states:

- **Accepted**: Message will be forwarded to this endpoint
- **Filtered**: Message was recognized but filtered out by rules
- **Rejected**: Message is not intended for this endpoint

## Filtering System

### Filter Types

#### 1. **Incoming Message Filters**
Applied to messages received FROM an endpoint:
- `AllowMsgIdIn` / `BlockMsgIdIn`: Filter by message ID
- `AllowSrcCompIn` / `BlockSrcCompIn`: Filter by source component ID
- `AllowSrcSysIn` / `BlockSrcSysIn`: Filter by source system ID

#### 2. **Outgoing Message Filters**  
Applied to messages being sent TO an endpoint:
- `AllowMsgIdOut` / `BlockMsgIdOut`: Filter by message ID
- `AllowSrcCompOut` / `BlockSrcCompOut`: Filter by source component ID
- `AllowSrcSysOut` / `BlockSrcSysOut`: Filter by source system ID

### Filter Implementation

```cpp
void filter_add_allowed_out_msg_id(uint32_t msg_id);
void filter_add_blocked_out_msg_id(uint32_t msg_id);
void filter_add_allowed_out_src_comp(uint8_t src_comp);
void filter_add_blocked_out_src_comp(uint8_t src_comp);
// ... and corresponding incoming filters
```

### Filter Precedence
1. **Allow lists**: If present, only listed items are allowed
2. **Block lists**: If present, listed items are blocked
3. **Default behavior**: Allow all if no filters configured

## Special Routing Modes

### 1. **Sniffer Mode**
```cpp
static uint16_t sniffer_sysid;
```
- An endpoint with the configured sniffer system ID receives ALL messages
- Useful for monitoring and debugging
- Configured via `--sniffer-sysid` command line option

### 2. **Endpoint Groups**
```cpp
std::string _group_name;
std::vector<std::shared_ptr<Endpoint>> _group_members;
```
- Endpoints can be grouped together
- Messages received by one group member are shared with all group members
- Useful for redundant connections

## De-duplication System

### Hash-based De-duplication
```cpp
class Dedup {
public:
    enum class PacketStatus { 
        NEW_PACKET_OR_TIMED_OUT, 
        ALREADY_EXISTS_IN_BUFFER 
    };
    
    PacketStatus check_packet(const uint8_t *buffer, uint32_t size);
};
```

#### De-duplication Process
1. **Hash Calculation**: Calculate hash of raw message data
2. **Time Window Check**: Check if hash was seen within configured period
3. **Status Return**: Return NEW_PACKET_OR_TIMED_OUT or ALREADY_EXISTS_IN_BUFFER
4. **Cleanup**: Automatically remove old hash entries

#### Configuration
- **DeduplicationPeriod**: Time window in milliseconds
- **Default**: Disabled (0ms)
- **Purpose**: Prevent message loops in complex topologies

## System ID and Component ID Tracking

### Endpoint Learning
```cpp
std::vector<uint16_t> _sys_comp_ids;
```

#### Learning Process
- Endpoints track system/component ID combinations they've seen
- Used for intelligent routing decisions
- Helps determine which endpoints can reach which systems

#### Helper Functions
```cpp
bool has_sys_id(unsigned sysid) const;
bool has_sys_comp_id(unsigned sys_comp_id) const;
bool has_sys_comp_id(unsigned sysid, unsigned compid) const;
```

## Message Statistics and Monitoring

### Per-Endpoint Statistics
```cpp
struct {
    struct {
        uint64_t crc_error_bytes = 0;
        uint64_t handled_bytes = 0;
        uint32_t total = 0;
        uint32_t crc_error = 0;
        uint32_t handled = 0;
        uint32_t drop_seq_total = 0;
        uint8_t expected_seq = 0;
    } read;
    struct {
        uint64_t bytes = 0;
        uint32_t total = 0;
    } write;
} _stat;
```

### Global Statistics
```cpp
struct {
    uint32_t msg_to_unknown = 0;
} _errors_aggregate;
```

## Endpoint-Specific Routing

### UART Endpoints
- **Autopilot Communication**: Typically connected to flight controller
- **Baud Rate Detection**: Automatic detection of communication speed
- **Flow Control**: Optional RTS/CTS flow control support

### UDP Endpoints
#### Client Mode
- Sends messages to configured address/port
- Used for ground station connections

#### Server Mode  
- Listens on configured port
- Accepts messages from any source
- Used for accepting connections from multiple ground stations

### TCP Endpoints
- **Client Mode**: Connects to remote TCP server
- **Server Mode**: Accepts incoming TCP connections (handled by mainloop)
- **Reconnection**: Automatic reconnection with configurable timeout
- **Connection Management**: Graceful handling of connection failures

### Log Endpoints
- **Transparent Routing**: Don't participate in normal message routing
- **Message Recording**: Record all messages for later analysis
- **Format Support**: Multiple log formats (BinLog, ULog, TLog)

## Error Handling and Recovery

### Message Parsing Errors
- **CRC Failures**: Messages with invalid CRC are dropped
- **Format Errors**: Malformed messages are logged and skipped
- **Sequence Gaps**: Sequence number gaps are detected and logged

### Endpoint Failures
- **TCP Disconnections**: Automatic reconnection attempts
- **UART Errors**: Baud rate retry mechanism
- **UDP Errors**: Generally non-fatal, operation continues

### Flow Control
- **Backpressure**: When an endpoint can't accept messages
- **EPOLLOUT**: Used to detect when endpoint can accept more data
- **Non-blocking I/O**: Prevents blocking the entire routing system

## Performance Characteristics

### Latency
- **Minimal Processing**: Direct forwarding without unnecessary processing
- **Zero-copy**: Message data is not copied between endpoints
- **Event-driven**: No polling, immediate response to new messages

### Throughput
- **Asynchronous I/O**: Multiple endpoints can send/receive simultaneously
- **Efficient Filtering**: Fast filter evaluation
- **Batched Processing**: Multiple messages processed per event loop iteration

### Memory Usage
- **Fixed Buffers**: Predictable memory usage
- **Shared Endpoints**: Smart pointer-based memory management
- **Hash Tables**: Efficient de-duplication storage