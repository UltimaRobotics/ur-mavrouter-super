/**
 * @file thread_manager.h
 * @brief Thread management API for dynamic thread creation and lifecycle control
 * 
 * This API provides functions to create, monitor, and control threads.
 * It allows operations like stopping, pausing, and restarting threads,
 * as well as providing status information about running threads.
 * It also supports executing and managing system binaries.
 */

#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * @brief Thread states
 */
typedef enum {
    THREAD_CREATED,  /**< Thread is created but not started */
    THREAD_RUNNING,  /**< Thread is currently running */
    THREAD_PAUSED,   /**< Thread is paused */
    THREAD_STOPPED,  /**< Thread is stopped */
    THREAD_ERROR     /**< Thread encountered an error */
} thread_state_t;

/**
 * @brief Thread types
 */
typedef enum {
    THREAD_TYPE_NORMAL,  /**< Normal thread executing a function */
    THREAD_TYPE_PROCESS  /**< Thread executing a system binary */
} thread_type_t;

/**
 * @brief Thread information structure
 */
typedef struct {
    pthread_t thread_id;        /**< Thread ID */
    void *(*func)(void *);      /**< Thread function */
    void *arg;                  /**< Thread function arguments */
    pthread_mutex_t mutex;      /**< Mutex for thread synchronization */
    pthread_cond_t cond;        /**< Condition variable for thread synchronization */
    thread_state_t state;       /**< Thread state */
    unsigned int id;            /**< Unique identifier for the thread */
    bool should_exit;           /**< Flag to indicate if thread should exit */
    bool is_paused;             /**< Flag to indicate if thread is paused */
    thread_type_t type;         /**< Thread type (normal or process) */
    pid_t process_id;           /**< Process ID for binary execution */
    char *command;              /**< Command to execute for binary threads */
    char **args;                /**< Arguments for the command */
    int exit_status;            /**< Exit status of the process */
    int stdout_pipe[2];         /**< Pipe for stdout of the process */
    int stderr_pipe[2];         /**< Pipe for stderr of the process */
    int stdin_pipe[2];          /**< Pipe for stdin of the process */
} thread_info_t;

/**
 * @brief Thread registration structure for tracking threads by attachment ID
 */
typedef struct {
    char *attachment_arg;       /**< Attachment identifier string */
    unsigned int thread_id;     /**< Associated thread ID */
} thread_registration_t;

/**
 * @brief Thread manager structure
 */
typedef struct {
    thread_info_t **threads;    /**< Array of thread information structures */
    unsigned int thread_count;  /**< Number of threads */
    unsigned int capacity;      /**< Capacity of the threads array */
    pthread_mutex_t mutex;      /**< Mutex for thread manager synchronization */
    unsigned int next_id;       /**< Next thread ID */
    thread_registration_t **registrations; /**< Array of thread registrations */
    unsigned int registration_count;        /**< Number of registrations */
    unsigned int registration_capacity;     /**< Capacity of registrations array */
} thread_manager_t;

/**
 * @brief Initialize thread manager
 * 
 * @param manager Pointer to thread manager structure
 * @param initial_capacity Initial capacity of the threads array
 * @return int 0 on success, -1 on failure
 */
int thread_manager_init(thread_manager_t *manager, unsigned int initial_capacity);

/**
 * @brief Destroy thread manager and free resources
 * 
 * @param manager Pointer to thread manager structure
 * @return int 0 on success, -1 on failure
 */
int thread_manager_destroy(thread_manager_t *manager);

/**
 * @brief Create and start a new thread
 * 
 * @param manager Pointer to thread manager structure
 * @param func Thread function
 * @param arg Thread function arguments
 * @param thread_id Pointer to store the thread ID
 * @return int Thread ID on success, -1 on failure
 */
int thread_create(thread_manager_t *manager, void *(*func)(void *), void *arg, unsigned int *thread_id);

/**
 * @brief Stop a thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @return int 0 on success, -1 on failure
 */
int thread_stop(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Pause a thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @return int 0 on success, -1 on failure
 */
int thread_pause(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Resume a paused thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @return int 0 on success, -1 on failure
 */
int thread_resume(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Restart a thread with new arguments
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param new_arg New thread function arguments
 * @return int 0 on success, -1 on failure
 */
int thread_restart(thread_manager_t *manager, unsigned int thread_id, void *new_arg);

/**
 * @brief Get thread state
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param state Pointer to store the thread state
 * @return int 0 on success, -1 on failure
 */
int thread_get_state(thread_manager_t *manager, unsigned int thread_id, thread_state_t *state);

/**
 * @brief Get the number of threads
 * 
 * @param manager Pointer to thread manager structure
 * @return unsigned int Number of threads
 */
unsigned int thread_get_count(thread_manager_t *manager);

/**
 * @brief Get thread information
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param info Pointer to store the thread information
 * @return int 0 on success, -1 on failure
 */
int thread_get_info(thread_manager_t *manager, unsigned int thread_id, thread_info_t *info);

/**
 * @brief Check if thread is still alive
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @return bool true if thread is alive, false otherwise
 */
bool thread_is_alive(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Wait for thread to complete
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param result Pointer to store the thread result
 * @return int 0 on success, -1 on failure
 */
int thread_join(thread_manager_t *manager, unsigned int thread_id, void **result);

/**
 * @brief Get a list of all thread IDs
 * 
 * @param manager Pointer to thread manager structure
 * @param ids Array to store the thread IDs
 * @param size Size of the array
 * @return int Number of thread IDs stored in the array
 */
int thread_get_all_ids(thread_manager_t *manager, unsigned int *ids, unsigned int size);

/**
 * @brief Helper function for thread functions to check if they should exit or pause
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @return int 1 if thread should exit, 0 otherwise
 */
int thread_should_exit(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Helper function for thread functions to check and handle pause
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 */
void thread_check_pause(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Create and execute a system binary as a thread
 * 
 * @param manager Pointer to thread manager structure
 * @param command Command to execute
 * @param args Arguments for the command (NULL-terminated array)
 * @param thread_id Pointer to store the thread ID
 * @return int Thread ID on success, -1 on failure
 */
int thread_create_process(thread_manager_t *manager, const char *command, char **args, unsigned int *thread_id);

/**
 * @brief Write data to the stdin of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param data Data to write
 * @param size Size of data
 * @return int Number of bytes written on success, -1 on failure
 */
int thread_write_to_process(thread_manager_t *manager, unsigned int thread_id, const void *data, size_t size);

/**
 * @brief Read data from the stdout of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param buffer Buffer to store the data
 * @param size Size of buffer
 * @return int Number of bytes read on success, -1 on failure
 */
int thread_read_from_process(thread_manager_t *manager, unsigned int thread_id, void *buffer, size_t size);

/**
 * @brief Read data from the stderr of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param buffer Buffer to store the data
 * @param size Size of buffer
 * @return int Number of bytes read on success, -1 on failure
 */
int thread_read_error_from_process(thread_manager_t *manager, unsigned int thread_id, void *buffer, size_t size);

/**
 * @brief Get the exit status of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param exit_status Pointer to store the exit status
 * @return int 0 on success, -1 on failure
 */
int thread_get_exit_status(thread_manager_t *manager, unsigned int thread_id, int *exit_status);

/**
 * @brief Register a thread with an attachment identifier
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID to register
 * @param attachment_arg Attachment identifier string
 * @return int 0 on success, -1 on failure
 */
int thread_register(thread_manager_t *manager, unsigned int thread_id, const char *attachment_arg);

/**
 * @brief Unregister a thread by attachment identifier
 * 
 * @param manager Pointer to thread manager structure
 * @param attachment_arg Attachment identifier string
 * @return int 0 on success, -1 on failure
 */
int thread_unregister(thread_manager_t *manager, const char *attachment_arg);

/**
 * @brief Find thread ID by attachment identifier
 * 
 * @param manager Pointer to thread manager structure
 * @param attachment_arg Attachment identifier string
 * @param thread_id Pointer to store the found thread ID
 * @return int 0 on success, -1 on failure
 */
int thread_find_by_attachment(thread_manager_t *manager, const char *attachment_arg, unsigned int *thread_id);

/**
 * @brief Stop a thread by attachment identifier
 * 
 * @param manager Pointer to thread manager structure
 * @param attachment_arg Attachment identifier string
 * @return int 0 on success, -1 on failure
 */
int thread_stop_by_attachment(thread_manager_t *manager, const char *attachment_arg);

/**
 * @brief Kill a thread by attachment identifier
 * 
 * @param manager Pointer to thread manager structure
 * @param attachment_arg Attachment identifier string
 * @return int 0 on success, -1 on failure
 */
int thread_kill_by_attachment(thread_manager_t *manager, const char *attachment_arg);

/**
 * @brief Restart a thread by attachment identifier
 * 
 * @param manager Pointer to thread manager structure
 * @param attachment_arg Attachment identifier string
 * @param new_arg New thread function arguments
 * @return int 0 on success, -1 on failure
 */
int thread_restart_by_attachment(thread_manager_t *manager, const char *attachment_arg, void *new_arg);

/**
 * @brief Get all registered attachment identifiers
 * 
 * @param manager Pointer to thread manager structure
 * @param attachments Array to store attachment identifiers (caller must free strings)
 * @param size Size of the array
 * @return int Number of attachments stored in the array
 */
int thread_get_all_attachments(thread_manager_t *manager, char **attachments, unsigned int size);

#endif /* THREAD_MANAGER_H */
