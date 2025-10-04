# Application Flow and Lifecycle

## Startup Sequence

### 1. Pre-initialization
```cpp
Configuration config{};
if (!pre_parse_argv(argc, argv, config)) {
    return 0;
}
```
- **Early Argument Parsing**: Extract configuration file paths and logging backend
- **Version Check**: Handle `--version` flag
- **Logging Backend**: Set up syslog or stderr logging

### 2. Configuration Loading
```cpp
Log::open(config.log_backend);
if (parse_conf_files(config) < 0) {
    goto close_log;
}
if (parse_argv(argc, argv, config) != 2) {
    goto close_log;
}
```
- **Log System Initialization**: Open logging backend
- **Configuration File Parsing**: Load and parse configuration files
- **Command Line Parsing**: Process all command-line arguments
- **Configuration Merging**: CLI arguments override file settings

### 3. Main Loop Initialization
```cpp
if (mainloop.open() < 0) {
    goto close_log;
}
if (!mainloop.add_endpoints(config)) {
    goto close_log;
}
```
- **Epoll Creation**: Initialize the main event loop
- **Endpoint Creation**: Create and configure all endpoints
- **File Descriptor Registration**: Add endpoints to epoll

### 4. Main Event Loop
```cpp
retcode = mainloop.loop();
```
- **Event Processing**: Enter the main message processing loop
- **Signal Handling**: Set up signal handlers for graceful shutdown

## Main Event Loop

### Loop Structure
```cpp
while (!should_exit.load(std::memory_order_relaxed)) {
    r = epoll_wait(epollfd, events, max_events, -1);
    // Process events
}
```

### Event Processing Pipeline

#### 1. **TCP Connection Handling**
```cpp
if (events[i].data.ptr == &g_tcp_fd) {
    handle_tcp_connection();
    continue;
}
```
- Accept new TCP connections
- Create dynamic TCP endpoints
- Add to epoll for monitoring

#### 2. **Read Event Processing**
```cpp
if (events[i].events & EPOLLIN) {
    int rd = p->handle_read();
    if (rd < 0 && !p->is_valid()) {
        should_process_tcp_hangups = true;
    }
}
```
- **Message Reception**: Read data from file descriptors
- **Message Parsing**: Parse MAVLink messages
- **Validity Check**: Mark invalid endpoints for cleanup

#### 3. **Write Event Processing**
```cpp
if (events[i].events & EPOLLOUT) {
    if (!p->handle_canwrite()) {
        mod_fd(p->fd, p, EPOLLIN);
    }
}
```
- **Pending Message Flush**: Send queued messages
- **Flow Control**: Remove EPOLLOUT when queue is empty

#### 4. **Error Handling**
```cpp
if (events[i].events & EPOLLERR) {
    if (p->is_critical()) {
        request_exit(EXIT_FAILURE);
    }
}
```
- **Critical Error Detection**: Exit on critical endpoint errors
- **Non-critical Error Handling**: Log and continue for non-critical endpoints

## Message Routing Process

### 1. **Message Reception**
- Read raw data from file descriptor
- Buffer management and message framing
- MAVLink message validation

### 2. **Message Parsing**
```cpp
enum read_msg_result {
    ReadOk = 1,
    ReadUnkownMsg,
};
```
- Parse MAVLink headers (v1.0 and v2.0)
- Extract message metadata (system ID, component ID, message ID)
- CRC validation and sequence checking

### 3. **Routing Decision**
```cpp
for (const auto &e : this->g_endpoints) {
    auto acceptState = e->accept_msg(buf);
    switch (acceptState) {
        case Endpoint::AcceptState::Accepted:
        case Endpoint::AcceptState::Filtered:
        case Endpoint::AcceptState::Rejected:
    }
}
```

#### Message Accept States:
- **Accepted**: Message will be forwarded to this endpoint
- **Filtered**: Message filtered out by endpoint rules
- **Rejected**: Message not intended for this endpoint

### 4. **Message Filtering**
Multiple filter layers:
- **System ID Filtering**: Allow/block specific system IDs
- **Component ID Filtering**: Allow/block specific component IDs
- **Message ID Filtering**: Allow/block specific message types
- **Sniffer Mode**: Special mode that accepts all messages

### 5. **De-duplication Check** (`dedup.cpp`)
```cpp
enum class PacketStatus { 
    NEW_PACKET_OR_TIMED_OUT, 
    ALREADY_EXISTS_IN_BUFFER 
};
```
- Hash-based duplicate detection
- Configurable time window
- Automatic cleanup of old entries

### 6. **Message Transmission**
```cpp
int r = e->write_msg(buf);
if (r == -EAGAIN) {
    mod_fd(e->fd, e.get(), EPOLLIN | EPOLLOUT);
}
```
- Non-blocking message transmission
- Flow control for busy endpoints
- Error handling and statistics

## Endpoint Lifecycle

### Creation Phase
1. **Configuration Parsing**: Extract endpoint-specific configuration
2. **Validation**: Validate configuration parameters
3. **Socket/Device Opening**: Open communication interface
4. **Epoll Registration**: Add to main event loop

### Runtime Phase
1. **Message Processing**: Handle read/write events
2. **Error Recovery**: Automatic reconnection for TCP
3. **Statistics Collection**: Track message counts and errors
4. **Filtering**: Apply configured message filters

### Cleanup Phase
1. **Graceful Shutdown**: Close connections cleanly
2. **Resource Cleanup**: Free allocated resources
3. **Statistics Reporting**: Final statistics output

## Signal Handling

### Supported Signals
```cpp
sigaction(SIGTERM, &sa, nullptr);  // Graceful shutdown
sigaction(SIGINT, &sa, nullptr);   // Interrupt (Ctrl+C)
sigaction(SIGPIPE, &sa, nullptr);  // Ignore broken pipes
```

### Shutdown Process
1. **Signal Reception**: Signal handler sets exit flag
2. **Loop Exit**: Main loop detects exit condition
3. **Endpoint Cleanup**: Close all endpoints and connections
4. **Resource Cleanup**: Free timeouts and other resources
5. **Log Closure**: Close logging system

## Timeout Management

### Timeout Types
- **Statistics Reporting**: Periodic statistics output
- **Log Aggregation**: Error count aggregation
- **TCP Reconnection**: Retry failed TCP connections
- **UART Baud Rate**: Automatic baud rate detection

### Timeout Implementation
```cpp
Timeout *add_timeout(uint32_t timeout_msec, 
                    std::function<bool(void *)> cb, 
                    const void *data);
```
- **Timer File Descriptors**: Linux timerfd for precise timing
- **Callback System**: Function-based timeout callbacks
- **Linked List Management**: Efficient timeout queue
- **Automatic Cleanup**: Remove expired timeouts

## Error Handling and Recovery

### Error Categories
1. **Critical Errors**: Cause application exit
2. **Non-critical Errors**: Logged but operation continues
3. **Recoverable Errors**: Automatic retry mechanisms
4. **Configuration Errors**: Detected during startup

### Recovery Mechanisms
- **TCP Reconnection**: Automatic reconnection with configurable timeout
- **UART Baud Rate**: Try multiple baud rates automatically
- **Message Parsing**: Continue processing after parse errors
- **Endpoint Isolation**: Failed endpoints don't affect others

## Performance Optimizations

### Memory Management
- **Fixed Buffers**: Avoid dynamic allocation in hot paths
- **Buffer Reuse**: Reuse buffers across messages
- **Smart Pointers**: Automatic lifetime management

### I/O Optimization
- **Non-blocking I/O**: All I/O operations are non-blocking
- **Epoll Edge-triggered**: Efficient event notification
- **Zero-copy**: Minimize data copying where possible

### CPU Optimization
- **Single-threaded**: Avoid context switching overhead
- **Efficient Algorithms**: Hash-based de-duplication
- **Lazy Evaluation**: Only process when necessary