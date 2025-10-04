/**
 * @file ThreadManager.tpp
 * @brief Template implementations for ThreadManager
 */

#ifndef THREAD_MANAGER_TPP
#define THREAD_MANAGER_TPP

namespace ThreadMgr {

template<typename Func, typename... Args>
unsigned int ThreadManager::createThread(Func&& func, Args&&... args) {
    // Create wrapper that stores the function and arguments  
    auto wrapper = ThreadFunctionWrapper<Func, Args...>(
        std::forward<Func>(func), std::forward<Args>(args)...);
    
    // Convert to std::function and delegate to the existing method
    return createThread(wrapper.callable);
}

template<typename Func, typename... Args>
void ThreadManager::restartThreadByAttachment(const std::string& attachmentArg, Func&& func, Args&&... args) {
    // Find the thread by attachment
    unsigned int threadId = findThreadByAttachment(attachmentArg);
    
    // Create wrapper that stores the function and arguments
    auto wrapper = ThreadFunctionWrapper<Func, Args...>(
        std::forward<Func>(func), std::forward<Args>(args)...);
    
    // Stop and restart the thread by delegating to existing template method
    stopThread(threadId);
    
    // Create new thread with same attachment
    unsigned int newThreadId = createThread(wrapper.callable);
    
    // Unregister old and register new
    unregisterThread(attachmentArg);
    registerThread(newThreadId, attachmentArg);
}

template<typename Func, typename... Args>
Thread::Thread(ThreadManager& manager, Func&& func, Args&&... args)
    : manager_(&manager)
    , threadId_(manager.createThread(std::forward<Func>(func), std::forward<Args>(args)...))
    , joined_(false) {
}

} // namespace ThreadMgr

#endif // THREAD_MANAGER_TPP