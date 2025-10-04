
/**
 * @file raii_example.cpp
 * @brief Example demonstrating RAII wrappers for threads and processes
 */

#include <ThreadManager.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

using namespace ThreadMgr;

// Worker functions for RAII examples
void simpleWorker(int workerId) {
    std::cout << "RAII worker " << workerId << " starting..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500 * workerId));
    std::cout << "RAII worker " << workerId << " completed!" << std::endl;
}

void longRunningWorker(int workerId, int duration) {
    std::cout << "Long-running worker " << workerId << " starting (duration: " << duration << "ms)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    std::cout << "Long-running worker " << workerId << " completed!" << std::endl;
}

void exceptionWorker() {
    std::cout << "Exception worker starting..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    throw std::runtime_error("Test exception from thread");
}

class WorkerClass {
public:
    void doWork(int iterations) {
        for (int i = 0; i < iterations; ++i) {
            std::cout << "Class worker iteration " << (i + 1) << "/" << iterations << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        std::cout << "Class worker completed!" << std::endl;
    }
};

int main() {
    try {
        ThreadManager::setLogLevel(LogLevel::Info);
        
        std::cout << "=== ThreadManager RAII Wrapper Example ===" << std::endl;
        
        ThreadManager manager;
        
        // Example 1: Basic RAII thread usage
        std::cout << "\n1. Basic RAII thread usage..." << std::endl;
        
        {
            // Use std::function to avoid template issues with local lambdas
            std::function<void()> worker1Func = []() {
                std::cout << "RAII thread 1 executing..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                std::cout << "RAII thread 1 done!" << std::endl;
            };
            
            Thread raiiThread1(manager, worker1Func);
            Thread raiiThread2(manager, simpleWorker, 2);
            
            std::cout << "Created RAII threads: " << raiiThread1.getId() << ", " << raiiThread2.getId() << std::endl;
            
            // Threads will be automatically joined when they go out of scope
            std::cout << "Waiting for RAII threads to complete..." << std::endl;
            raiiThread1.join(std::chrono::seconds(5));
            raiiThread2.join(std::chrono::seconds(5));
        }
        
        std::cout << "RAII threads completed (automatic cleanup)" << std::endl;
        
        // Example 2: Process RAII wrapper
        std::cout << "\n2. Process RAII wrapper..." << std::endl;
        
        {
            Process echoProcess(manager, "echo", {"Hello", "from", "RAII", "process!"});
            
            std::cout << "Created RAII process: " << echoProcess.getId() << std::endl;
            
            // Give process time to execute
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            // Read output
            auto io = echoProcess.read();
            if (io.hasStdoutData) {
                std::string output(io.stdoutData.begin(), io.stdoutData.end());
                std::cout << "Process output: " << output;
            }
            
            // Process will be automatically stopped and joined when it goes out of scope
        }
        
        std::cout << "RAII process completed (automatic cleanup)" << std::endl;
        
        // Example 3: Thread control operations
        std::cout << "\n3. Thread control operations..." << std::endl;
        
        {
            Thread controllableThread(manager, longRunningWorker, 3, 2000);
            
            std::cout << "Created controllable thread: " << controllableThread.getId() << std::endl;
            
            // Let it run for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            std::cout << "Thread alive: " << controllableThread.isAlive() << std::endl;
            std::cout << "Thread state: " << static_cast<int>(controllableThread.getState()) << std::endl;
            
            // Pause and resume
            std::cout << "Pausing thread..." << std::endl;
            controllableThread.pause();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            std::cout << "Resuming thread..." << std::endl;
            controllableThread.resume();
            
            // Wait for completion
            controllableThread.join(std::chrono::seconds(5));
        }
        
        // Example 4: Move semantics
        std::cout << "\n4. Move semantics..." << std::endl;
        
        {
            Thread originalThread(manager, simpleWorker, 4);
            unsigned int originalId = originalThread.getId();
            
            std::cout << "Original thread ID: " << originalId << std::endl;
            
            // Move the thread
            Thread movedThread = std::move(originalThread);
            
            std::cout << "Moved thread ID: " << movedThread.getId() << std::endl;
            std::cout << "Original thread ID after move: " << originalThread.getId() << std::endl;
            
            movedThread.join(std::chrono::seconds(5));
        }
        
        // Example 5: Exception safety
        std::cout << "\n5. Testing exception safety..." << std::endl;
        
        try {
            std::function<void()> exceptionFunc = []() {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                throw std::runtime_error("Test exception from thread");
            };
            
            Thread exceptionThread(manager, exceptionFunc);
            
            std::cout << "Exception thread created: " << exceptionThread.getId() << std::endl;
            
            // Wait for thread - it should handle the exception gracefully
            exceptionThread.join(std::chrono::seconds(2));
            std::cout << "Exception thread handled gracefully" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }
        
        // Example 6: Container of RAII objects
        std::cout << "\n6. Using containers of RAII objects..." << std::endl;
        
        {
            std::vector<std::unique_ptr<Thread>> workers;
            
            // Create multiple workers using std::function to avoid template issues
            for (int i = 1; i <= 3; ++i) {
                std::function<void()> workerFunc = [i]() {
                    std::cout << "Container worker " << i << " running..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(300 * i));
                    std::cout << "Container worker " << i << " done!" << std::endl;
                };
                
                workers.push_back(std::make_unique<Thread>(manager, workerFunc));
            }
            
            std::cout << "Created " << workers.size() << " workers in container" << std::endl;
            
            // Check their states
            for (const auto& worker : workers) {
                std::cout << "Worker " << worker->getId() << " alive: " << worker->isAlive() << std::endl;
            }
            
            std::cout << "Waiting for container workers to complete..." << std::endl;
            // All workers will be automatically joined when container is destroyed
            for (auto& worker : workers) {
                worker->join(std::chrono::seconds(5));
            }
        }
        
        std::cout << "All container workers completed" << std::endl;
        
        // Example 7: Process with I/O
        std::cout << "\n7. Process with I/O operations..." << std::endl;
        
        {
            Process catProcess(manager, "cat", {});
            
            std::cout << "Created cat process: " << catProcess.getId() << std::endl;
            
            // Write to process
            std::string input = "Hello from RAII process!\nThis is line 2.\n";
            catProcess.write(input);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            // Read output
            auto io = catProcess.read();
            if (io.hasStdoutData) {
                std::string output(io.stdoutData.begin(), io.stdoutData.end());
                std::cout << "Cat process output:\n" << output << std::endl;
            }
            
            // Process will be automatically stopped
        }
        
        std::cout << "\n=== RAII example completed successfully ===" << std::endl;
        
    } catch (const ThreadManagerException& e) {
        std::cerr << "ThreadManager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
