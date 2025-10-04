# Thread Management API Structure Documentation

This document provides a comprehensive overview of the structures and types used in the Thread Management API, along with examples of how to interact with them.

## Core Structures

### `thread_manager_t`

The central structure for managing threads and processes.

```c
typedef struct {
    thread_info_t **threads;    // Array of thread info pointers
    unsigned int thread_count;  // Number of active threads
    unsigned int capacity;      // Total capacity of threads array
    unsigned int next_id;       // Next thread ID to assign
    pthread_mutex_t mutex;      // Mutex for thread-safe operations
} thread_manager_t;
```

### `thread_info_t`

Contains information about a thread or process.

```c
typedef struct {
    unsigned int id;             // Unique identifier
    pthread_t thread_id;         // POSIX thread ID
    thread_func_t func;          // Function pointer for normal threads
    void *arg;                   // Arguments for thread function
    thread_state_t state;        // Current thread state
    bool is_paused;              // Flag to indicate if thread is paused
    bool should_exit;            // Flag to indicate if thread should exit
    pthread_mutex_t mutex;       // Mutex for thread-safe operations
    pthread_cond_t cond;         // Condition variable for pause/resume
    thread_type_t type;          // Type of thread (normal or process)
    
    // Process-specific fields
    char *command;               // Command to execute (for process threads)
    char **args;                 // Command arguments (for process threads)
    pid_t process_id;            // Process ID (for process threads)
    int exit_status;             // Exit status (for process threads)
    int stdin_pipe[2];           // Pipe for stdin
    int stdout_pipe[2];          // Pipe for stdout
    int stderr_pipe[2];          // Pipe for stderr
} thread_info_t;
```

### Enumerations

#### `thread_state_t`

Represents the state of a thread.

```c
typedef enum {
    THREAD_CREATED = 0,  // Thread is created but not yet started
    THREAD_RUNNING,      // Thread is running
    THREAD_PAUSED,       // Thread is paused
    THREAD_STOPPED,      // Thread has stopped/completed
    THREAD_ERROR         // Thread encountered an error
} thread_state_t;
```

#### `thread_type_t`

Indicates whether the thread executes a function or a system binary.

```c
typedef enum {
    THREAD_TYPE_NORMAL = 0,  // Normal function thread
    THREAD_TYPE_PROCESS      // Process execution thread
} thread_type_t;
```

### Type Definitions

```c
// Function pointer type for thread functions
typedef void *(*thread_func_t)(void *);
```

## API Usage Examples

### Basic Thread Management

#### Initializing the Thread Manager

```c
thread_manager_t manager;
if (thread_manager_init(&manager, 10) != 0) {
    // Error handling
    return -1;
}
```

#### Creating a Thread

```c
void *worker_thread(void *arg) {
    int *value = (int *)arg;
    printf("Worker thread with value: %d\n", *value);
    // Do work...
    return NULL;
}

// Create thread
int *value = malloc(sizeof(int));
*value = 42;
unsigned int thread_id;
if (thread_create(&manager, worker_thread, value, &thread_id) <= 0) {
    // Error handling
    free(value);
    return -1;
}
```

#### Managing Thread Lifecycle

```c
// Pause a thread
thread_pause(&manager, thread_id);

// Resume a thread
thread_resume(&manager, thread_id);

// Stop a thread
thread_stop(&manager, thread_id);

// Restart a thread with new arguments
int *new_value = malloc(sizeof(int));
*new_value = 100;
thread_restart(&manager, thread_id, new_value);
```

#### Thread Information and Status

```c
// Get thread count
unsigned int count = thread_get_count(&manager);

// Get all thread IDs
unsigned int *ids = malloc(count * sizeof(unsigned int));
int num_ids = thread_get_all_ids(&manager, ids, count);

// Check if thread is alive
bool alive = thread_is_alive(&manager, thread_id);

// Get thread state
thread_state_t state;
thread_get_state(&manager, thread_id, &state);

// Get detailed thread info
thread_info_t info;
thread_get_info(&manager, thread_id, &info);
```

#### Cleanup

```c
// Destroy the thread manager (stops all threads)
thread_manager_destroy(&manager);
```

### Process Management

#### Running a System Binary

```c
// Run 'ls -la' command
char *args[] = {"ls", "-la", NULL};
unsigned int process_id;
if (thread_create_process(&manager, "ls", args, &process_id) <= 0) {
    // Error handling
    return -1;
}
```

#### Process I/O

```c
// Write to process stdin
const char *input = "some input\n";
thread_write_to_process(&manager, process_id, input, strlen(input));

// Read from process stdout
char buffer[1024];
int bytes_read = thread_read_from_process(&manager, process_id, buffer, sizeof(buffer) - 1);
if (bytes_read > 0) {
    buffer[bytes_read] = '\0';
    printf("Process output: %s\n", buffer);
}

// Read from process stderr
bytes_read = thread_read_error_from_process(&manager, process_id, buffer, sizeof(buffer) - 1);
if (bytes_read > 0) {
    buffer[bytes_read] = '\0';
    printf("Process error: %s\n", buffer);
}
```

### JSON Configuration

#### Creating a Thread from JSON

```c
// Thread JSON format
const char *thread_json = 
    "{"
    "  \"type\": \"thread\","
    "  \"function\": \"worker_thread\","
    "  \"args\": {"
    "    \"iterations\": 5,"
    "    \"delay\": 1000"
    "  }"
    "}";

unsigned int thread_id;
thread_create_from_json(&manager, thread_json, &thread_id);
```

#### Creating a Process from JSON

```c
// Process JSON format
const char *process_json = 
    "{"
    "  \"type\": \"process\","
    "  \"command\": \"ls\","
    "  \"args\": [\"-la\", \"/tmp\"]"
    "}";

unsigned int process_id;
thread_create_process_from_json(&manager, process_json, &process_id);
```

#### Managing Configuration

```c
// Get JSON configuration for a thread
char *config = thread_get_json_config(&manager, thread_id);
printf("Thread config: %s\n", config);
free(config);

// Update a thread from JSON
const char *update_json = "{\"state\":\"paused\"}";
thread_update_from_json(&manager, thread_id, update_json);

// Save all thread configurations to a file
thread_manager_save_config(&manager, "threads.json");

// Load thread configurations from a file
thread_manager_load_config(&manager, "threads.json");
```

## Using the API in Your Project

1. Include the necessary headers:
   ```c
   #include "thread_manager.h"
   #include "json_config.h"  // If using JSON configuration
   ```

2. Link with the library:
   ```
   gcc -o your_program your_program.c -I./include -L./build -lthreadmanager -pthread
   ```

3. Initialize the thread manager at program start:
   ```c
   thread_manager_t manager;
   thread_manager_init(&manager, 10);
   ```

4. Create and manage threads as needed using the API functions.

5. Clean up when done:
   ```c
   thread_manager_destroy(&manager);
   ```

This API provides a flexible and powerful way to manage threads and processes in C applications, with support for dynamic thread creation, execution control, and JSON-based configuration.