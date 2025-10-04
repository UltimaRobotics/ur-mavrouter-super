/**
 * @file registration_example.cpp
 * @brief C++ example demonstrating thread registration functionality
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include "../include/ThreadManager.hpp"

using namespace ThreadMgr;

// Worker function for threads
void worker_function(const std::string& name, int iterations) {
    std::cout << "Worker '" << name << "' starting..." << std::endl;
    
    for (int i = 0; i < iterations; i++) {
        std::cout << "Worker '" << name << "' iteration " << (i + 1) << "/" << iterations << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "Worker '" << name << "' completed!" << std::endl;
}

// Long-running worker function
void long_running_task(const std::string& name) {
    std::cout << "Long-running task '" << name << "' starting..." << std::endl;
    
    for (int i = 0; i < 10; i++) {
        std::cout << "Long task '" << name << "' progress: " << (i + 1) << "/10" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    
    std::cout << "Long-running task '" << name << "' completed!" << std::endl;
}

// Static function for demonstration
void static_worker(int value) {
    std::cout << "Static worker processing value: " << value << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Static worker finished with value: " << value << std::endl;
}

int main() {
    std::cout << "=== Thread Registration C++ Example ===" << std::endl << std::endl;
    
    try {
        // Initialize thread manager
        ThreadManager manager(15);
        
        std::cout << "1. Creating and registering threads..." << std::endl;
        
        // Create and register threads with different attachment IDs
        auto thread1 = manager.createThread(worker_function, "Alpha", 5);
        manager.registerThread(thread1, "alpha-worker");
        std::cout << "Created and registered thread " << thread1 << " with attachment 'alpha-worker'" << std::endl;
        
        auto thread2 = manager.createThread(worker_function, "Beta", 3);
        manager.registerThread(thread2, "beta-worker");
        std::cout << "Created and registered thread " << thread2 << " with attachment 'beta-worker'" << std::endl;
        
        auto thread3 = manager.createThread(long_running_task, "Gamma-Long");
        manager.registerThread(thread3, "gamma-long-task");
        std::cout << "Created and registered thread " << thread3 << " with attachment 'gamma-long-task'" << std::endl;
        
        auto thread4 = manager.createThread(static_worker, 42);
        manager.registerThread(thread4, "static-worker-42");
        std::cout << "Created and registered thread " << thread4 << " with attachment 'static-worker-42'" << std::endl;
        
        std::cout << std::endl << "2. Listing all registered attachments..." << std::endl;
        
        // Get all attachments
        auto attachments = manager.getAllAttachments();
        std::cout << "Found " << attachments.size() << " registered attachments:" << std::endl;
        for (const auto& attachment : attachments) {
            std::cout << "  - " << attachment << std::endl;
        }
        
        std::cout << std::endl << "3. Finding threads by attachment..." << std::endl;
        
        // Find thread by attachment
        try {
            auto foundId = manager.findThreadByAttachment("beta-worker");
            std::cout << "Found thread " << foundId << " for attachment 'beta-worker'" << std::endl;
            
            // Check thread state
            auto state = manager.getThreadState(foundId);
            std::cout << "Thread " << foundId << " is in state: " << static_cast<int>(state) << std::endl;
        } catch (const ThreadManagerException& e) {
            std::cout << "Error finding thread: " << e.what() << std::endl;
        }
        
        std::cout << std::endl << "4. Demonstrating thread control by attachment..." << std::endl;
        
        // Let threads run for a bit
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Stop a thread by attachment
        try {
            std::cout << "Stopping thread with attachment 'gamma-long-task'..." << std::endl;
            manager.stopThreadByAttachment("gamma-long-task");
            std::cout << "Thread stopped successfully" << std::endl;
        } catch (const ThreadManagerException& e) {
            std::cout << "Error stopping thread: " << e.what() << std::endl;
        }
        
        std::cout << std::endl << "5. Restarting a thread with new parameters..." << std::endl;
        
        // Restart a thread with new parameters
        try {
            std::cout << "Restarting 'static-worker-42' with new value..." << std::endl;
            manager.restartThreadByAttachment("static-worker-42", static_worker, 100);
            std::cout << "Thread restarted successfully with value 100" << std::endl;
        } catch (const ThreadManagerException& e) {
            std::cout << "Error restarting thread: " << e.what() << std::endl;
        }
        
        std::cout << std::endl << "6. Using RAII Thread wrapper with registration..." << std::endl;
        
        // Demonstrate RAII Thread class
        {
            Thread raiiThread(manager, worker_function, "RAII-Delta", 4);
            unsigned int raiiId = raiiThread.getId();
            manager.registerThread(raiiId, "raii-delta-worker");
            std::cout << "Created RAII thread " << raiiId << " with attachment 'raii-delta-worker'" << std::endl;
            
            // Let it run for a bit
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Check if it's still alive
            if (raiiThread.isAlive()) {
                std::cout << "RAII thread is still running" << std::endl;
            }
            
            // Thread will be automatically cleaned up when raiiThread goes out of scope
            manager.unregisterThread("raii-delta-worker");
        }
        
        std::cout << std::endl << "7. Monitoring thread states..." << std::endl;
        
        // Monitor remaining threads
        auto remainingAttachments = manager.getAllAttachments();
        std::cout << "Monitoring " << remainingAttachments.size() << " remaining threads:" << std::endl;
        
        for (const auto& attachment : remainingAttachments) {
            try {
                auto threadId = manager.findThreadByAttachment(attachment);
                bool isAlive = manager.isThreadAlive(threadId);
                auto state = manager.getThreadState(threadId);
                
                std::cout << "  " << attachment << " (ID: " << threadId << "): "
                          << (isAlive ? "alive" : "dead") 
                          << ", state: " << static_cast<int>(state) << std::endl;
            } catch (const ThreadManagerException& e) {
                std::cout << "  " << attachment << ": Error - " << e.what() << std::endl;
            }
        }
        
        std::cout << std::endl << "8. Waiting for threads to complete..." << std::endl;
        
        // Wait for all threads to complete (with timeout)
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::cout << std::endl << "9. Cleaning up registrations..." << std::endl;
        
        // Unregister all remaining threads
        auto finalAttachments = manager.getAllAttachments();
        for (const auto& attachment : finalAttachments) {
            try {
                manager.unregisterThread(attachment);
                std::cout << "Unregistered: " << attachment << std::endl;
            } catch (const ThreadManagerException& e) {
                std::cout << "Error unregistering " << attachment << ": " << e.what() << std::endl;
            }
        }
        
        std::cout << std::endl << "Thread registration example completed successfully!" << std::endl;
        
    } catch (const ThreadManagerException& e) {
        std::cerr << "Thread manager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}