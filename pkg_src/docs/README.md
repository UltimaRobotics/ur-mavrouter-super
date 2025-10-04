# MAVLink Router Source Code Documentation

This directory contains comprehensive documentation for the MAVLink Router source code located in `pkg_src/src/`.

## Documentation Structure

### Core Documentation
- **[Architecture Overview](architecture.md)** - High-level system architecture and design patterns
- **[Application Flow](application-flow.md)** - Main execution flow and lifecycle management
- **[Message Routing](message-routing.md)** - How MAVLink messages are routed between endpoints

### Component Documentation
- **[Endpoints System](endpoints.md)** - Detailed documentation of endpoint types and mechanisms
- **[Logging System](logging-system.md)** - Flight logging and telemetry logging components
- **[Utility Modules](utilities.md)** - Support modules and common utilities

### File Reference
- **[Source Files Reference](source-files.md)** - Detailed breakdown of all source files
- **[Common Utilities Reference](common-utilities.md)** - Documentation of shared utility functions

## Quick Start

1. Read the [Architecture Overview](architecture.md) to understand the system design
2. Review [Application Flow](application-flow.md) to understand how the application starts and runs
3. Examine [Message Routing](message-routing.md) to understand the core functionality
4. Dive into specific component documentation as needed

## Key Concepts

- **Endpoints**: Communication interfaces (UART, UDP, TCP, Log)
- **Message Routing**: Intelligent forwarding of MAVLink messages
- **Event Loop**: Epoll-based asynchronous I/O handling
- **Configuration**: File-based and command-line configuration system
- **Logging**: Multiple logging formats (BinLog, ULog, TLog, AutoLog)
- **De-duplication**: Message filtering to prevent duplicates
- **Filtering**: Message and source filtering capabilities

## System Overview

MAVLink Router is a high-performance message routing system that:
- Routes MAVLink messages between multiple communication endpoints
- Supports UART, UDP, and TCP connections
- Provides comprehensive logging capabilities
- Offers message filtering and de-duplication
- Uses asynchronous I/O for optimal performance
- Supports configuration via files and command line

The system is built with C++11 and uses modern Linux features like epoll for efficient event handling.