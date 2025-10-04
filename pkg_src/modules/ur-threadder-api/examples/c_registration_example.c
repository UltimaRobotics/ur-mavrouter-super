/**
 * @file c_registration_example.c
 * @brief C example demonstrating thread registration functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../include/thread_manager.h"

// Worker function that prints messages
void* worker_function(void* arg) {
    char* worker_name = (char*)arg;
    printf("Worker '%s' starting...\n", worker_name);
    
    for (int i = 0; i < 5; i++) {
        printf("Worker '%s' iteration %d/5\n", worker_name, i + 1);
        sleep(1);
    }
    
    printf("Worker '%s' completed!\n", worker_name);
    return NULL;
}

// Long-running worker function
void* long_worker(void* arg) {
    char* worker_name = (char*)arg;
    printf("Long worker '%s' starting...\n", worker_name);
    
    for (int i = 0; i < 20; i++) {
        printf("Long worker '%s' iteration %d/20\n", worker_name, i + 1);
        sleep(1);
    }
    
    printf("Long worker '%s' completed!\n", worker_name);
    return NULL;
}

int main() {
    printf("=== Thread Registration C Example ===\n\n");
    
    thread_manager_t manager;
    
    // Initialize thread manager
    if (thread_manager_init(&manager, 10) != 0) {
        fprintf(stderr, "Failed to initialize thread manager\n");
        return 1;
    }
    
    printf("1. Creating and registering threads...\n");
    
    // Create and register some threads
    unsigned int thread1_id, thread2_id, thread3_id;
    char* worker1_name = strdup("Worker-Alpha");
    char* worker2_name = strdup("Worker-Beta");
    char* worker3_name = strdup("Long-Gamma");
    
    if (thread_create(&manager, worker_function, worker1_name, &thread1_id) == 0) {
        printf("Created thread %u\n", thread1_id);
        
        // Register thread with attachment ID
        if (thread_register(&manager, thread1_id, "alpha-worker") == 0) {
            printf("Registered thread %u with attachment 'alpha-worker'\n", thread1_id);
        }
    }
    
    if (thread_create(&manager, worker_function, worker2_name, &thread2_id) == 0) {
        printf("Created thread %u\n", thread2_id);
        
        // Register thread with attachment ID
        if (thread_register(&manager, thread2_id, "beta-worker") == 0) {
            printf("Registered thread %u with attachment 'beta-worker'\n", thread2_id);
        }
    }
    
    if (thread_create(&manager, long_worker, worker3_name, &thread3_id) == 0) {
        printf("Created thread %u\n", thread3_id);
        
        // Register thread with attachment ID
        if (thread_register(&manager, thread3_id, "gamma-long-worker") == 0) {
            printf("Registered thread %u with attachment 'gamma-long-worker'\n", thread3_id);
        }
    }
    
    printf("\n2. Listing all registered attachments...\n");
    
    // Get all attachments
    char* attachments[10];
    int attachment_count = thread_get_all_attachments(&manager, attachments, 10);
    
    if (attachment_count > 0) {
        printf("Found %d registered attachments:\n", attachment_count);
        for (int i = 0; i < attachment_count; i++) {
            printf("  - %s\n", attachments[i]);
            free(attachments[i]); // Free the strings
        }
    }
    
    printf("\n3. Finding threads by attachment...\n");
    
    // Find thread by attachment
    unsigned int found_id;
    if (thread_find_by_attachment(&manager, "beta-worker", &found_id) == 0) {
        printf("Found thread %u for attachment 'beta-worker'\n", found_id);
    }
    
    printf("\n4. Stopping a thread by attachment...\n");
    
    // Stop thread by attachment
    if (thread_stop_by_attachment(&manager, "gamma-long-worker") == 0) {
        printf("Stopped thread with attachment 'gamma-long-worker'\n");
    }
    
    printf("\n5. Waiting for threads to complete...\n");
    
    // Wait for threads to complete
    void* result;
    if (thread_join(&manager, thread1_id, &result) == 0) {
        printf("Thread %u completed\n", thread1_id);
    }
    
    if (thread_join(&manager, thread2_id, &result) == 0) {
        printf("Thread %u completed\n", thread2_id);
    }
    
    if (thread_join(&manager, thread3_id, &result) == 0) {
        printf("Thread %u completed\n", thread3_id);
    }
    
    printf("\n6. Cleaning up...\n");
    
    // Unregister threads
    thread_unregister(&manager, "alpha-worker");
    thread_unregister(&manager, "beta-worker");
    thread_unregister(&manager, "gamma-long-worker");
    
    // Clean up
    free(worker1_name);
    free(worker2_name);
    free(worker3_name);
    
    // Destroy thread manager
    thread_manager_destroy(&manager);
    
    printf("Thread registration example completed!\n");
    return 0;
}