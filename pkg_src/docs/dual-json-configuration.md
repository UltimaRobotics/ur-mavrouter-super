
# Dual JSON Configuration

ur-mavrouter now supports separating router configuration from statistics configuration using two dedicated JSON files.

## Overview

The system can be configured using two separate JSON configuration files:

1. **Router Configuration File** (`--json-conf-file`): Contains all router-related settings including endpoints, logging, and general configuration
2. **Statistics Configuration File** (`--stats-conf-file`): Contains only statistics system configuration

This separation allows for:
- **Modular Configuration**: Statistics settings can be managed independently
- **Role-based Configuration**: Different teams can manage different aspects
- **Deployment Flexibility**: Statistics configuration can be updated without touching router settings
- **Configuration Reuse**: Statistics configurations can be shared across different router setups

## Command Line Usage

```bash
# Use both configuration files
ur-mavrouter --json-conf-file router-config.json --stats-conf-file stats-config.json

# Use only router configuration (statistics use defaults)
ur-mavrouter --json-conf-file router-config.json

# Use only statistics configuration (router uses defaults/INI config)
ur-mavrouter --stats-conf-file stats-config.json

# Mix with traditional INI configuration
ur-mavrouter --conf-file main.conf --stats-conf-file stats-config.json
```

## Configuration Precedence

Configuration is loaded in the following order (later entries override earlier ones):

1. INI configuration files (main.conf + config.d/)
2. Router JSON configuration file (`--json-conf-file`)
3. Statistics JSON configuration file (`--stats-conf-file`)
4. Command-line arguments

## Router Configuration File Format

The router configuration file contains all non-statistics settings:

```json
{
  "general": {
    "tcp_server_port": 5760,
    "report_stats": true,
    "debug_log_level": "info",
    "deduplication_period": 100,
    "sniffer_sysid": 0
  },
  "log": {
    "logs_dir": "/tmp/mavlink-logs",
    "log_mode": "while_armed",
    "mavlink_dialect": "ArduPilot",
    "log_system_id": 1,
    "min_free_space": 100,
    "max_log_files": 10,
    "log_telemetry": true
  },
  "uart_endpoints": [...],
  "udp_endpoints": [...],
  "tcp_endpoints": [...]
}
```

## Statistics Configuration File Format

The statistics configuration file contains only statistics-related settings:

```json
{
  "statistics": {
    "enable_connection_health": true,
    "enable_message_stats": true,
    "enable_performance_metrics": true,
    "enable_filtering_stats": false,
    "enable_resource_stats": true,
    "enable_uart_stats": true,
    "enable_udp_stats": true,
    "enable_tcp_stats": true,
    "periodic_collection_interval_ms": 5000,
    "error_cleanup_interval_ms": 60000,
    "statistics_report_interval_ms": 30000,
    "resource_check_interval_ms": 10000
  }
}
```

## Benefits

### Separation of Concerns
- **Router Configuration**: Network topology, endpoints, routing rules
- **Statistics Configuration**: Monitoring, metrics collection, performance tuning

### Operational Flexibility
- Update statistics collection parameters without touching router configuration
- Deploy different statistics configurations for different environments (dev/prod)
- Share statistics configurations across multiple router instances

### Security and Access Control
- Different teams can have access to different configuration aspects
- Statistics configuration can be managed by monitoring teams
- Router configuration can be managed by network operations teams

## Backward Compatibility

The system remains fully backward compatible:
- Existing INI configuration files continue to work
- Single JSON configuration files (with all sections) continue to work
- Command-line arguments continue to override all file-based configuration

## Example Usage Scenarios

### Development Environment
```bash
# Use simplified router config with detailed statistics
ur-mavrouter --json-conf-file dev-router.json --stats-conf-file detailed-stats.json
```

### Production Environment
```bash
# Use production router config with optimized statistics
ur-mavrouter --json-conf-file prod-router.json --stats-conf-file prod-stats.json
```

### Statistics Tuning
```bash
# Keep existing router config, experiment with statistics settings
ur-mavrouter --conf-file /etc/mavlink-router/main.conf --stats-conf-file experimental-stats.json
```
