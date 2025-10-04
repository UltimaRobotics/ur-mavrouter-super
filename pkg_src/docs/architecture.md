# MAVLink Router Architecture Overview

## System Design

MAVLink Router follows a **event-driven, asynchronous I/O architecture** using the Linux epoll mechanism for high-performance message routing between multiple communication endpoints.

## Core Components

### 1. Main Application (`main.cpp`)
- **Entry Point**: Application initialization and configuration parsing
- **Configuration Management**: Handles command-line arguments and configuration files
- **Lifecycle Management**: Manages application startup and shutdown

**Key Functions**:
- `main()`: Entry point with configuration parsing and mainloop initialization
- `parse_argv()`: Command-line argument processing
- `parse_conf_files()`: Configuration file parsing
- `pre_parse_argv()`: Early configuration parsing for conf files and logging

### 2. Main Event Loop (`mainloop.cpp`, `mainloop.h`)
- **Singleton Pattern**: Single instance managing the entire application lifecycle
- **Epoll-based I/O**: Asynchronous event handling using Linux epoll
- **Message Routing Engine**: Central hub for all MAVLink message routing
- **Connection Management**: Handles TCP server and endpoint lifecycle

**Key Features**:
- Event-driven architecture with epoll
- Signal handling (SIGTERM, SIGINT, SIGPIPE)
- Timeout management system
- Statistics and error aggregation
- TCP connection handling

### 3. Endpoint System (`endpoint.cpp`, `endpoint.h`)
- **Polymorphic Design**: Base class with specialized implementations
- **Communication Interfaces**: UART, UDP, TCP, and logging endpoints
- **Message Filtering**: Comprehensive filtering based on message ID, system ID, component ID
- **Statistics Tracking**: Per-endpoint message and error statistics

**Endpoint Types**:
- **UartEndpoint**: Serial communication with autopilot
- **UdpEndpoint**: UDP client/server communication
- **TcpEndpoint**: TCP client connections with reconnection
- **LogEndpoint**: Base class for logging endpoints

### 4. Logging System
- **Multiple Formats**: Support for different logging formats
- **Auto-detection**: Automatic format detection based on MAVLink dialect
- **Log Management**: Automatic log rotation and cleanup

**Logging Types**:
- **AutoLog**: Automatically detects and uses appropriate logging format
- **BinLog**: ArduPilot binary log format (.bin)
- **ULog**: PX4 ULog format (.ulg)
- **TLog**: Telemetry log format (.tlog)

### 5. Utility Modules
- **Message De-duplication**: Prevents duplicate message forwarding
- **Timeout Management**: Timer-based operations
- **Configuration Parser**: INI-style configuration file support
- **Logging Framework**: Multi-level logging with multiple backends

## Architecture Patterns

### 1. **Singleton Pattern**
- `Mainloop` class uses singleton pattern for global access
- Ensures single instance of the main event loop

### 2. **Polymorphism**
- `Endpoint` base class with virtual methods
- Specialized implementations for different communication types
- `Pollable` interface for epoll-compatible objects

### 3. **Strategy Pattern**
- Different logging strategies (BinLog, ULog, TLog)
- Configurable message filtering strategies

### 4. **Observer Pattern**
- Timeout callbacks for periodic operations
- Event-driven message handling

## Data Flow

```
[UART/UDP/TCP] → [Endpoint] → [Message Parser] → [Router] → [Filters] → [Target Endpoints]
                                                     ↓
                                               [Log Endpoints]
```

### Message Processing Pipeline

1. **Reception**: Messages received from communication endpoints
2. **Parsing**: MAVLink message parsing and validation
3. **Routing Decision**: Determine target endpoints based on system/component IDs
4. **Filtering**: Apply configured filters (message ID, source filters)
5. **De-duplication**: Check for duplicate messages
6. **Distribution**: Forward to appropriate endpoints
7. **Logging**: Record messages to log files if configured

## Threading Model

**Single-threaded Design**: The application uses a single-threaded, event-driven model with:
- Main thread handles all I/O and processing
- Epoll for asynchronous I/O multiplexing
- Timeout-based periodic operations
- Signal-based shutdown handling

## Memory Management

- **RAII**: Resource Acquisition Is Initialization pattern
- **Smart Pointers**: `std::shared_ptr` for endpoint management
- **Buffer Management**: Fixed-size buffers for message handling
- **Automatic Cleanup**: Proper resource cleanup on shutdown

## Configuration System

### File-based Configuration
- INI-style configuration files
- Section-based organization for different endpoint types
- Support for multiple configuration files
- Command-line override capability

### Runtime Configuration
- Command-line arguments override file settings
- Environment variable support
- Dynamic endpoint creation from CLI

## Error Handling

- **Graceful Degradation**: Non-critical endpoint failures don't stop the router
- **Error Aggregation**: Statistics collection for monitoring
- **Logging**: Comprehensive error logging at multiple levels
- **Recovery Mechanisms**: Automatic reconnection for TCP endpoints

## Performance Characteristics

- **Low Latency**: Event-driven design minimizes message processing delay
- **High Throughput**: Efficient message routing without unnecessary copying
- **Scalable**: Supports multiple concurrent endpoints
- **Memory Efficient**: Fixed buffer sizes and careful memory management

## Security Considerations

- **Input Validation**: Message validation and CRC checking
- **Buffer Safety**: Bounds checking for all buffer operations
- **Configuration Validation**: Validation of all configuration parameters
- **Resource Limits**: Configurable limits to prevent resource exhaustion