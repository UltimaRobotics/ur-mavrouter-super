/**
 * @file process_example.c
 * @brief Example usage of thread management API for system binary execution
 */

#define _POSIX_C_SOURCE 200809L  /* Must be defined before any includes */

#include "../include/thread_manager.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  /* For sleep */
#include <string.h>
#include <signal.h>  /* For kill() and signal handling */
#include <stdint.h>
#include <time.h>    /* For nanosleep() */

/* Explicit function declarations for nanosleep and kill to avoid warnings */
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 199309L)
extern int nanosleep(const struct timespec *req, struct timespec *rem);
extern int kill(pid_t pid, int sig);
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
    printf("\n=== Process Manager Example ===\n");
    printf("1. Run 'ls -la' command\n");
    printf("2. Run 'echo' command\n");
    printf("3. Run 'sleep' command\n");
    printf("4. List processes\n");
    printf("5. Pause process\n");
    printf("6. Resume process\n");
    printf("7. Stop process\n");
    printf("8. Restart process\n");
    printf("9. Send input to process\n");
    printf("10. Read output from process\n");
    printf("11. Set log level\n");
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
void list_processes() {
    unsigned int count = thread_get_count(&manager);
    printf("\n=== Processes (%u) ===\n", count);
    
    unsigned int *ids = (unsigned int *)malloc(count * sizeof(unsigned int));
    int num_ids = thread_get_all_ids(&manager, ids, count);
    
    if (num_ids == 0) {
        printf("No processes running.\n");
    } else {
        printf("ID\tState\tAlive\tCommand\n");
        printf("--\t-----\t-----\t-------\n");
        
        for (int i = 0; i < num_ids; i++) {
            thread_state_t state;
            thread_get_state(&manager, ids[i], &state);
            bool alive = thread_is_alive(&manager, ids[i]);
            
            // Get thread info to check if it's a process
            thread_info_t info;
            if (thread_get_info(&manager, ids[i], &info) == 0 && 
                info.type == THREAD_TYPE_PROCESS) {
                printf("%u\t%s\t%s\t%s\n", 
                       ids[i], 
                       thread_state_to_string(state), 
                       alive ? "Yes" : "No",
                       info.command);
            }
        }
    }
    
    free(ids);
}

/**
 * @brief Read and display output from a process
 * 
 * @param thread_id Thread ID
 */
void read_process_output(unsigned int thread_id) {
    char buffer[1024];
    int bytes_read;
    
    // Read from stdout
    printf("\nStandard output:\n");
    while ((bytes_read = thread_read_from_process(&manager, thread_id, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    
    // Read from stderr
    printf("\nStandard error:\n");
    while ((bytes_read = thread_read_error_from_process(&manager, thread_id, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    
    printf("\n");
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
            case 1: {  // Run 'ls -la' command
                char *args[] = {"ls", "-la", NULL};
                
                unsigned int new_id;
                if (thread_create_process(&manager, "ls", args, &new_id) > 0) {
                    INFO_LOG("Created process thread with ID %u for 'ls -la'", new_id);
                    
                    // Sleep briefly to allow command to execute
                    struct timespec ts = {0, 500000000}; // 500ms
                    nanosleep(&ts, NULL);
                    
                    // Read and display output
                    read_process_output(new_id);
                } else {
                    ERROR_LOG("Failed to create process thread for 'ls -la'");
                }
                break;
            }
            
            case 2: {  // Run 'echo' command
                char *args[] = {"echo", "Hello, World!", NULL};
                
                unsigned int new_id;
                if (thread_create_process(&manager, "echo", args, &new_id) > 0) {
                    INFO_LOG("Created process thread with ID %u for 'echo'", new_id);
                    
                    // Sleep briefly to allow command to execute
                    struct timespec ts2 = {0, 500000000}; // 500ms
                    nanosleep(&ts2, NULL);
                    
                    // Read and display output
                    read_process_output(new_id);
                } else {
                    ERROR_LOG("Failed to create process thread for 'echo'");
                }
                break;
            }
            
            case 3: {  // Run 'sleep' command
                char *args[] = {"sleep", "10", NULL};
                
                unsigned int new_id;
                if (thread_create_process(&manager, "sleep", args, &new_id) > 0) {
                    INFO_LOG("Created process thread with ID %u for 'sleep 10'", new_id);
                } else {
                    ERROR_LOG("Failed to create process thread for 'sleep 10'");
                }
                break;
            }
            
            case 4:  // List processes
                list_processes();
                break;
            
            case 5: {  // Pause process
                list_processes();
                printf("Enter process ID to pause: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid process ID");
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                if (thread_pause(&manager, thread_id) == 0) {
                    INFO_LOG("Process %u paused", thread_id);
                } else {
                    ERROR_LOG("Failed to pause process %u", thread_id);
                }
                break;
            }
            
            case 6: {  // Resume process
                list_processes();
                printf("Enter process ID to resume: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid process ID");
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                if (thread_resume(&manager, thread_id) == 0) {
                    INFO_LOG("Process %u resumed", thread_id);
                } else {
                    ERROR_LOG("Failed to resume process %u", thread_id);
                }
                break;
            }
            
            case 7: {  // Stop process
                list_processes();
                printf("Enter process ID to stop: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid process ID");
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                if (thread_stop(&manager, thread_id) == 0) {
                    INFO_LOG("Process %u stopped", thread_id);
                } else {
                    ERROR_LOG("Failed to stop process %u", thread_id);
                }
                break;
            }
            
            case 8: {  // Restart process
                list_processes();
                printf("Enter process ID to restart: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid process ID");
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                // Get thread info to determine process details
                thread_info_t info;
                if (thread_get_info(&manager, thread_id, &info) != 0) {
                    ERROR_LOG("Failed to get thread info for %u", thread_id);
                    break;
                }
                
                // Check if it's a process thread
                if (info.type != THREAD_TYPE_PROCESS) {
                    ERROR_LOG("Thread %u is not a process thread", thread_id);
                    break;
                }
                
                // Use NULL to keep the same arguments
                if (thread_restart(&manager, thread_id, NULL) == 0) {
                    INFO_LOG("Process %u restarted", thread_id);
                } else {
                    ERROR_LOG("Failed to restart process %u", thread_id);
                }
                break;
            }
            
            case 9: {  // Send input to process
                list_processes();
                printf("Enter process ID to send input to: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid process ID");
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                // Get input from user
                char input[256];
                printf("Enter input to send: ");
                if (fgets(input, sizeof(input), stdin) == NULL) {
                    WARN_LOG("Failed to read input");
                    break;
                }
                
                // Send input to process
                int bytes_written = thread_write_to_process(&manager, thread_id, input, strlen(input));
                if (bytes_written > 0) {
                    INFO_LOG("Sent %d bytes to process %u", bytes_written, thread_id);
                } else {
                    ERROR_LOG("Failed to send input to process %u", thread_id);
                }
                break;
            }
            
            case 10: {  // Read output from process
                list_processes();
                printf("Enter process ID to read output from: ");
                if (scanf("%u", &thread_id) != 1) {
                    WARN_LOG("Invalid process ID");
                    break;
                }
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                // Read and display output
                read_process_output(thread_id);
                break;
            }
            
            case 11: {  // Set log level
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
                
                // Clear input buffer
                while ((c = getchar()) != '\n' && c != EOF);
                
                set_log_level((log_level_t)(log_level - 1));
                INFO_LOG("Log level set to %s", 
                         log_level == 1 ? "DEBUG" : 
                         log_level == 2 ? "INFO" : 
                         log_level == 3 ? "WARN" : "ERROR");
                break;
            }
            
            case 0:  // Exit
                running = 0;
                break;
                
            default:
                printf("Invalid choice.\n");
                break;
        }
        
        // Small delay to allow logging to complete
        struct timespec ts3 = {0, 100000000}; // 100ms
        nanosleep(&ts3, NULL);
    }
    
    // Cleanup
    INFO_LOG("Cleaning up...");
    thread_manager_destroy(&manager);
    INFO_LOG("Thread manager destroyed");
    
    return 0;
}