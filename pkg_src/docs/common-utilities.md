# Common Utilities Reference

## Overview

The `pkg_src/src/common/` directory contains shared utility modules that provide fundamental functionality used throughout the MAVLink Router application. These utilities handle configuration parsing, logging, system utilities, terminal I/O, and MAVLink protocol integration.

## Configuration File Parser (`conf_file.h`, `conf_file.cpp`)

### Purpose
The configuration file parser implements an INI-style configuration system with support for sections, various data types, and file validation. It uses memory mapping for efficient file access.

### Key Implementation Details

#### File Structure
The implementation uses internal data structures to represent parsed configuration:
- `struct config`: Represents individual key-value pairs
- `struct section`: Groups related configuration options
- `struct conffile`: Manages memory-mapped configuration files

#### Memory Management
The parser uses memory mapping (`mmap`) for efficient file access:
- Files are mapped read-only into memory
- Zero-copy parsing directly from mapped memory
- Automatic cleanup when the ConfFile object is destroyed

#### Configuration Format
Supports standard INI format:
- Sections defined with `[SectionName]`
- Key-value pairs with `Key=Value`
- Comments using `#`
- Multiple files can be parsed and merged
```

### Parser Functions
The implementation provides built-in parsers for common data types:
- String values with length validation
- Boolean values (true/false, yes/no, 1/0, on/off, enabled/disabled)
- Numeric values with overflow detection
- Vector values (comma-separated lists)

### Integration
The configuration parser is used throughout the application:
- Main configuration file parsing in `main.cpp`
- Endpoint-specific configuration sections
- Validation of all configuration parameters before use
- Support for configuration file discovery and multiple file loading


## Logging Framework (`log.h`, `log.cpp`)

### Purpose
Provides a structured logging system with multiple output backends and configurable log levels.

### Key Features
- Multiple log levels (ERROR, WARNING, NOTICE, INFO, DEBUG, TRACE)
- Two output backends: stderr and syslog
- Runtime log level configuration
- Color support for terminal output
- Thread-safe logging operations

### Implementation Details

#### Color Support
The logging system includes terminal color support:
- RED for errors
- ORANGE for warnings 
- YELLOW for notices
- WHITE for info
- LIGHTBLUE for debug
- Automatic detection of terminal capabilities
- Color codes defined as constants in the source

#### Backend Implementation
Two output backends are supported:
- **STDERR**: Direct output to standard error with color support
- **SYSLOG**: Integration with system syslog daemon

#### Message Formatting
The implementation uses vectored I/O (`struct iovec`) for efficient message construction and output.

### Usage
The logging system provides convenient macros for different log levels:
- `log_error()`, `log_warning()`, `log_info()`, etc.
- `assert_or_return()` macro for assertions with graceful error handling
- Printf-style format string support throughout


## System Utilities (`util.h`, `util.cpp`)

### Purpose
Provides common utility functions for time handling, string operations, numeric parsing, and file system operations.

### Time Utilities
Implements high-resolution timing functions:
- `now_usec()`: Gets current time using `CLOCK_MONOTONIC`
- `ts_usec()`: Converts timespec to microseconds with overflow handling
- Microsecond precision timing throughout the application
- Type definitions for time units (usec_t, nsec_t) and conversion constants

### String Utilities
Provides convenient string comparison macros:
- `streq()`, `strcaseeq()`: String equality comparisons
- `strncaseeq()`, `memcaseeq()`: Length-limited comparisons
- Used throughout the codebase for cleaner string comparisons

### Numeric Parsing
Implements safe numeric conversion functions:
- `safe_atoul()`, `safe_atoull()`, `safe_atoi()`: Safe string to number conversion
- Proper error detection using `strtoul()`, `strtoull()`, `strtol()`
- Overflow detection and range validation
- Used by configuration parser for numeric values

### File System Utilities
Provides directory creation functionality:
- `mkdir_p()`: Recursive directory creation with permission setting
- Used for creating log directories and other required paths
- Proper error handling for existing directories

### Template and Array Utilities
- `vector_contains()`: Template function to check vector membership
- `ARRAY_SIZE()`: Compile-time array size calculation macro
- Common utility patterns used throughout the codebase

## Terminal I/O Utilities (`xtermios.h`, `xtermios.cpp`)

### Purpose
Provides cross-platform terminal configuration for serial communication.

### Features
- Serial port configuration for various baud rates
- Raw mode setup for binary communication
- Flow control configuration (hardware/software)
- Used by UART endpoints for autopilot communication

## MAVLink Integration (`mavlink.h`)

### Purpose
Integration point for MAVLink protocol definitions and structures.

### Features
- Includes MAVLink C library headers
- Protocol version support (v1.0 and v2.0)
- Message structure definitions
- Used throughout the application for MAVLink message handling

## Debug Utilities (`dbg.h`)

### Purpose
Provides debug macros that are conditionally compiled based on build configuration.

### Features
- `dbg()` macro for debug output
- Conditional compilation (only in debug builds)
- Integration with the main logging system

## Macro Definitions (`macro.h`)

### Purpose
Provides common compiler attribute macros and utility definitions.

### Features
- `_packed_`: Structure packing attribute
- `_pure_`: Pure function attribute
- `_printf_format_`: Printf format checking
- Platform compatibility macros
- Used throughout the codebase for optimization and validation

## Summary

The common utilities provide essential foundational functionality:
- **Configuration**: Robust INI-style configuration parsing with validation
- **Logging**: Multi-level, multi-backend logging system with color support
- **Utilities**: Time handling, string operations, numeric parsing, file operations
- **Terminal I/O**: Serial port configuration for autopilot communication
- **Integration**: MAVLink protocol support and debug facilities

These utilities are used throughout the MAVLink Router to provide consistent, reliable operation with proper error handling and resource management.