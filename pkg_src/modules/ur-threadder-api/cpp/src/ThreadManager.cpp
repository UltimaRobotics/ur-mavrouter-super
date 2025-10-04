/**
 * @file ThreadManager.cpp
 * @brief Implementation of the C++ thread management wrapper
 */

#include "ThreadManager.hpp"
#include <algorithm>
#include <cstring>
#include <thread>
#include <chrono>
#include <sstream>
#include <any> // Required for std::any
#include <functional> // Required for std::function and std::bind

namespace ThreadMgr {



struct ThreadManager::Impl {
    thread_manager_t manager;
    mutable std::mutex wrappersMutex;
    std::map<unsigned int, std::shared_ptr<void>> functionWrappers;
    unsigned int nextWrapperId = 1;

    Impl(unsigned int initialCapacity) {
        int result = thread_manager_init(&manager, initialCapacity);
        if (result < 0) {
            throw ThreadManagerException("Failed to initialize thread manager");
        }
    }

    ~Impl() {
        thread_manager_destroy(&manager);
    }
};

ThreadManager::ThreadManager(unsigned int initialCapacity) 
    : pImpl(std::make_unique<Impl>(initialCapacity)) {
}

ThreadManager::~ThreadManager() = default;

ThreadManager::ThreadManager(ThreadManager&& other) noexcept 
    : pImpl(std::move(other.pImpl)) {
}

ThreadManager& ThreadManager::operator=(ThreadManager&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

unsigned int ThreadManager::createThread(std::function<void()> func) {
    INFO_LOG("ThreadManager::createThread - Starting thread creation");
    
    auto wrapper = std::make_shared<std::function<void()>>(std::move(func));

    // Store the wrapper to keep it alive
    std::lock_guard<std::mutex> lock(pImpl->wrappersMutex);
    auto wrapperId = pImpl->nextWrapperId++;
    pImpl->functionWrappers[wrapperId] = wrapper;
    
    INFO_LOG("ThreadManager::createThread - Stored function wrapper with ID %u", wrapperId);

    // Create C-style thread function
    auto cFunc = [](void* arg) -> void* {
        auto* wrapperInfo = static_cast<std::pair<std::shared_ptr<std::function<void()>>, ThreadManager::Impl*>*>(arg);
        try {
            INFO_LOG("Thread function starting execution");
            (*wrapperInfo->first)();
            INFO_LOG("Thread function completed execution");
        } catch (const std::exception& e) {
            ERROR_LOG("Thread function threw exception: %s", e.what());
        } catch (...) {
            ERROR_LOG("Thread function threw unknown exception");
        }

        delete wrapperInfo;
        return nullptr;
    };

    // Create argument pair
    auto* argPair = new std::pair<std::shared_ptr<std::function<void()>>, ThreadManager::Impl*>(wrapper, pImpl.get());

    unsigned int threadId;
    INFO_LOG("ThreadManager::createThread - Calling thread_create");
    int result = thread_create(&pImpl->manager, cFunc, argPair, &threadId);
    INFO_LOG("ThreadManager::createThread - thread_create returned result=%d, threadId=%u", result, result >= 0 ? threadId : 0);

    if (result < 0) {
        ERROR_LOG("ThreadManager::createThread - Failed to create thread with error code %d", result);
        delete argPair;
        std::lock_guard<std::mutex> lock2(pImpl->wrappersMutex);
        pImpl->functionWrappers.erase(wrapperId);
        handleCError(result, "createThread");
    }
    
    INFO_LOG("ThreadManager::createThread - Successfully created thread with ID %u", threadId);

    return threadId;
}

unsigned int ThreadManager::createProcess(const std::string& command, const std::vector<std::string>& args) {
    if (command.empty()) {
        throw ThreadManagerException("Command cannot be empty");
    }

    // Convert args to C-style array
    std::vector<char*> cArgs;
    cArgs.push_back(const_cast<char*>(command.c_str())); // First arg is command name

    std::vector<std::string> argsCopy = args; // Make a copy to ensure lifetime
    for (auto& arg : argsCopy) {
        cArgs.push_back(const_cast<char*>(arg.c_str()));
    }
    cArgs.push_back(nullptr); // NULL terminate

    unsigned int threadId;
    int result = thread_create_process(&pImpl->manager, command.c_str(), cArgs.data(), &threadId);

    if (result < 0) {
        handleCError(result, "createProcess");
    }

    return threadId;
}

void ThreadManager::stopThread(unsigned int threadId) {
    checkThreadExists(threadId);
    int result = thread_stop(&pImpl->manager, threadId);
    if (result < 0) {
        handleCError(result, "stopThread");
    }
}

void ThreadManager::pauseThread(unsigned int threadId) {
    checkThreadExists(threadId);
    int result = thread_pause(&pImpl->manager, threadId);
    if (result < 0) {
        handleCError(result, "pauseThread");
    }
}

void ThreadManager::resumeThread(unsigned int threadId) {
    checkThreadExists(threadId);
    int result = thread_resume(&pImpl->manager, threadId);
    if (result < 0) {
        handleCError(result, "resumeThread");
    }
}

ThreadState ThreadManager::getThreadState(unsigned int threadId) const {
    checkThreadExists(threadId);

    thread_state_t state;
    int result = thread_get_state(&pImpl->manager, threadId, &state);
    if (result < 0) {
        handleCError(result, "getThreadState");
    }

    return static_cast<ThreadState>(state);
}

ThreadInfo ThreadManager::getThreadInfo(unsigned int threadId) const {
    checkThreadExists(threadId);

    thread_info_t info;
    int result = thread_get_info(&pImpl->manager, threadId, &info);
    if (result < 0) {
        handleCError(result, "getThreadInfo");
    }

    ThreadInfo threadInfo;
    threadInfo.id = info.id;
    threadInfo.state = static_cast<ThreadState>(info.state);
    threadInfo.type = static_cast<ThreadType>(info.type);
    threadInfo.shouldExit = info.should_exit;
    threadInfo.isPaused = info.is_paused;

    if (info.command) {
        threadInfo.command = info.command;
    }

    if (info.args) {
        for (int i = 1; info.args[i] != nullptr; ++i) { // Skip first arg (command name)
            threadInfo.args.emplace_back(info.args[i]);
        }
    }

    threadInfo.exitStatus = info.exit_status;

    return threadInfo;
}

bool ThreadManager::isThreadAlive(unsigned int threadId) const {
    return thread_is_alive(&pImpl->manager, threadId);
}

bool ThreadManager::joinThread(unsigned int threadId, std::chrono::milliseconds timeout) {
    checkThreadExists(threadId);

    if (timeout == std::chrono::milliseconds::zero()) {
        // No timeout - block until thread completes
        void* result;
        int ret = thread_join(&pImpl->manager, threadId, &result);
        if (ret < 0) {
            handleCError(ret, "joinThread");
        }
        return true;
    } else {
        // Poll with timeout
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            if (!isThreadAlive(threadId)) {
                // Thread has finished - join it
                void* result;
                thread_join(&pImpl->manager, threadId, &result);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false; // Timeout
    }
}

std::vector<unsigned int> ThreadManager::getAllThreadIds() const {
    unsigned int count = getThreadCount();
    if (count == 0) {
        return {};
    }

    std::vector<unsigned int> ids(count);
    int actualCount = thread_get_all_ids(&pImpl->manager, ids.data(), count);

    if (actualCount < 0) {
        handleCError(actualCount, "getAllThreadIds");
    }

    ids.resize(actualCount);
    return ids;
}

unsigned int ThreadManager::getThreadCount() const {
    return thread_get_count(&pImpl->manager);
}

void ThreadManager::writeToProcess(unsigned int threadId, const std::vector<uint8_t>& data) {
    checkThreadExists(threadId);

    if (data.empty()) {
        return;
    }

    int result = thread_write_to_process(&pImpl->manager, threadId, data.data(), data.size());
    if (result < 0) {
        handleCError(result, "writeToProcess");
    }
}

void ThreadManager::writeToProcess(unsigned int threadId, const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    writeToProcess(threadId, bytes);
}

ProcessIO ThreadManager::readFromProcess(unsigned int threadId) {
    checkThreadExists(threadId);

    ProcessIO io;

    // Read stdout
    std::vector<uint8_t> stdoutBuffer(4096);
    int stdoutBytes = thread_read_from_process(&pImpl->manager, threadId, stdoutBuffer.data(), stdoutBuffer.size());
    if (stdoutBytes > 0) {
        io.stdoutData.assign(stdoutBuffer.begin(), stdoutBuffer.begin() + stdoutBytes);
        io.hasStdoutData = true;
    }

    // Read stderr
    std::vector<uint8_t> stderrBuffer(4096);
    int stderrBytes = thread_read_error_from_process(&pImpl->manager, threadId, stderrBuffer.data(), stderrBuffer.size());
    if (stderrBytes > 0) {
        io.stderrData.assign(stderrBuffer.begin(), stderrBuffer.begin() + stderrBytes);
        io.hasStderrData = true;
    }

    return io;
}

int ThreadManager::getProcessExitStatus(unsigned int threadId) const {
    checkThreadExists(threadId);

    int exitStatus;
    int result = thread_get_exit_status(&pImpl->manager, threadId, &exitStatus);
    if (result < 0) {
        handleCError(result, "getProcessExitStatus");
    }

    return exitStatus;
}

void ThreadManager::loadConfig(const std::string& filename) {
    if (filename.empty()) {
        throw ThreadManagerException("Filename cannot be empty");
    }

    int result = thread_manager_load_config(&pImpl->manager, filename.c_str());
    if (result < 0) {
        handleCError(result, "loadConfig");
    }
}

void ThreadManager::saveConfig(const std::string& filename) const {
    if (filename.empty()) {
        throw ThreadManagerException("Filename cannot be empty");
    }

    int result = thread_manager_save_config(&pImpl->manager, filename.c_str());
    if (result < 0) {
        handleCError(result, "saveConfig");
    }
}

unsigned int ThreadManager::createThreadFromJson(const std::string& jsonConfig) {
    if (jsonConfig.empty()) {
        throw ThreadManagerException("JSON config cannot be empty");
    }

    unsigned int threadId;
    int result = thread_create_from_json(&pImpl->manager, jsonConfig.c_str(), &threadId);
    if (result < 0) {
        handleCError(result, "createThreadFromJson");
    }

    return threadId;
}

unsigned int ThreadManager::createProcessFromJson(const std::string& jsonConfig) {
    if (jsonConfig.empty()) {
        throw ThreadManagerException("JSON config cannot be empty");
    }

    unsigned int threadId;
    int result = thread_create_process_from_json(&pImpl->manager, jsonConfig.c_str(), &threadId);
    if (result < 0) {
        handleCError(result, "createProcessFromJson");
    }

    return threadId;
}

std::string ThreadManager::getThreadJsonConfig(unsigned int threadId) const {
    checkThreadExists(threadId);

    char* jsonStr = thread_get_json_config(&pImpl->manager, threadId);
    if (!jsonStr) {
        throw ThreadManagerException("Failed to get JSON config for thread " + std::to_string(threadId));
    }

    std::string result(jsonStr);
    free(jsonStr);
    return result;
}

void ThreadManager::updateThreadFromJson(unsigned int threadId, const std::string& jsonConfig) {
    checkThreadExists(threadId);

    if (jsonConfig.empty()) {
        throw ThreadManagerException("JSON config cannot be empty");
    }

    int result = thread_update_from_json(&pImpl->manager, threadId, jsonConfig.c_str());
    if (result < 0) {
        handleCError(result, "updateThreadFromJson");
    }
}

void ThreadManager::setLogLevel(LogLevel level) {
    set_log_level(static_cast<log_level_t__>(level));
}

LogLevel ThreadManager::getLogLevel() {
    return static_cast<LogLevel>(get_log_level());
}

void ThreadManager::registerThread(unsigned int threadId, const std::string& attachmentArg) {
    INFO_LOG("ThreadManager::registerThread - Called with threadId=%u, attachment='%s'", 
             threadId, attachmentArg.c_str());
    
    // Verify thread exists before registering
    if (!isThreadAlive(threadId)) {
        thread_state_t state;
        int state_result = thread_get_state(&pImpl->manager, threadId, &state);
        if (state_result < 0) {
            ERROR_LOG("ThreadManager::registerThread - Thread %u does not exist (state check failed with code %d)", 
                     threadId, state_result);
            throw ThreadManagerException("Cannot register non-existent thread " + std::to_string(threadId));
        }
        WARN_LOG("ThreadManager::registerThread - Thread %u is not alive but exists (state=%d)", 
                threadId, state);
    }
    
    INFO_LOG("ThreadManager::registerThread - Calling thread_register with threadId=%u, attachment='%s'", 
             threadId, attachmentArg.c_str());
    
    int result = thread_register(&pImpl->manager, threadId, attachmentArg.c_str());
    
    INFO_LOG("ThreadManager::registerThread - thread_register returned result=%d", result);
    
    if (result < 0) {
        ERROR_LOG("ThreadManager::registerThread - Registration failed with error code %d for threadId=%u, attachment='%s'", 
                 result, threadId, attachmentArg.c_str());
        handleCError(result, "registerThread");
    } else {
        INFO_LOG("ThreadManager::registerThread - Successfully registered threadId=%u with attachment='%s'", 
                threadId, attachmentArg.c_str());
    }
}

void ThreadManager::unregisterThread(const std::string& attachmentArg) {
    int result = thread_unregister(&pImpl->manager, attachmentArg.c_str());
    if (result < 0) {
        handleCError(result, "unregisterThread");
    }
}

unsigned int ThreadManager::findThreadByAttachment(const std::string& attachmentArg) const {
    unsigned int threadId;
    int result = thread_find_by_attachment(&pImpl->manager, attachmentArg.c_str(), &threadId);
    if (result < 0) {
        handleCError(result, "findThreadByAttachment");
    }
    return threadId;
}

void ThreadManager::stopThreadByAttachment(const std::string& attachmentArg) {
    int result = thread_stop_by_attachment(&pImpl->manager, attachmentArg.c_str());
    if (result < 0) {
        handleCError(result, "stopThreadByAttachment");
    }
}

void ThreadManager::killThreadByAttachment(const std::string& attachmentArg) {
    int result = thread_kill_by_attachment(&pImpl->manager, attachmentArg.c_str());
    if (result < 0) {
        handleCError(result, "killThreadByAttachment");
    }
}

std::vector<std::string> ThreadManager::getAllAttachments() const {
    const unsigned int maxAttachments = 100; // Reasonable limit
    char* attachments[maxAttachments];
    
    int count = thread_get_all_attachments(&pImpl->manager, attachments, maxAttachments);
    if (count < 0) {
        handleCError(count, "getAllAttachments");
    }
    
    std::vector<std::string> result;
    for (int i = 0; i < count; i++) {
        result.emplace_back(attachments[i]);
        free(attachments[i]); // Free the C strings
    }
    
    return result;
}

void ThreadManager::checkThreadExists(unsigned int threadId) const {
    if (!isThreadAlive(threadId)) {
        // Check if thread exists at all
        thread_state_t state;
        int result = thread_get_state(&pImpl->manager, threadId, &state);
        if (result < 0) {
            throw ThreadManagerException("Thread " + std::to_string(threadId) + " does not exist");
        }
    }
}

void ThreadManager::handleCError(int result, const std::string& operation) const {
    std::string message = operation + " failed";

    switch (result) {
        case -1:
            message += " (general error)";
            break;
        case -2:
            message += " (thread not found)";
            break;
        default:
            message += " (error code: " + std::to_string(result) + ")";
            break;
    }

    throw ThreadManagerException(message);
}

// Thread class implementations
Thread::Thread(Thread&& other) noexcept 
    : manager_(other.manager_)
    , threadId_(other.threadId_)
    , joined_(other.joined_) {
    other.manager_ = nullptr;
    other.threadId_ = 0;
    other.joined_ = true;
}

Thread& Thread::operator=(Thread&& other) noexcept {
    if (this != &other) {
        if (manager_ && !joined_) {
            try {
                stop();
                join(std::chrono::seconds(5));
            } catch (...) {
                // Ignore exceptions during cleanup
            }
        }

        manager_ = other.manager_;
        threadId_ = other.threadId_;
        joined_ = other.joined_;

        other.manager_ = nullptr;
        other.threadId_ = 0;
        other.joined_ = true;
    }
    return *this;
}

Thread::~Thread() {
    if (manager_ && !joined_) {
        try {
            stop();
            join(std::chrono::seconds(5));
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }
}

void Thread::stop() {
    if (manager_ && !joined_) {
        manager_->stopThread(threadId_);
    }
}

void Thread::pause() {
    if (manager_ && !joined_) {
        manager_->pauseThread(threadId_);
    }
}

void Thread::resume() {
    if (manager_ && !joined_) {
        manager_->resumeThread(threadId_);
    }
}

ThreadState Thread::getState() const {
    if (!manager_) {
        return ThreadState::Error;
    }
    return manager_->getThreadState(threadId_);
}

bool Thread::isAlive() const {
    if (!manager_) {
        return false;
    }
    return manager_->isThreadAlive(threadId_);
}

bool Thread::join(std::chrono::milliseconds timeout) {
    if (!manager_ || joined_) {
        return true;
    }

    bool success = manager_->joinThread(threadId_, timeout);
    if (success) {
        joined_ = true;
    }
    return success;
}

// Process class implementations
Process::Process(ThreadManager& manager, const std::string& command, const std::vector<std::string>& args)
    : manager_(&manager)
    , threadId_(manager.createProcess(command, args))
    , stopped_(false) {
}

Process::~Process() {
    if (manager_ && !stopped_) {
        try {
            stop();
            join(std::chrono::seconds(5));
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }
}

Process::Process(Process&& other) noexcept
    : manager_(other.manager_)
    , threadId_(other.threadId_)
    , stopped_(other.stopped_) {
    other.manager_ = nullptr;
    other.threadId_ = 0;
    other.stopped_ = true;
}

Process& Process::operator=(Process&& other) noexcept {
    if (this != &other) {
        if (manager_ && !stopped_) {
            try {
                stop();
                join(std::chrono::seconds(5));
            } catch (...) {
                // Ignore exceptions during cleanup
            }
        }

        manager_ = other.manager_;
        threadId_ = other.threadId_;
        stopped_ = other.stopped_;

        other.manager_ = nullptr;
        other.threadId_ = 0;
        other.stopped_ = true;
    }
    return *this;
}

void Process::stop() {
    if (manager_ && !stopped_) {
        manager_->stopThread(threadId_);
        stopped_ = true;
    }
}

void Process::pause() {
    if (manager_ && !stopped_) {
        manager_->pauseThread(threadId_);
    }
}

void Process::resume() {
    if (manager_ && !stopped_) {
        manager_->resumeThread(threadId_);
    }
}

ThreadState Process::getState() const {
    if (!manager_) {
        return ThreadState::Error;
    }
    return manager_->getThreadState(threadId_);
}

bool Process::isAlive() const {
    if (!manager_) {
        return false;
    }
    return manager_->isThreadAlive(threadId_);
}

bool Process::join(std::chrono::milliseconds timeout) {
    if (!manager_) {
        return true;
    }

    bool success = manager_->joinThread(threadId_, timeout);
    if (success) {
        stopped_ = true;
    }
    return success;
}

void Process::write(const std::vector<uint8_t>& data) {
    if (manager_ && !stopped_) {
        manager_->writeToProcess(threadId_, data);
    }
}

void Process::write(const std::string& data) {
    if (manager_ && !stopped_) {
        manager_->writeToProcess(threadId_, data);
    }
}

ProcessIO Process::read() {
    if (!manager_ || stopped_) {
        return {};
    }
    return manager_->readFromProcess(threadId_);
}

int Process::getExitStatus() const {
    if (!manager_) {
        return -1;
    }
    return manager_->getProcessExitStatus(threadId_);
}



} // namespace ThreadMgr