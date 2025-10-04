
/**
 * @file HttpServer.hpp
 * @brief C++ wrapper for libmicrohttpd HTTP server
 */

#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <microhttpd.h>

namespace HttpModule {

/**
 * @brief HTTP request method enumeration
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH
};

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int statusCode;
    std::string content;
    std::string contentType;
    std::map<std::string, std::string> headers;

    HttpResponse() : statusCode(200), contentType("text/plain") {}
};

/**
 * @brief HTTP request structure
 */
struct HttpRequest {
    HttpMethod method;
    std::string url;
    std::string version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> queryParams;
    std::string body;
    std::map<std::string, std::string> params;
};

/**
 * @brief HTTP server configuration
 */
struct HttpServerConfig {
    std::string address;
    uint16_t port;
    unsigned int threadPoolSize;
    size_t connectionLimit;
    size_t connectionTimeout;
    bool enableTLS;
    std::string tlsCertFile;
    std::string tlsKeyFile;

    HttpServerConfig() 
        : address("0.0.0.0")
        , port(8080)
        , threadPoolSize(4)
        , connectionLimit(100)
        , connectionTimeout(30)
        , enableTLS(false) {}
};

/**
 * @brief Route handler function type
 */
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @brief Exception for HTTP server errors
 */
class HttpServerException : public std::runtime_error {
public:
    explicit HttpServerException(const std::string& message)
        : std::runtime_error("HttpServer: " + message) {}
};

/**
 * @brief HTTP Server class
 */
class HttpServer {
public:
    /**
     * @brief Constructor
     * @param config Server configuration
     */
    explicit HttpServer(const HttpServerConfig& config);

    /**
     * @brief Destructor
     */
    ~HttpServer();

    // Disable copy
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /**
     * @brief Start the HTTP server
     */
    void start();

    /**
     * @brief Stop the HTTP server
     */
    void stop();

    /**
     * @brief Check if server is running
     * @return True if running
     */
    bool isRunning() const;

    /**
     * @brief Register a route handler
     * @param method HTTP method
     * @param path URL path pattern
     * @param handler Route handler function
     */
    void addRoute(HttpMethod method, const std::string& path, RouteHandler handler);

    /**
     * @brief Remove a route handler
     * @param method HTTP method
     * @param path URL path pattern
     */
    void removeRoute(HttpMethod method, const std::string& path);

    /**
     * @brief Load configuration from JSON file
     * @param filename JSON configuration file path
     */
    void loadConfig(const std::string& filename);

    /**
     * @brief Get current configuration
     * @return Server configuration
     */
    const HttpServerConfig& getConfig() const { return config_; }

    /**
     * @brief Set RPC controller for thread management
     * @param rpcController Shared pointer to RPC controller
     */
    void setRpcController(std::shared_ptr<void> rpcController);

    /**
     * @brief Set extension manager for extension operations
     * @param extensionManager Shared pointer to extension manager
     */
    void setExtensionManager(std::shared_ptr<void> extensionManager);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
    HttpServerConfig config_;
    bool running_;
    mutable std::mutex mutex_;
    std::shared_ptr<void> rpcController_;
    std::shared_ptr<void> extensionManager_;

    // MHD callback functions
    static MHD_Result accessHandlerCallback(void* cls, struct MHD_Connection* connection,
                                            const char* url, const char* method,
                                            const char* version, const char* upload_data,
                                            size_t* upload_data_size, void** con_cls);
    
    static void requestCompletedCallback(void* cls, struct MHD_Connection* connection,
                                         void** con_cls, enum MHD_RequestTerminationCode toe);

    HttpResponse handleRequest(const HttpRequest& request);
    static HttpMethod stringToMethod(const std::string& method);
    static std::string methodToString(HttpMethod method);
};

/**
 * @brief Parse JSON configuration file
 * @param filename JSON file path
 * @return Server configuration
 */
HttpServerConfig parseHttpConfig(const std::string& filename);

} // namespace HttpModule

#endif // HTTP_SERVER_HPP
