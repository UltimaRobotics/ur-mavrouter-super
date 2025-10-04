/**
 * @file example.c
 * @brief Example usage of thread management API
 */

#define _POSIX_C_SOURCE 200809L  /* Must be defined before any includes */

#include "../include/thread_manager.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>    /* For nanosleep() */

/* Explicit function declarations for nanosleep to avoid warnings */
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 199309L)
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif

// Global thread manager for signal handler
thread_manager_t manager;
volatile sig_atomic_t running = 1;

/**
 * @brief Example worker thread function
 * 
 * @param arg Thread arguments
 * @return void* Thread result
 */
void *worker_thread(void *arg) {
    int thread_num = *((int *)arg);
    unsigned int thread_id = 0;
    
    INFO_LOG("Worker thread %d started", thread_num);
    
    // Get thread ID from manager
    for (unsigned int i = 0; i < thread_get_count(&manager); i++) {
        unsigned int *ids = (unsigned int *)malloc(thread_get_count(&manager) * sizeof(unsigned int));
        int count = thread_get_all_ids(&manager, ids, thread_get_count(&manager));
        
        for (int j = 0; j < count; j++) {
            thread_info_t info;
            if (thread_get_info(&manager, ids[j], &info) == 0) {
                if (info.arg == arg) {
                    thread_id = ids[j];
                    break;
                }
            }
        }
        
        free(ids);
        
        if (thread_id != 0) {
            break;
        }
    }
    
    // Simulate work with pause and exit checks
    for (int i = 0; i < 100; i++) {
        // Check if thread should exit
        if (thread_should_exit(&manager, thread_id)) {
            INFO_LOG("Worker thread %d exiting", thread_num);
            return NULL;
        }
        
        // Check if thread is paused
        thread_check_pause(&manager, thread_id);
        
        INFO_LOG("Worker thread %d working: %d/100", thread_num, i + 1);
        sleep(1);
    }
    
    INFO_LOG("Worker thread %d completed", thread_num);
    return NULL;
}

/**
 * @brief Counter thread function
 * 
 * @param arg Thread arguments
 * @return void* Thread result
 */
void *counter_thread(void *arg) {
    int *counter = (int *)arg;
    unsigned int thread_id = 0;
    
    INFO_LOG("Counter thread started with initial value %d", *counter);
    
    // Get thread ID from manager
    for (unsigned int i = 0; i < thread_get_count(&manager); i++) {
        unsigned int *ids = (unsigned int *)malloc(thread_get_count(&manager) * sizeof(unsigned int));
        int count = thread_get_all_ids(&manager, ids, thread_get_count(&manager));
        
        for (int j = 0; j < count; j++) {
            thread_info_t info;
            if (thread_get_info(&manager, ids[j], &info) == 0) {
                if (info.arg == arg) {
                    thread_id = ids[j];
                    break;
                }
            }
        }
        
        free(ids);
        
        if (thread_id != 0) {
            break;
        }
    }
    
    // Count up
    for (int i = 0; i < 5; i++) {
        // Check if thread should exit
        if (thread_should_exit(&manager, thread_id)) {
            INFO_LOG("Counter thread exiting");
            return NULL;
        }
        
        // Check if thread is paused
        thread_check_pause(&manager, thread_id);
        
        (*counter)++;
        INFO_LOG("Counter thread: %d", *counter);
        sleep(1);
    }
    
    INFO_LOG("Counter thread completed");
    return NULL;
}

/**
 * @brief Signal handler for graceful shutdown
 * 
 * @param sig Signal number
 */
void signal_handler(int sig) {
    INFO_LOG("Received signal %d, shutting down", sig);
    running = 0;
}

/**
 * @brief Print menu
 */
void print_menu() {
    printf("\n=== Thread Manager Example ===\n");
    printf("1. Create worker thread\n");
    printf("2. Create counter thread\n");
    printf("3. List threads\n");
    printf("4. Pause thread\n");
    printf("5. Resume thread\n");
    printf("6. Stop thread\n");
    printf("7. Restart thread\n");
    printf("8. Set log level\n");
    printf("9. Exit\n");
    printf("Enter choice: ");
}

/**
 * @brief Print thread state as string
 * 
 * @param state Thread state
 * @return const char* Thread state string
 */
const char *thread_state_to_string(thread_state_t state) {
    switch (state) {
        case THREAD_CREATED:
            return "CREATED";
        case THREAD_RUNNING:
            return "RUNNING";
        case THREAD_PAUSED:
            return "PAUSED";
        case THREAD_STOPPED:
            return "STOPPED";
        case THREAD_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief List all threads
 */
void list_threads() {
    unsigned int count = thread_get_count(&manager);
    printf("\n=== Threads (%u) ===\n", count);
    
    unsigned int *ids = (unsigned int *)malloc(count * sizeof(unsigned int));
    int num_ids = thread_get_all_ids(&manager, ids, count);
    
    if (num_ids == 0) {
        printf("No threads running.\n");
    } else {
        printf("ID\tState\tAlive\n");
        printf("--\t-----\t-----\n");
        
        for (int i = 0; i < num_ids; i++) {
            thread_state_t state;
            thread_get_state(&manager, ids[i], &state);
            bool alive = thread_is_alive(&manager, ids[i]);
            
            printf("%u\t%s\t%s\n", 
                   ids[i], 
                   thread_state_to_string(state), 
                   alive ? "Yes" : "No");
        }
    }
    
    free(ids);
}

int main() {
    // Set log level
    set_log_level(LOG_LEVEL_INFO);
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    // Initialize thread manager
    if (thread_manager_init(&manager, 10) != 0) {
        ERROR_LOG("Failed to initialize thread manager");
        return 1;
    }
    
    INFO_LOG("Thread manager initialized");
    
    int choice;
    unsigned int thread_id;
    
    while (running) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            // Clear input buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        
        switch (choice) {
            case 1: {  // Create worker thread
                int *thread_num = (int *)malloc(sizeof(int));
                *thread_num = thread_get_count(&manager) + 1;
                
                unsigned int new_id;
                if (thread_create(&manager, worker_thread, thread_num, &new_id) > 0) {
                    INFO_LOG("Created worker thread with ID %u", new_id);
                } else {
                    ERROR_LOG("Failed to create worker thread");
                    free(thread_num);
                }
                break;
            }
            
            case 2: {  // Create counter thread
                int *counter = (int *)malloc(sizeof(int));
                *counter = 0;
                
                unsigned int new_id;
                if (thread_create(&manager, counter_thread, counter, &new_id) > 0) {
                    INFO_LOG("Created counter thread with ID %u", new_id);
                } else {
                    ERROR_LOG("Failed to create counter thread");
                    free(counter);
                }
                break;
            }
            
            case 3:  // List threads
                list_threads();
                break;
                
            case 4: {  // Pause thread
                list_threads();
                printf("Enter thread ID to pause: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid thread ID");
                    break;
                }
                
                if (thread_pause(&manager, thread_id) == 0) {
                    INFO_LOG("Thread %u paused", thread_id);
                } else {
                    ERROR_LOG("Failed to pause thread %u", thread_id);
                }
                break;
            }
            
            case 5: {  // Resume thread
                list_threads();
                printf("Enter thread ID to resume: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid thread ID");
                    break;
                }
                
                if (thread_resume(&manager, thread_id) == 0) {
                    INFO_LOG("Thread %u resumed", thread_id);
                } else {
                    ERROR_LOG("Failed to resume thread %u", thread_id);
                }
                break;
            }
            
            case 6: {  // Stop thread
                list_threads();
                printf("Enter thread ID to stop: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid thread ID");
                    break;
                }
                
                if (thread_stop(&manager, thread_id) == 0) {
                    INFO_LOG("Thread %u stopped", thread_id);
                } else {
                    ERROR_LOG("Failed to stop thread %u", thread_id);
                }
                break;
            }
            
            case 7: {  // Restart thread
                list_threads();
                printf("Enter thread ID to restart: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid thread ID");
                    break;
                }
                
                // Get thread info to determine type
                thread_info_t info;
                if (thread_get_info(&manager, thread_id, &info) != 0) {
                    ERROR_LOG("Failed to get thread info for %u", thread_id);
                    break;
                }
                
                // Create new argument based on thread type
                void *new_arg = NULL;
                if (info.func == worker_thread) {
                    int *thread_num = (int *)malloc(sizeof(int));
                    *thread_num = *((int *)info.arg);
                    new_arg = thread_num;
                } else if (info.func == counter_thread) {
                    int *counter = (int *)malloc(sizeof(int));
                    *counter = 0;  // Reset counter
                    new_arg = counter;
                }
                
                if (thread_restart(&manager, thread_id, new_arg) == 0) {
                    INFO_LOG("Thread %u restarted", thread_id);
                } else {
                    ERROR_LOG("Failed to restart thread %u", thread_id);
                    free(new_arg);
                }
                break;
            }
            
            case 8: {  // Set log level
                printf("Log levels:\n");
                printf("1. DEBUG\n");
                printf("2. INFO\n");
                printf("3. WARN\n");
                printf("4. ERROR\n");
                printf("Enter log level: ");
                
                int log_level;
                if (scanf("%d", &log_level) != 1 || log_level < 1 || log_level > 4) {
                    WARN_LOG("Invalid log level");
                    break;
                }
                
                set_log_level((log_level_t)(log_level - 1));
                INFO_LOG("Log level set to %s", 
                         log_level == 1 ? "DEBUG" : 
                         log_level == 2 ? "INFO" : 
                         log_level == 3 ? "WARN" : "ERROR");
                break;
            }
            
            case 9:  // Exit
                running = 0;
                break;
                
            default:
                printf("Invalid choice.\n");
                break;
        }
        
        // Small delay to allow logging to complete
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000; // 100ms
        nanosleep(&ts, NULL);
    }
    
    // Cleanup
    INFO_LOG("Cleaning up...");
    thread_manager_destroy(&manager);
    INFO_LOG("Thread manager destroyed");
    
    return 0;
}
