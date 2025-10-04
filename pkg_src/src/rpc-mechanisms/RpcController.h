
/**
 * @file RpcController.h
 * @brief RPC controller for MAVLink router thread and statistics management
 */

#ifndef RPC_CONTROLLER_H
#define RPC_CONTROLLER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include "ThreadManager.hpp"

namespace RpcMechanisms {

/**
 * @brief Thread operation types
 */
enum class ThreadOperation {
    START,
    STOP,
    PAUSE,
    RESUME,
    RESTART,
    STATUS
};

/**
 * @brief Thread target types
 */
enum class ThreadTarget {
    MAINLOOP,
    HTTP_SERVER,
    STATISTICS,
    ALL
};

/**
 * @brief Operation result status
 */
enum class OperationStatus {
    SUCCESS,
    FAILED,
    THREAD_NOT_FOUND,
    INVALID_OPERATION,
    ALREADY_IN_STATE,
    TIMEOUT
};

/**
 * @brief Thread state information
 */
struct ThreadStateInfo {
    std::string threadName;
    unsigned int threadId;
    ThreadMgr::ThreadState state;
    bool isAlive;
    std::string attachmentId;
    
    ThreadStateInfo() 
        : threadName("")
        , threadId(0)
        , state(ThreadMgr::ThreadState::Created)
        , isAlive(false)
        , attachmentId("") {}
};

/**
 * @brief RPC request structure
 */
struct RpcRequest {
    ThreadOperation operation;
    ThreadTarget target;
    std::map<std::string, std::string> parameters;
    
    RpcRequest(ThreadOperation op = ThreadOperation::STATUS, 
               ThreadTarget tgt = ThreadTarget::ALL)
        : operation(op), target(tgt) {}
};

/**
 * @brief RPC response structure
 */
struct RpcResponse {
    OperationStatus status;
    std::string message;
    std::map<std::string, ThreadStateInfo> threadStates;
    
    RpcResponse() : status(OperationStatus::SUCCESS), message("") {}
    
    std::string toJson() const;
};

/**
 * @brief RPC Controller for thread management
 */
class RpcController {
public:
    /**
     * @brief Constructor
     * @param threadManager Reference to the thread manager
     */
    explicit RpcController(ThreadMgr::ThreadManager& threadManager);
    
    /**
     * @brief Destructor
     */
    ~RpcController();
    
    // Disable copy
    RpcController(const RpcController&) = delete;
    RpcController& operator=(const RpcController&) = delete;
    
    /**
     * @brief Register a thread for RPC control
     * @param threadName Logical name for the thread
     * @param threadId Thread ID from thread manager
     * @param attachmentId Attachment identifier
     */
    void registerThread(const std::string& threadName, 
                       unsigned int threadId,
                       const std::string& attachmentId);
    
    /**
     * @brief Unregister a thread
     * @param threadName Thread name
     */
    void unregisterThread(const std::string& threadName);
    
    /**
     * @brief Execute an RPC request
     * @param request RPC request structure
     * @return RPC response
     */
    RpcResponse executeRequest(const RpcRequest& request);
    
    /**
     * @brief Get status of all registered threads
     * @return RPC response with thread states
     */
    RpcResponse getAllThreadStatus();
    
    /**
     * @brief Get status of a specific thread
     * @param threadName Thread name
     * @return RPC response with thread state
     */
    RpcResponse getThreadStatus(const std::string& threadName);
    
    /**
     * @brief Start a thread
     * @param target Thread target
     * @return Operation result
     * 
     * @note If thread is not alive, attempts to restart it with original configuration
     */
    RpcResponse startThread(ThreadTarget target);
    
    /**
     * @brief Register a restart callback for a thread
     * @param threadName Thread name
     * @param restartCallback Function to call to recreate the thread
     */
    void registerRestartCallback(const std::string& threadName, 
                                 std::function<unsigned int()> restartCallback);
    
    /**
     * @brief Stop a thread gracefully
     * @param target Thread target
     * @return Operation result
     * 
     * @note For mainloop threads, uses Mainloop::request_exit()
     * @note For other threads, uses cooperative stopThread() with joinThread()
     * @note Waits up to 5 seconds for thread to complete
     */
    RpcResponse stopThread(ThreadTarget target);
    
    /**
     * @brief Pause a thread
     * @param target Thread target
     * @return Operation result
     */
    RpcResponse pauseThread(ThreadTarget target);
    
    /**
     * @brief Resume a thread
     * @param target Thread target
     * @return Operation result
     */
    RpcResponse resumeThread(ThreadTarget target);
    
    /**
     * @brief Restart a thread
     * @param target Thread target
     * @return Operation result
     * 
     * @note Currently only stops the thread gracefully
     * @note Caller must create a new thread to complete restart
     */
    RpcResponse restartThread(ThreadTarget target);
    
    /**
     * @brief Convert thread target to string
     * @param target Thread target
     * @return String representation
     */
    static std::string threadTargetToString(ThreadTarget target);
    
    /**
     * @brief Convert string to thread target
     * @param str String representation
     * @return Thread target
     */
    static ThreadTarget stringToThreadTarget(const std::string& str);
    
    /**
     * @brief Convert thread operation to string
     * @param op Thread operation
     * @return String representation
     */
    static std::string threadOperationToString(ThreadOperation op);
    
    /**
     * @brief Convert string to thread operation
     * @param str String representation
     * @return Thread operation
     */
    static ThreadOperation stringToThreadOperation(const std::string& str);

private:
    ThreadMgr::ThreadManager& threadManager_;
    std::map<std::string, unsigned int> threadRegistry_;
    std::map<std::string, std::string> threadAttachments_;
    std::map<std::string, std::function<unsigned int()>> restartCallbacks_;
    mutable std::mutex registryMutex_;
    
    ThreadStateInfo getThreadStateInfo(const std::string& threadName);
    RpcResponse executeOperationOnThread(const std::string& threadName, 
                                        ThreadOperation operation);
    std::vector<std::string> getThreadNamesForTarget(ThreadTarget target);
};

} // namespace RpcMechanisms

#endif // RPC_CONTROLLER_H
