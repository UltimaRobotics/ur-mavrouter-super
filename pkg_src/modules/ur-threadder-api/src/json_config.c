/**
 * @file json_config.c
 * @brief Implementation of JSON configuration support for thread manager
 */

#define _POSIX_C_SOURCE 200809L  /* Must be defined before any includes */

#include "../include/json_config.h"
#include "../include/thread_manager.h"
#include "../include/utils.h"
#include "../thirdparty/cJSON.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <time.h>    /* For nanosleep() - POSIX */
#include <unistd.h>  /* For general POSIX functions */
#include <signal.h>  /* For kill() */

/* Add missing function declarations */
#if defined(__APPLE__) || !defined(_GNU_SOURCE)
extern char *strdup(const char *s);
#endif

/* Explicit function declarations for nanosleep to avoid warnings */
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 199309L)
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif

/**
 * @brief Load thread configuration from a JSON file
 * 
 * @param manager The thread manager
 * @param filename The JSON configuration file
 * @return int 0 on success, -1 on failure
 */
int thread_manager_load_config(thread_manager_t *manager, const char *filename) {
    FILE *file = NULL;
    long file_size = 0;
    char *file_content = NULL;
    cJSON *root = NULL;
    cJSON *threads = NULL;
    cJSON *thread = NULL;
    int result = -1;
    
    if (!manager || !filename) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Open file
    file = fopen(filename, "r");
    if (!file) {
        ERROR_LOG("Failed to open file: %s", filename);
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer for file content
    file_content = (char *)malloc(file_size + 1);
    if (!file_content) {
        ERROR_LOG("Failed to allocate memory for file content");
        fclose(file);
        return -1;
    }
    
    // Read file content
    if (fread(file_content, 1, file_size, file) != (size_t)file_size) {
        ERROR_LOG("Failed to read file content");
        free(file_content);
        fclose(file);
        return -1;
    }
    file_content[file_size] = '\0';
    
    // Close file
    fclose(file);
    
    // Parse JSON
    root = cJSON_Parse(file_content);
    free(file_content);
    
    if (!root) {
        ERROR_LOG("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return -1;
    }
    
    // Get threads array
    threads = cJSON_GetObjectItem(root, "threads");
    if (!threads || !cJSON_IsArray(threads)) {
        ERROR_LOG("Invalid JSON: 'threads' must be an array");
        cJSON_Delete(root);
        return -1;
    }
    
    // Process each thread
    result = 0;
    cJSON_ArrayForEach(thread, threads) {
        unsigned int thread_id = 0;
        char *thread_json = cJSON_Print(thread);
        
        // Determine thread type
        cJSON *type = cJSON_GetObjectItem(thread, "type");
        if (!type || !cJSON_IsString(type)) {
            WARN_LOG("Thread has no type specified, skipping");
            free(thread_json);
            continue;
        }
        
        // Create thread based on type
        if (strcmp(type->valuestring, "thread") == 0) {
            if (thread_create_from_json(manager, thread_json, &thread_id) != 0) {
                ERROR_LOG("Failed to create thread from JSON");
                result = -1;
            } else {
                INFO_LOG("Created thread with ID %u from JSON", thread_id);
            }
        } else if (strcmp(type->valuestring, "process") == 0) {
            if (thread_create_process_from_json(manager, thread_json, &thread_id) != 0) {
                ERROR_LOG("Failed to create process from JSON");
                result = -1;
            } else {
                INFO_LOG("Created process with ID %u from JSON", thread_id);
            }
        } else {
            WARN_LOG("Unknown thread type: %s, skipping", type->valuestring);
        }
        
        free(thread_json);
    }
    
    cJSON_Delete(root);
    return result;
}

/**
 * @brief Save thread configuration to a JSON file
 * 
 * @param manager The thread manager
 * @param filename The JSON configuration file
 * @return int 0 on success, -1 on failure
 */
int thread_manager_save_config(const thread_manager_t *manager, const char *filename) {
    FILE *file = NULL;
    cJSON *root = NULL;
    cJSON *threads = NULL;
    unsigned int *thread_ids = NULL;
    int thread_count = 0;
    char *json_string = NULL;
    
    if (!manager || !filename) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Create root object
    root = cJSON_CreateObject();
    if (!root) {
        ERROR_LOG("Failed to create root JSON object");
        return -1;
    }
    
    // Create threads array
    threads = cJSON_CreateArray();
    if (!threads) {
        ERROR_LOG("Failed to create threads JSON array");
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddItemToObject(root, "threads", threads);
    
    // Get all thread IDs
    thread_count = thread_get_count((thread_manager_t *)manager);
    if (thread_count <= 0) {
        // No threads to save
        json_string = cJSON_Print(root);
        file = fopen(filename, "w");
        if (!file) {
            ERROR_LOG("Failed to open file: %s", filename);
            cJSON_Delete(root);
            return -1;
        }
        
        fputs(json_string, file);
        fclose(file);
        free(json_string);
        cJSON_Delete(root);
        return 0;
    }
    
    thread_ids = (unsigned int *)malloc(sizeof(unsigned int) * thread_count);
    if (!thread_ids) {
        ERROR_LOG("Failed to allocate memory for thread IDs");
        cJSON_Delete(root);
        return -1;
    }
    
    thread_get_all_ids((thread_manager_t *)manager, thread_ids, thread_count);
    
    // Add each thread to the array
    for (int i = 0; i < thread_count; i++) {
        char *thread_json = thread_get_json_config((thread_manager_t *)manager, thread_ids[i]);
        if (thread_json) {
            cJSON *thread = cJSON_Parse(thread_json);
            if (thread) {
                cJSON_AddItemToArray(threads, thread);
            } else {
                WARN_LOG("Failed to parse thread JSON for thread %u", thread_ids[i]);
            }
            free(thread_json);
        } else {
            WARN_LOG("Failed to get JSON config for thread %u", thread_ids[i]);
        }
    }
    
    // Convert JSON to string
    json_string = cJSON_Print(root);
    if (!json_string) {
        ERROR_LOG("Failed to convert JSON to string");
        free(thread_ids);
        cJSON_Delete(root);
        return -1;
    }
    
    // Write to file
    file = fopen(filename, "w");
    if (!file) {
        ERROR_LOG("Failed to open file: %s", filename);
        free(json_string);
        free(thread_ids);
        cJSON_Delete(root);
        return -1;
    }
    
    fputs(json_string, file);
    fclose(file);
    
    free(json_string);
    free(thread_ids);
    cJSON_Delete(root);
    
    return 0;
}

/**
 * @brief Create a thread from a JSON configuration
 * 
 * @param manager The thread manager
 * @param json_config The JSON configuration string
 * @param thread_id Pointer to store the created thread ID
 * @return int 0 on success, -1 on failure
 * 
 * JSON format:
 * {
 *   "type": "thread",
 *   "function": "worker_thread",
 *   "args": {
 *     "param1": "value1",
 *     "param2": 42
 *   }
 * }
 * 
 * Note: Since we can't directly specify a function pointer in JSON,
 * we use a string identifier for the function and then map it to
 * the actual function pointer in the implementation.
 */
int thread_create_from_json(thread_manager_t *manager, const char *json_config, unsigned int *thread_id) {
    cJSON *root = NULL;
    cJSON *function = NULL;
    cJSON *args = NULL;
    int result = -1;
    
    if (!manager || !json_config || !thread_id) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Parse JSON
    root = cJSON_Parse(json_config);
    if (!root) {
        ERROR_LOG("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return -1;
    }
    
    // Verify thread type
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "thread") != 0) {
        ERROR_LOG("Invalid JSON: 'type' must be 'thread'");
        cJSON_Delete(root);
        return -1;
    }
    
    // Get function name
    function = cJSON_GetObjectItem(root, "function");
    if (!function || !cJSON_IsString(function)) {
        ERROR_LOG("Invalid JSON: 'function' must be a string");
        cJSON_Delete(root);
        return -1;
    }
    
    // Get arguments
    args = cJSON_GetObjectItem(root, "args");
    if (!args) {
        ERROR_LOG("Invalid JSON: 'args' is required");
        cJSON_Delete(root);
        return -1;
    }
    
    // Create thread args object (this would be a custom struct for each function)
    // For now, we'll use a simple structure to hold the JSON args
    struct json_thread_args {
        char *json_args;
    };
    
    // Allocate memory for arguments
    struct json_thread_args *thread_args = (struct json_thread_args *)malloc(sizeof(struct json_thread_args));
    if (!thread_args) {
        ERROR_LOG("Failed to allocate memory for thread arguments");
        cJSON_Delete(root);
        return -1;
    }
    
    // Store JSON args as a string
    thread_args->json_args = cJSON_Print(args);
    
    // In a real implementation, you would have a mapping from function names to function pointers
    // For this example, we're going to use a placeholder function
    extern void *generic_json_thread_function(void *arg);
    
    // Create the thread
    result = thread_create(manager, generic_json_thread_function, thread_args, thread_id);
    
    cJSON_Delete(root);
    
    return result;
}

/**
 * @brief Create a process from a JSON configuration
 * 
 * @param manager The thread manager 
 * @param json_config The JSON configuration string
 * @param thread_id Pointer to store the created thread ID
 * @return int 0 on success, -1 on failure
 * 
 * JSON format:
 * {
 *   "type": "process",
 *   "command": "ls",
 *   "args": ["-la", "/tmp"]
 * }
 */
int thread_create_process_from_json(thread_manager_t *manager, const char *json_config, unsigned int *thread_id) {
    cJSON *root = NULL;
    cJSON *command = NULL;
    cJSON *args_array = NULL;
    char **args = NULL;
    int args_count = 0;
    int result = -1;
    
    if (!manager || !json_config || !thread_id) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Parse JSON
    root = cJSON_Parse(json_config);
    if (!root) {
        ERROR_LOG("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return -1;
    }
    
    // Verify thread type
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "process") != 0) {
        ERROR_LOG("Invalid JSON: 'type' must be 'process'");
        cJSON_Delete(root);
        return -1;
    }
    
    // Get command
    command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command)) {
        ERROR_LOG("Invalid JSON: 'command' must be a string");
        cJSON_Delete(root);
        return -1;
    }
    
    // Get arguments array
    args_array = cJSON_GetObjectItem(root, "args");
    if (args_array && cJSON_IsArray(args_array)) {
        args_count = cJSON_GetArraySize(args_array);
        args = (char **)malloc(sizeof(char *) * (args_count + 2)); // +1 for command name, +1 for NULL terminator
        
        if (!args) {
            ERROR_LOG("Failed to allocate memory for process arguments");
            cJSON_Delete(root);
            return -1;
        }
        
        // First argument is the command name
        args[0] = strdup(command->valuestring);
        
        // Parse arguments
        for (int i = 0; i < args_count; i++) {
            cJSON *arg = cJSON_GetArrayItem(args_array, i);
            if (arg && cJSON_IsString(arg)) {
                args[i + 1] = strdup(arg->valuestring);
            } else {
                // Skip non-string arguments
                args[i + 1] = strdup("");
                WARN_LOG("Process argument %d is not a string, using empty string", i);
            }
        }
        
        // NULL-terminate arguments array
        args[args_count + 1] = NULL;
    } else {
        // No arguments or invalid format
        args = (char **)malloc(sizeof(char *) * 2); // Just command name and NULL terminator
        if (!args) {
            ERROR_LOG("Failed to allocate memory for process arguments");
            cJSON_Delete(root);
            return -1;
        }
        
        args[0] = strdup(command->valuestring);
        args[1] = NULL;
        args_count = 0;
    }
    
    // Create the process
    result = thread_create_process(manager, command->valuestring, args, thread_id);
    
    // Free args memory
    if (args) {
        for (int i = 0; i <= args_count; i++) {
            if (args[i]) {
                free(args[i]);
            }
        }
        free(args);
    }
    
    cJSON_Delete(root);
    
    return result;
}

/**
 * @brief Get thread configuration as a JSON string
 * 
 * @param manager The thread manager
 * @param thread_id The thread ID
 * @return char* The JSON configuration string (must be freed by the caller)
 */
char *thread_get_json_config(thread_manager_t *manager, unsigned int thread_id) {
    thread_info_t info;
    cJSON *root = NULL;
    char *result = NULL;
    
    if (!manager || !thread_id) {
        ERROR_LOG("Invalid parameters");
        return NULL;
    }
    
    if (thread_get_info(manager, thread_id, &info) != 0) {
        ERROR_LOG("Failed to get thread info for thread %u", thread_id);
        return NULL;
    }
    
    // Create root object
    root = cJSON_CreateObject();
    if (!root) {
        ERROR_LOG("Failed to create root JSON object");
        return NULL;
    }
    
    // Add thread ID
    cJSON_AddNumberToObject(root, "id", thread_id);
    
    // Add thread state
    switch (info.state) {
        case THREAD_CREATED:
            cJSON_AddStringToObject(root, "state", "created");
            break;
        case THREAD_RUNNING:
            cJSON_AddStringToObject(root, "state", "running");
            break;
        case THREAD_PAUSED:
            cJSON_AddStringToObject(root, "state", "paused");
            break;
        case THREAD_STOPPED:
            cJSON_AddStringToObject(root, "state", "stopped");
            break;
        case THREAD_ERROR:
            cJSON_AddStringToObject(root, "state", "error");
            break;
        default:
            cJSON_AddStringToObject(root, "state", "unknown");
            break;
    }
    
    // Add thread type
    if (info.type == THREAD_TYPE_NORMAL) {
        cJSON_AddStringToObject(root, "type", "thread");
    } else if (info.type == THREAD_TYPE_PROCESS) {
        cJSON_AddStringToObject(root, "type", "process");
        
        // Add command and arguments for process
        if (info.command) {
            cJSON_AddStringToObject(root, "command", info.command);
            
            // Add arguments array
            cJSON *args_array = cJSON_CreateArray();
            if (args_array && info.args) {
                for (int i = 1; info.args[i] != NULL; i++) {
                    cJSON_AddItemToArray(args_array, cJSON_CreateString(info.args[i]));
                }
                cJSON_AddItemToObject(root, "args", args_array);
            }
        }
    } else {
        cJSON_AddStringToObject(root, "type", "unknown");
    }
    
    // Generate JSON string
    result = cJSON_Print(root);
    cJSON_Delete(root);
    
    return result;
}

/**
 * @brief Update thread based on JSON configuration
 * 
 * @param manager The thread manager
 * @param thread_id The thread ID
 * @param json_config The JSON configuration string
 * @return int 0 on success, -1 on failure
 */
int thread_update_from_json(thread_manager_t *manager, unsigned int thread_id, const char *json_config) {
    cJSON *root = NULL;
    thread_info_t info;
    int result = 0;
    
    if (!manager || !json_config) {
        ERROR_LOG("Invalid parameters");
        return -1;
    }
    
    // Get thread info
    if (thread_get_info(manager, thread_id, &info) != 0) {
        ERROR_LOG("Failed to get thread info for thread %u", thread_id);
        return -1;
    }
    
    // Parse JSON
    root = cJSON_Parse(json_config);
    if (!root) {
        ERROR_LOG("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return -1;
    }
    
    // Process state update if specified
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (state && cJSON_IsString(state)) {
        if (strcmp(state->valuestring, "running") == 0) {
            if (info.state == THREAD_PAUSED) {
                result = thread_resume(manager, thread_id);
            }
        } else if (strcmp(state->valuestring, "paused") == 0) {
            if (info.state == THREAD_RUNNING) {
                result = thread_pause(manager, thread_id);
            }
        } else if (strcmp(state->valuestring, "stopped") == 0) {
            if (info.state != THREAD_STOPPED) {
                result = thread_stop(manager, thread_id);
            }
        } else if (strcmp(state->valuestring, "restart") == 0) {
            // For restart, we need to check if there are new args
            cJSON *args = cJSON_GetObjectItem(root, "args");
            if (args) {
                // Convert args to appropriate format based on thread type
                if (info.type == THREAD_TYPE_NORMAL) {
                    // For thread function, create new args struct
                    struct json_thread_args {
                        char *json_args;
                    };
                    
                    struct json_thread_args *thread_args = (struct json_thread_args *)malloc(sizeof(struct json_thread_args));
                    if (thread_args) {
                        thread_args->json_args = cJSON_Print(args);
                        result = thread_restart(manager, thread_id, thread_args);
                    } else {
                        ERROR_LOG("Failed to allocate memory for thread arguments");
                        result = -1;
                    }
                } else if (info.type == THREAD_TYPE_PROCESS) {
                    // For process, we need to create a new args array
                    if (cJSON_IsArray(args)) {
                        cJSON *command = cJSON_GetObjectItem(root, "command");
                        if (command && cJSON_IsString(command)) {
                            // Use new command if specified, otherwise use existing
                            const char *cmd = command->valuestring;
                            
                            int args_count = cJSON_GetArraySize(args);
                            char **new_args = (char **)malloc(sizeof(char *) * (args_count + 2));
                            
                            if (new_args) {
                                new_args[0] = strdup(cmd);
                                
                                for (int i = 0; i < args_count; i++) {
                                    cJSON *arg = cJSON_GetArrayItem(args, i);
                                    if (arg && cJSON_IsString(arg)) {
                                        new_args[i + 1] = strdup(arg->valuestring);
                                    } else {
                                        new_args[i + 1] = strdup("");
                                    }
                                }
                                
                                new_args[args_count + 1] = NULL;
                                
                                // Restart process with new args
                                result = thread_restart(manager, thread_id, new_args);
                                
                                // Free args memory (thread_restart makes its own copy)
                                for (int i = 0; i <= args_count; i++) {
                                    if (new_args[i]) {
                                        free(new_args[i]);
                                    }
                                }
                                free(new_args);
                            } else {
                                ERROR_LOG("Failed to allocate memory for process arguments");
                                result = -1;
                            }
                        } else {
                            ERROR_LOG("Command not specified for process restart");
                            result = -1;
                        }
                    } else {
                        ERROR_LOG("Invalid args format for process");
                        result = -1;
                    }
                } else {
                    ERROR_LOG("Unknown thread type");
                    result = -1;
                }
            } else {
                // Restart with NULL args (use existing args)
                result = thread_restart(manager, thread_id, NULL);
            }
        } else {
            WARN_LOG("Unknown state: %s", state->valuestring);
        }
    }
    
    cJSON_Delete(root);
    
    return result;
}

/**
 * @brief Generic JSON thread function for handling JSON-configured threads
 * 
 * This function takes a json_thread_args structure, parses the JSON arguments,
 * and performs the appropriate actions based on the arguments.
 * 
 * @param arg Thread arguments (json_thread_args structure)
 * @return void* Thread result (NULL for this implementation)
 */
void *generic_json_thread_function(void *arg) {
    if (!arg) {
        return NULL;
    }
    
    struct json_thread_args {
        char *json_args;
    };
    
    struct json_thread_args *args = (struct json_thread_args *)arg;
    
    // Parse JSON args if available
    if (args->json_args) {
        cJSON *json = cJSON_Parse(args->json_args);
        if (json) {
            // Example of parsing a specific parameter
            cJSON *iterations_json = cJSON_GetObjectItem(json, "iterations");
            int iterations = 10; // Default value
            
            if (iterations_json && cJSON_IsNumber(iterations_json)) {
                iterations = iterations_json->valueint;
            }
            
            INFO_LOG("JSON thread starting with %d iterations", iterations);
            
            // Simulate some work
            for (int i = 0; i < iterations; i++) {
                INFO_LOG("JSON thread iteration %d/%d", i + 1, iterations);
                
                // Sleep for a bit
                struct timespec ts = {1, 0}; // 1 second
                nanosleep(&ts, NULL);
                
                // Check if thread should exit
                if (thread_should_exit(NULL, 0)) {
                    INFO_LOG("JSON thread received exit signal");
                    break;
                }
                
                // Check if thread should pause
                thread_check_pause(NULL, 0);
            }
            
            cJSON_Delete(json);
        }
    }
    
    INFO_LOG("JSON thread finished");
    
    // Free memory
    if (args->json_args) {
        free(args->json_args);
    }
    free(args);
    
    return NULL;
}