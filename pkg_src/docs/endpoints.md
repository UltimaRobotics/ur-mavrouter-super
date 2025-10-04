# Endpoints System Documentation

## Overview

The endpoints system provides a polymorphic interface for different communication types. All endpoints inherit from the base `Endpoint` class, which provides common functionality for message handling, filtering, and statistics.

## Base Endpoint Class (`endpoint.h`, `endpoint.cpp`)

### Class Hierarchy
```
Pollable (pollable.h)
    ↓
Endpoint (endpoint.h)
    ├── UartEndpoint
    ├── UdpEndpoint  
    ├── TcpEndpoint
    └── LogEndpoint
            ├── AutoLog
            ├── BinLog
            ├── ULog
            └── TLog
```

### Core Endpoint Interface

#### Virtual Methods
```cpp
virtual int write_msg(const struct buffer *pbuf) = 0;
virtual int flush_pending_msgs() = 0;
virtual ssize_t _read_msg(uint8_t *buf, size_t len) = 0;
```

#### Key Properties
- **Type**: String identifier (UART, UDP, TCP, Log)
- **Name**: User-configurable endpoint name
- **File Descriptor**: Linux file descriptor for I/O
- **Buffers**: Separate RX and TX buffers
- **Statistics**: Comprehensive I/O and error statistics

## UART Endpoint (`endpoint.cpp`)

### Purpose
Communication with autopilot systems over serial interfaces.

### Configuration
```cpp
struct UartEndpointConfig {
    std::string name;
    std::string device;                    // e.g., "/dev/ttyUSB0"
    std::vector<uint32_t> baudrates;      // e.g., {57600, 115200}
    bool flowcontrol{false};              // RTS/CTS flow control
    
    // Filtering configuration
    std::vector<uint32_t> allow_msg_id_out;
    std::vector<uint32_t> block_msg_id_out;
    // ... (complete filtering options)
    
    std::string group;                    // Endpoint group name
};
```

### Key Features

#### Automatic Baud Rate Detection
```cpp
bool _change_baud_cb(void *data);
```
- **Multi-rate Support**: Try multiple baud rates automatically
- **Timeout-based**: Switch baud rates if no valid messages received
- **Common Rates**: 57600, 115200, 230400, 460800, 500000, 921600, 1500000

#### Flow Control Support
```cpp
int set_flow_control(bool enabled);
```
- **RTS/CTS**: Hardware flow control for high-speed communication
- **Configuration**: Enabled via `FlowControl=true` in config

#### Device Validation
- **Character Device Check**: Ensures device is a valid serial port
- **Permission Validation**: Checks read/write permissions
- **Existence Check**: Verifies device file exists

### Implementation Details

#### Opening Device
```cpp
bool open(const char *path);
```
1. **Device Validation**: Check if device is character device
2. **Non-blocking Mode**: Set O_NONBLOCK for async I/O
3. **Terminal Settings**: Configure serial port parameters
4. **Baud Rate**: Set initial baud rate
5. **Flow Control**: Configure if enabled

#### Message Reading
- **Raw Serial Data**: Read directly from serial port
- **Message Framing**: Parse MAVLink message boundaries
- **Error Recovery**: Handle partial reads and CRC errors

## UDP Endpoint (`endpoint.cpp`)

### Purpose
Network communication using UDP protocol, supporting both client and server modes.

### Configuration
```cpp
struct UdpEndpointConfig {
    enum class Mode { Undefined = 0, Server, Client };
    
    std::string name;
    std::string address;          // IP address
    unsigned long port;           // UDP port
    Mode mode;                    // Client or Server
    
    // Filtering configuration (same as UART)
    std::string group;
};
```

### Operating Modes

#### Client Mode
- **Outbound Connection**: Sends messages to configured address:port
- **Ground Station**: Typically used for connecting to ground stations
- **Port Assignment**: Automatic port assignment if not specified (starts at 14550)

#### Server Mode
- **Inbound Connection**: Listens on configured port
- **Multi-client**: Accepts messages from any source
- **Broadcast Support**: Can broadcast to multiple destinations

### Implementation Details

#### IPv4/IPv6 Support
```cpp
int open_ipv4(const char *ip, unsigned long port, UdpEndpointConfig::Mode mode);
int open_ipv6(const char *ip, unsigned long port, UdpEndpointConfig::Mode mode);
```
- **Dual Stack**: Supports both IPv4 and IPv6
- **Automatic Detection**: Detects IP version from address format
- **Square Brackets**: IPv6 addresses require square brackets

#### Socket Configuration
- **Non-blocking**: All sockets configured for async I/O
- **Reuse Address**: SO_REUSEADDR for rapid restart
- **Bind/Connect**: Appropriate configuration based on mode

#### Message Handling
- **Datagram**: Each MAVLink message in separate UDP packet
- **Source Tracking**: Remembers source address for replies
- **Error Handling**: Graceful handling of network errors

## TCP Endpoint (`endpoint.cpp`)

### Purpose
Reliable network communication using TCP protocol with automatic reconnection.

### Configuration
```cpp
struct TcpEndpointConfig {
    std::string name;
    std::string address;          // IP address
    unsigned long port;           // TCP port
    int retry_timeout{5};         // Reconnection timeout in seconds
    
    // Filtering configuration (same as UART)
    std::string group;
};
```

### Key Features

#### Client Mode Operation
- **Outbound Connection**: Connects to remote TCP server
- **Stream Protocol**: Reliable, ordered message delivery
- **Connection Management**: Automatic connection establishment

#### Automatic Reconnection
```cpp
void _schedule_reconnect();
bool _retry_timeout_cb(void *data);
```
- **Configurable Timeout**: Retry interval configurable per endpoint
- **Exponential Backoff**: Not implemented, uses fixed interval
- **Graceful Handling**: Non-blocking reconnection attempts

#### Connection Validation
```cpp
bool is_valid() override { return _valid; };
```
- **Connection State**: Tracks connection validity
- **Error Detection**: Detects broken connections
- **Cleanup**: Automatic cleanup of invalid connections

### Implementation Details

#### Connection Establishment
```cpp
bool open(const std::string &ip, unsigned long port);
```
1. **Address Resolution**: Support for hostnames and IP addresses
2. **Non-blocking Connect**: Async connection establishment
3. **Error Handling**: Graceful handling of connection failures
4. **Timeout Management**: Connection timeout handling

#### Message Streaming
- **Stream Reassembly**: Handle partial TCP reads
- **Message Boundaries**: Proper MAVLink message framing
- **Flow Control**: TCP's built-in flow control

#### Server Mode (Mainloop)
```cpp
void handle_tcp_connection();
```
- **Dynamic Endpoints**: Create endpoints for incoming connections
- **Connection Acceptance**: Handle new client connections
- **Resource Management**: Automatic cleanup of disconnected clients

## Log Endpoints (`logendpoint.h`)

### Purpose
Record MAVLink messages to files for later analysis and debugging.

### Base Log Endpoint
```cpp
class LogEndpoint : public Endpoint {
public:
    virtual bool start();
    virtual void stop();
    void mark_unfinished_logs();
    
protected:
    virtual const char *_get_logfile_extension() = 0;
    virtual bool _logging_start_timeout() = 0;
};
```

### Log Configuration
```cpp
struct LogOptions {
    enum class MavDialect { Auto, Common, Ardupilotmega };
    
    std::string logs_dir;                    // Log directory
    LogMode log_mode{LogMode::always};       // When to log
    MavDialect mavlink_dialect{MavDialect::Auto}; // MAVLink dialect
    unsigned long min_free_space;            // Minimum free space
    unsigned long max_log_files;             // Maximum log files
    int fcu_id{-1};                         // Target system ID
    bool log_telemetry{false};              // Enable telemetry logging
};
```

### Logging Modes
```cpp
enum class LogMode {
    always = 0,     // Log from start until exit
    while_armed,    // Log only when vehicle is armed
    disabled        // Do not log
};
```

## Message Filtering System

### Filter Configuration
Each endpoint supports comprehensive message filtering:

#### Message ID Filters
- **AllowMsgIdOut/In**: Whitelist of allowed message IDs
- **BlockMsgIdOut/In**: Blacklist of blocked message IDs

#### Source Component Filters  
- **AllowSrcCompOut/In**: Whitelist of allowed component IDs
- **BlockSrcCompOut/In**: Blacklist of blocked component IDs

#### Source System Filters
- **AllowSrcSysOut/In**: Whitelist of allowed system IDs
- **BlockSrcSysOut/In**: Blacklist of blocked system IDs

### Filter Application
```cpp
AcceptState accept_msg(const struct buffer *pbuf) const;
```

#### Accept States
- **Accepted**: Message passes all filters
- **Filtered**: Message blocked by filters but recognized
- **Rejected**: Message not intended for this endpoint

#### Filter Precedence
1. **Sniffer Mode**: If endpoint has sniffer system ID, accept all
2. **Allow Lists**: If present, only listed items allowed
3. **Block Lists**: If present, listed items blocked
4. **Default**: Allow all if no filters configured

## Endpoint Groups

### Group Configuration
```cpp
std::string group;  // In endpoint configuration
```

### Group Behavior
- **Message Sharing**: Messages received by one group member shared with all
- **Redundancy**: Provides redundant communication paths
- **Load Distribution**: Can distribute load across multiple endpoints

### Group Linking
```cpp
void link_group_member(std::shared_ptr<Endpoint> other);
```
- **Mutual Linking**: Group members reference each other
- **Runtime Linking**: Groups formed during endpoint initialization
- **Dynamic Groups**: Groups can be modified at runtime

## Statistics and Monitoring

### Per-Endpoint Statistics
```cpp
struct {
    struct {
        uint64_t crc_error_bytes = 0;    // Bytes with CRC errors
        uint64_t handled_bytes = 0;      // Successfully handled bytes
        uint32_t total = 0;              // Total messages
        uint32_t crc_error = 0;          // CRC error count
        uint32_t handled = 0;            // Successfully handled messages
        uint32_t drop_seq_total = 0;     // Sequence number gaps
        uint8_t expected_seq = 0;        // Next expected sequence
    } read;
    struct {
        uint64_t bytes = 0;              // Transmitted bytes
        uint32_t total = 0;              // Transmitted messages
    } write;
} _stat;
```

### Statistics Reporting
```cpp
virtual void print_statistics();
void log_aggregate(unsigned int interval_sec);
```
- **Periodic Reporting**: Configurable statistics intervals
- **Error Aggregation**: Batch error reporting
- **Performance Metrics**: Throughput and error rate tracking

## Buffer Management

### Buffer Structure
```cpp
struct buffer {
    unsigned int len;        // Current data length
    uint8_t *data;          // Raw data buffer
    
    struct {                // Parsed message metadata
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

### Buffer Sizes
```cpp
#define RX_BUF_MAX_SIZE (MAVLINK_MAX_PACKET_LEN * 4)  // ~1KB
#define TX_BUF_MAX_SIZE (8U * 1024U)                  // 8KB
```

### Buffer Management
- **Fixed Size**: Predictable memory usage
- **Reuse**: Buffers reused across messages
- **Overflow Protection**: Bounds checking on all operations

## Error Handling

### Error Categories
1. **Communication Errors**: Network, serial port failures
2. **Protocol Errors**: MAVLink parsing, CRC failures
3. **Configuration Errors**: Invalid endpoint configuration
4. **Resource Errors**: Memory, file descriptor exhaustion

### Recovery Mechanisms
- **Automatic Retry**: TCP reconnection, UART baud rate retry
- **Graceful Degradation**: Continue operation with failed endpoints
- **Error Reporting**: Comprehensive error logging and statistics
- **Resource Cleanup**: Automatic cleanup of failed endpoints

### Critical vs Non-Critical
```cpp
virtual bool is_critical() { return true; };
```
- **Critical Endpoints**: Failure causes application exit
- **Non-Critical Endpoints**: Failure logged but operation continues
- **TCP Endpoints**: Generally non-critical (can reconnect)