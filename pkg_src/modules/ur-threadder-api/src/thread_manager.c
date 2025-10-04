/**
 * @file thread_manager.c
 * @brief Implementation of thread management API
 */

#define _POSIX_C_SOURCE 200809L  /* Must be defined before any includes */

#include "../include/thread_manager.h"
#include "../include/utils.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>  /* For kill() */
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>    /* For nanosleep() */

/* Function declarations for better portability */
#if defined(__APPLE__) || !defined(_GNU_SOURCE)
/* For systems without proper string.h implementation */
extern char *strdup(const char *s);
#endif

/* Explicit function declarations for nanosleep and kill to avoid warnings */
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 199309L)
extern int nanosleep(const struct timespec *req, struct timespec *rem);
extern int kill(pid_t pid, int sig);
#endif

#define INITIAL_CAPACITY 10
#define GROWTH_FACTOR 2

/**
 * @brief Wrapper function for thread execution
 * 
 * This function wraps the user-provided thread function and handles
 * thread lifecycle operations like pausing and stopping.
 * 
 * @param arg Thread information
 * @return void* Thread function result
 */
static void *thread_wrapper(void *arg) {
    thread_info_t *info = (thread_info_t *)arg;
    void *result = NULL;
    
    // Set thread state to running
    pthread_mutex_lock(&info->mutex);
    info->state = THREAD_RUNNING;
    pthread_mutex_unlock(&info->mutex);
    
    // Execute thread function while checking for pause and exit conditions
    while (!info->should_exit) {
        // Check if thread is paused
        pthread_mutex_lock(&info->mutex);
        while (info->is_paused && !info->should_exit) {
            info->state = THREAD_PAUSED;
            pthread_cond_wait(&info->cond, &info->mutex);
        }
        info->state = THREAD_RUNNING;
        pthread_mutex_unlock(&info->mutex);
        
        // If thread should exit, break the loop
        if (info->should_exit) {
            break;
        }
        
        // Execute thread function
        result = info->func(info->arg);
        
        // Break the loop after executing the function once
        // This way we can restart the thread with new arguments
        break;
    }
    
    // Set thread state to stopped
    pthread_mutex_lock(&info->mutex);
    info->state = THREAD_STOPPED;
    pthread_mutex_unlock(&info->mutex);
    
    return result;
}

/**
 * @brief Wrapper function for process execution
 * 
 * This function manages the execution of a system binary as a thread.
 * It handles process creation, I/O redirection, and monitoring.
 * 
 * @param arg Thread information
 * @return void* Thread result (always NULL for processes)
 */
static void *process_wrapper(void *arg) {
    thread_info_t *info = (thread_info_t *)arg;
    int status;
    pid_t pid;
    
    // Set thread state to running
    pthread_mutex_lock(&info->mutex);
    info->state = THREAD_RUNNING;
    pthread_mutex_unlock(&info->mutex);
    
    // Create pipes for communication with the process
    if (pipe(info->stdout_pipe) != 0 || 
        pipe(info->stderr_pipe) != 0 || 
        pipe(info->stdin_pipe) != 0) {
        ERROR_LOG("Failed to create pipes for process %u: %s", info->id, strerror(errno));
        
        // Set thread state to error
        pthread_mutex_lock(&info->mutex);
        info->state = THREAD_ERROR;
        pthread_mutex_unlock(&info->mutex);
        
        return NULL;
    }
    
    // Set pipes to non-blocking mode
    fcntl(info->stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(info->stderr_pipe[0], F_SETFL, O_NONBLOCK);
    
    // Fork a child process
    pid = fork();
    
    if (pid < 0) {
        // Fork failed
        ERROR_LOG("Failed to fork process for thread %u: %s", info->id, strerror(errno));
        
        // Close pipes
        close(info->stdout_pipe[0]);
        close(info->stdout_pipe[1]);
        close(info->stderr_pipe[0]);
        close(info->stderr_pipe[1]);
        close(info->stdin_pipe[0]);
        close(info->stdin_pipe[1]);
        
        // Set thread state to error
        pthread_mutex_lock(&info->mutex);
        info->state = THREAD_ERROR;
        pthread_mutex_unlock(&info->mutex);
        
        return NULL;
    } else if (pid == 0) {
        // Child process
        
        // Redirect stdin, stdout, and stderr
        dup2(info->stdin_pipe[0], STDIN_FILENO);
        dup2(info->stdout_pipe[1], STDOUT_FILENO);
        dup2(info->stderr_pipe[1], STDERR_FILENO);
        
        // Close unused pipe ends
        close(info->stdin_pipe[1]);
        close(info->stdout_pipe[0]);
        close(info->stderr_pipe[0]);
        close(info->stdin_pipe[0]);
        close(info->stdout_pipe[1]);
        close(info->stderr_pipe[1]);
        
        // Execute command
        execvp(info->command, info->args);
        
        // If execvp returns, it failed
        fprintf(stderr, "Failed to execute command '%s': %s\n", info->command, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        
        // Store process ID
        info->process_id = pid;
        
        // Close unused pipe ends
        close(info->stdin_pipe[0]);
        close(info->stdout_pipe[1]);
        close(info->stderr_pipe[1]);
        
        // Monitor the process
        while (!info->should_exit) {
            // Check if process has exited
            int ret = waitpid(pid, &status, WNOHANG);
            
            if (ret > 0) {
                // Process has exited
                DEBUG_LOG("Process %u (PID %d) exited with status %d", 
                         info->id, pid, WEXITSTATUS(status));
                
                info->exit_status = WEXITSTATUS(status);
                break;
            } else if (ret < 0) {
                // Error
                ERROR_LOG("Error waiting for process %u (PID %d): %s", 
                         info->id, pid, strerror(errno));
                break;
            }
            
            // Check if thread is paused
            pthread_mutex_lock(&info->mutex);
            if (info->is_paused) {
                // Pause process by sending SIGSTOP
                kill(pid, SIGSTOP);
                info->state = THREAD_PAUSED;
                DEBUG_LOG("Process %u (PID %d) paused", info->id, pid);
                
                // Wait for resume signal
                while (info->is_paused && !info->should_exit) {
                    pthread_cond_wait(&info->cond, &info->mutex);
                }
                
                // Resume process if not exiting
                if (!info->should_exit) {
                    kill(pid, SIGCONT);
                    info->state = THREAD_RUNNING;
                    DEBUG_LOG("Process %u (PID %d) resumed", info->id, pid);
                }
            }
            pthread_mutex_unlock(&info->mutex);
            
            // If thread should exit, terminate the process
            if (info->should_exit) {
                DEBUG_LOG("Terminating process %u (PID %d)", info->id, pid);
                kill(pid, SIGTERM);
                
                // Wait for process to terminate
                int count = 0;
                while (waitpid(pid, &status, WNOHANG) == 0 && count < 10) {
                    struct timespec ts = {0, 100000000}; // 100ms
                    nanosleep(&ts, NULL);
                    count++;
                }
                
                // If process didn't terminate, kill it
                if (count >= 10) {
                    DEBUG_LOG("Process %u (PID %d) didn't terminate, killing", info->id, pid);
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                }
                
                info->exit_status = WEXITSTATUS(status);
                break;
            }
            
            // Sleep briefly to avoid busy-waiting
            struct timespec ts2 = {0, 50000000}; // 50ms
            nanosleep(&ts2, NULL);
        }
        
        // Close remaining pipe ends
        close(info->stdin_pipe[1]);
        close(info->stdout_pipe[0]);
        close(info->stderr_pipe[0]);
        
        // Set thread state to stopped
        pthread_mutex_lock(&info->mutex);
        info->state = THREAD_STOPPED;
        pthread_mutex_unlock(&info->mutex);
    }
    
    return NULL;
}

int thread_manager_init(thread_manager_t *manager, unsigned int initial_capacity) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_CAPACITY;
    }
    
    // Initialize manager structure
    manager->threads = (thread_info_t **)calloc(initial_capacity, sizeof(thread_info_t *));
    if (!manager->threads) {
        ERROR_LOG("Failed to allocate memory for thread manager");
        return -1;
    }
    
    // Initialize registrations
    manager->registrations = (thread_registration_t **)calloc(initial_capacity, sizeof(thread_registration_t *));
    if (!manager->registrations) {
        ERROR_LOG("Failed to allocate memory for thread registrations");
        free(manager->threads);
        return -1;
    }
    
    manager->thread_count = 0;
    manager->capacity = initial_capacity;
    manager->next_id = 1;  // Start with ID 1
    manager->registration_count = 0;
    manager->registration_capacity = initial_capacity;
    
    // Initialize mutex
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        ERROR_LOG("Failed to initialize mutex");
        free(manager->threads);
        return -1;
    }
    
    DEBUG_LOG("Thread manager initialized with capacity %u", initial_capacity);
    return 0;
}

int thread_manager_destroy(thread_manager_t *manager) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Stop and join all threads
    for (unsigned int i = 0; i < manager->capacity; i++) {
        thread_info_t *info = manager->threads[i];
        if (info) {
            // Set thread to exit
            pthread_mutex_lock(&info->mutex);
            info->should_exit = true;
            info->is_paused = false;
            pthread_cond_signal(&info->cond);
            pthread_mutex_unlock(&info->mutex);
            
            // Join thread
            pthread_join(info->thread_id, NULL);
            
            // Destroy mutex and condition variable
            pthread_mutex_destroy(&info->mutex);
            pthread_cond_destroy(&info->cond);
            
            // Free process-specific resources
            if (info->type == THREAD_TYPE_PROCESS) {
                // Close any open pipes
                if (info->stdin_pipe[1] > 0) close(info->stdin_pipe[1]);
                if (info->stdout_pipe[0] > 0) close(info->stdout_pipe[0]);
                if (info->stderr_pipe[0] > 0) close(info->stderr_pipe[0]);
                
                // Free command and arguments
                if (info->command) free(info->command);
                
                if (info->args) {
                    for (int j = 0; info->args[j] != NULL; j++) {
                        free(info->args[j]);
                    }
                    free(info->args);
                }
            }
            
            // Free thread info
            free(info);
            manager->threads[i] = NULL;
        }
    }
    
    // Clean up registrations
    for (unsigned int i = 0; i < manager->registration_capacity; i++) {
        thread_registration_t *reg = manager->registrations[i];
        if (reg) {
            free(reg->attachment_arg);
            free(reg);
            manager->registrations[i] = NULL;
        }
    }
    
    // Free arrays
    free(manager->threads);
    free(manager->registrations);
    
    // Reset manager structure
    manager->threads = NULL;
    manager->registrations = NULL;
    manager->thread_count = 0;
    manager->capacity = 0;
    manager->registration_count = 0;
    manager->registration_capacity = 0;
    
    // Unlock and destroy mutex
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    
    DEBUG_LOG("Thread manager destroyed");
    return 0;
}

static int resize_thread_array(thread_manager_t *manager) {
    unsigned int new_capacity = manager->capacity * GROWTH_FACTOR;
    thread_info_t **new_threads = (thread_info_t **)realloc(manager->threads, new_capacity * sizeof(thread_info_t *));
    
    if (!new_threads) {
        ERROR_LOG("Failed to resize thread array");
        return -1;
    }
    
    // Initialize new memory to NULL
    for (unsigned int i = manager->capacity; i < new_capacity; i++) {
        new_threads[i] = NULL;
    }
    
    manager->threads = new_threads;
    manager->capacity = new_capacity;
    
    DEBUG_LOG("Thread array resized to capacity %u", new_capacity);
    return 0;
}

int thread_create(thread_manager_t *manager, void *(*func)(void *), void *arg, unsigned int *thread_id) {
    if (!manager || !func) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Check if we need to resize the thread array
    if (manager->thread_count >= manager->capacity) {
        if (resize_thread_array(manager) != 0) {
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
    }
    
    // Find an empty slot for the new thread
    int slot = -1;
    for (unsigned int i = 0; i < manager->capacity; i++) {
        if (manager->threads[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ERROR_LOG("Failed to find empty slot for thread");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Create thread info structure
    thread_info_t *info = (thread_info_t *)malloc(sizeof(thread_info_t));
    if (!info) {
        ERROR_LOG("Failed to allocate memory for thread info");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Initialize thread info
    memset(info, 0, sizeof(thread_info_t));
    info->func = func;
    info->arg = arg;
    info->state = THREAD_CREATED;
    info->id = manager->next_id++;
    info->should_exit = false;
    info->is_paused = false;
    info->type = THREAD_TYPE_NORMAL;
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&info->mutex, NULL) != 0) {
        ERROR_LOG("Failed to initialize thread mutex");
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    if (pthread_cond_init(&info->cond, NULL) != 0) {
        ERROR_LOG("Failed to initialize thread condition variable");
        pthread_mutex_destroy(&info->mutex);
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Create thread
    if (pthread_create(&info->thread_id, NULL, thread_wrapper, info) != 0) {
        ERROR_LOG("Failed to create thread");
        pthread_mutex_destroy(&info->mutex);
        pthread_cond_destroy(&info->cond);
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Store thread info
    manager->threads[slot] = info;
    manager->thread_count++;
    
    // Store thread ID for return
    if (thread_id) {
        *thread_id = info->id;
    }
    
    DEBUG_LOG("Thread created with ID %u", info->id);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return info->id;
}

/**
 * @brief Create and execute a system binary as a thread
 * 
 * @param manager Pointer to thread manager structure
 * @param command Command to execute
 * @param args Arguments for the command (NULL-terminated array)
 * @param thread_id Pointer to store the thread ID
 * @return int Thread ID on success, -1 on failure
 */
int thread_create_process(thread_manager_t *manager, const char *command, char **args, unsigned int *thread_id) {
    if (!manager || !command || !args) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Check if we need to resize the thread array
    if (manager->thread_count >= manager->capacity) {
        if (resize_thread_array(manager) != 0) {
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
    }
    
    // Find an empty slot for the new thread
    int slot = -1;
    for (unsigned int i = 0; i < manager->capacity; i++) {
        if (manager->threads[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ERROR_LOG("Failed to find empty slot for thread");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Create thread info structure
    thread_info_t *info = (thread_info_t *)malloc(sizeof(thread_info_t));
    if (!info) {
        ERROR_LOG("Failed to allocate memory for thread info");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Initialize thread info with zeros
    memset(info, 0, sizeof(thread_info_t));
    
    // Set thread properties
    info->state = THREAD_CREATED;
    info->id = manager->next_id++;
    info->should_exit = false;
    info->is_paused = false;
    info->type = THREAD_TYPE_PROCESS;
    info->exit_status = -1;
    
    // Duplicate command string
    info->command = strdup(command);
    if (!info->command) {
        ERROR_LOG("Failed to duplicate command string");
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Count arguments
    int arg_count = 0;
    while (args[arg_count] != NULL) {
        arg_count++;
    }
    
    // Allocate memory for arguments array (including NULL terminator)
    info->args = (char **)malloc((arg_count + 1) * sizeof(char *));
    if (!info->args) {
        ERROR_LOG("Failed to allocate memory for arguments array");
        free(info->command);
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Copy arguments
    for (int i = 0; i < arg_count; i++) {
        info->args[i] = strdup(args[i]);
        if (!info->args[i]) {
            ERROR_LOG("Failed to duplicate argument string");
            
            // Free previously allocated memory
            for (int j = 0; j < i; j++) {
                free(info->args[j]);
            }
            free(info->args);
            free(info->command);
            free(info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
    }
    
    // NULL-terminate the arguments array
    info->args[arg_count] = NULL;
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&info->mutex, NULL) != 0) {
        ERROR_LOG("Failed to initialize thread mutex");
        
        // Free allocated memory
        for (int i = 0; i < arg_count; i++) {
            free(info->args[i]);
        }
        free(info->args);
        free(info->command);
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    if (pthread_cond_init(&info->cond, NULL) != 0) {
        ERROR_LOG("Failed to initialize thread condition variable");
        
        // Free allocated memory
        pthread_mutex_destroy(&info->mutex);
        for (int i = 0; i < arg_count; i++) {
            free(info->args[i]);
        }
        free(info->args);
        free(info->command);
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Create thread
    if (pthread_create(&info->thread_id, NULL, process_wrapper, info) != 0) {
        ERROR_LOG("Failed to create thread for process");
        
        // Free allocated memory
        pthread_mutex_destroy(&info->mutex);
        pthread_cond_destroy(&info->cond);
        for (int i = 0; i < arg_count; i++) {
            free(info->args[i]);
        }
        free(info->args);
        free(info->command);
        free(info);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Store thread info
    manager->threads[slot] = info;
    manager->thread_count++;
    
    // Store thread ID for return
    if (thread_id) {
        *thread_id = info->id;
    }
    
    DEBUG_LOG("Process thread created with ID %u for command '%s'", info->id, command);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return info->id;
}

static thread_info_t *find_thread_by_id(thread_manager_t *manager, unsigned int thread_id) {
    for (unsigned int i = 0; i < manager->capacity; i++) {
        if (manager->threads[i] && manager->threads[i]->id == thread_id) {
            return manager->threads[i];
        }
    }
    return NULL;
}

int thread_stop(thread_manager_t *manager, unsigned int thread_id) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    DEBUG_LOG("Thread mutex lock..");

    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    DEBUG_LOG("Thread mutex lock done..");

    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -2;
    }
    
    // Set thread to exit
    pthread_mutex_lock(&info->mutex);
    info->should_exit = true;
    info->is_paused = false;
    pthread_cond_signal(&info->cond);
    pthread_mutex_unlock(&info->mutex);
    
    DEBUG_LOG("Thread %u set to stop", thread_id);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

int thread_pause(thread_manager_t *manager, unsigned int thread_id) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Set thread to pause
    pthread_mutex_lock(&info->mutex);
    if (info->state == THREAD_RUNNING) {
        info->is_paused = true;
        DEBUG_LOG("Thread %u set to pause", thread_id);
    } else {
        DEBUG_LOG("Thread %u is not running, cannot pause", thread_id);
    }
    pthread_mutex_unlock(&info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

int thread_resume(thread_manager_t *manager, unsigned int thread_id) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Resume thread
    pthread_mutex_lock(&info->mutex);
    if (info->is_paused) {
        info->is_paused = false;
        pthread_cond_signal(&info->cond);
        DEBUG_LOG("Thread %u resumed", thread_id);
    } else {
        DEBUG_LOG("Thread %u is not paused, cannot resume", thread_id);
    }
    pthread_mutex_unlock(&info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

int thread_restart(thread_manager_t *manager, unsigned int thread_id, void *new_arg) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *old_info = find_thread_by_id(manager, thread_id);
    if (!old_info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Handle restarting based on thread type
    if (old_info->type == THREAD_TYPE_NORMAL) {
        // Get thread function
        void *(*func)(void *) = old_info->func;
        
        // Stop the old thread
        pthread_mutex_lock(&old_info->mutex);
        old_info->should_exit = true;
        old_info->is_paused = false;
        pthread_cond_signal(&old_info->cond);
        pthread_mutex_unlock(&old_info->mutex);
        
        // Wait for thread to stop
        pthread_join(old_info->thread_id, NULL);
        
        // Find the slot for the old thread
        int slot = -1;
        for (unsigned int i = 0; i < manager->capacity; i++) {
            if (manager->threads[i] == old_info) {
                slot = i;
                break;
            }
        }
        
        if (slot == -1) {
            ERROR_LOG("Failed to find slot for thread %u", thread_id);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Create new thread info
        thread_info_t *new_info = (thread_info_t *)malloc(sizeof(thread_info_t));
        if (!new_info) {
            ERROR_LOG("Failed to allocate memory for thread info");
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Initialize new thread info with zeros
        memset(new_info, 0, sizeof(thread_info_t));
        
        // Set thread properties
        new_info->func = func;
        new_info->arg = new_arg;
        new_info->state = THREAD_CREATED;
        new_info->id = thread_id;  // Keep the same ID
        new_info->should_exit = false;
        new_info->is_paused = false;
        new_info->type = THREAD_TYPE_NORMAL;
        
        // Initialize mutex and condition variable
        if (pthread_mutex_init(&new_info->mutex, NULL) != 0) {
            ERROR_LOG("Failed to initialize thread mutex");
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        if (pthread_cond_init(&new_info->cond, NULL) != 0) {
            ERROR_LOG("Failed to initialize thread condition variable");
            pthread_mutex_destroy(&new_info->mutex);
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Create new thread
        if (pthread_create(&new_info->thread_id, NULL, thread_wrapper, new_info) != 0) {
            ERROR_LOG("Failed to create thread");
            pthread_mutex_destroy(&new_info->mutex);
            pthread_cond_destroy(&new_info->cond);
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Destroy old thread resources
        pthread_mutex_destroy(&old_info->mutex);
        pthread_cond_destroy(&old_info->cond);
        free(old_info);
        
        // Store new thread info
        manager->threads[slot] = new_info;
        
        DEBUG_LOG("Thread %u restarted with new arguments", thread_id);
    } else if (old_info->type == THREAD_TYPE_PROCESS) {
        char *command = old_info->command;
        char **args = NULL;
        
        // Only use new_arg if it's a char** for the arguments
        if (new_arg) {
            args = (char **)new_arg;
        } else {
            // Use old arguments if no new ones provided
            args = old_info->args;
        }
        
        // Stop the old process
        pthread_mutex_lock(&old_info->mutex);
        old_info->should_exit = true;
        old_info->is_paused = false;
        pthread_cond_signal(&old_info->cond);
        pthread_mutex_unlock(&old_info->mutex);
        
        // Wait for thread to stop
        pthread_join(old_info->thread_id, NULL);
        
        // Find the slot for the old thread
        int slot = -1;
        for (unsigned int i = 0; i < manager->capacity; i++) {
            if (manager->threads[i] == old_info) {
                slot = i;
                break;
            }
        }
        
        if (slot == -1) {
            ERROR_LOG("Failed to find slot for process thread %u", thread_id);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Create new thread info
        thread_info_t *new_info = (thread_info_t *)malloc(sizeof(thread_info_t));
        if (!new_info) {
            ERROR_LOG("Failed to allocate memory for thread info");
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Initialize thread info with zeros
        memset(new_info, 0, sizeof(thread_info_t));
        
        // Set thread properties
        new_info->state = THREAD_CREATED;
        new_info->id = thread_id;  // Keep the same ID
        new_info->should_exit = false;
        new_info->is_paused = false;
        new_info->type = THREAD_TYPE_PROCESS;
        new_info->exit_status = -1;
        
        // Duplicate command string
        new_info->command = strdup(command);
        if (!new_info->command) {
            ERROR_LOG("Failed to duplicate command string");
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Count arguments
        int arg_count = 0;
        while (args[arg_count] != NULL) {
            arg_count++;
        }
        
        // Allocate memory for arguments array (including NULL terminator)
        new_info->args = (char **)malloc((arg_count + 1) * sizeof(char *));
        if (!new_info->args) {
            ERROR_LOG("Failed to allocate memory for arguments array");
            free(new_info->command);
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Copy arguments
        for (int i = 0; i < arg_count; i++) {
            new_info->args[i] = strdup(args[i]);
            if (!new_info->args[i]) {
                ERROR_LOG("Failed to duplicate argument string");
                
                // Free previously allocated memory
                for (int j = 0; j < i; j++) {
                    free(new_info->args[j]);
                }
                free(new_info->args);
                free(new_info->command);
                free(new_info);
                pthread_mutex_unlock(&manager->mutex);
                return -1;
            }
        }
        
        // NULL-terminate the arguments array
        new_info->args[arg_count] = NULL;
        
        // Initialize mutex and condition variable
        if (pthread_mutex_init(&new_info->mutex, NULL) != 0) {
            ERROR_LOG("Failed to initialize thread mutex");
            
            // Free allocated memory
            for (int i = 0; i < arg_count; i++) {
                free(new_info->args[i]);
            }
            free(new_info->args);
            free(new_info->command);
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        if (pthread_cond_init(&new_info->cond, NULL) != 0) {
            ERROR_LOG("Failed to initialize thread condition variable");
            
            // Free allocated memory
            pthread_mutex_destroy(&new_info->mutex);
            for (int i = 0; i < arg_count; i++) {
                free(new_info->args[i]);
            }
            free(new_info->args);
            free(new_info->command);
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Create thread
        if (pthread_create(&new_info->thread_id, NULL, process_wrapper, new_info) != 0) {
            ERROR_LOG("Failed to create thread for process");
            
            // Free allocated memory
            pthread_mutex_destroy(&new_info->mutex);
            pthread_cond_destroy(&new_info->cond);
            for (int i = 0; i < arg_count; i++) {
                free(new_info->args[i]);
            }
            free(new_info->args);
            free(new_info->command);
            free(new_info);
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
        
        // Free process-specific resources for old thread
        if (old_info->stdin_pipe[1] > 0) close(old_info->stdin_pipe[1]);
        if (old_info->stdout_pipe[0] > 0) close(old_info->stdout_pipe[0]);
        if (old_info->stderr_pipe[0] > 0) close(old_info->stderr_pipe[0]);
        
        // Free old command and arguments
        if (old_info->command) free(old_info->command);
        
        if (old_info->args) {
            for (int i = 0; old_info->args[i] != NULL; i++) {
                free(old_info->args[i]);
            }
            free(old_info->args);
        }
        
        // Destroy old thread resources
        pthread_mutex_destroy(&old_info->mutex);
        pthread_cond_destroy(&old_info->cond);
        free(old_info);
        
        // Store new thread info
        manager->threads[slot] = new_info;
        
        DEBUG_LOG("Process thread %u restarted with command '%s'", thread_id, command);
    } else {
        ERROR_LOG("Unknown thread type for thread %u", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

int thread_get_state(thread_manager_t *manager, unsigned int thread_id, thread_state_t *state) {
    if (!manager || !state) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Get thread state
    pthread_mutex_lock(&info->mutex);
    *state = info->state;
    pthread_mutex_unlock(&info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

unsigned int thread_get_count(thread_manager_t *manager) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return 0;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Get thread count
    unsigned int count = manager->thread_count;
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return count;
}

int thread_get_info(thread_manager_t *manager, unsigned int thread_id, thread_info_t *info) {
    if (!manager || !info) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *thread_info = find_thread_by_id(manager, thread_id);
    if (!thread_info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Copy thread info
    pthread_mutex_lock(&thread_info->mutex);
    info->thread_id = thread_info->thread_id;
    info->func = thread_info->func;
    info->arg = thread_info->arg;
    info->state = thread_info->state;
    info->id = thread_info->id;
    info->should_exit = thread_info->should_exit;
    info->is_paused = thread_info->is_paused;
    pthread_mutex_unlock(&thread_info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}

bool thread_is_alive(thread_manager_t *manager, unsigned int thread_id) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return false;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        DEBUG_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return false;
    }
    
    // Check if thread is alive
    pthread_mutex_lock(&info->mutex);
    bool alive = (info->state == THREAD_RUNNING || info->state == THREAD_PAUSED);
    pthread_mutex_unlock(&info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return alive;
}

int thread_join(thread_manager_t *manager, unsigned int thread_id, void **result) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Handle differently based on thread type
    if (info->type == THREAD_TYPE_NORMAL) {
        // Get thread ID
        pthread_t thread_to_join = info->thread_id;
        
        // Unlock manager mutex before blocking on join
        pthread_mutex_unlock(&manager->mutex);
        
        // Join thread
        if (pthread_join(thread_to_join, result) != 0) {
            ERROR_LOG("Failed to join thread %u", thread_id);
            return -1;
        }
    } else if (info->type == THREAD_TYPE_PROCESS) {
        // For processes, we just wait for the thread monitoring the process to complete
        pthread_t thread_to_join = info->thread_id;
        
        // Unlock manager mutex before blocking on join
        pthread_mutex_unlock(&manager->mutex);
        
        // Join thread
        void *thread_result;
        if (pthread_join(thread_to_join, &thread_result) != 0) {
            ERROR_LOG("Failed to join process thread %u", thread_id);
            return -1;
        }
        
        // Set exit status as result if requested
        if (result) {
            *result = (void*)(intptr_t)info->exit_status;
        }
    } else {
        pthread_mutex_unlock(&manager->mutex);
        ERROR_LOG("Unknown thread type for thread %u", thread_id);
        return -1;
    }
    
    DEBUG_LOG("Thread %u joined", thread_id);
    return 0;
}

int thread_get_all_ids(thread_manager_t *manager, unsigned int *ids, unsigned int size) {
    if (!manager || !ids || size == 0) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Get thread IDs
    unsigned int count = 0;
    for (unsigned int i = 0; i < manager->capacity && count < size; i++) {
        if (manager->threads[i]) {
            ids[count++] = manager->threads[i]->id;
        }
    }
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return count;
}

int thread_should_exit(thread_manager_t *manager, unsigned int thread_id) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return 1;  // Default to exit
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return 1;  // Thread not found, exit
    }
    
    // Check if thread should exit
    pthread_mutex_lock(&info->mutex);
    int should_exit = info->should_exit ? 1 : 0;
    pthread_mutex_unlock(&info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return should_exit;
}

void thread_check_pause(thread_manager_t *manager, unsigned int thread_id) {
    if (!manager) {
        ERROR_LOG("Invalid manager pointer");
        return;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return;
    }
    
    // Check if thread is paused
    pthread_mutex_lock(&info->mutex);
    while (info->is_paused && !info->should_exit) {
        info->state = THREAD_PAUSED;
        pthread_cond_wait(&info->cond, &info->mutex);
    }
    info->state = THREAD_RUNNING;
    pthread_mutex_unlock(&info->mutex);
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
}

/**
 * @brief Write data to the stdin of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param data Data to write
 * @param size Size of data
 * @return int Number of bytes written on success, -1 on failure
 */
int thread_write_to_process(thread_manager_t *manager, unsigned int thread_id, const void *data, size_t size) {
    if (!manager || !data || size == 0) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if thread is a process
    if (info->type != THREAD_TYPE_PROCESS) {
        ERROR_LOG("Thread %u is not a process thread", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if process is still running
    if (info->state != THREAD_RUNNING) {
        ERROR_LOG("Process %u is not running", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Write to stdin pipe
    ssize_t bytes_written = write(info->stdin_pipe[1], data, size);
    if (bytes_written < 0) {
        ERROR_LOG("Failed to write to process %u: %s", thread_id, strerror(errno));
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return (int)bytes_written;
}

/**
 * @brief Read data from the stdout of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param buffer Buffer to store the data
 * @param size Size of buffer
 * @return int Number of bytes read on success, -1 on failure
 */
int thread_read_from_process(thread_manager_t *manager, unsigned int thread_id, void *buffer, size_t size) {
    if (!manager || !buffer || size == 0) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if thread is a process
    if (info->type != THREAD_TYPE_PROCESS) {
        ERROR_LOG("Thread %u is not a process thread", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Read from stdout pipe
    ssize_t bytes_read = read(info->stdout_pipe[0], buffer, size);
    if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        ERROR_LOG("Failed to read from process %u: %s", thread_id, strerror(errno));
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return (int)bytes_read;
}

/**
 * @brief Read data from the stderr of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param buffer Buffer to store the data
 * @param size Size of buffer
 * @return int Number of bytes read on success, -1 on failure
 */
int thread_read_error_from_process(thread_manager_t *manager, unsigned int thread_id, void *buffer, size_t size) {
    if (!manager || !buffer || size == 0) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if thread is a process
    if (info->type != THREAD_TYPE_PROCESS) {
        ERROR_LOG("Thread %u is not a process thread", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Read from stderr pipe
    ssize_t bytes_read = read(info->stderr_pipe[0], buffer, size);
    if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        ERROR_LOG("Failed to read error from process %u: %s", thread_id, strerror(errno));
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return (int)bytes_read;
}

/**
 * @brief Get the exit status of a process thread
 * 
 * @param manager Pointer to thread manager structure
 * @param thread_id Thread ID
 * @param exit_status Pointer to store the exit status
 * @return int 0 on success, -1 on failure
 */
int thread_get_exit_status(thread_manager_t *manager, unsigned int thread_id, int *exit_status) {
    if (!manager || !exit_status) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Find thread info
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if thread is a process
    if (info->type != THREAD_TYPE_PROCESS) {
        ERROR_LOG("Thread %u is not a process thread", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if process has exited
    if (info->state != THREAD_STOPPED) {
        ERROR_LOG("Process %u has not exited yet", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Get exit status
    *exit_status = info->exit_status;
    
    // Unlock manager mutex
    pthread_mutex_unlock(&manager->mutex);
    
    return 0;
}


// Thread Registration Functions Implementation

/**
 * @brief Resize registration array when needed
 */
static int resize_registration_array(thread_manager_t *manager) {
    unsigned int new_capacity = manager->registration_capacity * GROWTH_FACTOR;
    thread_registration_t **new_registrations = (thread_registration_t **)realloc(
        manager->registrations, new_capacity * sizeof(thread_registration_t *));
    
    if (!new_registrations) {
        ERROR_LOG("Failed to resize registration array");
        return -1;
    }
    
    // Initialize new memory to NULL
    for (unsigned int i = manager->registration_capacity; i < new_capacity; i++) {
        new_registrations[i] = NULL;
    }
    
    manager->registrations = new_registrations;
    manager->registration_capacity = new_capacity;
    
    DEBUG_LOG("Registration array resized to capacity %u", new_capacity);
    return 0;
}

/**
 * @brief Find registration by attachment identifier
 */
static thread_registration_t *find_registration_by_attachment(thread_manager_t *manager, const char *attachment_arg) {
    for (unsigned int i = 0; i < manager->registration_capacity; i++) {
        thread_registration_t *reg = manager->registrations[i];
        if (reg && reg->attachment_arg && strcmp(reg->attachment_arg, attachment_arg) == 0) {
            return reg;
        }
    }
    return NULL;
}

/**
 * @brief Find empty slot in registration array
 */
static int find_empty_registration_slot(thread_manager_t *manager) {
    for (unsigned int i = 0; i < manager->registration_capacity; i++) {
        if (manager->registrations[i] == NULL) {
            return (int)i;
        }
    }
    return -1;
}

int thread_register(thread_manager_t *manager, unsigned int thread_id, const char *attachment_arg) {
    if (!manager || !attachment_arg) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Lock manager mutex
    pthread_mutex_lock(&manager->mutex);
    
    // Check if thread exists
    thread_info_t *info = find_thread_by_id(manager, thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if attachment already exists
    if (find_registration_by_attachment(manager, attachment_arg) != NULL) {
        ERROR_LOG("Attachment ID already registered", attachment_arg);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Check if we need to resize registration array
    if (manager->registration_count >= manager->registration_capacity) {
        if (resize_registration_array(manager) != 0) {
            pthread_mutex_unlock(&manager->mutex);
            return -1;
        }
    }
    
    // Find empty slot
    int slot = find_empty_registration_slot(manager);
    if (slot < 0) {
        ERROR_LOG("No empty slot found for registration");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Create new registration
    thread_registration_t *reg = (thread_registration_t *)malloc(sizeof(thread_registration_t));
    if (!reg) {
        ERROR_LOG("Failed to allocate memory for registration");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    reg->attachment_arg = strdup(attachment_arg);
    if (!reg->attachment_arg) {
        ERROR_LOG("Failed to duplicate attachment string");
        free(reg);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    reg->thread_id = thread_id;
    manager->registrations[slot] = reg;
    manager->registration_count++;
    
    DEBUG_LOG("Thread %u registered with attachment ID", thread_id);
    
    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

int thread_unregister(thread_manager_t *manager, const char *attachment_arg) {
    if (!manager || !attachment_arg) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    // Find registration
    for (unsigned int i = 0; i < manager->registration_capacity; i++) {
        thread_registration_t *reg = manager->registrations[i];
        if (reg && reg->attachment_arg && strcmp(reg->attachment_arg, attachment_arg) == 0) {
            // Free registration
            free(reg->attachment_arg);
            free(reg);
            manager->registrations[i] = NULL;
            manager->registration_count--;
            
            DEBUG_LOG("Unregistered attachment ID");
            pthread_mutex_unlock(&manager->mutex);
            return 0;
        }
    }
    
    ERROR_LOG("Attachment ID not found");
    pthread_mutex_unlock(&manager->mutex);
    return -1;
}

int thread_find_by_attachment(thread_manager_t *manager, const char *attachment_arg, unsigned int *thread_id) {
    if (!manager || !attachment_arg || !thread_id) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    thread_registration_t *reg = find_registration_by_attachment(manager, attachment_arg);
    if (!reg) {
        ERROR_LOG("Attachment ID not found");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    *thread_id = reg->thread_id;
    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

int thread_stop_by_attachment(thread_manager_t *manager, const char *attachment_arg) {
    if (!manager || !attachment_arg) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    unsigned int thread_id;
    if (thread_find_by_attachment(manager, attachment_arg, &thread_id) != 0) {
        return -1;
    }
    
    return thread_stop(manager, thread_id);
}

int thread_kill_by_attachment(thread_manager_t *manager, const char *attachment_arg) {
    if (!manager || !attachment_arg) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    thread_registration_t *reg = find_registration_by_attachment(manager, attachment_arg);
    if (!reg) {
        ERROR_LOG("Attachment ID not found");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    thread_info_t *info = find_thread_by_id(manager, reg->thread_id);
    if (!info) {
        ERROR_LOG("Thread with ID %u not found", reg->thread_id);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    // Force kill the thread
    if (info->type == THREAD_TYPE_PROCESS && info->process_id > 0) {
        // Kill process immediately
        kill(info->process_id, SIGKILL);
        DEBUG_LOG("Process %d killed forcefully", info->process_id);
    } else {
        // For normal threads, use pthread_cancel (platform dependent)
        pthread_cancel(info->thread_id);
        DEBUG_LOG("Thread %u cancelled forcefully", reg->thread_id);
    }
    
    // Set thread to stopped state
    pthread_mutex_lock(&info->mutex);
    info->should_exit = true;
    info->state = THREAD_STOPPED;
    pthread_cond_signal(&info->cond);
    pthread_mutex_unlock(&info->mutex);
    
    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

int thread_restart_by_attachment(thread_manager_t *manager, const char *attachment_arg, void *new_arg) {
    if (!manager || !attachment_arg) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    unsigned int thread_id;
    if (thread_find_by_attachment(manager, attachment_arg, &thread_id) != 0) {
        return -1;
    }
    
    return thread_restart(manager, thread_id, new_arg);
}

int thread_get_all_attachments(thread_manager_t *manager, char **attachments, unsigned int size) {
    if (!manager || !attachments) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    unsigned int count = 0;
    for (unsigned int i = 0; i < manager->registration_capacity && count < size; i++) {
        thread_registration_t *reg = manager->registrations[i];
        if (reg && reg->attachment_arg) {
            attachments[count] = strdup(reg->attachment_arg);
            if (!attachments[count]) {
                ERROR_LOG("Failed to duplicate attachment string");
                // Free previously allocated strings
                for (unsigned int j = 0; j < count; j++) {
                    free(attachments[j]);
                }
                pthread_mutex_unlock(&manager->mutex);
                return -1;
            }
            count++;
        }
    }
    
    pthread_mutex_unlock(&manager->mutex);
    return (int)count;
}
