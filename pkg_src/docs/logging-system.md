# Logging System Documentation

## Overview

The MAVLink Router includes a comprehensive logging system that can record flight data and telemetry in multiple formats. The logging system is designed to be transparent to message routing while providing detailed recording capabilities for analysis and debugging.

## Logging Architecture

### Base Logging Class

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

#### Key Features
- **Non-intrusive**: Logging doesn't affect message routing
- **Asynchronous**: File I/O doesn't block message processing
- **Configurable**: Flexible configuration options
- **Automatic Management**: Log rotation and cleanup

## Logging Formats

### 1. AutoLog (`autolog.h`, `autolog.cpp`)

#### Purpose
Automatically detects the appropriate logging format based on the MAVLink dialect in use.

#### Implementation
```cpp
class AutoLog : public LogEndpoint {
public:
    int write_msg(const struct buffer *buffer) override;
    bool start() override;
    void stop() override;
    
private:
    std::unique_ptr<LogEndpoint> _logger;
};
```

#### Detection Logic
1. **Message Analysis**: Examines incoming MAVLink messages
2. **Dialect Detection**: Determines if ArduPilot or PX4 messages
3. **Logger Selection**: Creates appropriate logger (BinLog or ULog)
4. **Transparent Operation**: Forwards all operations to selected logger

### 2. BinLog (`binlog.h`, `binlog.cpp`)

#### Purpose
Records ArduPilot-style binary logs (.bin format) for ArduPilot-based autopilots.

#### Features
- **Native ArduPilot Format**: Compatible with ArduPilot log analysis tools
- **Remote Logging**: Supports ArduPilot remote logging protocol
- **Sequence Management**: Handles message sequencing and acknowledgment
- **Flow Control**: Manages data flow with autopilot

#### Protocol Support
```cpp
bool _logging_seq(uint16_t seq, bool *drop);
void _logging_data_process(mavlink_remote_log_data_block_t *msg);
void _send_ack(uint32_t seqno);
void _send_stop();
```

#### Remote Logging Messages
- **REMOTE_LOG_DATA_BLOCK**: Raw log data from autopilot
- **REMOTE_LOG_BLOCK_STATUS**: Acknowledgment and status
- **LOGGING_ACK**: Confirmation of received data

### 3. ULog (`ulog.h`, `ulog.cpp`)

#### Purpose
Records PX4-style ULog format (.ulg) for PX4-based autopilots.

#### Features
- **Native PX4 Format**: Compatible with PX4 log analysis tools
- **Structured Data**: Defined data structure format
- **Streaming Support**: Real-time log streaming
- **Header Management**: Proper ULog header generation

#### Buffer Management
```cpp
uint8_t _buffer[BUFFER_LEN];
uint16_t _buffer_len = 0;
uint16_t _buffer_index = 0;
uint8_t _buffer_partial[BUFFER_LEN / 2];
uint16_t _buffer_partial_len = 0;
```

#### State Management
- **Header Processing**: Handle ULog header messages
- **Data Processing**: Process log data messages
- **Sequence Tracking**: Maintain message sequence integrity

### 4. TLog (`tlog.h`, `tlog.cpp`)

#### Purpose
Records telemetry logs (.tlog format) containing raw MAVLink messages.

#### Features
- **Raw MAVLink**: Records complete MAVLink messages
- **Universal Format**: Works with any MAVLink-compatible system
- **Simple Format**: Straightforward message recording
- **Timestamp Support**: Includes timing information

#### Usage
- **Telemetry Analysis**: Review all MAVLink communication
- **Protocol Debugging**: Analyze message flows
- **System Integration**: Universal logging format

## Log Configuration

### Configuration Options

```cpp
struct LogOptions {
    enum class MavDialect { Auto, Common, Ardupilotmega };
    
    std::string logs_dir;                         // Log directory path
    LogMode log_mode{LogMode::always};            // When to start logging
    MavDialect mavlink_dialect{MavDialect::Auto}; // MAVLink dialect
    unsigned long min_free_space;                 // Minimum disk space
    unsigned long max_log_files;                  // Maximum log files
    int fcu_id{-1};                              // Target system ID
    bool log_telemetry{false};                   // Enable telemetry logs
};
```

### Logging Modes

```cpp
enum class LogMode {
    always = 0,     // Log from start until router exits
    while_armed,    // Start when vehicle armed, stop when disarmed
    disabled        // Do not attempt to start logging
};
```

#### Mode Details

##### Always Mode
- **Continuous Logging**: Starts immediately when router starts
- **Complete Coverage**: Records all messages from startup to shutdown
- **Use Case**: Development, debugging, comprehensive analysis

##### While Armed Mode
- **Conditional Logging**: Only logs when vehicle is armed
- **Flight Logging**: Focuses on actual flight operations
- **Space Efficient**: Saves disk space by not logging ground operations
- **Use Case**: Operational flights, focused flight analysis

### MAVLink Dialect Selection

#### Auto Detection
- **Message Analysis**: Examines message types to determine autopilot
- **Dynamic Selection**: Chooses appropriate logging format
- **Fallback**: Defaults to common MAVLink if detection fails

#### Manual Selection
- **Common**: Use ULog format for PX4-compatible systems
- **ArduPilotMega**: Use BinLog format for ArduPilot systems

## File Management

### Log Directory Structure
```
logs_dir/
├── YYYYMMDD_HHMMSS.bin    # ArduPilot binary logs
├── YYYYMMDD_HHMMSS.ulg    # PX4 ULog files
└── YYYYMMDD_HHMMSS.tlog   # Telemetry logs
```

### Automatic File Naming
```cpp
char _filename[64];
```
- **Timestamp-based**: Files named with creation timestamp
- **Unique Names**: Prevents filename collisions
- **Sortable**: Chronological sorting by filename

### Log Rotation and Cleanup

#### Space Management
```cpp
unsigned long min_free_space;  // Minimum free disk space
unsigned long max_log_files;   // Maximum number of log files
```

#### Cleanup Process
1. **Space Check**: Monitor available disk space
2. **File Count**: Track number of log files
3. **Oldest First**: Delete oldest logs when limits exceeded
4. **Safety Margin**: Maintain minimum free space

### Unfinished Log Handling

#### Recovery Process
```cpp
void mark_unfinished_logs();
```
1. **Startup Check**: Scan for incomplete logs on startup
2. **Crash Recovery**: Handle logs from previous crash/power loss
3. **Read-only Marking**: Mark incomplete logs as finished
4. **Data Preservation**: Preserve all recoverable data

## Asynchronous I/O

### Non-blocking File Operations
```cpp
aiocb _fsync_cb = {};
bool _fsync();
```

#### Features
- **Async fsync**: Non-blocking file synchronization
- **Write Buffering**: Efficient disk write operations
- **Flow Control**: Prevent memory exhaustion from buffering

### Timeout Management
```cpp
struct {
    Timeout *logging_start = nullptr;
    Timeout *fsync = nullptr;
    Timeout *alive = nullptr;
} _timeout;
```

#### Timeout Types
- **Logging Start**: Delay before starting logging operations
- **File Sync**: Periodic file synchronization
- **Alive Check**: Periodic health checks

## Message Flow Integration

### Transparent Logging
- **Pass-through**: Messages flow normally through routing system
- **Copy Operation**: Logging endpoints receive copies of all messages
- **No Interference**: Logging doesn't affect routing decisions

### System Integration
```cpp
// In mainloop.cpp endpoint creation
switch (conf.mavlink_dialect) {
case LogOptions::MavDialect::Ardupilotmega:
    this->_log_endpoint = std::make_shared<BinLog>(conf);
    break;
case LogOptions::MavDialect::Common:
    this->_log_endpoint = std::make_shared<ULog>(conf);
    break;
case LogOptions::MavDialect::Auto:
    this->_log_endpoint = std::make_shared<AutoLog>(conf);
    break;
}
```

## Remote Logging Protocol

### ArduPilot Remote Logging

#### Message Types
- **REMOTE_LOG_DATA_BLOCK** (ID: 184): Log data from autopilot
- **REMOTE_LOG_BLOCK_STATUS** (ID: 185): Status and acknowledgment
- **LOGGING_ACK** (ID: 267): Acknowledgment message

#### Protocol Flow
1. **Start Request**: Router requests logging start
2. **Data Blocks**: Autopilot sends log data blocks
3. **Acknowledgment**: Router acknowledges received blocks
4. **Flow Control**: Manage data flow rate
5. **Stop Request**: Router requests logging stop

#### Sequence Management
```cpp
uint32_t _last_acked_seqno = 0;
bool _logging_seq(uint16_t seq, bool *drop);
```
- **Sequence Tracking**: Track message sequence numbers
- **Gap Detection**: Detect and handle missing sequences
- **Acknowledgment**: Send appropriate acknowledgments

## Error Handling and Recovery

### File System Errors
- **Disk Full**: Graceful handling of insufficient disk space
- **Permission Errors**: Handle file permission issues
- **I/O Errors**: Recover from temporary I/O failures

### Protocol Errors
- **Missing Messages**: Handle gaps in log data
- **Sequence Errors**: Recover from sequence number issues
- **Timeout Handling**: Manage communication timeouts

### Logging State Management
```cpp
bool _waiting_header = true;
bool _waiting_first_msg_offset = false;
uint16_t _expected_seq = 0;
```
- **State Tracking**: Maintain logging protocol state
- **Recovery**: Graceful recovery from error conditions
- **Restart Capability**: Ability to restart logging after errors

## Performance Considerations

### Memory Management
- **Fixed Buffers**: Predictable memory usage
- **Buffer Reuse**: Efficient buffer management
- **Memory Limits**: Prevent memory exhaustion

### Disk I/O Optimization
- **Write Buffering**: Batch disk writes for efficiency
- **Async Operations**: Non-blocking file operations
- **Compression**: Consider future compression support

### CPU Usage
- **Minimal Processing**: Efficient log data handling
- **Background Operations**: Async operations don't block routing
- **Priority Management**: Logging has lower priority than routing

## Configuration Examples

### Complete Logging Configuration
```ini
[General]
LogSystemId=1
MinFreeSpace=1000000    # 1GB minimum free space
MaxLogFiles=100         # Keep maximum 100 log files

[Log]
logs_dir=/var/log/mavlink
LogMode=while_armed
MavlinkDialect=Auto
LogTelemetry=true
```

### Command Line Configuration
```bash
ur-mavrouter -l /var/log/mavlink -T --log-mode=while_armed
```

## Integration with Analysis Tools

### ArduPilot Integration
- **Mission Planner**: Direct compatibility with .bin logs
- **APM Planner**: Native log analysis support
- **Log Analysis Tools**: Full compatibility with ArduPilot ecosystem

### PX4 Integration
- **Flight Review**: Direct upload of .ulg files
- **PX4 Log Analysis**: Native tool support
- **PlotJuggler**: ULog format support

### Universal Tools
- **MAVProxy**: TLog file replay capability
- **Custom Analysis**: Raw MAVLink message access
- **Protocol Analysis**: Complete message flow visibility