# Utility Modules Documentation

## Overview

The MAVLink Router includes several utility modules that provide essential supporting functionality for the core routing system. These modules handle cross-cutting concerns like timing, de-duplication, configuration parsing, and system utilities.

## Message De-duplication (`dedup.h`, `dedup.cpp`)

### Purpose
Prevents duplicate MAVLink messages from being forwarded in complex network topologies where message loops could occur.

### Class Interface

```cpp
class Dedup {
public:
    enum class PacketStatus { 
        NEW_PACKET_OR_TIMED_OUT, 
        ALREADY_EXISTS_IN_BUFFER 
    };
    
    Dedup(uint32_t dedup_period_ms = 0);
    ~Dedup();
    
    void set_dedup_period(uint32_t dedup_period_ms);
    PacketStatus check_packet(const uint8_t *buffer, uint32_t size);
    
private:
    uint32_t _dedup_period_ms;
    std::unique_ptr<DedupImpl> _impl;
};
```

### Implementation Strategy

#### Hash-based Detection
- **Content Hashing**: Calculate hash of complete message data
- **Time Window**: Track hash values within configurable time period
- **Automatic Cleanup**: Remove expired hash entries automatically

#### Configuration
- **Dedup Period**: Time window in milliseconds for duplicate detection
- **Default State**: Disabled by default (0ms period)
- **Runtime Configuration**: Can be enabled via configuration file or CLI

#### Usage Patterns
```cpp
// In mainloop.cpp
bool Mainloop::dedup_check_msg(const buffer *buf)
{
    return _msg_dedup.check_packet(buf->data, buf->len)
        == Dedup::PacketStatus::NEW_PACKET_OR_TIMED_OUT;
}
```

### Performance Characteristics
- **O(1) Lookup**: Hash table-based implementation for fast lookups
- **Memory Bounded**: Automatic cleanup prevents unbounded memory growth
- **Low Overhead**: Minimal impact on message processing performance

## Timeout Management (`timeout.h`, `timeout.cpp`)

### Purpose
Provides precise timing capabilities for periodic operations, retry mechanisms, and timeouts using Linux timer file descriptors.

### Class Interface

```cpp
class Timeout : public Pollable {
public:
    Timeout(std::function<bool(void *)> cb, const void *data);
    bool remove_me = false;
    Timeout *next = nullptr;
    
    int handle_read() override;
    bool handle_canwrite() override;
    
private:
    std::function<bool(void *)> _cb;
    const void *_data;
};
```

### Timeout System Architecture

#### Timer File Descriptors
- **Linux timerfd**: Precise timing using kernel timer facility
- **Epoll Integration**: Timers integrated into main event loop
- **Non-blocking**: Timer events don't block message processing

#### Callback System
```cpp
std::function<bool(void *)> _cb;
```
- **Flexible Callbacks**: Function objects with arbitrary logic
- **Return Value**: Boolean indicates if timer should continue
- **Data Parameter**: Arbitrary data passed to callback

#### Linked List Management
```cpp
Timeout *next = nullptr;
bool remove_me = false;
```
- **Efficient Queue**: Linked list for O(1) insertion/removal
- **Deferred Cleanup**: Mark for removal, clean up during event processing
- **Memory Management**: Automatic cleanup of expired timeouts

### Common Timeout Use Cases

#### Statistics Reporting
```cpp
bool _print_statistics_timeout_cb(void *data);
```
- **Periodic Reporting**: Regular statistics output
- **Configurable Interval**: Typically every 1-5 seconds
- **Performance Monitoring**: Track system performance over time

#### TCP Reconnection
```cpp
bool _retry_timeout_cb(void *data);
```
- **Connection Recovery**: Automatic reconnection for failed TCP endpoints
- **Configurable Delay**: Retry interval per endpoint configuration
- **Exponential Backoff**: Could be implemented for progressive delays

#### UART Baud Rate Detection
```cpp
bool _change_baud_cb(void *data);
```
- **Automatic Detection**: Try different baud rates automatically
- **Timeout-based**: Switch rates if no valid messages received
- **Recovery Mechanism**: Ensure communication establishment

#### Log Aggregation
```cpp
bool _log_aggregate_timeout(void *data);
```
- **Error Aggregation**: Batch error reporting to reduce log spam
- **Regular Intervals**: Periodic summary of error conditions
- **System Health**: Overall system health monitoring

### Timeout Creation and Management

#### Adding Timeouts
```cpp
Timeout *Mainloop::add_timeout(uint32_t timeout_msec, 
                              std::function<bool(void *)> cb,
                              const void *data)
{
    auto *t = new Timeout(cb, data);
    t->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    mod_timeout(t, timeout_msec);
    add_fd(t->fd, t, EPOLLIN);
    
    t->next = _timeouts;
    _timeouts = t;
    return t;
}
```

#### Modifying Timeouts
```cpp
void mod_timeout(Timeout *t, uint32_t timeout_msec);
```
- **Dynamic Adjustment**: Change timeout intervals at runtime
- **Precision Timing**: Microsecond precision for timing
- **Kernel Interface**: Direct timerfd system call interface

#### Cleanup Process
```cpp
void _del_timeouts();
```
- **Deferred Cleanup**: Remove timeouts marked for deletion
- **Resource Management**: Proper file descriptor cleanup
- **Memory Safety**: Prevent memory leaks from expired timeouts

## Pollable Interface (`pollable.h`)

### Purpose
Provides a common interface for objects that can be monitored by the epoll event loop.

### Interface Definition

```cpp
class Pollable {
public:
    int fd = -1;
    virtual ~Pollable();
    
    virtual int handle_read() = 0;
    virtual bool handle_canwrite() = 0;
    virtual bool is_valid() { return true; }
    virtual bool is_critical() { return true; }
};
```

### Interface Methods

#### handle_read()
- **Purpose**: Process incoming data when EPOLLIN event occurs
- **Return Value**: Number of bytes processed or error code
- **Implementation**: Varies by pollable type (Endpoint, Timeout)

#### handle_canwrite()
- **Purpose**: Send pending data when EPOLLOUT event occurs
- **Return Value**: Boolean indicating if more data to send
- **Flow Control**: Manage output flow control

#### is_valid()
- **Purpose**: Check if pollable object is still valid
- **Use Case**: Detect disconnected TCP connections
- **Cleanup**: Invalid objects removed from epoll

#### is_critical()
- **Purpose**: Determine if failure should cause application exit
- **Critical Objects**: Main listening sockets, essential endpoints
- **Non-critical**: Dynamic TCP connections, optional endpoints

### Polymorphic Usage
- **Endpoint Objects**: All endpoints implement Pollable
- **Timeout Objects**: Timers implement Pollable for epoll integration
- **Future Extensions**: Other event sources can implement Pollable

## Configuration File Parser (`common/conf_file.h`, `common/conf_file.cpp`)

### Purpose
Parse INI-style configuration files with support for multiple sections and data types.

### Class Interface

```cpp
class ConfFile {
public:
    struct OptionsTable {
        const char *key;
        bool required;
        int (*parser_func)(const char *val, size_t val_len, 
                          void *storage, size_t storage_len);
        struct {
            off_t offset;
            size_t len;
        } storage;
    };
    
    int parse(const std::string &filename);
    int extract_options(const char *section, 
                       const OptionsTable *table, 
                       void *storage);
};
```

### Configuration File Format

#### Section-based Organization
```ini
[General]
TcpServerPort=5760
ReportStats=true
DebugLogLevel=info

[UartEndpoint autopilot]
device=/dev/ttyUSB0
baud=115200
FlowControl=false

[UdpEndpoint groundstation]
address=192.168.1.100
port=14550
mode=client
```

#### Supported Data Types
- **Strings**: Text values with configurable length
- **Integers**: Signed and unsigned integer values
- **Boolean**: true/false values
- **Vectors**: Comma-separated lists of values
- **Custom**: Custom parser functions for complex types

### Parser Features

#### Memory Mapping
- **mmap**: Use memory mapping for efficient file access
- **Zero-copy**: Parse directly from mapped memory
- **Performance**: Fast parsing of large configuration files

#### Multiple File Support
- **File Merging**: Load multiple configuration files
- **Override Behavior**: Later files override earlier settings
- **Directory Support**: Load all files from configuration directory

#### Validation
- **Required Fields**: Mark required configuration options
- **Type Checking**: Automatic type validation
- **Range Checking**: Validate numeric ranges

## Logging Framework (`common/log.h`, `common/log.cpp`)

### Purpose
Provides structured logging with multiple levels and output backends.

### Logging Interface

```cpp
class Log {
public:
    enum class Level {
        ERROR = 0, WARNING, NOTICE, INFO, DEBUG, TRACE,
    };
    
    enum class Backend {
        STDERR, SYSLOG,
    };
    
    static int open(Backend backend);
    static void set_max_level(Level level);
    static void log(Level level, const char *format, ...);
};
```

### Logging Levels

#### Level Hierarchy
- **ERROR**: Critical errors that may cause application failure
- **WARNING**: Important issues that don't prevent operation
- **NOTICE**: Significant events worth noting
- **INFO**: General information about operation
- **DEBUG**: Detailed debugging information
- **TRACE**: Very detailed trace information

#### Level Filtering
```cpp
static void set_max_level(Level level);
```
- **Runtime Configuration**: Change log level at runtime
- **Performance**: Higher levels filtered out at compile time
- **Configuration**: Set via command line or configuration file

### Output Backends

#### Standard Error
- **Default Backend**: Output to stderr by default
- **Color Support**: Automatic color coding based on terminal capability
- **Immediate Output**: Direct output for real-time monitoring

#### Syslog
- **System Integration**: Integration with system logging
- **Remote Logging**: Support for remote syslog servers
- **Structured Logging**: Proper syslog priority and facility

### Convenience Macros

```cpp
#define log_trace(...)   Log::log(Log::Level::TRACE, __VA_ARGS__)
#define log_debug(...)   Log::log(Log::Level::DEBUG, __VA_ARGS__)
#define log_info(...)    Log::log(Log::Level::INFO, __VA_ARGS__)
#define log_warning(...) Log::log(Log::Level::WARNING, __VA_ARGS__)
#define log_error(...)   Log::log(Log::Level::ERROR, __VA_ARGS__)
```

## Common Utilities (`common/util.h`, `common/util.cpp`)

### Purpose
Provides common utility functions used throughout the application.

### Time Utilities

```cpp
using usec_t = uint64_t;
using nsec_t = uint64_t;

#define MSEC_PER_SEC  1000ULL
#define USEC_PER_SEC  ((usec_t)1000000ULL)
#define NSEC_PER_SEC  ((nsec_t)1000000000ULL)

usec_t now_usec();
usec_t ts_usec(const struct timespec *ts);
```

#### High-Resolution Timing
- **Microsecond Precision**: Time measurements in microseconds
- **Monotonic Clock**: Uses CLOCK_MONOTONIC for reliable timing
- **Performance**: Fast time measurement for performance monitoring

### String Utilities

```cpp
#define streq(a, b)                   (strcmp((a), (b)) == 0)
#define strcaseeq(a, b)               (strcasecmp((a), (b)) == 0)
#define strncaseeq(a, b, len)         (strncasecmp((a), (b), (len)) == 0)
#define memcaseeq(a, len_a, b, len_b) ((len_a) == (len_b) && strncaseeq(a, b, len_a))
```

#### Safe String Comparison
- **Null Safety**: Safe string comparison macros
- **Case Insensitive**: Case-insensitive comparison options
- **Memory Comparison**: Safe memory comparison with length checking

### Numeric Parsing

```cpp
int safe_atoull(const char *s, unsigned long long *ret);
int safe_atoul(const char *s, unsigned long *ret);
int safe_atoi(const char *s, int *ret);
```

#### Safe Conversion
- **Error Checking**: Proper error detection for numeric conversion
- **Overflow Detection**: Detect numeric overflow conditions
- **Range Validation**: Validate numeric ranges

### File System Utilities

```cpp
int mkdir_p(const char *path, int len, mode_t mode);
```

#### Directory Creation
- **Recursive Creation**: Create directory trees recursively
- **Permission Setting**: Set appropriate directory permissions
- **Error Handling**: Proper error handling for file system operations

### Template Utilities

```cpp
template <typename type> 
bool vector_contains(std::vector<type> vect, type elem)
{
    return std::find(vect.begin(), vect.end(), elem) != vect.end();
}
```

#### STL Extensions
- **Container Utilities**: Common operations on STL containers
- **Type Safety**: Template-based type-safe operations
- **Performance**: Efficient implementations using STL algorithms

## Terminal I/O Utilities (`common/xtermios.h`, `common/xtermios.cpp`)

### Purpose
Provides cross-platform terminal I/O configuration for serial communication.

### Features
- **Baud Rate Configuration**: Support for various baud rates
- **Flow Control**: Hardware and software flow control
- **Terminal Modes**: Raw mode for binary communication
- **Cross-platform**: Consistent interface across different platforms

### Usage
- **UART Configuration**: Configure serial ports for MAVLink communication
- **Performance Optimization**: Optimal settings for high-speed communication
- **Reliability**: Proper terminal configuration for reliable communication