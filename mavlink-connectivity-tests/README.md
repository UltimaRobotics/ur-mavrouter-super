
# MAVLink Endpoint Connectivity Tests

Real-time monitoring and validation of MAVLink endpoint availability and protocol compliance.

## Features

- **Real-time Monitoring**: Continuously monitors endpoint availability
- **MAVLink Validation**: Verifies endpoints are sending valid MAVLink messages
- **Auto-reconnect**: Automatically attempts to reconnect to failed endpoints
- **JSON Configuration**: Easy endpoint configuration via JSON files
- **Protocol Support**: Works with both UDP and TCP endpoints
- **Status Display**: Live status updates every second

## Configuration

Edit `config/endpoints.json` to configure your MAVLink endpoints:

```json
{
  "endpoints": [
    {
      "name": "flight_controller",
      "type": "udp",
      "address": "127.0.0.1",
      "port": 14550,
      "enabled": true
    }
  ],
  "test_settings": {
    "status_interval_seconds": 1,
    "heartbeat_timeout_seconds": 5,
    "reconnect_delay_seconds": 2
  }
}
```

### Endpoint Configuration

- **name**: Friendly name for the endpoint
- **type**: "udp" or "tcp"
- **address**: IP address or hostname
- **port**: Port number
- **enabled**: true/false to enable/disable monitoring

### Test Settings

- **status_interval_seconds**: How often to display status (default: 1)
- **heartbeat_timeout_seconds**: Time before considering endpoint stale (default: 5)
- **reconnect_delay_seconds**: Delay between reconnection attempts (default: 2)

## Usage

### Run with default config:
```bash
./test_endpoints.sh
```

### Run with custom config:
```bash
./test_endpoints.sh path/to/custom-config.json
```

### Run directly with Python:
```bash
python3 endpoint_monitor.py config/endpoints.json
```

## Status Display

The monitor displays color-coded status for each endpoint:

- **DISCONNECTED** (Gray): Not connected
- **CONNECTING** (Yellow): Attempting to connect
- **CONNECTED** (Blue): Connected but no MAVLink detected
- **MAVLINK_VERIFIED** (Green): Connected and receiving MAVLink messages
- **ERROR** (Red): Connection error

## Example Output

```
================================================================================
MAVLink Endpoint Monitor - 2025-01-08 10:30:45
================================================================================

[flight_controller] UDP 127.0.0.1:14550
  Status: MAVLINK_VERIFIED
  MAVLink Messages: 142
  Last Message: 0.3s ago

[ground_station] UDP 127.0.0.1:14551
  Status: ERROR
  Error: [Errno 111] Connection refused

[tcp_extension] TCP 127.0.0.1:5760
  Status: CONNECTED

================================================================================
```

## Testing Strategy

1. **Initial Connection**: Tests attempt to connect to each endpoint
2. **MAVLink Detection**: Monitors for MAVLink magic bytes (0xFD or 0xFE)
3. **Continuous Monitoring**: Keeps monitoring even if endpoints fail
4. **Auto-recovery**: Automatically reconnects when endpoints become available
5. **Message Counting**: Tracks number of MAVLink messages received

## Requirements

- Python 3.6+
- No external dependencies (uses standard library only)

## Integration

This test suite can be integrated into your CI/CD pipeline or used for:

- Development environment validation
- Production endpoint monitoring
- Network connectivity testing
- MAVLink protocol compliance verification
