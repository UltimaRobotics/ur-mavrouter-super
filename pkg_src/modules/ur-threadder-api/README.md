# Thread Management API

A comprehensive C-based thread and process management API that enables dynamic creation, monitoring, and lifecycle control of both threads and system processes. The API now supports JSON-based configuration for thread and process management.

## Features

- Create threads dynamically without interrupting existing threads
- Control thread lifecycle (stop, pause, resume, restart)
- Monitor thread status and state
- Support passing new arguments when restarting threads
- Execute system binaries as managed threads
- Control process lifecycle (stop, pause, resume)
- Interact with processes via stdin, stdout, and stderr
- Thread-safe implementation with proper synchronization
- Comprehensive error handling
- **JSON Configuration Support**:
  - Create threads and processes from JSON configuration
  - Export thread and process configuration as JSON
  - Update thread and process parameters via JSON
  - Save and load configuration to/from JSON files

## API Overview

The Thread Management API provides the following core functionality:

### Thread Manager Initialization

```c
int thread_manager_init(thread_manager_t *manager, unsigned int initial_capacity);
int thread_manager_destroy(thread_manager_t *manager);
```

### Thread Creation and Control

```c
int thread_create(thread_manager_t *manager, void *(*func)(void *), void *arg, unsigned int *thread_id);
int thread_stop(thread_manager_t *manager, unsigned int thread_id);
int thread_pause(thread_manager_t *manager, unsigned int thread_id);
int thread_resume(thread_manager_t *manager, unsigned int thread_id);
int thread_restart(thread_manager_t *manager, unsigned int thread_id, void *new_arg);
```

### Thread Status and Monitoring

```c
int thread_get_state(thread_manager_t *manager, unsigned int thread_id, thread_state_t *state);
unsigned int thread_get_count(thread_manager_t *manager);
int thread_get_info(thread_manager_t *manager, unsigned int thread_id, thread_info_t *info);
bool thread_is_alive(thread_manager_t *manager, unsigned int thread_id);
int thread_join(thread_manager_t *manager, unsigned int thread_id, void **result);
int thread_get_all_ids(thread_manager_t *manager, unsigned int *ids, unsigned int size);
```

### System Binary Execution

```c
// Execute a system binary as a managed thread
int thread_create_process(thread_manager_t *manager, const char *command, char **args, unsigned int *thread_id);

// Interact with process I/O
int thread_write_to_process(thread_manager_t *manager, unsigned int thread_id, const void *data, size_t size);
int thread_read_from_process(thread_manager_t *manager, unsigned int thread_id, void *buffer, size_t size);
int thread_read_error_from_process(thread_manager_t *manager, unsigned int thread_id, void *buffer, size_t size);

// Get process exit status
int thread_get_exit_status(thread_manager_t *manager, unsigned int thread_id, int *exit_status);
```

### JSON Configuration API

```c
// Create thread from JSON configuration
int thread_create_from_json(thread_manager_t *manager, const char *json_config, unsigned int *thread_id);

// Create process from JSON configuration
int thread_create_process_from_json(thread_manager_t *manager, const char *json_config, unsigned int *thread_id);

// Get thread configuration as JSON
char *thread_get_json_config(thread_manager_t *manager, unsigned int thread_id);

// Update thread based on JSON configuration
int thread_update_from_json(thread_manager_t *manager, unsigned int thread_id, const char *json_config);

// Save all thread configurations to a JSON file
int thread_manager_save_config(const thread_manager_t *manager, const char *filename);

// Load thread configurations from a JSON file
int thread_manager_load_config(thread_manager_t *manager, const char *filename);
```

## Thread Types

The API supports two thread types:

- **THREAD_TYPE_NORMAL**: Regular threads executing a function
- **THREAD_TYPE_PROCESS**: Threads executing system binaries

## Thread States

A thread or process can be in one of the following states:

- **THREAD_CREATED**: Thread is created but not started
- **THREAD_RUNNING**: Thread is currently running
- **THREAD_PAUSED**: Thread is paused
- **THREAD_STOPPED**: Thread is stopped
- **THREAD_ERROR**: Thread encountered an error

## JSON Configuration Format

### Thread Configuration

```json
{
  "type": "thread",
  "function": "worker_thread",
  "args": {
    "iterations": 5,
    "param1": "value1",
    "param2": 42
  }
}
```

### Process Configuration

```json
{
  "type": "process",
  "command": "ls",
  "args": ["-la", "/tmp"]
}
```

### Thread Status Update

```json
{
  "state": "paused"
}
```

or 

```json
{
  "state": "restart",
  "args": {
    "iterations": 10
  }
}
```

### Configuration File Format

```json
{
  "threads": [
    {
      "type": "thread",
      "id": 1,
      "function": "worker_thread",
      "args": {
        "iterations": 5
      }
    },
    {
      "type": "process",
      "id": 2,
      "command": "ls",
      "args": ["-la"]
    }
  ]
}
```

## Usage Examples

See the example directory for detailed usage examples:

- `example.c`: Basic thread management example
- `process_example.c`: System binary execution example
- `json_config_example.c`: JSON configuration example

## Building and Running

```bash
# Build the library and examples
make

# Run the regular thread example
./bin/example

# Run the process management example
./bin/process_example

# Run the JSON configuration example
./bin/json_config_example
```

## Integration Use Cases

The Thread Management API with JSON configuration can be used for:

1. **Service Management**: Create and control multiple service threads from configuration files
2. **Task Orchestration**: Coordinate multiple independent tasks with different priorities
3. **Process Supervision**: Launch and manage external processes with I/O redirection
4. **Dynamic System Configuration**: Update system behavior at runtime through JSON
5. **Configuration-Driven Applications**: Build applications that can change their thread architecture without recompilation
