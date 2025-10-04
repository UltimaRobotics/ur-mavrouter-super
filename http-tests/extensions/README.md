
# Extension Management HTTP Tests

This directory contains HTTP client test scripts for testing the MAVLink Router extension management endpoints.

## Overview

The extension management system allows you to dynamically add, remove, start, stop, and monitor MAVLink extensions through HTTP API endpoints. These test scripts validate the functionality of each endpoint.

## Prerequisites

- MAVLink Router must be running with HTTP server enabled
- Python 3.x with `requests` library installed
- HTTP server running on `http://0.0.0.0:5000` (default)

Install dependencies:
```bash
pip3 install requests
```

## Test Scripts

### 1. Extension Status (`test_extension_status.py`)

Check the status of extensions (all or specific).

**Usage:**
```bash
# Get status of all extensions
python3 test_extension_status.py --all

# Get status of a specific extension
python3 test_extension_status.py --name my_extension

# Default behavior (shows all)
python3 test_extension_status.py
```

**Arguments:**
- `--name <extension_name>`: Name of the extension to check status for
- `--all`: Get status of all extensions (default if no name specified)

**Example:**
```bash
python3 test_extension_status.py --name test_extension_1
```

### 2. Add Extension (`test_extension_add.py`)

Add a new extension to the router.

**Usage:**
```bash
python3 test_extension_add.py \
  --name <extension_name> \
  --type <udp|tcp> \
  --address <ip_address> \
  --port <port_number> \
  --extension-point <extension_point_name>
```

**Arguments:**
- `--name`: Extension name (default: `test_extension_1`)
- `--type`: Extension type - `udp` or `tcp` (default: `udp`)
- `--address`: IP address (default: `127.0.0.1`)
- `--port`: Port number (default: `44100`)
- `--extension-point`: Assigned extension point name (default: `udp-extension-point-1`)
- `--test-invalid`: Also test invalid configuration (optional flag)

**Examples:**
```bash
# Add UDP extension with defaults
python3 test_extension_add.py

# Add TCP extension with custom parameters
python3 test_extension_add.py \
  --name my_tcp_ext \
  --type tcp \
  --address 192.168.1.100 \
  --port 5761 \
  --extension-point tcp-extension-point-1

# Test with invalid config validation
python3 test_extension_add.py --test-invalid
```

### 3. Start Extension (`test_extension_start.py`)

Start a previously stopped extension.

**Usage:**
```bash
python3 test_extension_start.py --name <extension_name>
```

**Arguments:**
- `--name`: Extension name to start (default: `test_extension_1`)

**Example:**
```bash
python3 test_extension_start.py --name my_extension
```

### 4. Stop Extension (`test_extension_stop.py`)

Stop a running extension.

**Usage:**
```bash
python3 test_extension_stop.py --name <extension_name>
```

**Arguments:**
- `--name`: Extension name to stop (default: `test_extension_1`)

**Example:**
```bash
python3 test_extension_stop.py --name my_extension
```

### 5. Delete Extension (`test_extension_delete.py`)

Remove an extension from the router.

**Usage:**
```bash
python3 test_extension_delete.py --name <extension_name>
```

**Arguments:**
- `--name`: Extension name to delete (default: `test_extension_1`)

**Example:**
```bash
python3 test_extension_delete.py --name my_extension
```

### 6. Complete Workflow Test (`test_extension_workflow.py`)

Tests the complete extension lifecycle: add → status → stop → start → delete

**Usage:**
```bash
python3 test_extension_workflow.py
```

No arguments needed. This script runs through the entire workflow automatically.

## Running All Tests

Use the provided shell script to run all extension tests:

```bash
./run_extension_tests.sh
```

This script will:
1. Check if the HTTP server is running on port 5000
2. Run all individual test scripts in sequence
3. Run the comprehensive workflow test
4. Display a summary of results

## API Endpoints Reference

### GET `/api/extensions/status`
Get status of all extensions

### GET `/api/extensions/status/:name`
Get status of a specific extension

### POST `/api/extensions/add`
Add a new extension
```json
{
  "name": "extension_name",
  "type": "udp",
  "address": "127.0.0.1",
  "port": 44100,
  "assigned_extension_point": "udp-extension-point-1"
}
```

### POST `/api/extensions/start/:name`
Start an extension

### POST `/api/extensions/stop/:name`
Stop an extension

### DELETE `/api/extensions/delete/:name`
Delete an extension

## Expected Responses

### Success (200/201)
```json
{
  "status": "success",
  "message": "Operation completed successfully"
}
```

### Not Found (404)
```json
{
  "error": "Extension not found"
}
```

### Bad Request (400)
```json
{
  "error": "Invalid configuration or parameters"
}
```

## Typical Workflow

1. **Add an extension:**
   ```bash
   python3 test_extension_add.py --name my_ext --port 44100
   ```

2. **Check its status:**
   ```bash
   python3 test_extension_status.py --name my_ext
   ```

3. **Stop it temporarily:**
   ```bash
   python3 test_extension_stop.py --name my_ext
   ```

4. **Restart it:**
   ```bash
   python3 test_extension_start.py --name my_ext
   ```

5. **Remove it when done:**
   ```bash
   python3 test_extension_delete.py --name my_ext
   ```

## Troubleshooting

### Server Not Running
```
❌ ERROR: HTTP server is not running on port 5000
```
**Solution:** Start the MAVLink Router with HTTP server enabled:
```bash
cd pkg_src && make && ./build/ur-mavrouter \
  -j config/router-config.json \
  -H config/http-server-config.json
```

### Connection Refused
**Solution:** Ensure the HTTP server configuration uses `0.0.0.0` as the address in `config/http-server-config.json`

### Extension Already Exists (400)
**Solution:** Delete the existing extension first or use a different name

### Extension Not Found (404)
**Solution:** Check that the extension was added successfully and the name is correct

## Notes

- Extension names must be unique
- Extensions must be assigned to valid extension points configured in the router
- UDP extensions use client mode by default
- TCP extensions attempt to connect to the specified address:port
- Extension configuration is stored in the directory specified by `extension_conf_dir` in the router configuration

## Configuration Location

Extension configurations are stored in: `pkg_src/config/` (or as specified in `router-config.json` under `extension_conf_dir`)

## See Also

- [Main HTTP Tests Documentation](../README.md)
- [Extension Workflow Test](test_extension_workflow.py)
- [Run All Tests Script](run_extension_tests.sh)
