/**
 * @file json_config_example.c
 * @brief Example demonstrating JSON configuration for thread management
 */

#define _POSIX_C_SOURCE 200809L  /* Must be defined before any includes */

#include "../include/thread_manager.h"
#include "../include/json_config.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>    /* For nanosleep */

/* Explicit function declarations for nanosleep to avoid warnings */
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 199309L)
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif

// Global thread manager for signal handler
thread_manager_t manager;
volatile sig_atomic_t running = 1;

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
    printf("\n=== JSON Configuration Example ===\n");
    printf("1. Create thread from JSON\n");
    printf("2. Create process from JSON\n");
    printf("3. List threads\n");
    printf("4. Get thread JSON configuration\n");
    printf("5. Update thread from JSON\n");
    printf("6. Save configuration to file\n");
    printf("7. Load configuration from file\n");
    printf("8. Set log level\n");
    printf("0. Exit\n");
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
        printf("ID\tState\tAlive\tType\n");
        printf("--\t-----\t-----\t----\n");
        
        for (int i = 0; i < num_ids; i++) {
            thread_state_t state;
            thread_get_state(&manager, ids[i], &state);
            bool alive = thread_is_alive(&manager, ids[i]);
            
            // Get thread info to check type
            thread_info_t info;
            if (thread_get_info(&manager, ids[i], &info) == 0) {
                printf("%u\t%s\t%s\t%s\n", 
                       ids[i], 
                       thread_state_to_string(state), 
                       alive ? "Yes" : "No",
                       info.type == THREAD_TYPE_NORMAL ? "Thread" : "Process");
            }
        }
    }
    
    free(ids);
}

/**
 * @brief Sample worker thread function
 * 
 * @param arg Thread arguments
 * @return void* Thread result
 */
void *worker_thread(void *arg) {
    int iterations = 0;
    
    if (arg) {
        iterations = *((int *)arg);
        free(arg);
    } else {
        iterations = 10; // Default
    }
    
    INFO_LOG("Worker thread starting with %d iterations", iterations);
    
    for (int i = 0; i < iterations; i++) {
        INFO_LOG("Worker thread iteration %d/%d", i + 1, iterations);
        sleep(1);
        
        // Check if thread should exit
        thread_manager_t *mgr = &manager;
        if (thread_should_exit(mgr, 0)) {
            INFO_LOG("Worker thread received exit signal");
            break;
        }
        
        // Check if thread should pause
        thread_check_pause(mgr, 0);
    }
    
    INFO_LOG("Worker thread finished");
    return NULL;
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
        
        // Clear input buffer
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        switch (choice) {
            case 1: {  // Create thread from JSON
                char json_config[512];
                printf("Enter JSON configuration for thread:\n");
                printf("Example: {\"type\":\"thread\",\"function\":\"worker_thread\",\"args\":{\"iterations\":5}}\n");
                
                if (fgets(json_config, sizeof(json_config), stdin) == NULL) {
                    WARN_LOG("Failed to read input");
                    break;
                }
                
                unsigned int new_id;
                if (thread_create_from_json(&manager, json_config, &new_id) == 0) {
                    INFO_LOG("Created thread with ID %u from JSON", new_id);
                } else {
                    ERROR_LOG("Failed to create thread from JSON");
                }
                break;
            }
            
            case 2: {  // Create process from JSON
                char json_config[512];
                printf("Enter JSON configuration for process:\n");
                printf("Example: {\"type\":\"process\",\"command\":\"ls\",\"args\":[\"-la\"]}\n");
                
                if (fgets(json_config, sizeof(json_config), stdin) == NULL) {
                    WARN_LOG("Failed to read input");
                    break;
                }
                
                unsigned int new_id;
                if (thread_create_process_from_json(&manager, json_config, &new_id) == 0) {
                    INFO_LOG("Created process with ID %u from JSON", new_id);
                } else {
                    ERROR_LOG("Failed to create process from JSON");
                }
                break;
            }
            
            case 3:  // List threads
                list_threads();
                break;
            
            case 4: {  // Get thread JSON configuration
                list_threads();
                printf("Enter thread ID to get configuration: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid thread ID");
                    // Clear input buffer
                    while ((c = getchar()) != '\n' && c != EOF);
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                char *config = thread_get_json_config(&manager, thread_id);
                if (config) {
                    printf("\nJSON Configuration for thread %u:\n", thread_id);
                    printf("%s\n", config);
                    free(config);
                } else {
                    ERROR_LOG("Failed to get JSON configuration for thread %u", thread_id);
                }
                break;
            }
            
            case 5: {  // Update thread from JSON
                list_threads();
                printf("Enter thread ID to update: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid thread ID");
                    // Clear input buffer
                    while ((c = getchar()) != '\n' && c != EOF);
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                char json_config[512];
                printf("Enter JSON configuration update:\n");
                printf("Example: {\"state\":\"paused\"} or {\"state\":\"running\"} or {\"state\":\"restart\",\"args\":{\"iterations\":3}}\n");
                
                if (fgets(json_config, sizeof(json_config), stdin) == NULL) {
                    WARN_LOG("Failed to read input");
                    break;
                }
                
                if (thread_update_from_json(&manager, thread_id, json_config) == 0) {
                    INFO_LOG("Updated thread %u from JSON", thread_id);
                } else {
                    ERROR_LOG("Failed to update thread %u from JSON", thread_id);
                }
                break;
            }
            
            case 6: {  // Save configuration to file
                char filename[256];
                printf("Enter filename to save configuration: ");
                if (fgets(filename, sizeof(filename), stdin) == NULL) {
                    WARN_LOG("Failed to read input");
                    break;
                }
                
                // Remove newline if present
                size_t len = strlen(filename);
                if (len > 0 && filename[len - 1] == '\n') {
                    filename[len - 1] = '\0';
                }
                
                if (thread_manager_save_config(&manager, filename) == 0) {
                    INFO_LOG("Saved configuration to %s", filename);
                } else {
                    ERROR_LOG("Failed to save configuration to %s", filename);
                }
                break;
            }
            
            case 7: {  // Load configuration from file
                char filename[256];
                printf("Enter filename to load configuration: ");
                if (fgets(filename, sizeof(filename), stdin) == NULL) {
                    WARN_LOG("Failed to read input");
                    break;
                }
                
                // Remove newline if present
                size_t len = strlen(filename);
                if (len > 0 && filename[len - 1] == '\n') {
                    filename[len - 1] = '\0';
                }
                
                if (thread_manager_load_config(&manager, filename) == 0) {
                    INFO_LOG("Loaded configuration from %s", filename);
                } else {
                    ERROR_LOG("Failed to load configuration from %s", filename);
                }
                break;
            }
            
            case 8: {  // Set log level
                printf("Log levels:\n");
                printf("0 - DEBUG\n");
                printf("1 - INFO\n");
                printf("2 - WARN\n");
                printf("3 - ERROR\n");
                printf("Enter log level: ");
                
                int level;
                if (scanf("%d", &level) != 1 || level < 0 || level > 3) {
                    WARN_LOG("Invalid log level");
                    // Clear input buffer
                    while ((c = getchar()) != '\n' && c != EOF);
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                set_log_level(level);
                INFO_LOG("Log level set to %d", level);
                break;
            }
            
            case 0:  // Exit
                running = 0;
                break;
                
            default:
                printf("Invalid choice\n");
                break;
        }
    }
    
    // Cleanup
    INFO_LOG("Cleaning up...");
    thread_manager_destroy(&manager);
    INFO_LOG("Thread manager destroyed");
    
    return 0;
}