
#pragma once

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include "ThreadManager.hpp"
#include "../mainloop.h"
#include "../config.h"

namespace MavlinkExtensions {

enum class ExtensionType {
    INTERNAL,
    TCP,
    UDP
};

struct ExtensionConfig {
    std::string name;
    ExtensionType type;
    std::string address;
    unsigned long port;
    std::string assigned_extension_point;
    
    // Extension thread configuration with full router config
    Configuration extension_thread_config;
};

struct ExtensionInfo {
    std::string name;
    unsigned int threadId;
    ExtensionConfig config;
    bool isRunning;
    // Store mainloop instance pointer for proper shutdown
    // Protected by extensionsMutex_ in ExtensionManager
    Mainloop* mainloopInstance;
};

class ExtensionManager {
public:
    explicit ExtensionManager(ThreadMgr::ThreadManager& threadManager);
    ~ExtensionManager();

    // Extension lifecycle management
    std::string createExtension(const ExtensionConfig& config);
    bool deleteExtension(const std::string& name);
    bool stopExtension(const std::string& name);
    bool startExtension(const std::string& name);
    
    // Query operations
    ExtensionInfo getExtensionInfo(const std::string& name) const;
    std::vector<ExtensionInfo> getAllExtensions() const;
    bool extensionExists(const std::string& name) const;
    
    // Configuration persistence
    bool saveExtensionConfig(const std::string& name);
    bool loadExtensionConfigs(const std::string& configDir);
    
    // Extension point assignment
    std::string assignAvailableExtensionPoint(const Configuration& globalConfig, ExtensionType type = ExtensionType::UDP);
    
    // Configuration directory management
    void setExtensionConfDir(const std::string& confDir);
    void setGlobalConfig(const Configuration* config);
    
    // JSON operations
    ExtensionConfig parseExtensionConfigFromJson(const std::string& jsonStr) const;
    std::string extensionInfoToJson(const ExtensionInfo& info) const;
    std::string allExtensionsToJson() const;

private:
    ThreadMgr::ThreadManager& threadManager_;
    std::map<std::string, ExtensionInfo> extensions_;
    mutable std::mutex extensionsMutex_;
    std::string extensionConfDir_;
    const Configuration* globalConfig_;
    
    std::string getConfigFilePath(const std::string& name) const;
    bool validateExtensionConfig(const ExtensionConfig& config) const;
    unsigned int launchExtensionThread(const ExtensionConfig& config);
};

// Helper functions
ExtensionType stringToExtensionType(const std::string& typeStr);
std::string extensionTypeToString(ExtensionType type);

} // namespace MavlinkExtensions
