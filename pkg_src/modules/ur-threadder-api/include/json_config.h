/**
 * @file json_config.h
 * @brief JSON configuration support for thread manager
 */

#ifndef JSON_CONFIG_H
#define JSON_CONFIG_H

#include "thread_manager.h"
#include "../thirdparty/cJSON.h"

/**
 * @brief Load thread configuration from a JSON file
 * 
 * @param manager The thread manager
 * @param filename The JSON configuration file
 * @return int 0 on success, -1 on failure
 */
int thread_manager_load_config(thread_manager_t *manager, const char *filename);

/**
 * @brief Save thread configuration to a JSON file
 * 
 * @param manager The thread manager
 * @param filename The JSON configuration file
 * @return int 0 on success, -1 on failure
 */
int thread_manager_save_config(const thread_manager_t *manager, const char *filename);

/**
 * @brief Create a thread from a JSON configuration
 * 
 * @param manager The thread manager
 * @param json_config The JSON configuration string
 * @param thread_id Pointer to store the created thread ID
 * @return int 0 on success, -1 on failure
 */
int thread_create_from_json(thread_manager_t *manager, const char *json_config, unsigned int *thread_id);

/**
 * @brief Create a process from a JSON configuration
 * 
 * @param manager The thread manager 
 * @param json_config The JSON configuration string
 * @param thread_id Pointer to store the created thread ID
 * @return int 0 on success, -1 on failure
 */
int thread_create_process_from_json(thread_manager_t *manager, const char *json_config, unsigned int *thread_id);

/**
 * @brief Get thread configuration as a JSON string
 * 
 * @param manager The thread manager
 * @param thread_id The thread ID
 * @return char* The JSON configuration string (must be freed by the caller)
 */
char *thread_get_json_config(thread_manager_t *manager, unsigned int thread_id);

/**
 * @brief Update thread based on JSON configuration
 * 
 * @param manager The thread manager
 * @param thread_id The thread ID
 * @param json_config The JSON configuration string
 * @return int 0 on success, -1 on failure
 */
int thread_update_from_json(thread_manager_t *manager, unsigned int thread_id, const char *json_config);

#endif /* JSON_CONFIG_H */