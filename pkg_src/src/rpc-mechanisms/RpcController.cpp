/**
 * @file RpcController.cpp
 * @brief RPC controller implementation
 */

#include "RpcController.h"
#include "common/log.h"
#include "mainloop.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

namespace RpcMechanisms {

std::string RpcResponse::toJson() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"status\":\"" << static_cast<int>(status) << "\",";
    ss << "\"message\":\"" << message << "\",";
    ss << "\"threads\":{";

    bool first = true;
    for (const auto& pair : threadStates) {
        if (!first) ss << ",";
        first = false;

        const auto& info = pair.second;
        ss << "\"" << pair.first << "\":{";
        ss << "\"threadId\":" << info.threadId << ",";
        ss << "\"state\":" << static_cast<int>(info.state) << ",";
        ss << "\"isAlive\":" << (info.isAlive ? "true" : "false") << ",";
        ss << "\"attachmentId\":\"" << info.attachmentId << "\"";
        ss << "}";
    }

    ss << "}}";
    return ss.str();
}

RpcController::RpcController(ThreadMgr::ThreadManager& threadManager)
    : threadManager_(threadManager) {
    log_info("RpcController initialized");
}

RpcController::~RpcController() {
    log_info("RpcController destroyed");
}

void RpcController::registerThread(const std::string& threadName, 
                                   unsigned int threadId,
                                   const std::string& attachmentId) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    threadRegistry_[threadName] = threadId;
    threadAttachments_[threadName] = attachmentId;

    // Register with thread manager
    threadManager_.registerThread(threadId, attachmentId);

    log_info("RPC: Registered thread '%s' with ID %u and attachment '%s'",
             threadName.c_str(), threadId, attachmentId.c_str());
}

void RpcController::registerRestartCallback(const std::string& threadName, 
                                           std::function<unsigned int()> restartCallback) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    restartCallbacks_[threadName] = restartCallback;
    log_info("RPC: Registered restart callback for thread '%s'", threadName.c_str());
}

void RpcController::unregisterThread(const std::string& threadName) {
    std::lock_guard<std::mutex> lock(registryMutex_);

    auto it = threadAttachments_.find(threadName);
    if (it != threadAttachments_.end()) {
        threadManager_.unregisterThread(it->second);
        threadAttachments_.erase(it);
    }

    threadRegistry_.erase(threadName);
    log_info("RPC: Unregistered thread '%s'", threadName.c_str());
}

ThreadStateInfo RpcController::getThreadStateInfo(const std::string& threadName) {
    ThreadStateInfo info;
    info.threadName = threadName;

    // Note: registryMutex_ should already be locked by caller
    auto it = threadRegistry_.find(threadName);
    if (it != threadRegistry_.end()) {
        info.threadId = it->second;

        try {
            info.state = threadManager_.getThreadState(it->second);
            info.isAlive = threadManager_.isThreadAlive(it->second);
        } catch (const ThreadMgr::ThreadManagerException& e) {
            log_error("RPC: Failed to get thread state for '%s': %s", threadName.c_str(), e.what());
            info.state = ThreadMgr::ThreadState::Error;
            info.isAlive = false;
        }

        auto attachIt = threadAttachments_.find(threadName);
        if (attachIt != threadAttachments_.end()) {
            info.attachmentId = attachIt->second;
        }
    }

    return info;
}

RpcResponse RpcController::getAllThreadStatus() {
    RpcResponse response;
    response.status = OperationStatus::SUCCESS;
    response.message = "Retrieved status for all threads";

    std::lock_guard<std::mutex> lock(registryMutex_);
    for (const auto& pair : threadRegistry_) {
        ThreadStateInfo info = getThreadStateInfo(pair.first);
        response.threadStates[pair.first] = info;
    }

    return response;
}

RpcResponse RpcController::getThreadStatus(const std::string& threadName) {
    RpcResponse response;

    std::lock_guard<std::mutex> lock(registryMutex_);
    if (threadRegistry_.find(threadName) == threadRegistry_.end()) {
        response.status = OperationStatus::THREAD_NOT_FOUND;
        response.message = "Thread not found: " + threadName;
        return response;
    }

    ThreadStateInfo info = getThreadStateInfo(threadName);

    response.status = OperationStatus::SUCCESS;
    response.message = "Retrieved thread status";
    response.threadStates[threadName] = info;

    return response;
}

RpcResponse RpcController::executeOperationOnThread(const std::string& threadName, 
                                                   ThreadOperation operation) {
    RpcResponse response;

    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = threadRegistry_.find(threadName);
    
    // For START operation, check for restart callback even if thread not registered
    if (it == threadRegistry_.end()) {
        if (operation == ThreadOperation::START) {
            // Thread not registered - check if we have a restart callback
            auto callbackIt = restartCallbacks_.find(threadName);
            if (callbackIt != restartCallbacks_.end()) {
                log_info("RPC: Thread '%s' not registered, but restart callback exists - creating new thread", 
                         threadName.c_str());
                
                std::function<unsigned int()> restartCallback = callbackIt->second;
                registryMutex_.unlock();
                
                // Create thread using the callback
                unsigned int newThreadId = restartCallback();
                
                response.status = OperationStatus::SUCCESS;
                response.message = "Thread created successfully with ID: " + std::to_string(newThreadId);
                
                // Get thread info
                std::lock_guard<std::mutex> lock(registryMutex_);
                ThreadStateInfo info = getThreadStateInfo(threadName);
                response.threadStates[threadName] = info;
                
                log_info("RPC: Thread '%s' created successfully with ID %u", 
                         threadName.c_str(), newThreadId);
                return response;
            }
        }
        
        response.status = OperationStatus::THREAD_NOT_FOUND;
        response.message = "Thread not found: " + threadName;
        return response;
    }

    unsigned int threadId = it->second;
    std::string attachmentId;
    auto attachIt = threadAttachments_.find(threadName);
    if (attachIt != threadAttachments_.end()) {
        attachmentId = attachIt->second;
    }
    registryMutex_.unlock();

    try {
        switch (operation) {
            case ThreadOperation::START:
                // For start, check if thread is already running
                if (threadManager_.isThreadAlive(threadId)) {
                    response.status = OperationStatus::ALREADY_IN_STATE;
                    response.message = "Thread is already running";
                } else {
                    // Thread is not alive - attempt to restart it
                    log_info("RPC: Thread '%s' (ID: %u) is not alive, attempting restart", 
                             threadName.c_str(), threadId);
                    
                    // Look for restart callback while holding the lock
                    std::function<unsigned int()> restartCallback;
                    bool hasCallback = false;
                    
                    {
                        std::lock_guard<std::mutex> lock(registryMutex_);
                        auto callbackIt = restartCallbacks_.find(threadName);
                        if (callbackIt != restartCallbacks_.end()) {
                            restartCallback = callbackIt->second;
                            hasCallback = true;
                            log_info("RPC: Found restart callback for thread '%s'", threadName.c_str());
                        } else {
                            log_error("RPC: No restart callback found for thread '%s'", threadName.c_str());
                        }
                    }
                    
                    if (hasCallback) {
                        // Clean up old thread resources
                        log_info("RPC: Cleaning up old thread '%s' (ID: %u)", 
                                 threadName.c_str(), threadId);
                        
                        // Force kill the old thread if it's still registered
                        try {
                            // First try to stop it gracefully
                            threadManager_.stopThread(threadId);
                            
                            // Wait a bit for graceful shutdown
                            if (!threadManager_.joinThread(threadId, std::chrono::milliseconds(500))) {
                                log_warning("RPC: Thread '%s' did not stop gracefully, forcing cleanup", 
                                           threadName.c_str());
                            }
                        } catch (const std::exception& e) {
                            log_warning("RPC: Exception during thread cleanup: %s (this is expected for dead threads)", 
                                       e.what());
                        }
                        
                        // Try to unregister from thread manager first (before removing from our registry)
                        try {
                            if (!attachmentId.empty()) {
                                threadManager_.unregisterThread(attachmentId);
                                log_info("RPC: Successfully unregistered attachment '%s' from thread manager", 
                                        attachmentId.c_str());
                            }
                        } catch (const std::exception& e) {
                            // Ignore errors - thread may not be registered or already cleaned up
                            log_info("RPC: Could not unregister attachment '%s': %s (this is normal for dead threads)", 
                                    attachmentId.c_str(), e.what());
                        }
                        
                        // Unregister old thread from our registry
                        {
                            std::lock_guard<std::mutex> lock(registryMutex_);
                            threadRegistry_.erase(threadName);
                            threadAttachments_.erase(threadName);
                        }
                        
                        // Create new thread using the callback
                        log_info("RPC: Creating new thread for '%s'", threadName.c_str());
                        unsigned int newThreadId = restartCallback();
                        
                        response.status = OperationStatus::SUCCESS;
                        response.message = "Thread restarted successfully with new ID: " + 
                                         std::to_string(newThreadId);
                        
                        // Get updated thread info
                        std::lock_guard<std::mutex> lock(registryMutex_);
                        ThreadStateInfo info = getThreadStateInfo(threadName);
                        response.threadStates[threadName] = info;
                        
                        log_info("RPC: Thread '%s' restarted successfully with new ID %u", 
                                 threadName.c_str(), newThreadId);
                    } else {
                        response.status = OperationStatus::FAILED;
                        response.message = "Thread is not alive and no restart callback registered";
                        log_error("RPC: Cannot restart thread '%s' - no restart callback available", 
                                 threadName.c_str());
                    }
                }
                break;

            case ThreadOperation::STOP:
                log_info("RPC: Stopping thread '%s' (ID: %u) via attachment '%s'", 
                         threadName.c_str(), threadId, attachmentId.c_str());

                // Handle mainloop thread - use cooperative stop via ThreadManager
                // This signals the thread to stop without calling request_exit on the singleton
                if (threadName == "mainloop") {
                    log_info("RPC: Requesting mainloop thread cooperative stop");
                    
                    // Signal the mainloop to exit by calling request_exit on its instance
                    // The mainloop's loop() function checks _should_exit flag
                    Mainloop::instance().request_exit(0);
                    
                    // Don't join here - let the thread stop naturally
                    response.status = OperationStatus::SUCCESS;
                    response.message = "Mainloop thread stop requested";
                    log_info("RPC: Mainloop thread stop requested successfully");
                } else if (threadName == "ALL") { // Special case for stopping all threads
                    log_info("RPC: Stopping ALL threads (mainloop + extensions)");
                    response.status = OperationStatus::SUCCESS;
                    response.message = "All threads stop requested";
                    
                    // Stop all threads cooperatively without shutting down the application
                    for (const auto& pair : threadRegistry_) {
                        unsigned int currentThreadId = pair.second;
                        std::string currentAttachmentId;
                        auto currentAttachIt = threadAttachments_.find(pair.first);
                        if (currentAttachIt != threadAttachments_.end()) {
                            currentAttachmentId = currentAttachIt->second;
                        }
                        
                        log_info("RPC: Stopping thread '%s' (ID: %u) cooperatively", 
                                 pair.first.c_str(), currentThreadId);
                        
                        if (pair.first == "mainloop") {
                            // For mainloop, call request_exit
                            Mainloop::instance().request_exit(0);
                        } else if (pair.first != "http_server") {
                            // For other non-HTTP threads, use cooperative stop
                            threadManager_.stopThread(currentThreadId);
                        }
                        // Don't stop http_server when stopping "ALL" - keep it running
                    }
                    log_info("RPC: All threads (except HTTP server) stop requested successfully");
                } else {
                    // For other threads, use cooperative stop mechanism
                    log_info("RPC: Stopping thread '%s' cooperatively", threadName.c_str());
                    threadManager_.stopThread(threadId);

                    response.status = OperationStatus::SUCCESS;
                    response.message = "Thread stop requested";
                    log_info("RPC: Thread '%s' stop requested successfully", threadName.c_str());
                }
            break;

            case ThreadOperation::PAUSE:
                log_info("RPC: Pausing thread '%s' (ID: %u)", threadName.c_str(), threadId);
                threadManager_.pauseThread(threadId);
                response.message = "Thread paused successfully";
                response.status = OperationStatus::SUCCESS;
                break;

            case ThreadOperation::RESUME:
                log_info("RPC: Resuming thread '%s' (ID: %u)", threadName.c_str(), threadId);
                threadManager_.resumeThread(threadId);
                response.message = "Thread resumed successfully";
                response.status = OperationStatus::SUCCESS;
                break;

            case ThreadOperation::RESTART: {
                log_info("RPC: Restarting thread '%s' (ID: %u)", threadName.c_str(), threadId);

                // First stop the thread gracefully
                threadManager_.stopThread(threadId);

                // Wait for it to complete
                bool stopped = threadManager_.joinThread(threadId, std::chrono::seconds(5));

                if (stopped) {
                    // Thread stopped successfully - restart would need to be implemented
                    // by the caller creating a new thread
                    response.status = OperationStatus::SUCCESS;
                    response.message = "Thread stopped successfully - ready for restart";
                    log_info("RPC: Thread '%s' stopped for restart", threadName.c_str());
                } else {
                    log_warning("RPC: Thread '%s' did not stop in time for restart", threadName.c_str());
                    response.status = OperationStatus::TIMEOUT;
                    response.message = "Thread did not stop within timeout - restart aborted";
                }
                break;
            }

            case ThreadOperation::STATUS:
                response = getThreadStatus(threadName);
                return response;

            default:
                response.status = OperationStatus::INVALID_OPERATION;
                response.message = "Invalid operation";
                return response;
        }

        // Add current thread state to response
        if (response.status == OperationStatus::SUCCESS || 
            response.status == OperationStatus::ALREADY_IN_STATE) {
            registryMutex_.lock();
            ThreadStateInfo info = getThreadStateInfo(threadName);
            registryMutex_.unlock();
            response.threadStates[threadName] = info;
        }

    } catch (const ThreadMgr::ThreadManagerException& e) {
        response.status = OperationStatus::FAILED;
        response.message = std::string("Operation failed: ") + e.what();
        log_error("RPC: Thread operation failed: %s", e.what());
    }

    return response;
}

std::vector<std::string> RpcController::getThreadNamesForTarget(ThreadTarget target) {
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(registryMutex_);

    switch (target) {
        case ThreadTarget::MAINLOOP:
            if (threadRegistry_.find("mainloop") != threadRegistry_.end()) {
                names.push_back("mainloop");
            } else if (restartCallbacks_.find("mainloop") != restartCallbacks_.end()) {
                // Thread not registered but has restart callback - add it so we can create it
                names.push_back("mainloop");
            }
            break;

        case ThreadTarget::HTTP_SERVER:
            if (threadRegistry_.find("http_server") != threadRegistry_.end()) {
                names.push_back("http_server");
            } else if (restartCallbacks_.find("http_server") != restartCallbacks_.end()) {
                names.push_back("http_server");
            }
            break;

        case ThreadTarget::STATISTICS:
            if (threadRegistry_.find("statistics") != threadRegistry_.end()) {
                names.push_back("statistics");
            } else if (restartCallbacks_.find("statistics") != restartCallbacks_.end()) {
                names.push_back("statistics");
            }
            break;

        case ThreadTarget::ALL:
            for (const auto& pair : threadRegistry_) {
                names.push_back(pair.first);
            }
            // Also add threads that have restart callbacks but aren't registered
            for (const auto& pair : restartCallbacks_) {
                if (threadRegistry_.find(pair.first) == threadRegistry_.end()) {
                    names.push_back(pair.first);
                }
            }
            break;
    }

    return names;
}

RpcResponse RpcController::executeRequest(const RpcRequest& request) {
    RpcResponse response;

    std::vector<std::string> targetThreads = getThreadNamesForTarget(request.target);

    if (targetThreads.empty()) {
        response.status = OperationStatus::THREAD_NOT_FOUND;
        response.message = "No threads found for target";
        return response;
    }

    bool allSuccess = true;
    std::stringstream messageStream;

    for (const auto& threadName : targetThreads) {
        RpcResponse threadResponse = executeOperationOnThread(threadName, request.operation);

        if (threadResponse.status != OperationStatus::SUCCESS) {
            allSuccess = false;
            messageStream << threadName << ": " << threadResponse.message << "; ";
        }

        // Merge thread states
        for (const auto& pair : threadResponse.threadStates) {
            response.threadStates[pair.first] = pair.second;
        }
    }

    response.status = allSuccess ? OperationStatus::SUCCESS : OperationStatus::FAILED;
    response.message = allSuccess ? "Operation completed successfully" : messageStream.str();

    return response;
}

RpcResponse RpcController::startThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::START, target);
    return executeRequest(request);
}

RpcResponse RpcController::stopThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::STOP, target);
    return executeRequest(request);
}

RpcResponse RpcController::pauseThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::PAUSE, target);
    return executeRequest(request);
}

RpcResponse RpcController::resumeThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::RESUME, target);
    return executeRequest(request);
}

RpcResponse RpcController::restartThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::RESTART, target);
    return executeRequest(request);
}

std::string RpcController::threadTargetToString(ThreadTarget target) {
    switch (target) {
        case ThreadTarget::MAINLOOP: return "mainloop";
        case ThreadTarget::HTTP_SERVER: return "http_server";
        case ThreadTarget::STATISTICS: return "statistics";
        case ThreadTarget::ALL: return "all";
        default: return "unknown";
    }
}

ThreadTarget RpcController::stringToThreadTarget(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "mainloop") return ThreadTarget::MAINLOOP;
    if (lower == "http_server") return ThreadTarget::HTTP_SERVER;
    if (lower == "statistics") return ThreadTarget::STATISTICS;
    if (lower == "all") return ThreadTarget::ALL;

    return ThreadTarget::ALL;
}

std::string RpcController::threadOperationToString(ThreadOperation op) {
    switch (op) {
        case ThreadOperation::START: return "start";
        case ThreadOperation::STOP: return "stop";
        case ThreadOperation::PAUSE: return "pause";
        case ThreadOperation::RESUME: return "resume";
        case ThreadOperation::RESTART: return "restart";
        case ThreadOperation::STATUS: return "status";
        default: return "unknown";
    }
}

ThreadOperation RpcController::stringToThreadOperation(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "start") return ThreadOperation::START;
    if (lower == "stop") return ThreadOperation::STOP;
    if (lower == "pause") return ThreadOperation::PAUSE;
    if (lower == "resume") return ThreadOperation::RESUME;
    if (lower == "restart") return ThreadOperation::RESTART;
    if (lower == "status") return ThreadOperation::STATUS;

    return ThreadOperation::STATUS;
}

} // namespace RpcMechanisms