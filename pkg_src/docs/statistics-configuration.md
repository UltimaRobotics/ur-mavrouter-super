
# Statistics Configuration

The statistics system in ur-mavrouter can be configured via JSON configuration files to enable/disable specific statistics categories and control collection timing.

## Statistics Categories

The following statistics categories can be individually enabled or disabled:

- **connection_health**: Connection state, uptime, reconnection attempts
- **message_stats**: Message rates, protocol versions, error counts  
- **performance_metrics**: Latency, buffer utilization, processing times
- **filtering_stats**: Message filtering and acceptance rates
- **resource_stats**: Memory usage, file descriptors, CPU time
- **uart_stats**: UART-specific metrics (baudrate, hardware errors)
- **udp_stats**: UDP-specific metrics (packet loss, socket errors)
- **tcp_stats**: TCP-specific metrics (retransmissions, connection duration)

## Timing Configuration

Various timing intervals can be configured:

- **periodic_collection_interval_ms**: How often statistics are updated (default: 5000ms)
- **error_cleanup_interval_ms**: How often old errors are cleaned up (default: 60000ms)
- **statistics_report_interval_ms**: How often statistics are reported to logs (default: 30000ms)
- **resource_check_interval_ms**: How often resource limits are checked (default: 10000ms)

## JSON File Output Configuration

Statistics can be continuously written to a JSON file for external monitoring and analysis:

- **enable_json_file_output**: Enable/disable JSON file output (default: false)
- **json_output_file_path**: Path to the JSON output file (required if enabled)
- **json_file_write_interval_ms**: How often to write statistics to the JSON file (default: 10000ms)

## JSON Configuration Format

Add a `statistics` or `stats` section to your JSON configuration:

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
    "resource_check_interval_ms": 10000,
    "enable_json_file_output": true,
    "json_output_file_path": "/var/log/mavlink-router-stats.json",
    "json_file_write_interval_ms": 10000
  }
}
```

### JSON File Output Example

When JSON file output is enabled, the system will write detailed statistics to the specified file at regular intervals. The output includes timestamps, all enabled statistics categories, and comprehensive endpoint information.

Example output structure:
```json
{
  "timestamp": 1705123456,
  "endpoint_name": "uart_endpoint_0",
  "enabled_categories": {
    "connection_health": true,
    "message_stats": true,
    "performance_metrics": true
  },
  "connection_health": {
    "state": "CONNECTED",
    "stability_ratio": 98.5,
    "current_uptime_ms": 125000
  },
  "message_stats": {
    "message_rate": 25.3,
    "byte_rate": 1024.7,
    "protocol_v2_ratio": 95.2
  },
  "performance": {
    "avg_latency_us": 150.2,
    "rx_buffer_utilization": 45.0,
    "tx_buffer_utilization": 23.1
  }
}
```

## Performance Impact

Disabling unused statistics categories can improve performance, especially in high-throughput scenarios. Consider disabling categories that are not relevant to your use case.

## Compatibility

If no statistics configuration is provided, all categories default to enabled with standard timing intervals. This ensures backward compatibility with existing configurations.
