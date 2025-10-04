
/**
 * @file process_example.cpp
 * @brief Example demonstrating process management with ThreadManager
 */

#include <ThreadManager.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

using namespace ThreadMgr;

int main() {
    try {
        ThreadManager::setLogLevel(LogLevel::Info);
        
        std::cout << "=== ThreadManager Process Management Example ===" << std::endl;
        
        ThreadManager manager;
        
        // Example 1: Simple command execution
        std::cout << "\n1. Running simple command (ls -la)..." << std::endl;
        auto lsProcess = manager.createProcess("ls", {"-la", "/tmp"});
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Read output
        auto io = manager.readFromProcess(lsProcess);
        if (io.hasStdoutData) {
            std::string output(io.stdoutData.begin(), io.stdoutData.end());
            std::cout << "ls output:\n" << output << std::endl;
        }
        
        // Wait for process to complete
        if (manager.joinThread(lsProcess, std::chrono::seconds(5))) {
            int exitStatus = manager.getProcessExitStatus(lsProcess);
            std::cout << "ls process completed with exit status: " << exitStatus << std::endl;
        }
        
        // Example 2: Interactive process (cat)
        std::cout << "\n2. Running interactive process (cat)..." << std::endl;
        auto catProcess = manager.createProcess("cat", {});
        
        // Write some data to the process
        std::string input = "Hello, World!\nThis is a test.\n";
        manager.writeToProcess(catProcess, input);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Read output from cat
        io = manager.readFromProcess(catProcess);
        if (io.hasStdoutData) {
            std::string output(io.stdoutData.begin(), io.stdoutData.end());
            std::cout << "cat echoed back: " << output << std::endl;
        }
        
        // Stop the cat process
        manager.stopThread(catProcess);
        manager.joinThread(catProcess, std::chrono::seconds(2));
        
        // Example 3: Running a command with arguments
        std::cout << "\n3. Running command with arguments (echo)..." << std::endl;
        auto echoProcess = manager.createProcess("echo", {"Hello", "from", "process", "example!"});
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Read echo output
        io = manager.readFromProcess(echoProcess);
        if (io.hasStdoutData) {
            std::string output(io.stdoutData.begin(), io.stdoutData.end());
            std::cout << "echo output: " << output;
        }
        
        // Wait for echo to complete
        if (manager.joinThread(echoProcess, std::chrono::seconds(5))) {
            int exitStatus = manager.getProcessExitStatus(echoProcess);
            std::cout << "echo process completed with exit status: " << exitStatus << std::endl;
        }
        
        // Example 4: Multiple processes
        std::cout << "\n4. Running multiple processes..." << std::endl;
        
        std::vector<unsigned int> processes;
        
        // Start multiple date processes
        for (int i = 0; i < 3; ++i) {
            auto dateProcess = manager.createProcess("date", {});
            processes.push_back(dateProcess);
            std::cout << "Started date process " << dateProcess << std::endl;
        }
        
        // Wait a bit for processes to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Read outputs from all processes
        for (size_t i = 0; i < processes.size(); ++i) {
            auto processId = processes[i];
            auto io = manager.readFromProcess(processId);
            if (io.hasStdoutData) {
                std::string output(io.stdoutData.begin(), io.stdoutData.end());
                std::cout << "Date process " << (i + 1) << " output: " << output;
            }
            
            // Join each process
            if (manager.joinThread(processId, std::chrono::seconds(2))) {
                int exitStatus = manager.getProcessExitStatus(processId);
                std::cout << "Date process " << (i + 1) << " exit status: " << exitStatus << std::endl;
            }
        }
        
        // Example 5: Error handling
        std::cout << "\n5. Testing error handling with invalid command..." << std::endl;
        try {
            auto invalidProcess = manager.createProcess("nonexistent_command", {});
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Try to read from the process
            auto io = manager.readFromProcess(invalidProcess);
            if (io.hasStderrData) {
                std::string errorOutput(io.stderrData.begin(), io.stderrData.end());
                std::cout << "Error output: " << errorOutput << std::endl;
            }
            
            // Check exit status
            if (!manager.isThreadAlive(invalidProcess)) {
                int exitStatus = manager.getProcessExitStatus(invalidProcess);
                std::cout << "Invalid command exit status: " << exitStatus << std::endl;
            }
            
        } catch (const ThreadManagerException& e) {
            std::cout << "Caught expected error: " << e.what() << std::endl;
        }
        
        // Example 6: Process monitoring
        std::cout << "\n6. Process monitoring example..." << std::endl;
        auto sleepProcess = manager.createProcess("sleep", {"2"});
        
        std::cout << "Started sleep process " << sleepProcess << std::endl;
        
        // Monitor the process
        int checks = 0;
        while (manager.isThreadAlive(sleepProcess) && checks < 10) {
            std::cout << "Sleep process is still running... (check " << (checks + 1) << ")" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            checks++;
        }
        
        if (manager.joinThread(sleepProcess, std::chrono::seconds(5))) {
            int exitStatus = manager.getProcessExitStatus(sleepProcess);
            std::cout << "Sleep process completed with exit status: " << exitStatus << std::endl;
        }
        
        std::cout << "\n=== Process example completed successfully ===" << std::endl;
        
    } catch (const ThreadManagerException& e) {
        std::cerr << "ThreadManager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
