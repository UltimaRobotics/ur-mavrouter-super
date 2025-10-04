
# Configuration File Reference

## Overview

ur-mavrouter supports both INI-style and JSON configuration files to define routing behavior, endpoint configurations, and system settings. The configuration system supports:

- Main configuration file (default: `/etc/mavlink-router/main.conf`)
- JSON configuration file (specified via `--json-conf-file` option)
- Configuration directory with multiple files (default: `/etc/mavlink-router/config.d/`)
- Command-line overrides
- Environment variable support

## File Structure

### INI Format

Configuration files use standard INI format with sections and key-value pairs:

```ini
[SectionName]
Key=Value
AnotherKey=Value

[AnotherSection]
Key=Value
```

### JSON Format

JSON configuration files use a structured format with nested objects and arrays:

```json
{
  "general": {
    "tcp_server_port": 5760,
    "report_stats": true,
    "debug_log_level": "info"
  },
  "uart_endpoints": [
    {
      "name": "pixhawk",
      "device": "/dev/ttyUSB0",
      "baud": "115200"
    }
  ],
  "udp_endpoints": [
    {
      "name": "groundstation",
      "address": "192.168.1.100",
      "port": 14550,
      "mode": "client"
    }
  ]
}
```

## Global Configuration

### [General] Section

Controls global router behavior and system-wide settings.

```ini
[General]
TcpServerPort=5760
ReportStats=true
DebugLogLevel=info
DeduplicationPeriod=100
SnifferSysid=255
```

#### Available Options

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `TcpServerPort` | unsigned long | 5760 | TCP server listening port (0 to disable) |
| `ReportStats` | boolean | false | Enable message statistics reporting |
| `DebugLogLevel` | string | info | Log level: error, warning, info, debug, trace |
| `DeduplicationPeriod` | unsigned long | 100 | Message deduplication period in milliseconds |
| `SnifferSysid` | unsigned long | 0 | System ID that receives all messages (0 to disable) |

## Endpoint Configuration

### UART Endpoints

UART endpoints handle serial communication with flight controllers and other MAVLink devices.

#### Section Pattern
```ini
[uartendpoint <name>]
```

#### Example Configuration
```ini
[uartendpoint pixhawk]
device=/dev/ttyUSB0
baud=57600,115200,921600
FlowControl=false
AllowMsgIdOut=0,1,2,33
BlockMsgIdOut=150,151
AllowSrcCompOut=1,190
BlockSrcCompOut=200
AllowSrcSysOut=1,255
BlockSrcSysOut=100
AllowMsgIdIn=0,1,2,33
BlockMsgIdIn=150,151
AllowSrcCompIn=1,190
BlockSrcCompIn=200
AllowSrcSysIn=1,255
BlockSrcSysIn=100
group=autopilot
```

#### UART Options

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `device` | string | Yes | Serial device path (e.g., /dev/ttyUSB0) |
| `baud` | uint32 list | No | Baud rates to try (default: 57600) |
| `FlowControl` | boolean | No | Enable hardware flow control |
| `AllowMsgIdOut` | uint32 list | No | Allowed outgoing message IDs |
| `BlockMsgIdOut` | uint32 list | No | Blocked outgoing message IDs |
| `AllowSrcCompOut` | uint8 list | No | Allowed outgoing component IDs |
| `BlockSrcCompOut` | uint8 list | No | Blocked outgoing component IDs |
| `AllowSrcSysOut` | uint8 list | No | Allowed outgoing system IDs |
| `BlockSrcSysOut` | uint8 list | No | Blocked outgoing system IDs |
| `AllowMsgIdIn` | uint32 list | No | Allowed incoming message IDs |
| `BlockMsgIdIn` | uint32 list | No | Blocked incoming message IDs |
| `AllowSrcCompIn` | uint8 list | No | Allowed incoming component IDs |
| `BlockSrcCompIn` | uint8 list | No | Blocked incoming component IDs |
| `AllowSrcSysIn` | uint8 list | No | Allowed incoming system IDs |
| `BlockSrcSysIn` | uint8 list | No | Blocked incoming system IDs |
| `group` | string | No | Endpoint group name for message routing |

### UDP Endpoints

UDP endpoints handle network communication in both client and server modes.

#### Section Pattern
```ini
[udpendpoint <name>]
```

#### Example Configuration
```ini
[udpendpoint groundstation]
address=192.168.1.100
port=14550
mode=client
AllowMsgIdOut=0,1,2,33
BlockMsgIdOut=150,151
AllowSrcCompOut=1,190
BlockSrcCompOut=200
AllowSrcSysOut=1,255
BlockSrcSysOut=100
AllowMsgIdIn=0,1,2,33
BlockMsgIdIn=150,151
AllowSrcCompIn=1,190
BlockSrcCompIn=200
AllowSrcSysIn=1,255
BlockSrcSysIn=100
group=groundstation
```

#### UDP Options

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `address` | string | Yes | IP address to bind/connect to |
| `port` | unsigned long | Conditional | UDP port number (required for server mode) |
| `mode` | string | No | Connection mode: "client" or "server" |
| `AllowMsgIdOut` | uint32 list | No | Allowed outgoing message IDs |
| `BlockMsgIdOut` | uint32 list | No | Blocked outgoing message IDs |
| `AllowSrcCompOut` | uint8 list | No | Allowed outgoing component IDs |
| `BlockSrcCompOut` | uint8 list | No | Blocked outgoing component IDs |
| `AllowSrcSysOut` | uint8 list | No | Allowed outgoing system IDs |
| `BlockSrcSysOut` | uint8 list | No | Blocked outgoing system IDs |
| `AllowMsgIdIn` | uint32 list | No | Allowed incoming message IDs |
| `BlockMsgIdIn` | uint32 list | No | Blocked incoming message IDs |
| `AllowSrcCompIn` | uint8 list | No | Allowed incoming component IDs |
| `BlockSrcCompIn` | uint8 list | No | Blocked incoming component IDs |
| `AllowSrcSysIn` | uint8 list | No | Allowed incoming system IDs |
| `BlockSrcSysIn` | uint8 list | No | Blocked incoming system IDs |
| `group` | string | No | Endpoint group name for message routing |

### TCP Endpoints

TCP endpoints handle TCP client connections to remote servers.

#### Section Pattern
```ini
[tcpendpoint <name>]
```

#### Example Configuration
```ini
[tcpendpoint telemetry_server]
address=telemetry.example.com
port=14550
```

#### TCP Options

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `address` | string | Yes | Server IP address or hostname |
| `port` | unsigned long | Yes | Server port number |

## Logging Configuration

### [Log] Section

Controls flight logging and telemetry recording.

```ini
[Log]
logs_dir=/var/log/mavlink
LogMode=while_armed
MavlinkDialect=Auto
LogSystemId=1
MinFreeSpace=1000000
MaxLogFiles=100
LogTelemetry=true
```

#### Logging Options

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `logs_dir` | string | - | Directory for log files |
| `LogMode` | string | always | Logging mode: "always", "while_armed", "disabled" |
| `MavlinkDialect` | string | Auto | MAVLink dialect: "Auto", "ArduPilot", "PX4" |
| `LogSystemId` | unsigned long | 1 | System ID for logging |
| `MinFreeSpace` | unsigned long | 0 | Minimum free disk space in bytes |
| `MaxLogFiles` | unsigned long | 0 | Maximum number of log files (0 = unlimited) |
| `LogTelemetry` | boolean | false | Enable telemetry logging (.tlog files) |

## Data Types

### Boolean Values
- `true`, `false` (case-insensitive)
- `1`, `0` (numeric)

### Numeric Lists
Comma-separated values:
```ini
baud=57600,115200,921600
AllowMsgIdOut=0,1,2,33,36
```

### String Values
Plain text, no quotes needed:
```ini
device=/dev/ttyUSB0
address=192.168.1.100
```

## Message Filtering

### Filter Logic

- **Allow filters**: If specified, only listed items are allowed
- **Block filters**: Listed items are explicitly blocked
- **Priority**: Allow filters take precedence over block filters
- **Direction**: Separate filters for incoming (`In`) and outgoing (`Out`) messages

### Filter Types

1. **Message ID Filters**: Filter by MAVLink message type
2. **Source Component Filters**: Filter by component ID
3. **Source System Filters**: Filter by system ID

### Example Filtering Configuration
```ini
[uartendpoint flight_controller]
device=/dev/ttyUSB0
# Only allow heartbeat, system status, and attitude messages
AllowMsgIdOut=0,1,30
# Block parameter messages from going out
BlockMsgIdOut=20,21,22,23
# Only accept messages from autopilot component
AllowSrcCompIn=1
# Block messages from system ID 100
BlockSrcSysIn=100
```

## Configuration File Locations

### Default Locations
- Main config: `/etc/mavlink-router/main.conf`
- Config directory: `/etc/mavlink-router/config.d/`

### Environment Variables
- `MAVLINK_ROUTERD_CONF_FILE`: Override main config file path
- `MAVLINK_ROUTERD_CONF_DIR`: Override config directory path

### Command Line Override
```bash
# INI configuration
ur-mavrouter --conf-file /custom/path/config.conf --conf-dir /custom/config/dir

# JSON configuration
ur-mavrouter --json-conf-file /custom/path/config.json

# Both formats (JSON takes precedence for overlapping settings)
ur-mavrouter --conf-file /custom/path/config.conf --json-conf-file /custom/path/config.json
```

## Complete Example Configuration

### INI Format Example

```ini
# /etc/mavlink-router/main.conf

[General]
TcpServerPort=5760
ReportStats=true
DebugLogLevel=info
DeduplicationPeriod=100

[Log]
logs_dir=/var/log/mavlink
LogMode=while_armed
LogTelemetry=true

[uartendpoint pixhawk]
device=/dev/ttyUSB0
baud=57600,115200,921600
FlowControl=false
group=autopilot

[udpendpoint groundstation]
address=192.168.1.100
port=14550
mode=client
group=gcs

[udpendpoint qgc]
address=192.168.1.200
port=14551
mode=client
group=gcs

[tcpendpoint cloud_telemetry]
address=telemetry.example.com
port=14550
```

### JSON Format Example

```json
{
  "general": {
    "tcp_server_port": 5760,
    "report_stats": true,
    "debug_log_level": "info",
    "deduplication_period": 100
  },
  "log": {
    "logs_dir": "/var/log/mavlink",
    "log_mode": "while_armed",
    "log_telemetry": true
  },
  "uart_endpoints": [
    {
      "name": "pixhawk",
      "device": "/dev/ttyUSB0",
      "baud": "57600,115200,921600",
      "flow_control": false,
      "group": "autopilot"
    }
  ],
  "udp_endpoints": [
    {
      "name": "groundstation",
      "address": "192.168.1.100",
      "port": 14550,
      "mode": "client",
      "group": "gcs"
    },
    {
      "name": "qgc",
      "address": "192.168.1.200",
      "port": 14551,
      "mode": "client",
      "group": "gcs"
    }
  ],
  "tcp_endpoints": [
    {
      "name": "cloud_telemetry",
      "address": "telemetry.example.com",
      "port": 14550
    }
  ]
}
```

## JSON Configuration Advantages

- **Structured Data**: Natural support for arrays and nested objects
- **Validation**: Easier to validate and parse programmatically
- **Modern Format**: Widely supported by development tools and editors
- **API Integration**: Easy to generate from web interfaces or APIs
- **Type Safety**: Better type checking and validation support

## Configuration Format Priority

When both INI and JSON configuration files are specified:

1. JSON configuration is parsed first
2. INI configuration is parsed second (may override JSON settings)
3. Command-line arguments override both file formats
4. The most specific configuration wins for each setting

## Validation Rules

1. **Required Fields**: Some fields are mandatory (marked as required)
2. **Port Ranges**: Port numbers must be valid (1-65535)
3. **IP Addresses**: Must be valid IPv4/IPv6 addresses
4. **File Paths**: Device paths must exist and be accessible
5. **Baud Rates**: Must be standard serial baud rates
6. **Log Levels**: Must be one of the defined levels

## Best Practices

1. **Use Groups**: Organize endpoints into logical groups for efficient routing
2. **Filter Messages**: Use filtering to reduce unnecessary traffic
3. **Monitor Logs**: Enable statistics and appropriate log levels for troubleshooting
4. **Backup Configuration**: Keep copies of working configurations
5. **Test Changes**: Validate configuration changes in safe environments
6. **Document Custom Settings**: Comment custom configurations for team members

## Troubleshooting

### Common Issues
1. **Permission Errors**: Ensure ur-mavrouter has access to serial devices
2. **Port Conflicts**: Check for other services using the same ports
3. **Network Issues**: Verify IP addresses and network connectivity
4. **Syntax Errors**: Check for proper INI format and valid values

### Debug Configuration
```ini
[General]
DebugLogLevel=debug
ReportStats=true
```

This enables detailed logging to help diagnose configuration and runtime issues.
