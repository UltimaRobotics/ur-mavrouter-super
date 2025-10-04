
/**
 * @file basic_example.cpp
 * @brief Basic example demonstrating ThreadManager C++ wrapper usage
 */

#include <ThreadManager.hpp>
#include <iostream>
#include <chrono>
#include <thread>

using namespace ThreadMgr;

// Example worker function
void workerFunction(int workerId, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        std::cout << "Worker " << workerId << " iteration " << (i + 1) << "/" << iterations << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "Worker " << workerId << " completed!" << std::endl;
}

// Lambda function as a regular function
void lambdaWorkerFunction() {
    for (int i = 0; i < 3; ++i) {
        std::cout << "Lambda-style thread iteration " << (i + 1) << "/3" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    std::cout << "Lambda-style thread completed!" << std::endl;
}

// Example class method
class Worker {
public:
    void doWork(int iterations) {
        for (int i = 0; i < iterations; ++i) {
            std::cout << "Class worker iteration " << (i + 1) << "/" << iterations << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        std::cout << "Class worker completed!" << std::endl;
    }
    
    // Static method that can be called with arguments
    static void staticWork(int workerId, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            std::cout << "Static worker " << workerId << " iteration " << (i + 1) << "/" << iterations << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
        }
        std::cout << "Static worker " << workerId << " completed!" << std::endl;
    }
};

// Function object
struct FunctionObjectWorker {
    int workerId;
    int iterations;
    
    FunctionObjectWorker(int id, int iter) : workerId(id), iterations(iter) {}
    
    void operator()() {
        for (int i = 0; i < iterations; ++i) {
            std::cout << "Function object worker " << workerId << " iteration " << (i + 1) << "/" << iterations << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(450));
        }
        std::cout << "Function object worker " << workerId << " completed!" << std::endl;
    }
};

int main() {
    try {
        // Set log level to info
        ThreadManager::setLogLevel(LogLevel::Info);
        
        std::cout << "=== ThreadManager C++ Wrapper Basic Example ===" << std::endl;
        
        // Create thread manager
        ThreadManager manager(5);
        
        std::cout << "\n1. Creating threads with function pointers..." << std::endl;
        
        // Create threads using function pointers
        auto thread1 = manager.createThread(workerFunction, 1, 3);
        auto thread2 = manager.createThread(workerFunction, 2, 2);
        
        std::cout << "Created threads: " << thread1 << ", " << thread2 << std::endl;
        
        // Create thread using std::function with lambda
        std::cout << "\n2. Creating thread with std::function..." << std::endl;
        std::function<void()> lambdaFunc = []() {
            for (int i = 0; i < 3; ++i) {
                std::cout << "std::function lambda thread iteration " << (i + 1) << "/3" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
            std::cout << "std::function lambda thread completed!" << std::endl;
        };
        auto thread3 = manager.createThread(lambdaFunc);
        
        // Create thread using regular function (lambda-style)
        std::cout << "\n3. Creating thread with regular function..." << std::endl;
        auto thread4 = manager.createThread(lambdaWorkerFunction);
        
        // Create thread using static class method
        std::cout << "\n4. Creating thread with static class method..." << std::endl;
        auto thread5 = manager.createThread(Worker::staticWork, 3, 2);
        
        // Create thread using function object
        std::cout << "\n5. Creating thread with function object..." << std::endl;
        FunctionObjectWorker funcObj(4, 2);
        std::function<void()> funcObjWrapper = funcObj;
        auto thread6 = manager.createThread(funcObjWrapper);
        
        std::cout << "Created threads: " << thread3 << ", " << thread4 << ", " << thread5 << ", " << thread6 << std::endl;
        std::cout << "\nTotal threads created: " << manager.getThreadCount() << std::endl;
        
        // Monitor threads
        std::cout << "\n6. Monitoring thread states..." << std::endl;
        auto threadIds = manager.getAllThreadIds();
        for (auto id : threadIds) {
            auto info = manager.getThreadInfo(id);
            std::cout << "Thread " << id << " state: ";
            switch (info.state) {
                case ThreadState::Created: std::cout << "Created"; break;
                case ThreadState::Running: std::cout << "Running"; break;
                case ThreadState::Paused: std::cout << "Paused"; break;
                case ThreadState::Stopped: std::cout << "Stopped"; break;
                case ThreadState::Error: std::cout << "Error"; break;
            }
            std::cout << std::endl;
        }
        
        // Pause and resume a thread
        std::cout << "\n7. Testing pause/resume functionality..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        
        if (manager.isThreadAlive(thread1)) {
            std::cout << "Pausing thread " << thread1 << std::endl;
            manager.pauseThread(thread1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            std::cout << "Resuming thread " << thread1 << std::endl;
            manager.resumeThread(thread1);
        }
        
        // Wait for all threads to complete
        std::cout << "\n8. Waiting for threads to complete..." << std::endl;
        threadIds = manager.getAllThreadIds(); // Refresh the list
        for (auto id : threadIds) {
            if (manager.isThreadAlive(id)) {
                std::cout << "Waiting for thread " << id << " to complete..." << std::endl;
                bool completed = manager.joinThread(id, std::chrono::seconds(10));
                if (completed) {
                    std::cout << "Thread " << id << " completed successfully" << std::endl;
                } else {
                    std::cout << "Thread " << id << " timed out, stopping..." << std::endl;
                    manager.stopThread(id);
                    manager.joinThread(id, std::chrono::seconds(2));
                }
            }
        }
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const ThreadManagerException& e) {
        std::cerr << "ThreadManager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
