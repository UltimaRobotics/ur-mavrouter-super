
/**
 * @file ThreadManager.hpp
 * @brief C++ wrapper for the thread management library
 * 
 * This header provides a modern C++ interface to the C thread management library,
 * featuring RAII, exception safety, and STL integration.
 */

#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP
// Ensure we're in C++ mode before including the C header
#ifndef __cplusplus
#error "This header requires C++ compilation"
#endif

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <stdexcept>
#include <future>
#include <chrono>
#include <mutex>
#include <type_traits>

// Forward declarations for C library
extern "C" {
    #include "thread_manager.h"
    #include "json_config.h"
    #include "utils.h"
}

namespace ThreadMgr {

// Forward declaration
template<typename Func, typename... Args>
struct ThreadFunctionWrapper {
    std::function<void()> callable;

    ThreadFunctionWrapper(Func&& func, Args&&... args) {
        callable = [func = std::forward<Func>(func), args...]() mutable {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                std::invoke(func, args...);
            } else {
                auto result = std::invoke(func, args...);
                // Store result somewhere if needed
                (void)result; // Suppress unused variable warning
            }
        };
    }
};

/**
 * @brief Exception thrown by thread management operations
 */
class ThreadManagerException : public std::runtime_error {
public:
    explicit ThreadManagerException(const std::string& message) 
        : std::runtime_error("ThreadManager: " + message) {}
};

/**
 * @brief C++ enumeration for thread states
 */
enum class ThreadState {
    Created = THREAD_CREATED,
    Running = THREAD_RUNNING,
    Paused = THREAD_PAUSED,
    Stopped = THREAD_STOPPED,
    Error = THREAD_ERROR
};

/**
 * @brief C++ enumeration for thread types
 */
enum class ThreadType {
    Normal = THREAD_TYPE_NORMAL,
    Process = THREAD_TYPE_PROCESS
};

/**
 * @brief C++ enumeration for log levels
 */
enum class LogLevel {
    Debug = LOG_LEVEL_DEBUG,
    Info = LOG_LEVEL_INFO,
    Warn = LOG_LEVEL_WARN,
    Error = LOG_LEVEL_ERROR
};

/**
 * @brief Thread information structure
 */
struct ThreadInfo {
    unsigned int id;
    ThreadState state;
    ThreadType type;
    bool shouldExit;
    bool isPaused;
    std::string command; // For process threads
    std::vector<std::string> args; // For process threads
    int exitStatus; // For process threads
};

/**
 * @brief Process I/O data structure
 */
struct ProcessIO {
    std::vector<uint8_t> stdoutData;
    std::vector<uint8_t> stderrData;
    bool hasStdoutData = false;
    bool hasStderrData = false;
};

/**
 * @brief Main thread manager class
 * 
 * This class provides a modern C++ interface to the underlying C thread management library.
 * It follows RAII principles and provides exception-safe operations.
 */
class ThreadManager {
public:
    /**
     * @brief Constructor
     * @param initialCapacity Initial capacity for thread storage
     */
    explicit ThreadManager(unsigned int initialCapacity = 10);

    /**
     * @brief Destructor - automatically cleans up all threads
     */
    ~ThreadManager();

    // Disable copy construction and assignment
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    // Enable move construction and assignment
    ThreadManager(ThreadManager&& other) noexcept;
    ThreadManager& operator=(ThreadManager&& other) noexcept;

    /**
     * @brief Create a thread using std::function
     * @param func std::function to execute
     * @return Thread ID
     */
    unsigned int createThread(std::function<void()> func);

    /**
     * @brief Create and start a new thread
     * @param func Function to execute in the thread
     * @param args Arguments to pass to the function
     * @return Thread ID
     */
    template<typename Func, typename... Args>
    unsigned int createThread(Func&& func, Args&&... args);

    /**
     * @brief Create and execute a system process as a thread
     * @param command Command to execute
     * @param args Arguments for the command
     * @return Thread ID
     */
    unsigned int createProcess(const std::string& command, const std::vector<std::string>& args = {});

    /**
     * @brief Stop a thread
     * @param threadId Thread ID
     */
    void stopThread(unsigned int threadId);

    /**
     * @brief Pause a thread
     * @param threadId Thread ID
     */
    void pauseThread(unsigned int threadId);

    /**
     * @brief Resume a paused thread
     * @param threadId Thread ID
     */
    void resumeThread(unsigned int threadId);

    /**
     * @brief Restart a thread with new arguments
     * @param threadId Thread ID
     * @param func New function to execute
     * @param args New arguments
     */
    template<typename Func, typename... Args>
    void restartThread(unsigned int threadId, Func&& func, Args&&... args);

    /**
     * @brief Get thread state
     * @param threadId Thread ID
     * @return Thread state
     */
    ThreadState getThreadState(unsigned int threadId) const;

    /**
     * @brief Get thread information
     * @param threadId Thread ID
     * @return ThreadInfo structure
     */
    ThreadInfo getThreadInfo(unsigned int threadId) const;

    /**
     * @brief Check if thread is alive
     * @param threadId Thread ID
     * @return True if thread is alive
     */
    bool isThreadAlive(unsigned int threadId) const;

    /**
     * @brief Wait for thread to complete
     * @param threadId Thread ID
     * @param timeout Timeout duration (optional)
     * @return True if thread completed within timeout
     */
    bool joinThread(unsigned int threadId, std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /**
     * @brief Get all active thread IDs
     * @return Vector of thread IDs
     */
    std::vector<unsigned int> getAllThreadIds() const;

    /**
     * @brief Get number of threads
     * @return Thread count
     */
    unsigned int getThreadCount() const;

    /**
     * @brief Write data to process stdin
     * @param threadId Thread ID
     * @param data Data to write
     */
    void writeToProcess(unsigned int threadId, const std::vector<uint8_t>& data);

    /**
     * @brief Write string to process stdin
     * @param threadId Thread ID
     * @param data String to write
     */
    void writeToProcess(unsigned int threadId, const std::string& data);

    /**
     * @brief Read data from process stdout/stderr
     * @param threadId Thread ID
     * @return ProcessIO structure with available data
     */
    ProcessIO readFromProcess(unsigned int threadId);

    /**
     * @brief Get process exit status
     * @param threadId Thread ID
     * @return Exit status
     */
    int getProcessExitStatus(unsigned int threadId) const;

    /**
     * @brief Load configuration from JSON file
     * @param filename JSON configuration file path
     */
    void loadConfig(const std::string& filename);

    /**
     * @brief Save configuration to JSON file
     * @param filename JSON configuration file path
     */
    void saveConfig(const std::string& filename) const;

    /**
     * @brief Create thread from JSON configuration
     * @param jsonConfig JSON configuration string
     * @return Thread ID
     */
    unsigned int createThreadFromJson(const std::string& jsonConfig);

    /**
     * @brief Create process from JSON configuration
     * @param jsonConfig JSON configuration string
     * @return Thread ID
     */
    unsigned int createProcessFromJson(const std::string& jsonConfig);

    /**
     * @brief Get thread configuration as JSON
     * @param threadId Thread ID
     * @return JSON configuration string
     */
    std::string getThreadJsonConfig(unsigned int threadId) const;

    /**
     * @brief Update thread from JSON configuration
     * @param threadId Thread ID
     * @param jsonConfig JSON configuration string
     */
    void updateThreadFromJson(unsigned int threadId, const std::string& jsonConfig);

    /**
     * @brief Set logging level
     * @param level Log level
     */
    static void setLogLevel(LogLevel level);

    /**
     * @brief Get current logging level
     * @return Current log level
     */
    static LogLevel getLogLevel();

    /**
     * @brief Register a thread with an attachment identifier
     * @param threadId Thread ID to register
     * @param attachmentArg Attachment identifier string
     */
    void registerThread(unsigned int threadId, const std::string& attachmentArg);

    /**
     * @brief Unregister a thread by attachment identifier
     * @param attachmentArg Attachment identifier string
     */
    void unregisterThread(const std::string& attachmentArg);

    /**
     * @brief Find thread ID by attachment identifier
     * @param attachmentArg Attachment identifier string
     * @return Thread ID
     */
    unsigned int findThreadByAttachment(const std::string& attachmentArg) const;

    /**
     * @brief Stop a thread by attachment identifier
     * @param attachmentArg Attachment identifier string
     */
    void stopThreadByAttachment(const std::string& attachmentArg);

    /**
     * @brief Kill a thread by attachment identifier
     * @param attachmentArg Attachment identifier string
     */
    void killThreadByAttachment(const std::string& attachmentArg);

    /**
     * @brief Restart a thread by attachment identifier
     * @param attachmentArg Attachment identifier string
     * @param func New function to execute
     * @param args New arguments
     */
    template<typename Func, typename... Args>
    void restartThreadByAttachment(const std::string& attachmentArg, Func&& func, Args&&... args);

    /**
     * @brief Get all registered attachment identifiers
     * @return Vector of attachment identifiers
     */
    std::vector<std::string> getAllAttachments() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // Helper methods
    void checkThreadExists(unsigned int threadId) const;
    void handleCError(int result, const std::string& operation) const;
};

/**
 * @brief RAII wrapper for individual threads
 */
class Thread {
public:
    /**
     * @brief Constructor - creates a new thread
     * @param manager Reference to thread manager
     * @param func Function to execute
     * @param args Arguments to pass to function
     */
    template<typename Func, typename... Args>
    Thread(ThreadManager& manager, Func&& func, Args&&... args);

    /**
     * @brief Destructor - automatically joins thread if not already joined
     */
    ~Thread();

    // Disable copy construction and assignment
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Enable move construction and assignment
    Thread(Thread&& other) noexcept;
    Thread& operator=(Thread&& other) noexcept;

    /**
     * @brief Get thread ID
     * @return Thread ID
     */
    unsigned int getId() const { return threadId_; }

    /**
     * @brief Stop the thread
     */
    void stop();

    /**
     * @brief Pause the thread
     */
    void pause();

    /**
     * @brief Resume the thread
     */
    void resume();

    /**
     * @brief Get thread state
     * @return Thread state
     */
    ThreadState getState() const;

    /**
     * @brief Check if thread is alive
     * @return True if thread is alive
     */
    bool isAlive() const;

    /**
     * @brief Wait for thread to complete
     * @param timeout Timeout duration
     * @return True if thread completed within timeout
     */
    bool join(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

private:
    ThreadManager* manager_;
    unsigned int threadId_;
    bool joined_;
};

/**
 * @brief RAII wrapper for process threads
 */
class Process {
public:
    /**
     * @brief Constructor - creates a new process
     * @param manager Reference to thread manager
     * @param command Command to execute
     * @param args Arguments for the command
     */
    Process(ThreadManager& manager, const std::string& command, const std::vector<std::string>& args = {});

    /**
     * @brief Destructor - automatically stops process if still running
     */
    ~Process();

    // Disable copy construction and assignment
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // Enable move construction and assignment
    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;

    /**
     * @brief Get thread ID
     * @return Thread ID
     */
    unsigned int getId() const { return threadId_; }

    /**
     * @brief Stop the process
     */
    void stop();

    /**
     * @brief Pause the process
     */
    void pause();

    /**
     * @brief Resume the process
     */
    void resume();

    /**
     * @brief Get process state
     * @return Process state
     */
    ThreadState getState() const;

    /**
     * @brief Check if process is alive
     * @return True if process is alive
     */
    bool isAlive() const;

    /**
     * @brief Wait for process to complete
     * @param timeout Timeout duration
     * @return True if process completed within timeout
     */
    bool join(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /**
     * @brief Write data to process stdin
     * @param data Data to write
     */
    void write(const std::vector<uint8_t>& data);

    /**
     * @brief Write string to process stdin
     * @param data String to write
     */
    void write(const std::string& data);

    /**
     * @brief Read available data from process
     * @return ProcessIO structure with available data
     */
    ProcessIO read();

    /**
     * @brief Get process exit status
     * @return Exit status (only valid after process has exited)
     */
    int getExitStatus() const;

private:
    ThreadManager* manager_;
    unsigned int threadId_;
    bool stopped_;
};

} // namespace ThreadMgr

// Template implementations
#include "ThreadManager.tpp"

#endif // THREAD_MANAGER_HPP
