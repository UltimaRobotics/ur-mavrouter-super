
#include "ExtensionManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <set>
#include <ctime>
#include <cstdlib>
#include <dirent.h>
#include <mutex>
#include <map>
#include <atomic>
#include "../../modules/nholmann/json.hpp"
#include "../common/log.h"

using json = nlohmann::json;

namespace MavlinkExtensions {

ExtensionType stringToExtensionType(const std::string& typeStr) {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "internal") return ExtensionType::INTERNAL;
    if (lower == "tcp") return ExtensionType::TCP;
    if (lower == "udp") return ExtensionType::UDP;
    
    return ExtensionType::UDP; // default
}

std::string extensionTypeToString(ExtensionType type) {
    switch (type) {
        case ExtensionType::INTERNAL: return "internal";
        case ExtensionType::TCP: return "tcp";
        case ExtensionType::UDP: return "udp";
        default: return "udp";
    }
}

ExtensionManager::ExtensionManager(ThreadMgr::ThreadManager& threadManager)
    : threadManager_(threadManager)
    , extensionConfDir_("pkg_src/config")
    , globalConfig_(nullptr) {
    log_info("ExtensionManager initialized");
}

ExtensionManager::~ExtensionManager() {
    // Stop all extensions
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    for (auto& pair : extensions_) {
        if (pair.second.isRunning) {
            try {
                threadManager_.stopThread(pair.second.threadId);
            } catch (...) {
                // Ignore errors during cleanup
            }
        }
    }
    log_info("ExtensionManager destroyed");
}

void ExtensionManager::setExtensionConfDir(const std::string& confDir) {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    extensionConfDir_ = confDir;
    log_info("ExtensionManager: Configuration directory set to '%s'", confDir.c_str());
}

void ExtensionManager::setGlobalConfig(const Configuration* config) {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    globalConfig_ = config;
    log_info("ExtensionManager: Global configuration reference set");
}

std::string ExtensionManager::assignAvailableExtensionPoint(const Configuration& globalConfig, ExtensionType type) {
    // Collect all used extension points from existing extensions
    std::set<std::string> usedPoints;
    for (const auto& pair : extensions_) {
        if (!pair.second.config.assigned_extension_point.empty()) {
            usedPoints.insert(pair.second.config.assigned_extension_point);
            log_debug("ExtensionManager: Extension '%s' is using '%s'", 
                     pair.first.c_str(), pair.second.config.assigned_extension_point.c_str());
        }
    }
    
    // For INTERNAL type, look for available internal-router-point-<N>
    if (type == ExtensionType::INTERNAL) {
        log_info("ExtensionManager: Looking for available internal-router-point for INTERNAL type extension");
        for (const auto& udp : globalConfig.udp_configs) {
            if (udp.name.find("internal-router-point") != std::string::npos) {
                if (usedPoints.find(udp.name) == usedPoints.end()) {
                    log_info("ExtensionManager: Assigned internal router point '%s' (address: %s:%lu)", 
                            udp.name.c_str(), udp.address.c_str(), udp.port);
                    return udp.name;
                } else {
                    log_debug("ExtensionManager: Internal router point '%s' is already in use", udp.name.c_str());
                }
            }
        }
        log_warning("ExtensionManager: No available internal-router-point found for INTERNAL type extension");
        return "";
    }
    
    // For TCP type, look for available tcp-extension-point-<N>
    if (type == ExtensionType::TCP) {
        log_info("ExtensionManager: Looking for available tcp-extension-point for TCP type extension");
        for (const auto& tcp : globalConfig.tcp_configs) {
            if (tcp.name.find("tcp-extension-point") != std::string::npos) {
                if (usedPoints.find(tcp.name) == usedPoints.end()) {
                    log_info("ExtensionManager: Assigned TCP extension point '%s' (address: %s:%lu)", 
                            tcp.name.c_str(), tcp.address.c_str(), tcp.port);
                    return tcp.name;
                } else {
                    log_debug("ExtensionManager: TCP extension point '%s' is already in use", tcp.name.c_str());
                }
            }
        }
        log_warning("ExtensionManager: No available tcp-extension-point found for TCP type extension");
        return "";
    }
    
    // For UDP type (default), look for available udp-extension-point-<N>
    log_info("ExtensionManager: Looking for available udp-extension-point for UDP type extension");
    for (const auto& udp : globalConfig.udp_configs) {
        if (udp.name.find("udp-extension-point") != std::string::npos) {
            if (usedPoints.find(udp.name) == usedPoints.end()) {
                log_info("ExtensionManager: Assigned UDP extension point '%s' (address: %s:%lu)", 
                        udp.name.c_str(), udp.address.c_str(), udp.port);
                return udp.name;
            } else {
                log_debug("ExtensionManager: UDP extension point '%s' is already in use", udp.name.c_str());
            }
        }
    }
    
    log_warning("ExtensionManager: No available udp-extension-point found for UDP type extension");
    return "";
}

unsigned int ExtensionManager::launchExtensionThread(const ExtensionConfig& config) {
    auto extensionFunc = [config, this]() {
        log_info("Extension thread '%s' starting", config.name.c_str());
        
        Mainloop* extensionLoop = nullptr;
        
        try {
            // Create a new independent Mainloop instance for this extension thread
            extensionLoop = Mainloop::create_extension_instance();
            
            if (!extensionLoop) {
                log_error("Extension '%s': Failed to create mainloop instance", config.name.c_str());
                return;
            }
            
            log_info("Extension '%s': Created independent mainloop instance at %p", 
                     config.name.c_str(), extensionLoop);
            
            // CRITICAL: Set this as the thread-local mainloop instance
            Mainloop::set_thread_instance(extensionLoop);
            log_info("Extension '%s': Set thread-local mainloop instance for thread-safe operation", 
                     config.name.c_str());
            
            // CRITICAL: Store the mainloop instance pointer in ExtensionInfo
            // This must happen BEFORE entering the event loop so stopExtension() can access it
            {
                std::lock_guard<std::mutex> lock(extensionsMutex_);
                auto it = extensions_.find(config.name);
                if (it != extensions_.end()) {
                    it->second.mainloopInstance = extensionLoop;
                    log_info("Extension '%s': Stored mainloop instance pointer %p in ExtensionInfo", 
                             config.name.c_str(), extensionLoop);
                } else {
                    log_error("Extension '%s': Not found in extensions map, cannot store instance pointer", 
                             config.name.c_str());
                }
            }
            
            if (extensionLoop->open() < 0) {
                log_error("Extension '%s': Failed to open mainloop", config.name.c_str());
                throw std::runtime_error("Failed to open mainloop");
            }
            
            // Use only the extension_thread_config portion
            if (!extensionLoop->add_endpoints(config.extension_thread_config)) {
                log_error("Extension '%s': Failed to add endpoints", config.name.c_str());
                throw std::runtime_error("Failed to add endpoints");
            }
            
            log_info("Extension '%s': Starting event loop with %zu UDP and %zu TCP endpoints", 
                     config.name.c_str(), 
                     config.extension_thread_config.udp_configs.size(),
                     config.extension_thread_config.tcp_configs.size());
            log_info("Extension '%s': Each endpoint uses thread-local dedup for isolation", 
                     config.name.c_str());
            
            // Enter the mainloop - this will block until request_exit() is called
            // The mainloop's loop() function will exit cleanly when should_exit is set
            int ret = extensionLoop->loop();
            log_info("Extension '%s': Event loop exited gracefully with code %d", config.name.c_str(), ret);
            
        } catch (const std::exception& e) {
            log_error("Extension '%s': Exception in thread: %s", config.name.c_str(), e.what());
        } catch (...) {
            log_error("Extension '%s': Unknown exception in thread", config.name.c_str());
        }
        
        // CRITICAL: Always clean up resources - this happens whether the thread
        // exited normally via request_exit() or via exception
        log_info("Extension '%s': Beginning comprehensive resource cleanup", config.name.c_str());
        
        if (extensionLoop) {
            // STEP 1: Clear thread-local reference to prevent any new operations
            Mainloop::set_thread_instance(nullptr);
            log_info("Extension '%s': Cleared thread-local mainloop reference", config.name.c_str());
            
            // STEP 2: Comprehensive cleanup via destroy_extension_instance:
            // This function uses the FD tracking system to force close all FDs:
            // 1. Force closes all tracked FDs (endpoints, timeouts, epoll, TCP server)
            // 2. Clears endpoint vectors
            // 3. Frees timeout structures
            // 4. Deletes the mainloop instance
            log_info("Extension '%s': Destroying mainloop instance using FD tracking", 
                     config.name.c_str());
            
            Mainloop::destroy_extension_instance(extensionLoop);
            extensionLoop = nullptr; // Prevent dangling pointer
            
            log_info("Extension '%s': All file descriptors closed via FD tracking system", 
                     config.name.c_str());
        } else {
            log_warning("Extension '%s': No mainloop instance to clean up", config.name.c_str());
        }
        
        log_info("Extension thread '%s' finished with complete graceful shutdown", config.name.c_str());
    };
    
    unsigned int threadId = threadManager_.createThread(extensionFunc);
    std::string attachmentId = "extension_" + config.name;
    threadManager_.registerThread(threadId, attachmentId);
    
    log_info("Extension '%s' launched with thread ID %u (mainloop instance will be stored by thread)", 
             config.name.c_str(), threadId);
    return threadId;
}

std::string ExtensionManager::createExtension(const ExtensionConfig& config) {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    if (extensions_.find(config.name) != extensions_.end()) {
        log_error("Extension '%s' already exists", config.name.c_str());
        return "Extension already exists";
    }
    
    // Make a copy so we can modify it
    ExtensionConfig modifiedConfig = config;
    
    // Always auto-assign extension point based on type, ignoring any user-provided value
    if (globalConfig_) {
        // If user provided a value, log that we're ignoring it
        if (!modifiedConfig.assigned_extension_point.empty()) {
            log_info("Ignoring user-provided extension point '%s' for extension '%s', will auto-assign based on type '%s'", 
                     modifiedConfig.assigned_extension_point.c_str(), 
                     config.name.c_str(), 
                     extensionTypeToString(config.type).c_str());
        }
        
        // Auto-assign based on type and availability
        modifiedConfig.assigned_extension_point = assignAvailableExtensionPoint(*globalConfig_, modifiedConfig.type);
        if (modifiedConfig.assigned_extension_point.empty()) {
            log_error("No available extension points for extension '%s' of type '%s'", 
                     config.name.c_str(), extensionTypeToString(config.type).c_str());
            return "No available extension points";
        }
        log_info("Auto-assigned extension point '%s' to extension '%s'", 
                 modifiedConfig.assigned_extension_point.c_str(), config.name.c_str());
    } else {
        log_error("Global configuration not set, cannot assign extension point for '%s'", config.name.c_str());
        return "Global configuration not available";
    }
    
    if (!validateExtensionConfig(modifiedConfig)) {
        return "Invalid extension configuration";
    }
    
    // Build extension_thread_config
    modifiedConfig.extension_thread_config = Configuration{};
    
    // Generate random TCP server port for extension (range 50000-60000)
    std::srand(std::time(nullptr) + extensions_.size());
    modifiedConfig.extension_thread_config.tcp_port = 50000 + (std::rand() % 10000);
    
    // Handle INTERNAL type - connect to internal-router-point via UDP
    if (modifiedConfig.type == ExtensionType::INTERNAL) {
        // For internal extensions: use UDP to connect to internal-router-point
        if (globalConfig_) {
            // Verify that assigned extension point is an internal-router-point
            if (modifiedConfig.assigned_extension_point.find("internal-router-point") == std::string::npos) {
                log_error("Extension '%s' of type INTERNAL must use internal-router-point, not '%s'", 
                         config.name.c_str(), modifiedConfig.assigned_extension_point.c_str());
                return "INTERNAL type must use internal-router-point";
            }
            
            // Find the assigned internal-router-point in global config
            bool found = false;
            for (const auto& udp : globalConfig_->udp_configs) {
                if (udp.name == modifiedConfig.assigned_extension_point) {
                    UdpEndpointConfig serverEndpoint = udp;
                    // Internal-router-point acts as Server mode (waiting for connection)
                    serverEndpoint.mode = UdpEndpointConfig::Mode::Server;
                    modifiedConfig.extension_thread_config.udp_configs.push_back(serverEndpoint);
                    log_info("Added internal-router-point '%s' as Server endpoint at %s:%lu", 
                             serverEndpoint.name.c_str(), serverEndpoint.address.c_str(), serverEndpoint.port);
                    
                    // The extension itself connects as a client
                    // Use the user-provided address and port from the original config
                    UdpEndpointConfig extensionEndpoint;
                    extensionEndpoint.name = modifiedConfig.name;
                    extensionEndpoint.address = modifiedConfig.address;  // Use user-provided address
                    extensionEndpoint.port = modifiedConfig.port;        // Use user-provided port
                    extensionEndpoint.mode = UdpEndpointConfig::Mode::Client;
                    modifiedConfig.extension_thread_config.udp_configs.push_back(extensionEndpoint);
                    log_info("Added internal extension '%s' as Client endpoint at %s:%lu", 
                             extensionEndpoint.name.c_str(), extensionEndpoint.address.c_str(), extensionEndpoint.port);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                log_error("Internal router point '%s' not found in global configuration", 
                         modifiedConfig.assigned_extension_point.c_str());
                return "Internal router point not found in configuration";
            }
        }
    }
    // Handle TCP vs UDP extension points differently
    else if (modifiedConfig.type == ExtensionType::TCP) {
        // For TCP extensions: add TCP endpoints
        if (globalConfig_) {
            // Find the assigned TCP extension point in global config
            for (const auto& tcp : globalConfig_->tcp_configs) {
                if (tcp.name == modifiedConfig.assigned_extension_point) {
                    TcpEndpointConfig serverEndpoint = tcp;
                    // TCP extension point acts as Server mode (waiting for connection)
                    modifiedConfig.extension_thread_config.tcp_configs.push_back(serverEndpoint);
                    log_info("Added TCP extension point '%s' as Server endpoint at %s:%lu", 
                             serverEndpoint.name.c_str(), serverEndpoint.address.c_str(), serverEndpoint.port);
                    break;
                }
            }
        }
        
        // Add the TCP extension itself as Client endpoint
        TcpEndpointConfig extensionEndpoint;
        extensionEndpoint.name = modifiedConfig.name;
        extensionEndpoint.address = modifiedConfig.address;
        extensionEndpoint.port = modifiedConfig.port;
        extensionEndpoint.retry_timeout = 5000; // 5 second retry
        modifiedConfig.extension_thread_config.tcp_configs.push_back(extensionEndpoint);
        log_info("Added TCP extension '%s' as Client endpoint at %s:%lu", 
                 extensionEndpoint.name.c_str(), extensionEndpoint.address.c_str(), extensionEndpoint.port);
    } else {
        // For UDP extensions: add UDP endpoints (original logic)
        if (globalConfig_) {
            for (const auto& udp : globalConfig_->udp_configs) {
                if (udp.name == modifiedConfig.assigned_extension_point) {
                    UdpEndpointConfig serverEndpoint = udp;
                    serverEndpoint.mode = UdpEndpointConfig::Mode::Server;  // Extension point as Server
                    modifiedConfig.extension_thread_config.udp_configs.push_back(serverEndpoint);
                    log_info("Added UDP extension point '%s' as Server endpoint", serverEndpoint.name.c_str());
                    break;
                }
            }
        }
        
        // Add the UDP extension itself as Client mode
        UdpEndpointConfig extensionEndpoint;
        extensionEndpoint.name = modifiedConfig.name;
        extensionEndpoint.address = modifiedConfig.address;
        extensionEndpoint.port = modifiedConfig.port;
        extensionEndpoint.mode = UdpEndpointConfig::Mode::Client;  // Extension as Client
        modifiedConfig.extension_thread_config.udp_configs.push_back(extensionEndpoint);
        log_info("Added UDP extension '%s' as Client endpoint at %s:%lu", 
                 extensionEndpoint.name.c_str(), extensionEndpoint.address.c_str(), extensionEndpoint.port);
    }
    
    ExtensionInfo info;
    info.name = modifiedConfig.name;
    info.config = modifiedConfig;
    info.isRunning = false;
    info.mainloopInstance = nullptr;
    
    try {
        info.threadId = launchExtensionThread(modifiedConfig);
        info.isRunning = true;
        extensions_[modifiedConfig.name] = info;
        
        // Save configuration to file
        saveExtensionConfig(modifiedConfig.name);
        
        log_info("Extension '%s' created successfully with extension point '%s'", 
                 modifiedConfig.name.c_str(), modifiedConfig.assigned_extension_point.c_str());
        return "Success";
    } catch (const std::exception& e) {
        log_error("Failed to create extension '%s': %s", config.name.c_str(), e.what());
        return std::string("Failed: ") + e.what();
    }
}

bool ExtensionManager::deleteExtension(const std::string& name) {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    auto it = extensions_.find(name);
    if (it == extensions_.end()) {
        log_warning("Extension '%s' not found", name.c_str());
        return false;
    }
    
    log_info("Deleting extension '%s' (thread ID: %u, running: %s)", 
             name.c_str(), it->second.threadId, it->second.isRunning ? "yes" : "no");
    
    // STEP 1: Stop the extension thread gracefully if running
    // Use the same graceful shutdown mechanism as stopExtension()
    if (it->second.isRunning) {
        try {
            unsigned int threadId = it->second.threadId;
            std::string attachmentId = "extension_" + name;
            
            // Wait for the extension's mainloop instance to be initialized
            Mainloop* extensionMainloop = nullptr;
            int retries = 0;
            const int maxRetries = 20; // 20 * 50ms = 1 second max
            
            while (!extensionMainloop && retries < maxRetries) {
                extensionMainloop = it->second.mainloopInstance;
                if (!extensionMainloop) {
                    log_debug("Waiting for extension mainloop initialization (attempt %d/%d)", 
                             retries + 1, maxRetries);
                    extensionsMutex_.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    extensionsMutex_.lock();
                    
                    // Re-validate extension still exists
                    it = extensions_.find(name);
                    if (it == extensions_.end()) {
                        log_error("Extension '%s' was removed during delete operation", name.c_str());
                        return false;
                    }
                    retries++;
                }
            }
            
            if (!extensionMainloop) {
                log_error("Extension '%s' mainloop never initialized after %d retries", 
                         name.c_str(), maxRetries);
                log_warning("Forcing thread termination via thread manager");
                
                try {
                    threadManager_.stopThread(threadId);
                    threadManager_.joinThread(threadId, std::chrono::seconds(2));
                    threadManager_.unregisterThread(attachmentId);
                } catch (const std::exception& e) {
                    log_error("Force termination failed: %s", e.what());
                }
            } else {
                // Signal graceful exit on the SPECIFIC extension mainloop instance
                log_info("Signaling graceful exit for extension '%s' mainloop at %p", 
                         name.c_str(), extensionMainloop);
                
                // Call request_exit(0) on THIS SPECIFIC instance
                extensionMainloop->request_exit(0);
                
                log_info("Exit signal sent to extension mainloop, waiting for thread to terminate");
                
                // Wait for the thread to exit gracefully
                bool exitedCleanly = threadManager_.joinThread(threadId, std::chrono::seconds(5));
                
                if (exitedCleanly) {
                    log_info("Extension '%s' thread exited gracefully", name.c_str());
                } else {
                    log_warning("Extension '%s' thread did not exit within timeout", name.c_str());
                }
                
                // Unregister from thread manager
                try {
                    threadManager_.unregisterThread(attachmentId);
                    log_info("Extension '%s' unregistered from thread manager", name.c_str());
                } catch (const std::exception& e) {
                    log_warning("Failed to unregister extension '%s': %s", name.c_str(), e.what());
                }
            }
            
            // Update extension state
            it->second.isRunning = false;
            it->second.mainloopInstance = nullptr;
            
        } catch (const std::exception& e) {
            log_warning("Error stopping extension '%s': %s", name.c_str(), e.what());
        }
    }
    
    // STEP 2: Remove config file
    std::string configPath = getConfigFilePath(name);
    log_info("Removing config file: %s", configPath.c_str());
    if (std::remove(configPath.c_str()) == 0) {
        log_info("Config file removed successfully");
    } else {
        log_warning("Failed to remove config file (may not exist)");
    }
    
    // STEP 3: Remove from extensions map
    extensions_.erase(it);
    log_info("Extension '%s' deleted successfully with graceful shutdown", name.c_str());
    return true;
}

bool ExtensionManager::stopExtension(const std::string& name) {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    auto it = extensions_.find(name);
    if (it == extensions_.end()) {
        log_warning("Extension '%s' not found", name.c_str());
        return false;
    }
    
    log_info("Stop request for extension '%s' (thread ID: %u, running: %s)", 
             name.c_str(), it->second.threadId, it->second.isRunning ? "yes" : "no");
    
    if (!it->second.isRunning) {
        log_info("Extension '%s' is already stopped", name.c_str());
        return true;
    }
    
    try {
        unsigned int threadId = it->second.threadId;
        std::string attachmentId = "extension_" + name;
        
        // STEP 1: Wait for the extension's mainloop instance to be initialized
        Mainloop* extensionMainloop = nullptr;
        int retries = 0;
        const int maxRetries = 20; // 20 * 50ms = 1 second max
        
        while (!extensionMainloop && retries < maxRetries) {
            extensionMainloop = it->second.mainloopInstance;
            if (!extensionMainloop) {
                log_debug("Waiting for extension mainloop initialization (attempt %d/%d)", 
                         retries + 1, maxRetries);
                extensionsMutex_.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                extensionsMutex_.lock();
                
                // Re-validate extension still exists
                it = extensions_.find(name);
                if (it == extensions_.end()) {
                    log_error("Extension '%s' was removed during stop operation", name.c_str());
                    return false;
                }
                retries++;
            }
        }
        
        if (!extensionMainloop) {
            log_error("Extension '%s' mainloop never initialized after %d retries", 
                     name.c_str(), maxRetries);
            log_error("Forcing thread termination via thread manager");
            
            try {
                threadManager_.stopThread(threadId);
                threadManager_.joinThread(threadId, std::chrono::seconds(2));
                threadManager_.unregisterThread(attachmentId);
            } catch (const std::exception& e) {
                log_error("Force termination failed: %s", e.what());
            }
            
            it->second.isRunning = false;
            it->second.mainloopInstance = nullptr;
            return false;
        }
        
        // STEP 2: Signal graceful exit on the SPECIFIC extension mainloop instance
        // This is the ONLY mainloop that will be affected
        log_info("Signaling graceful exit for extension '%s' mainloop at %p", 
                 name.c_str(), extensionMainloop);
        log_info("Main router mainloop remains unaffected");
        
        // Call request_exit(0) on THIS SPECIFIC instance - not the singleton
        extensionMainloop->request_exit(0);
        
        log_info("Exit signal sent to extension mainloop, waiting for thread to terminate");
        
        // STEP 3: Wait for the thread to exit gracefully
        bool exitedCleanly = threadManager_.joinThread(threadId, std::chrono::seconds(5));
        
        if (exitedCleanly) {
            log_info("Extension '%s' thread exited gracefully", name.c_str());
        } else {
            log_warning("Extension '%s' thread did not exit within timeout", name.c_str());
        }
        
        // STEP 4: Unregister from thread manager
        try {
            threadManager_.unregisterThread(attachmentId);
            log_info("Extension '%s' unregistered from thread manager", name.c_str());
        } catch (const std::exception& e) {
            log_warning("Failed to unregister extension '%s': %s", name.c_str(), e.what());
        }
        
        // STEP 5: Update extension state
        it->second.isRunning = false;
        it->second.mainloopInstance = nullptr;
        
        log_info("Extension '%s' stopped successfully", name.c_str());
        return true;
        
    } catch (const std::exception& e) {
        log_error("Failed to stop extension '%s': %s", name.c_str(), e.what());
        return false;
    }
}

bool ExtensionManager::startExtension(const std::string& name) {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    auto it = extensions_.find(name);
    if (it == extensions_.end()) {
        log_warning("Extension '%s' not found", name.c_str());
        return false;
    }
    
    log_info("Starting extension '%s' (current thread ID: %u, running: %s)", 
             name.c_str(), it->second.threadId, it->second.isRunning ? "yes" : "no");
    
    if (!it->second.isRunning) {
        try {
            // If there's an old thread ID, forcefully clean up any lingering resources
            if (it->second.threadId != 0) {
                unsigned int oldThreadId = it->second.threadId;
                std::string oldAttachmentId = "extension_" + name;
                
                log_info("Forcefully cleaning up old thread %u for extension '%s'", oldThreadId, name.c_str());
                
                // Try to stop the thread forcefully
                try {
                    threadManager_.stopThread(oldThreadId);
                    log_info("Old thread %u stopped", oldThreadId);
                } catch (...) {
                    log_debug("Old thread was already stopped or doesn't exist");
                }
                
                // Try to unregister the thread
                try {
                    threadManager_.unregisterThread(oldAttachmentId);
                    log_info("Old thread attachment '%s' unregistered", oldAttachmentId.c_str());
                } catch (...) {
                    log_debug("Old thread attachment was already unregistered");
                }
                
                // Wait briefly for any cleanup to complete
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            log_info("Launching new thread for extension '%s' using existing config", name.c_str());
            unsigned int newThreadId = launchExtensionThread(it->second.config);
            
            it->second.threadId = newThreadId;
            it->second.isRunning = true;
            
            log_info("Extension '%s' started successfully with new thread ID %u", name.c_str(), newThreadId);
            return true;
        } catch (const std::exception& e) {
            log_error("Failed to start extension '%s': %s", name.c_str(), e.what());
            return false;
        }
    }
    
    log_info("Extension '%s' is already running", name.c_str());
    return true;
}

ExtensionInfo ExtensionManager::getExtensionInfo(const std::string& name) const {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    auto it = extensions_.find(name);
    if (it != extensions_.end()) {
        return it->second;
    }
    
    return ExtensionInfo();
}

std::vector<ExtensionInfo> ExtensionManager::getAllExtensions() const {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    std::vector<ExtensionInfo> result;
    for (const auto& pair : extensions_) {
        result.push_back(pair.second);
    }
    return result;
}

bool ExtensionManager::extensionExists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    return extensions_.find(name) != extensions_.end();
}

std::string ExtensionManager::getConfigFilePath(const std::string& name) const {
    return extensionConfDir_ + "/extension_" + name + ".json";
}

bool ExtensionManager::saveExtensionConfig(const std::string& name) {
    auto it = extensions_.find(name);
    if (it == extensions_.end()) {
        return false;
    }
    
    json config;
    config["name"] = it->second.config.name;
    config["type"] = extensionTypeToString(it->second.config.type);
    config["address"] = it->second.config.address;
    config["port"] = it->second.config.port;
    config["assigned_extension_point"] = it->second.config.assigned_extension_point;
    
    // Build extension_thread_config
    json threadConfig;
    
    // General section with TCP server port
    json general;
    general["tcp_server_port"] = it->second.config.extension_thread_config.tcp_port;
    threadConfig["general"] = general;
    
    // UDP endpoints (if any)
    if (!it->second.config.extension_thread_config.udp_configs.empty()) {
        json udp_endpoints = json::array();
        for (const auto& udp : it->second.config.extension_thread_config.udp_configs) {
            json ep;
            ep["name"] = udp.name;
            ep["address"] = udp.address;
            ep["port"] = udp.port;
            ep["mode"] = (udp.mode == UdpEndpointConfig::Mode::Server) ? "Server" : "Client";
            udp_endpoints.push_back(ep);
        }
        threadConfig["udp_endpoints"] = udp_endpoints;
    }
    
    // For internal type extensions, also save the original config values
    // This preserves the extension's conceptual address/port even though it uses UDP internally
    if (it->second.config.type == ExtensionType::INTERNAL) {
        // The address and port at the top level represent the logical endpoint,
        // while the UDP endpoints show the actual internal-router-point connection
        log_info("Saving internal extension '%s' with internal-router-point '%s'",
                 it->second.config.name.c_str(), it->second.config.assigned_extension_point.c_str());
    }
    
    // TCP endpoints (if any)
    if (!it->second.config.extension_thread_config.tcp_configs.empty()) {
        json tcp_endpoints = json::array();
        for (const auto& tcp : it->second.config.extension_thread_config.tcp_configs) {
            json ep;
            ep["name"] = tcp.name;
            ep["address"] = tcp.address;
            ep["port"] = tcp.port;
            ep["retry_timeout"] = tcp.retry_timeout;
            tcp_endpoints.push_back(ep);
        }
        threadConfig["tcp_endpoints"] = tcp_endpoints;
    }
    
    config["extension_thread_config"] = threadConfig;
    
    std::string filepath = getConfigFilePath(name);
    std::ofstream file(filepath);
    if (!file.is_open()) {
        log_error("Failed to save extension config: %s", filepath.c_str());
        return false;
    }
    
    file << config.dump(2);
    file.close();
    log_info("Saved extension config: %s", filepath.c_str());
    return true;
}

bool ExtensionManager::loadExtensionConfigs(const std::string& configDir) {
    log_info("Loading extension configs from: %s", configDir.c_str());
    
    // Scan for extension_*.json files
    DIR* dir = opendir(configDir.c_str());
    if (!dir) {
        log_warning("Cannot open config directory: %s", configDir.c_str());
        return false;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.find("extension_") == 0 && filename.find(".json") != std::string::npos) {
            std::string filepath = configDir + "/" + filename;
            log_info("Loading extension config: %s", filepath.c_str());
            
            std::ifstream file(filepath);
            if (!file.is_open()) {
                log_warning("Cannot open extension config: %s", filepath.c_str());
                continue;
            }
            
            try {
                json config = json::parse(file);
                
                ExtensionConfig extConfig;
                extConfig.name = config["name"];
                extConfig.type = stringToExtensionType(config["type"]);
                extConfig.address = config["address"];
                extConfig.port = config["port"];
                extConfig.assigned_extension_point = config["assigned_extension_point"];
                
                // Load extension_thread_config
                if (config.contains("extension_thread_config")) {
                    auto threadConfig = config["extension_thread_config"];
                    
                    // Load general section
                    if (threadConfig.contains("general") && threadConfig["general"].contains("tcp_server_port")) {
                        extConfig.extension_thread_config.tcp_port = threadConfig["general"]["tcp_server_port"];
                    }
                    
                    // Load UDP endpoints
                    if (threadConfig.contains("udp_endpoints")) {
                        for (const auto& ep : threadConfig["udp_endpoints"]) {
                            UdpEndpointConfig udp;
                            udp.name = ep["name"];
                            udp.address = ep["address"];
                            udp.port = ep["port"];
                            std::string mode = ep["mode"];
                            udp.mode = (mode == "Server" || mode == "server") ? 
                                UdpEndpointConfig::Mode::Server : UdpEndpointConfig::Mode::Client;
                            extConfig.extension_thread_config.udp_configs.push_back(udp);
                        }
                    }
                    
                    // Load TCP endpoints
                    if (threadConfig.contains("tcp_endpoints")) {
                        for (const auto& ep : threadConfig["tcp_endpoints"]) {
                            TcpEndpointConfig tcp;
                            tcp.name = ep["name"];
                            tcp.address = ep["address"];
                            tcp.port = ep["port"];
                            if (ep.contains("retry_timeout")) {
                                tcp.retry_timeout = ep["retry_timeout"];
                            }
                            extConfig.extension_thread_config.tcp_configs.push_back(tcp);
                        }
                    }
                }
                
                createExtension(extConfig);
            } catch (const std::exception& e) {
                log_error("Failed to parse extension config %s: %s", filepath.c_str(), e.what());
            }
        }
    }
    
    closedir(dir);
    return true;
}

bool ExtensionManager::validateExtensionConfig(const ExtensionConfig& config) const {
    if (config.name.empty()) {
        log_error("Extension name cannot be empty");
        return false;
    }
    
    if (config.address.empty()) {
        log_error("Extension address cannot be empty");
        return false;
    }
    
    if (config.port == 0) {
        log_error("Extension port cannot be 0");
        return false;
    }
    
    return true;
}

ExtensionConfig ExtensionManager::parseExtensionConfigFromJson(const std::string& jsonStr) const {
    ExtensionConfig config;
    
    try {
        json j = json::parse(jsonStr);
        
        // Parse basic required fields
        if (!j.contains("name") || !j.contains("type") || !j.contains("address") || !j.contains("port")) {
            throw std::runtime_error("Missing required fields in extension configuration");
        }
        
        config.name = j["name"].get<std::string>();
        config.type = stringToExtensionType(j["type"].get<std::string>());
        config.address = j["address"].get<std::string>();
        config.port = j["port"].get<unsigned long>();
        
        // assigned_extension_point is optional (will be auto-assigned if not provided)
        if (j.contains("assigned_extension_point")) {
            config.assigned_extension_point = j["assigned_extension_point"].get<std::string>();
        }
        
        // Parse extension_thread_config if present (for loading saved configs)
        if (j.contains("extension_thread_config")) {
            auto threadConfig = j["extension_thread_config"];
            
            // Parse general section
            if (threadConfig.contains("general") && threadConfig["general"].contains("tcp_server_port")) {
                config.extension_thread_config.tcp_port = threadConfig["general"]["tcp_server_port"];
            }
            
            // Parse UDP endpoints
            if (threadConfig.contains("udp_endpoints") && threadConfig["udp_endpoints"].is_array()) {
                for (const auto& ep : threadConfig["udp_endpoints"]) {
                    UdpEndpointConfig udp;
                    udp.name = ep["name"].get<std::string>();
                    udp.address = ep["address"].get<std::string>();
                    udp.port = ep["port"].get<unsigned long>();
                    std::string mode = ep["mode"].get<std::string>();
                    udp.mode = (mode == "Server" || mode == "server") ? 
                        UdpEndpointConfig::Mode::Server : UdpEndpointConfig::Mode::Client;
                    config.extension_thread_config.udp_configs.push_back(udp);
                }
            }
            
            // Parse TCP endpoints
            if (threadConfig.contains("tcp_endpoints") && threadConfig["tcp_endpoints"].is_array()) {
                for (const auto& ep : threadConfig["tcp_endpoints"]) {
                    TcpEndpointConfig tcp;
                    tcp.name = ep["name"].get<std::string>();
                    tcp.address = ep["address"].get<std::string>();
                    tcp.port = ep["port"].get<unsigned long>();
                    if (ep.contains("retry_timeout")) {
                        tcp.retry_timeout = ep["retry_timeout"].get<int>();
                    }
                    config.extension_thread_config.tcp_configs.push_back(tcp);
                }
            }
        }
        
    } catch (const std::exception& e) {
        log_error("Failed to parse extension config JSON: %s", e.what());
        throw;
    }
    
    return config;
}

std::string ExtensionManager::extensionInfoToJson(const ExtensionInfo& info) const {
    json j;
    j["name"] = info.name;
    j["threadId"] = info.threadId;
    j["type"] = extensionTypeToString(info.config.type);
    j["address"] = info.config.address;
    j["port"] = info.config.port;
    j["assigned_extension_point"] = info.config.assigned_extension_point;
    j["isRunning"] = info.isRunning;
    
    return j.dump();
}

std::string ExtensionManager::allExtensionsToJson() const {
    std::lock_guard<std::mutex> lock(extensionsMutex_);
    
    json j = json::array();
    
    for (const auto& pair : extensions_) {
        json ext;
        ext["name"] = pair.second.name;
        ext["threadId"] = pair.second.threadId;
        ext["type"] = extensionTypeToString(pair.second.config.type);
        ext["address"] = pair.second.config.address;
        ext["port"] = pair.second.config.port;
        ext["assigned_extension_point"] = pair.second.config.assigned_extension_point;
        ext["isRunning"] = pair.second.isRunning;
        j.push_back(ext);
    }
    
    return j.dump();
}

} // namespace MavlinkExtensions
