# Source Files Reference

## Overview

This document provides a detailed breakdown of all source files in the MAVLink Router codebase, organized by functionality and purpose.

## Core Application Files

### `main.cpp`
**Purpose**: Application entry point and configuration management

**Key Functions**:
- `main()`: Application entry point, configuration parsing, and main loop initialization
- `parse_argv()`: Command-line argument processing with validation
- `pre_parse_argv()`: Early configuration file path extraction
- `parse_conf_files()`: Configuration file loading and parsing
- `help()`: Display usage information
- Various utility functions for parsing and validation

**Key Features**:
- Uses `getopt_long` for command-line processing
- Supports configuration files and directory-based configuration
- Environment variable support for configuration paths
- Comprehensive input validation
- Signal handling setup
- Proper error handling and cleanup

### `mainloop.cpp` / `mainloop.h`
**Purpose**: Central event loop and message routing engine

**Architecture**:
- Singleton pattern for single application instance
- Epoll-based asynchronous I/O for high performance
- Central message routing hub
- TCP server management
- Timeout system for periodic operations

**Key Methods**:
- `loop()`: Main event processing loop using epoll
- `route_msg()`: Core message routing between endpoints
- `add_endpoints()`: Create and configure endpoints from configuration
- `handle_tcp_connection()`: Accept and manage TCP connections
- Timeout management methods for periodic operations
- Statistics aggregation and reporting methods

**Features**:
- Signal handling for graceful shutdown
- TCP connection management
- Error aggregation and reporting
- Performance statistics tracking

## Endpoint Implementation Files

### `endpoint.cpp` / `endpoint.h`
**Purpose**: Base endpoint class and specialized endpoint implementations

**Architecture**:
- Abstract base class `Endpoint` with virtual methods
- Specialized implementations: `UartEndpoint`, `UdpEndpoint`, `TcpEndpoint`
- Polymorphic design for different communication types
- Message filtering system with allow/block lists
- Statistics tracking for monitoring

**Base Endpoint Features**:
- Message filtering by ID, system ID, component ID
- Statistics collection for read/write operations
- Buffer management for message handling
- Group support for endpoint linking
- Configuration validation

**Endpoint Types**:
- **UartEndpoint**: Serial communication with baud rate detection
- **UdpEndpoint**: UDP networking with client/server modes  
- **TcpEndpoint**: TCP networking with automatic reconnection

**Key Capabilities**:
- IPv4/IPv6 support for network endpoints
- Flow control for UART endpoints
- Dynamic TCP connection handling
- Comprehensive error handling and recovery

### `logendpoint.cpp` / `logendpoint.h`  
**Purpose**: Base class for logging endpoints

**Features**:
- Abstract base for different logging formats
- File management with automatic naming
- Log rotation and cleanup
- Asynchronous file operations
- Crash recovery for incomplete logs

**Configuration Support**:
- Multiple logging modes
- Directory and file management
- Space and file count limits
- MAVLink dialect detection

## Logging Implementation Files

### `autolog.cpp` / `autolog.h`
**Purpose**: Automatic logging format detection

**Features**:
- Analyzes MAVLink messages to detect autopilot type
- Dynamically creates appropriate logger (BinLog or ULog)
- Transparent delegation to selected logger
- Fallback behavior for unknown message types

### `binlog.cpp` / `binlog.h`
**Purpose**: ArduPilot binary log format support

**Features**:
- Implements ArduPilot remote logging protocol
- Handles sequence numbers and acknowledgments
- Manages log data flow control
- Supports ArduPilot-specific logging messages
- Creates .bin format log files

### `ulog.cpp` / `ulog.h`
**Purpose**: PX4 ULog format support

**Features**:
- Implements PX4 ULog logging protocol
- Handles ULog header and data messages
- Manages message sequencing
- Buffer management for data reassembly
- Creates .ulg format log files

### `tlog.cpp` / `tlog.h`
**Purpose**: Telemetry log format support

**Features**:
- Records raw MAVLink messages
- Universal format compatible with any MAVLink system
- Simple binary format with timestamps
- Creates .tlog format log files

## Utility Implementation Files

### `dedup.cpp` / `dedup.h`
**Purpose**: Message de-duplication system

**Features**:
- Hash-based duplicate message detection
- Configurable time window for duplicate tracking
- Automatic cleanup of expired entries
- Memory-bounded operation
- Prevents message loops in complex topologies

### `timeout.cpp` / `timeout.h`
**Purpose**: Timer management system

**Features**:
- Linux timerfd-based precise timing
- Callback-based timer system
- Integration with main epoll event loop
- Linked list management for multiple timers
- Used for periodic operations and retries

### `pollable.cpp` / `pollable.h`
**Purpose**: Base class for epoll-compatible objects

**Features**:
- Abstract interface for objects that can be monitored by epoll
- Virtual methods for read/write event handling
- File descriptor management
- Used by endpoints and timeout objects
- Simple destructor that closes file descriptors

## Common Utilities

### `common/conf_file.cpp` / `common/conf_file.h`
**Purpose**: INI-style configuration file parser

**Features**:
- **Section-based**: Organize configuration by sections
- **Type System**: Support for various data types
- **Validation**: Required field and type validation
- **Multiple Files**: Merge multiple configuration files
- **Memory Mapping**: Efficient file access using mmap

**Supported Types**:
- Strings with length limits
- Signed/unsigned integers
- Boolean values
- Vector (comma-separated lists)
- Custom parser functions

### `common/log.cpp` / `common/log.h`
**Purpose**: Structured logging framework

**Features**:
- **Multiple Levels**: ERROR, WARNING, NOTICE, INFO, DEBUG, TRACE
- **Multiple Backends**: stderr and syslog support
- **Runtime Configuration**: Change log level at runtime
- **Color Support**: Terminal color coding
- **Performance**: Compile-time level filtering

**Convenience Macros**:
- `log_error()`, `log_warning()`, `log_info()`
- `log_debug()`, `log_trace()`
- `assert_or_return()` for validation

### `common/util.cpp` / `common/util.h`
**Purpose**: Common utility functions

**Time Utilities**:
- High-resolution timing (microsecond precision)
- Monotonic clock support
- Time conversion utilities
- Performance measurement support

**String Utilities**:
- Safe string comparison macros
- Case-insensitive comparisons
- Memory comparison with length checking

**Numeric Utilities**:
- Safe numeric conversion with error checking
- Overflow detection
- Range validation

**File System Utilities**:
- Recursive directory creation
- Permission management
- Error handling for file operations

### `common/xtermios.cpp` / `common/xtermios.h`
**Purpose**: Cross-platform terminal I/O configuration

**Features**:
- **Baud Rate Configuration**: Support for various speeds
- **Flow Control**: Hardware and software flow control
- **Terminal Modes**: Raw mode for binary communication
- **Cross-platform**: Consistent interface

## Build and Version Files

### `git_version.cpp` / `git_version.h`
**Purpose**: Build version information

**Features**:
- Contains build version string
- Generated during build process from git information
- Simple implementation with version constant
- Used for version reporting in help and startup messages

### `version.h.in`
**Purpose**: Version template file for CMake processing

**Features**:
- Template file processed during build
- Contains version string placeholders
- Used to generate `git_version.h`
- Build system integration

## Test Files

### `endpoints_test.cpp`
**Purpose**: Unit tests for endpoint functionality

**Features**:
- Test cases for endpoint configuration and validation
- Message filtering tests
- Error handling verification
- Part of the testing framework

### `mainloop_test.cpp`
**Purpose**: Unit tests for main loop functionality

**Features**:
- Tests for event loop behavior
- Message routing verification
- Timeout system tests
- Part of the testing framework

## Configuration Files

### `config.h`
**Purpose**: Compile-time configuration definitions

**Features**:
- Build-time constants and definitions
- Configuration macros
- Platform-specific settings
- Used throughout the build process

## MAVLink Integration

### `common/mavlink.h`
**Purpose**: MAVLink protocol integration

**Features**:
- MAVLink library integration
- Protocol definitions
- Message structure definitions
- CRC and validation support

### `common/macro.h`
**Purpose**: Common preprocessor macros

**Macros**:
- Compiler attributes (`_packed_`, `_pure_`)
- Utility macros for common operations
- Platform compatibility macros
- Debug and optimization hints

### `common/dbg.h`
**Purpose**: Debug utilities

**Features**:
- Debug logging macros
- Conditional compilation support
- Performance measurement
- Memory debugging support

## Complete File List

### Main Application (3 files)
- `main.cpp`: Application entry point and configuration
- `mainloop.cpp/.h`: Event loop and message routing
- `config.h`: Compile-time configuration

### Endpoints (9 files)
- `endpoint.cpp/.h`: Base endpoint class and implementations
- `logendpoint.cpp/.h`: Logging endpoint base class
- `pollable.cpp/.h`: Base class for epoll objects
- `endpoints_test.cpp`: Endpoint unit tests
- `mainloop_test.cpp`: Main loop unit tests

### Logging (8 files)
- `autolog.cpp/.h`: Automatic format detection
- `binlog.cpp/.h`: ArduPilot binary logs
- `ulog.cpp/.h`: PX4 ULog format
- `tlog.cpp/.h`: Telemetry logs

### Utilities (6 files)
- `dedup.cpp/.h`: Message de-duplication
- `timeout.cpp/.h`: Timer management
- `comm.h`: Message buffer definitions

### Build Support (3 files)
- `git_version.cpp/.h`: Version information
- `version.h.in`: Version template

### Common Directory (11 files)
- `conf_file.cpp/.h`: Configuration parsing
- `log.cpp/.h`: Logging framework
- `util.cpp/.h`: System utilities
- `xtermios.cpp/.h`: Terminal I/O
- `mavlink.h`: MAVLink integration
- `dbg.h`: Debug utilities
- `macro.h`: Compiler macros

**Total**: 37 source files providing complete MAVLink routing functionality