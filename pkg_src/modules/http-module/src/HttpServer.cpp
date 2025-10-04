
/**
 * @file HttpServer.cpp
 * @brief HTTP server implementation using libmicrohttpd
 */

#include "HttpServer.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "../../nholmann/json.hpp"
#include "RpcController.h"
#include "../../src/mavlink-extensions/ExtensionManager.h"

namespace HttpModule {

/**
 * @brief Connection info structure for tracking request state
 */
struct ConnectionInfo {
    std::string postData;
    HttpMethod method;
    std::string url;
    std::map<std::string, std::string> headers;
};

/**
 * @brief Implementation details
 */
struct HttpServer::Impl {
    struct MHD_Daemon* daemon;
    std::map<std::string, std::map<HttpMethod, RouteHandler>> routes;
    std::mutex routesMutex;

    Impl() : daemon(nullptr) {}

    ~Impl() {
        if (daemon) {
            MHD_stop_daemon(daemon);
            daemon = nullptr;
        }
    }
};

HttpServer::HttpServer(const HttpServerConfig& config)
    : pImpl_(std::make_unique<Impl>())
    , config_(config)
    , running_(false)
    , rpcController_(nullptr)
    , extensionManager_(nullptr) {
}

void HttpServer::setRpcController(std::shared_ptr<void> rpcController) {
    rpcController_ = rpcController;
}

void HttpServer::setExtensionManager(std::shared_ptr<void> extensionManager) {
    extensionManager_ = extensionManager;
    
    // Add RPC endpoints
    if (rpcController_) {
        auto* controller = static_cast<RpcMechanisms::RpcController*>(rpcController_.get());
        
        // GET /api/threads - Get all thread status
        addRoute(HttpMethod::GET, "/api/threads", 
            [controller](const HttpRequest& req) {
                std::cout << "\n[HTTP] Client request: GET /api/threads" << std::endl;
                std::cout << "[HTTP] Action: Retrieve all thread status" << std::endl;
                
                HttpResponse resp;
                auto rpcResp = controller->getAllThreadStatus();
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = rpcResp.toJson();
                
                std::cout << "[HTTP] Response status: " << static_cast<int>(rpcResp.status) << std::endl;
                std::cout << "[HTTP] Current thread states:" << std::endl;
                for (const auto& pair : rpcResp.threadStates) {
                    std::cout << "[HTTP]   - " << pair.first << ": state=" << static_cast<int>(pair.second.state) 
                              << ", alive=" << (pair.second.isAlive ? "yes" : "no") << std::endl;
                }
                std::cout << std::endl;
                
                return resp;
            });
        
        // GET /api/threads/:name - Get specific thread status
        addRoute(HttpMethod::GET, "/api/threads/mainloop", 
            [controller](const HttpRequest& req) {
                std::cout << "\n[HTTP] Client request: GET /api/threads/mainloop" << std::endl;
                std::cout << "[HTTP] Action: Retrieve mainloop thread status" << std::endl;
                
                HttpResponse resp;
                auto rpcResp = controller->getThreadStatus("mainloop");
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = rpcResp.toJson();
                
                if (rpcResp.threadStates.find("mainloop") != rpcResp.threadStates.end()) {
                    const auto& info = rpcResp.threadStates["mainloop"];
                    std::cout << "[HTTP] Thread 'mainloop': state=" << static_cast<int>(info.state)
                              << ", alive=" << (info.isAlive ? "yes" : "no") 
                              << ", id=" << info.threadId << std::endl;
                }
                std::cout << std::endl;
                
                return resp;
            });
        
        addRoute(HttpMethod::GET, "/api/threads/http_server", 
            [controller](const HttpRequest& req) {
                std::cout << "\n[HTTP] Client request: GET /api/threads/http_server" << std::endl;
                std::cout << "[HTTP] Action: Retrieve http_server thread status" << std::endl;
                
                HttpResponse resp;
                auto rpcResp = controller->getThreadStatus("http_server");
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = rpcResp.toJson();
                
                if (rpcResp.threadStates.find("http_server") != rpcResp.threadStates.end()) {
                    const auto& info = rpcResp.threadStates["http_server"];
                    std::cout << "[HTTP] Thread 'http_server': state=" << static_cast<int>(info.state)
                              << ", alive=" << (info.isAlive ? "yes" : "no")
                              << ", id=" << info.threadId << std::endl;
                }
                std::cout << std::endl;
                
                return resp;
            });
        
        // POST /api/threads/:name/start - Start thread
        addRoute(HttpMethod::POST, "/api/threads/mainloop/start", 
            [controller, this](const HttpRequest& req) {
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: POST /api/threads/mainloop/start" << std::endl;
                std::cout << "[HTTP] Action: START mainloop thread AND load/start all extensions" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                
                // First, start the mainloop thread (this initializes the global config)
                auto mainloopResp = controller->startThread(RpcMechanisms::ThreadTarget::MAINLOOP);
                std::cout << "[HTTP] Mainloop start result: " << mainloopResp.message << std::endl;
                
                if (mainloopResp.status != RpcMechanisms::OperationStatus::SUCCESS) {
                    // If mainloop failed to start, return error
                    resp.statusCode = 500;
                    resp.contentType = "application/json";
                    resp.content = mainloopResp.toJson();
                    std::cout << "[HTTP] Mainloop start failed, aborting extension loading" << std::endl;
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                // Wait a moment for mainloop to initialize
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                
                // Now load and start extension configurations
                if (extensionManager_) {
                    auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                    auto allExtensions = extMgr->getAllExtensions();
                    
                    // Only load configs if no extensions are loaded yet
                    if (allExtensions.empty()) {
                        std::cout << "[HTTP] Loading extension configurations..." << std::endl;
                        
                        std::string extensionConfDir = "config";
                        extMgr->loadExtensionConfigs(extensionConfDir);
                        
                        // Refresh the extensions list
                        allExtensions = extMgr->getAllExtensions();
                        std::cout << "[HTTP] Loaded and started " << allExtensions.size() << " extensions from config" << std::endl;
                    } else {
                        // Extensions already exist, ensure they're started
                        std::cout << "[HTTP] Extensions already loaded (" << allExtensions.size() << " found)" << std::endl;
                        std::cout << "[HTTP] Ensuring all extension threads are running..." << std::endl;
                        
                        for (const auto& ext : allExtensions) {
                            if (!ext.isRunning) {
                                std::cout << "[HTTP] Starting extension: " << ext.name << std::endl;
                                bool started = extMgr->startExtension(ext.name);
                                if (started) {
                                    std::cout << "[HTTP] Extension '" << ext.name << "' started successfully" << std::endl;
                                } else {
                                    std::cout << "[HTTP] Failed to start extension '" << ext.name << "'" << std::endl;
                                }
                            } else {
                                std::cout << "[HTTP] Extension '" << ext.name << "' already running" << std::endl;
                            }
                        }
                    }
                }
                
                // Return combined status
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = mainloopResp.toJson();
                
                std::cout << "[HTTP] Overall operation status: " << static_cast<int>(mainloopResp.status) << std::endl;
                
                if (mainloopResp.threadStates.find("mainloop") != mainloopResp.threadStates.end()) {
                    const auto& info = mainloopResp.threadStates["mainloop"];
                    std::cout << "[HTTP] NEW STATE for 'mainloop':" << std::endl;
                    std::cout << "[HTTP]   - Thread ID: " << info.threadId << std::endl;
                    std::cout << "[HTTP]   - State: " << static_cast<int>(info.state) << std::endl;
                    std::cout << "[HTTP]   - Alive: " << (info.isAlive ? "YES" : "NO") << std::endl;
                    std::cout << "[HTTP]   - Attachment: " << info.attachmentId << std::endl;
                }
                std::cout << "[HTTP] ========================================\n" << std::endl;
                
                return resp;
            });
        
        // POST /api/threads/:name/stop - Stop thread
        addRoute(HttpMethod::POST, "/api/threads/mainloop/stop", 
            [controller, this](const HttpRequest& req) {
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: POST /api/threads/mainloop/stop" << std::endl;
                std::cout << "[HTTP] Action: STOP mainloop thread AND all extensions" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                
                // First stop all extension threads
                if (extensionManager_) {
                    auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                    auto allExtensions = extMgr->getAllExtensions();
                    
                    std::cout << "[HTTP] Stopping " << allExtensions.size() << " extension threads..." << std::endl;
                    
                    for (const auto& ext : allExtensions) {
                        if (ext.isRunning) {
                            std::cout << "[HTTP] Stopping extension: " << ext.name << std::endl;
                            bool stopped = extMgr->stopExtension(ext.name);
                            if (stopped) {
                                std::cout << "[HTTP] Extension '" << ext.name << "' stopped successfully" << std::endl;
                            } else {
                                std::cout << "[HTTP] Failed to stop extension '" << ext.name << "'" << std::endl;
                            }
                        } else {
                            std::cout << "[HTTP] Extension '" << ext.name << "' already stopped" << std::endl;
                        }
                    }
                }
                
                // Then stop the mainloop thread
                auto mainloopResp = controller->stopThread(RpcMechanisms::ThreadTarget::MAINLOOP);
                std::cout << "[HTTP] Mainloop stop result: " << mainloopResp.message << std::endl;
                
                // Return combined status
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = mainloopResp.toJson();
                
                std::cout << "[HTTP] Overall operation status: " << static_cast<int>(mainloopResp.status) << std::endl;
                
                if (mainloopResp.threadStates.find("mainloop") != mainloopResp.threadStates.end()) {
                    const auto& info = mainloopResp.threadStates["mainloop"];
                    std::cout << "[HTTP] NEW STATE for 'mainloop':" << std::endl;
                    std::cout << "[HTTP]   - Thread ID: " << info.threadId << std::endl;
                    std::cout << "[HTTP]   - State: " << static_cast<int>(info.state) << std::endl;
                    std::cout << "[HTTP]   - Alive: " << (info.isAlive ? "YES" : "NO") << std::endl;
                    std::cout << "[HTTP]   - Attachment: " << info.attachmentId << std::endl;
                }
                std::cout << "[HTTP] ========================================\n" << std::endl;
                
                return resp;
            });
        
        // POST /api/threads/:name/pause - Pause thread
        addRoute(HttpMethod::POST, "/api/threads/mainloop/pause", 
            [controller](const HttpRequest& req) {
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: POST /api/threads/mainloop/pause" << std::endl;
                std::cout << "[HTTP] Action: PAUSE mainloop thread" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                auto rpcResp = controller->pauseThread(RpcMechanisms::ThreadTarget::MAINLOOP);
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = rpcResp.toJson();
                
                std::cout << "[HTTP] Operation result: " << rpcResp.message << std::endl;
                std::cout << "[HTTP] Operation status: " << static_cast<int>(rpcResp.status) << std::endl;
                
                if (rpcResp.threadStates.find("mainloop") != rpcResp.threadStates.end()) {
                    const auto& info = rpcResp.threadStates["mainloop"];
                    std::cout << "[HTTP] NEW STATE for 'mainloop':" << std::endl;
                    std::cout << "[HTTP]   - Thread ID: " << info.threadId << std::endl;
                    std::cout << "[HTTP]   - State: " << static_cast<int>(info.state) << std::endl;
                    std::cout << "[HTTP]   - Alive: " << (info.isAlive ? "YES" : "NO") << std::endl;
                    std::cout << "[HTTP]   - Attachment: " << info.attachmentId << std::endl;
                }
                std::cout << "[HTTP] ========================================\n" << std::endl;
                
                return resp;
            });
        
        // POST /api/threads/:name/resume - Resume thread
        addRoute(HttpMethod::POST, "/api/threads/mainloop/resume", 
            [controller](const HttpRequest& req) {
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: POST /api/threads/mainloop/resume" << std::endl;
                std::cout << "[HTTP] Action: RESUME mainloop thread" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                auto rpcResp = controller->resumeThread(RpcMechanisms::ThreadTarget::MAINLOOP);
                resp.statusCode = 200;
                resp.contentType = "application/json";
                resp.content = rpcResp.toJson();
                
                std::cout << "[HTTP] Operation result: " << rpcResp.message << std::endl;
                std::cout << "[HTTP] Operation status: " << static_cast<int>(rpcResp.status) << std::endl;
                
                if (rpcResp.threadStates.find("mainloop") != rpcResp.threadStates.end()) {
                    const auto& info = rpcResp.threadStates["mainloop"];
                    std::cout << "[HTTP] NEW STATE for 'mainloop':" << std::endl;
                    std::cout << "[HTTP]   - Thread ID: " << info.threadId << std::endl;
                    std::cout << "[HTTP]   - State: " << static_cast<int>(info.state) << std::endl;
                    std::cout << "[HTTP]   - Alive: " << (info.isAlive ? "YES" : "NO") << std::endl;
                    std::cout << "[HTTP]   - Attachment: " << info.attachmentId << std::endl;
                }
                std::cout << "[HTTP] ========================================\n" << std::endl;
                
                return resp;
            });
        
        // Extension Management Endpoints
        
        // Helper function to extract extension name from URL
        auto extractExtensionName = [](const std::string& url, const std::string& prefix) -> std::string {
            if (url.find(prefix) == 0) {
                return url.substr(prefix.length());
            }
            return "";
        };
        
        // POST /api/extensions/add - Add new extension
        addRoute(HttpMethod::POST, "/api/extensions/add",
            [this](const HttpRequest& req) {
                std::cout << "\n[HTTP] Client request: POST /api/extensions/add" << std::endl;
                std::cout << "[HTTP] Action: Add new extension" << std::endl;
                
                HttpResponse resp;
                resp.contentType = "application/json";
                
                if (!extensionManager_) {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Extension manager not available\"}";
                    std::cout << "[HTTP] Error: Extension manager not set" << std::endl;
                    return resp;
                }
                
                auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                
                try {
                    auto extConfig = extMgr->parseExtensionConfigFromJson(req.body);
                    std::string result = extMgr->createExtension(extConfig);
                    
                    if (result == "Success") {
                        resp.statusCode = 200;
                        auto info = extMgr->getExtensionInfo(extConfig.name);
                        resp.content = extMgr->extensionInfoToJson(info);
                        std::cout << "[HTTP] Extension '" << extConfig.name << "' created successfully" << std::endl;
                    } else {
                        resp.statusCode = 400;
                        resp.content = "{\"error\": \"" + result + "\"}";
                        std::cout << "[HTTP] Failed to create extension: " << result << std::endl;
                    }
                } catch (const std::exception& e) {
                    resp.statusCode = 400;
                    resp.content = "{\"error\": \"" + std::string(e.what()) + "\"}";
                    std::cout << "[HTTP] Exception: " << e.what() << std::endl;
                }
                
                return resp;
            });
        
        // GET /api/extensions/status - Get all extensions status (must be registered before specific status route)
        addRoute(HttpMethod::GET, "/api/extensions/status",
            [this](const HttpRequest& req) {
                std::cout << "\n[HTTP] Client request: GET /api/extensions/status" << std::endl;
                std::cout << "[HTTP] Action: Get all extensions status" << std::endl;
                
                HttpResponse resp;
                resp.contentType = "application/json";
                
                if (!extensionManager_) {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Extension manager not available\"}";
                    return resp;
                }
                
                auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                resp.statusCode = 200;
                resp.content = extMgr->allExtensionsToJson();
                
                std::cout << "[HTTP] Returned status for all extensions" << std::endl;
                return resp;
            });
        
        // Dynamic route handler for GET /api/extensions/status/:name
        addRoute(HttpMethod::GET, "/api/extensions/status/",
            [this, extractExtensionName](const HttpRequest& req) {
                std::string name = extractExtensionName(req.url, "/api/extensions/status/");
                
                std::cout << "\n[HTTP] Client request: GET /api/extensions/status/" << name << std::endl;
                std::cout << "[HTTP] Action: Get extension status" << std::endl;
                
                HttpResponse resp;
                resp.contentType = "application/json";
                
                if (name.empty()) {
                    resp.statusCode = 400;
                    resp.content = "{\"error\": \"Extension name is required\"}";
                    return resp;
                }
                
                if (!extensionManager_) {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Extension manager not available\"}";
                    return resp;
                }
                
                auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                
                if (!extMgr->extensionExists(name)) {
                    resp.statusCode = 404;
                    resp.content = "{\"error\": \"Extension not found\"}";
                    std::cout << "[HTTP] Error: Extension '" << name << "' not found" << std::endl;
                    return resp;
                }
                
                auto info = extMgr->getExtensionInfo(name);
                resp.statusCode = 200;
                resp.content = extMgr->extensionInfoToJson(info);
                
                std::cout << "[HTTP] Extension '" << name << "' status:" << std::endl;
                std::cout << "[HTTP]   - Thread ID: " << info.threadId << std::endl;
                std::cout << "[HTTP]   - Running: " << (info.isRunning ? "YES" : "NO") << std::endl;
                std::cout << "[HTTP]   - Type: " << MavlinkExtensions::extensionTypeToString(info.config.type) << std::endl;
                std::cout << "[HTTP]   - Address: " << info.config.address << ":" << info.config.port << std::endl;
                std::cout << "[HTTP]   - Extension Point: " << info.config.assigned_extension_point << std::endl;
                
                return resp;
            });
        
        // Dynamic route handler for DELETE /api/extensions/:name
        addRoute(HttpMethod::DELETE, "/api/extensions/",
            [this, extractExtensionName](const HttpRequest& req) {
                std::string name = extractExtensionName(req.url, "/api/extensions/");
                
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: DELETE /api/extensions/" << name << std::endl;
                std::cout << "[HTTP] Action: DELETE extension (stop thread + remove config)" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                resp.contentType = "application/json";
                
                if (name.empty()) {
                    resp.statusCode = 400;
                    resp.content = "{\"error\": \"Extension name is required\"}";
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                if (!extensionManager_) {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Extension manager not available\"}";
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                
                if (!extMgr->extensionExists(name)) {
                    resp.statusCode = 404;
                    resp.content = "{\"error\": \"Extension not found\"}";
                    std::cout << "[HTTP] Error: Extension '" << name << "' not found" << std::endl;
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                // Get info before deletion
                auto info = extMgr->getExtensionInfo(name);
                std::cout << "[HTTP] Extension '" << name << "' info:" << std::endl;
                std::cout << "[HTTP]   - Thread ID: " << info.threadId << std::endl;
                std::cout << "[HTTP]   - Running: " << (info.isRunning ? "YES" : "NO") << std::endl;
                
                // Delete extension (stops thread + removes config file)
                bool success = extMgr->deleteExtension(name);
                
                if (success) {
                    resp.statusCode = 200;
                    resp.content = "{\"message\": \"Extension deleted successfully\"}";
                    std::cout << "[HTTP] SUCCESS: Extension '" << name << "' deleted" << std::endl;
                    std::cout << "[HTTP]   - Thread stopped and joined" << std::endl;
                    std::cout << "[HTTP]   - Config file removed" << std::endl;
                } else {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Failed to delete extension\"}";
                    std::cout << "[HTTP] ERROR: Failed to delete extension '" << name << "'" << std::endl;
                }
                
                std::cout << "[HTTP] ========================================\n" << std::endl;
                return resp;
            });
        
        // Dynamic route handler for POST /api/extensions/stop/:name
        addRoute(HttpMethod::POST, "/api/extensions/stop/",
            [this, extractExtensionName](const HttpRequest& req) {
                std::string name = extractExtensionName(req.url, "/api/extensions/stop/");
                
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: POST /api/extensions/stop/" << name << std::endl;
                std::cout << "[HTTP] Action: STOP extension thread (keep config)" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                resp.contentType = "application/json";
                
                if (name.empty()) {
                    resp.statusCode = 400;
                    resp.content = "{\"error\": \"Extension name is required\"}";
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                if (!extensionManager_) {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Extension manager not available\"}";
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                
                if (!extMgr->extensionExists(name)) {
                    resp.statusCode = 404;
                    resp.content = "{\"error\": \"Extension not found\"}";
                    std::cout << "[HTTP] Error: Extension '" << name << "' not found" << std::endl;
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                // Get info before stopping
                auto infoBefore = extMgr->getExtensionInfo(name);
                std::cout << "[HTTP] Extension '" << name << "' status BEFORE stop:" << std::endl;
                std::cout << "[HTTP]   - Thread ID: " << infoBefore.threadId << std::endl;
                std::cout << "[HTTP]   - Running: " << (infoBefore.isRunning ? "YES" : "NO") << std::endl;
                
                // Stop extension thread only (config remains)
                bool success = extMgr->stopExtension(name);
                
                if (success) {
                    auto infoAfter = extMgr->getExtensionInfo(name);
                    resp.statusCode = 200;
                    resp.content = extMgr->extensionInfoToJson(infoAfter);
                    
                    std::cout << "[HTTP] SUCCESS: Extension '" << name << "' thread stopped" << std::endl;
                    std::cout << "[HTTP] Extension '" << name << "' status AFTER stop:" << std::endl;
                    std::cout << "[HTTP]   - Thread ID: " << infoAfter.threadId << std::endl;
                    std::cout << "[HTTP]   - Running: " << (infoAfter.isRunning ? "YES" : "NO") << std::endl;
                    std::cout << "[HTTP]   - Config file: PRESERVED" << std::endl;
                } else {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Failed to stop extension\"}";
                    std::cout << "[HTTP] ERROR: Failed to stop extension '" << name << "'" << std::endl;
                }
                
                std::cout << "[HTTP] ========================================\n" << std::endl;
                return resp;
            });
        
        // Dynamic route handler for POST /api/extensions/start/:name
        addRoute(HttpMethod::POST, "/api/extensions/start/",
            [this, extractExtensionName](const HttpRequest& req) {
                std::string name = extractExtensionName(req.url, "/api/extensions/start/");
                
                std::cout << "\n[HTTP] ========================================" << std::endl;
                std::cout << "[HTTP] Client request: POST /api/extensions/start/" << name << std::endl;
                std::cout << "[HTTP] Action: START extension thread" << std::endl;
                std::cout << "[HTTP] ========================================" << std::endl;
                
                HttpResponse resp;
                resp.contentType = "application/json";
                
                if (name.empty()) {
                    resp.statusCode = 400;
                    resp.content = "{\"error\": \"Extension name is required\"}";
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                if (!extensionManager_) {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Extension manager not available\"}";
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                auto* extMgr = static_cast<MavlinkExtensions::ExtensionManager*>(extensionManager_.get());
                
                if (!extMgr->extensionExists(name)) {
                    resp.statusCode = 404;
                    resp.content = "{\"error\": \"Extension not found\"}";
                    std::cout << "[HTTP] Error: Extension '" << name << "' not found" << std::endl;
                    std::cout << "[HTTP] ========================================\n" << std::endl;
                    return resp;
                }
                
                // Get info before starting
                auto infoBefore = extMgr->getExtensionInfo(name);
                std::cout << "[HTTP] Extension '" << name << "' status BEFORE start:" << std::endl;
                std::cout << "[HTTP]   - Thread ID: " << infoBefore.threadId << std::endl;
                std::cout << "[HTTP]   - Running: " << (infoBefore.isRunning ? "YES" : "NO") << std::endl;
                
                // Start extension thread (using existing config)
                bool success = extMgr->startExtension(name);
                
                if (success) {
                    auto infoAfter = extMgr->getExtensionInfo(name);
                    resp.statusCode = 200;
                    resp.content = extMgr->extensionInfoToJson(infoAfter);
                    
                    std::cout << "[HTTP] SUCCESS: Extension '" << name << "' thread started" << std::endl;
                    std::cout << "[HTTP] Extension '" << name << "' status AFTER start:" << std::endl;
                    std::cout << "[HTTP]   - Thread ID: " << infoAfter.threadId << std::endl;
                    std::cout << "[HTTP]   - Running: " << (infoAfter.isRunning ? "YES" : "NO") << std::endl;
                } else {
                    resp.statusCode = 500;
                    resp.content = "{\"error\": \"Failed to start extension\"}";
                    std::cout << "[HTTP] ERROR: Failed to start extension '" << name << "'" << std::endl;
                }
                
                std::cout << "[HTTP] ========================================\n" << std::endl;
                return resp;
            });
    }
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "[HTTP] Attempting to start HTTP server..." << std::endl;
    std::cout << "[HTTP] Configuration:" << std::endl;
    std::cout << "[HTTP]   Address: " << config_.address << std::endl;
    std::cout << "[HTTP]   Port: " << config_.port << std::endl;
    std::cout << "[HTTP]   Thread Pool Size: " << config_.threadPoolSize << std::endl;
    std::cout << "[HTTP]   Connection Limit: " << config_.connectionLimit << std::endl;
    std::cout << "[HTTP]   Connection Timeout: " << config_.connectionTimeout << std::endl;
    std::cout << "[HTTP]   TLS Enabled: " << (config_.enableTLS ? "yes" : "no") << std::endl;
    
    if (running_) {
        std::cerr << "[HTTP] ERROR: Server is already running" << std::endl;
        throw HttpServerException("Server is already running");
    }

    unsigned int flags = MHD_USE_INTERNAL_POLLING_THREAD;
    std::cout << "[HTTP] Using MHD_USE_INTERNAL_POLLING_THREAD mode" << std::endl;
    
    // Set up IPv4 socket address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    
    if (config_.address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, config_.address.c_str(), &addr.sin_addr);
    }
    
    std::cout << "[HTTP] Calling MHD_start_daemon..." << std::endl;
    pImpl_->daemon = MHD_start_daemon(
        flags,
        config_.port,
        nullptr, nullptr,
        &HttpServer::accessHandlerCallback, this,
        MHD_OPTION_NOTIFY_COMPLETED, &HttpServer::requestCompletedCallback, nullptr,
        MHD_OPTION_CONNECTION_LIMIT, config_.connectionLimit,
        MHD_OPTION_CONNECTION_TIMEOUT, config_.connectionTimeout,
        MHD_OPTION_SOCK_ADDR, (struct sockaddr*)&addr,
        MHD_OPTION_END
    );

    if (!pImpl_->daemon) {
        std::cerr << "[HTTP] ERROR: MHD_start_daemon returned NULL" << std::endl;
        std::cerr << "[HTTP] ERROR: Failed to start HTTP server on port " << config_.port << std::endl;
        std::cerr << "[HTTP] ERROR: Possible causes:" << std::endl;
        std::cerr << "[HTTP] ERROR:   - Port " << config_.port << " is already in use" << std::endl;
        std::cerr << "[HTTP] ERROR:   - Insufficient permissions to bind to port" << std::endl;
        std::cerr << "[HTTP] ERROR:   - libmicrohttpd configuration error" << std::endl;
        std::cerr << "[HTTP] ERROR: errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
        throw HttpServerException("Failed to start HTTP server on port " + std::to_string(config_.port));
    }

    running_ = true;
    std::cout << "[HTTP] SUCCESS: HTTP server started on " << config_.address << ":" << config_.port << std::endl;
    std::cout << "[HTTP] Server is now accepting connections" << std::endl;
}

void HttpServer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "[HTTP] Attempting to stop HTTP server..." << std::endl;
    
    if (running_ && pImpl_->daemon) {
        std::cout << "[HTTP] Stopping daemon..." << std::endl;
        MHD_stop_daemon(pImpl_->daemon);
        pImpl_->daemon = nullptr;
        running_ = false;
        std::cout << "[HTTP] SUCCESS: HTTP server stopped" << std::endl;
    } else {
        std::cout << "[HTTP] Server was not running (running_=" << running_ 
                  << ", daemon=" << (pImpl_->daemon ? "valid" : "null") << ")" << std::endl;
    }
}

bool HttpServer::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void HttpServer::addRoute(HttpMethod method, const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(pImpl_->routesMutex);
    pImpl_->routes[path][method] = handler;
}

void HttpServer::removeRoute(HttpMethod method, const std::string& path) {
    std::lock_guard<std::mutex> lock(pImpl_->routesMutex);
    auto it = pImpl_->routes.find(path);
    if (it != pImpl_->routes.end()) {
        it->second.erase(method);
        if (it->second.empty()) {
            pImpl_->routes.erase(it);
        }
    }
}

HttpMethod HttpServer::stringToMethod(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "HEAD") return HttpMethod::HEAD;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    if (method == "PATCH") return HttpMethod::PATCH;
    return HttpMethod::GET;
}

std::string HttpServer::methodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::PATCH: return "PATCH";
        default: return "GET";
    }
}

HttpResponse HttpServer::handleRequest(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(pImpl_->routesMutex);
    
    // First try exact match
    auto routeIt = pImpl_->routes.find(request.url);
    if (routeIt != pImpl_->routes.end()) {
        auto handlerIt = routeIt->second.find(request.method);
        if (handlerIt != routeIt->second.end()) {
            return handlerIt->second(request);
        }
    }
    
    // Then try prefix match for dynamic routes (ending with /)
    for (const auto& route : pImpl_->routes) {
        if (route.first.back() == '/' && request.url.find(route.first) == 0) {
            auto handlerIt = route.second.find(request.method);
            if (handlerIt != route.second.end()) {
                return handlerIt->second(request);
            }
        }
    }

    // Default 404 response
    HttpResponse response;
    response.statusCode = 404;
    response.content = "Not Found";
    response.contentType = "text/plain";
    return response;
}

MHD_Result HttpServer::accessHandlerCallback(void* cls, struct MHD_Connection* connection,
                                             const char* url, const char* method,
                                             const char* version, const char* upload_data,
                                             size_t* upload_data_size, void** con_cls) {
    HttpServer* server = static_cast<HttpServer*>(cls);

    if (*con_cls == nullptr) {
        // First call - create connection info
        std::cout << "[HTTP] New client connection: " << method << " " << url << std::endl;
        
        ConnectionInfo* conInfo = new ConnectionInfo();
        conInfo->method = stringToMethod(method);
        conInfo->url = url;
        *con_cls = conInfo;

        if (conInfo->method == HttpMethod::POST || conInfo->method == HttpMethod::PUT) {
            return MHD_YES;
        }
    }

    ConnectionInfo* conInfo = static_cast<ConnectionInfo*>(*con_cls);

    // Handle POST/PUT data
    if (*upload_data_size != 0) {
        conInfo->postData.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }

    // Build request
    HttpRequest request;
    request.method = conInfo->method;
    request.url = url;
    request.version = version;
    request.body = conInfo->postData;

    // Parse query parameters
    std::string urlStr(url);
    size_t queryPos = urlStr.find('?');
    if (queryPos != std::string::npos) {
        request.url = urlStr.substr(0, queryPos);
        std::string query = urlStr.substr(queryPos + 1);
        // Simple query parsing
        size_t pos = 0;
        while (pos < query.length()) {
            size_t eqPos = query.find('=', pos);
            size_t ampPos = query.find('&', pos);
            if (eqPos != std::string::npos) {
                std::string key = query.substr(pos, eqPos - pos);
                std::string value;
                if (ampPos != std::string::npos) {
                    value = query.substr(eqPos + 1, ampPos - eqPos - 1);
                    pos = ampPos + 1;
                } else {
                    value = query.substr(eqPos + 1);
                    pos = query.length();
                }
                request.queryParams[key] = value;
            } else {
                break;
            }
        }
    }

    // Handle request and create response
    HttpResponse response = server->handleRequest(request);

    // Create MHD response
    struct MHD_Response* mhdResponse = MHD_create_response_from_buffer(
        response.content.length(),
        (void*)response.content.c_str(),
        MHD_RESPMEM_MUST_COPY
    );

    // Add headers
    MHD_add_response_header(mhdResponse, "Content-Type", response.contentType.c_str());
    for (const auto& header : response.headers) {
        MHD_add_response_header(mhdResponse, header.first.c_str(), header.second.c_str());
    }

    MHD_Result ret = static_cast<MHD_Result>(MHD_queue_response(connection, response.statusCode, mhdResponse));
    MHD_destroy_response(mhdResponse);

    return ret;
}

void HttpServer::requestCompletedCallback(void* cls, struct MHD_Connection* connection,
                                          void** con_cls, enum MHD_RequestTerminationCode toe) {
    if (*con_cls != nullptr) {
        ConnectionInfo* conInfo = static_cast<ConnectionInfo*>(*con_cls);
        delete conInfo;
        *con_cls = nullptr;
    }
}

void HttpServer::loadConfig(const std::string& filename) {
    config_ = parseHttpConfig(filename);
}

HttpServerConfig parseHttpConfig(const std::string& filename) {
    HttpServerConfig config;

    std::cout << "[HTTP] Loading configuration from: " << filename << std::endl;
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[HTTP] ERROR: Failed to open config file: " << filename << std::endl;
        throw HttpServerException("Failed to open config file: " + filename);
    }

    try {
        nlohmann::json j;
        file >> j;
        std::cout << "[HTTP] Configuration file parsed successfully" << std::endl;

        if (j.contains("address")) {
            config.address = j["address"].get<std::string>();
        }
        if (j.contains("port")) {
            config.port = j["port"].get<uint16_t>();
        }
        if (j.contains("threadPoolSize") || j.contains("thread_pool_size")) {
            config.threadPoolSize = j.contains("threadPoolSize") 
                ? j["threadPoolSize"].get<unsigned int>()
                : j["thread_pool_size"].get<unsigned int>();
        }
        if (j.contains("connectionLimit") || j.contains("connection_limit")) {
            config.connectionLimit = j.contains("connectionLimit")
                ? j["connectionLimit"].get<size_t>()
                : j["connection_limit"].get<size_t>();
        }
        if (j.contains("connectionTimeout") || j.contains("connection_timeout")) {
            config.connectionTimeout = j.contains("connectionTimeout")
                ? j["connectionTimeout"].get<size_t>()
                : j["connection_timeout"].get<size_t>();
        }
        if (j.contains("enableTLS") || j.contains("enable_tls")) {
            config.enableTLS = j.contains("enableTLS")
                ? j["enableTLS"].get<bool>()
                : j["enable_tls"].get<bool>();
        }
        if (j.contains("tlsCertFile") || j.contains("tls_cert_file")) {
            config.tlsCertFile = j.contains("tlsCertFile")
                ? j["tlsCertFile"].get<std::string>()
                : j["tls_cert_file"].get<std::string>();
        }
        if (j.contains("tlsKeyFile") || j.contains("tls_key_file")) {
            config.tlsKeyFile = j.contains("tlsKeyFile")
                ? j["tlsKeyFile"].get<std::string>()
                : j["tls_key_file"].get<std::string>();
        }

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[HTTP] ERROR: JSON parsing error: " << e.what() << std::endl;
        throw HttpServerException("JSON parsing error: " + std::string(e.what()));
    }

    std::cout << "[HTTP] Configuration loaded successfully:" << std::endl;
    std::cout << "[HTTP]   Address: " << config.address << std::endl;
    std::cout << "[HTTP]   Port: " << config.port << std::endl;
    std::cout << "[HTTP]   Thread Pool Size: " << config.threadPoolSize << std::endl;
    
    return config;
}

} // namespace HttpModule
