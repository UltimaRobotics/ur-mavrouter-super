
#!/bin/bash

# MAVLink Endpoint Monitoring Test Runner

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${1:-$SCRIPT_DIR/config/endpoints.json}"

echo "MAVLink Endpoint Connectivity Test"
echo "==================================="
echo ""

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "ERROR: Configuration file not found: $CONFIG_FILE"
    echo ""
    echo "Usage: $0 [config_file.json]"
    exit 1
fi

echo "Using configuration: $CONFIG_FILE"
echo ""

# Run the monitor
python3 "$SCRIPT_DIR/endpoint_monitor.py" "$CONFIG_FILE"

exit $?
