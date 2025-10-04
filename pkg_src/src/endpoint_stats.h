
#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <atomic>
#include <mutex>

// Forward declarations
class Endpoint;

namespace EndpointStats {

using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

/**
 * Statistics collection configuration
 */
struct StatsConfig {
    // Category enable/disable flags
    bool enable_connection_health = true;
    bool enable_message_stats = true;
    bool enable_performance_metrics = true;
    bool enable_filtering_stats = true;
    bool enable_resource_stats = true;
    bool enable_uart_stats = true;
    bool enable_udp_stats = true;
    bool enable_tcp_stats = true;
    
    // Timing configuration (in milliseconds)
    uint32_t periodic_collection_interval_ms = 5000;  // 5 seconds default
    uint32_t error_cleanup_interval_ms = 60000;       // 1 minute default
    uint32_t statistics_report_interval_ms = 30000;   // 30 seconds default
    uint32_t resource_check_interval_ms = 10000;      // 10 seconds default
    
    // JSON file output configuration
    bool enable_json_file_output = false;             // Enable/disable JSON file output
    std::string json_output_file_path = "";           // Path to JSON output file
    uint32_t json_file_write_interval_ms = 10000;     // How often to write to JSON file (10 seconds default)
};

/**
 * Connection state enumeration
 */
enum class ConnectionState {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR_STATE
};

/**
 * Error categories for diagnostics
 */
enum class ErrorCategory {
    HARDWARE = 0,
    NETWORK,
    PROTOCOL,
    CONFIGURATION,
    RESOURCE
};

/**
 * Error event structure for ring buffer
 */
struct ErrorEvent {
    TimePoint timestamp;
    ErrorCategory category;
    std::string description;
    int error_code;
    
    ErrorEvent(ErrorCategory cat, const std::string& desc, int code = 0)
        : timestamp(std::chrono::steady_clock::now())
        , category(cat)
        , description(desc)
        , error_code(code) {}
};

/**
 * Rolling average calculator
 */
class RollingAverage {
public:
    RollingAverage(size_t window_size = 10);
    void add_sample(double value);
    double get_average() const;
    void reset();
    
private:
    std::deque<double> samples_;
    size_t window_size_;
    double sum_;
};

/**
 * Rate calculator for messages/bytes per second
 */
class RateCalculator {
public:
    RateCalculator(Duration window = std::chrono::seconds(5));
    void add_event(size_t count = 1);
    double get_rate() const;
    void reset();
    
private:
    struct Event {
        TimePoint timestamp;
        size_t count;
    };
    
    std::deque<Event> events_;
    Duration window_duration_;
    mutable TimePoint last_cleanup_;
    
    void cleanup_old_events() const;
};

/**
 * Connection health statistics
 */
struct ConnectionHealth {
    std::atomic<ConnectionState> state{ConnectionState::DISCONNECTED};
    TimePoint connection_start_time;
    TimePoint last_connection_time;
    std::atomic<uint32_t> reconnection_attempts{0};
    std::atomic<uint32_t> successful_reconnections{0};
    std::atomic<uint32_t> connection_drops{0};
    Duration total_uptime{0};
    Duration total_downtime{0};
    
    // Methods
    void set_state(ConnectionState new_state);
    void on_connection_established();
    void on_connection_lost();
    void on_reconnection_attempt();
    void on_successful_reconnection();
    double get_stability_ratio() const;
    Duration get_current_uptime() const;
    std::string get_state_string() const;
};

/**
 * Enhanced message statistics
 */
struct MessageStats {
    // Rate calculations
    RateCalculator message_rate;
    RateCalculator byte_rate;
    RollingAverage message_size_avg;
    
    // Peak values
    std::atomic<double> peak_message_rate{0.0};
    std::atomic<double> peak_byte_rate{0.0};
    
    // Protocol version tracking
    std::atomic<uint32_t> mavlink_v1_count{0};
    std::atomic<uint32_t> mavlink_v2_count{0};
    
    // Error counts
    std::atomic<uint32_t> malformed_packets{0};
    std::atomic<uint32_t> buffer_overruns{0};
    std::atomic<uint32_t> timeout_errors{0};
    
    // Methods
    void on_message_received(size_t size, bool is_v2);
    void on_malformed_packet();
    void on_buffer_overrun();
    void on_timeout_error();
    void update_peaks();
    double get_protocol_v2_ratio() const;
};

/**
 * Performance metrics
 */
struct PerformanceMetrics {
    // Latency tracking (microseconds)
    std::atomic<uint64_t> min_latency_us{UINT64_MAX};
    std::atomic<uint64_t> max_latency_us{0};
    RollingAverage avg_latency;
    
    // Buffer utilization (0-100%)
    std::atomic<double> rx_buffer_utilization{0.0};
    std::atomic<double> tx_buffer_utilization{0.0};
    
    // Processing efficiency
    RollingAverage processing_time_avg;
    std::atomic<uint32_t> queue_depth{0};
    
    // Methods
    void record_latency(uint64_t latency_us);
    void update_buffer_utilization(size_t rx_used, size_t rx_total, 
                                   size_t tx_used, size_t tx_total);
    void record_processing_time(uint64_t processing_time_us);
};

/**
 * UART-specific statistics
 */
struct UartStats {
    std::atomic<uint32_t> current_baudrate{0};
    std::atomic<uint32_t> baudrate_changes{0};
    std::atomic<uint32_t> flow_control_events{0};
    std::atomic<uint32_t> hardware_errors{0};
    std::atomic<uint32_t> device_scan_count{0};
    
    // Device path history
    std::vector<std::string> device_path_history;
    
    // Methods
    void on_baudrate_change(uint32_t new_baudrate);
    void on_flow_control_event();
    void on_hardware_error();
    void on_device_scan();
    void add_device_path(const std::string& path);
};

/**
 * UDP-specific statistics  
 */
struct UdpStats {
    std::atomic<uint32_t> address_changes{0};
    std::atomic<uint32_t> socket_errors{0};
    std::atomic<uint32_t> multicast_packets{0};
    std::atomic<uint32_t> broadcast_packets{0};
    std::atomic<uint32_t> icmp_errors{0};
    std::atomic<uint32_t> out_of_order_packets{0};
    
    // Network quality
    RateCalculator packet_loss_rate;
    
    // Methods
    void on_address_change();
    void on_socket_error();
    void on_multicast_packet();
    void on_broadcast_packet();
    void on_icmp_error();
    void on_out_of_order_packet();
    void on_packet_loss();
    double get_packet_loss_rate() const;
};

/**
 * TCP-specific statistics
 */
struct TcpStats {
    TimePoint connection_start_time;
    std::atomic<uint32_t> retransmissions{0};
    std::atomic<uint32_t> window_zero_events{0};
    std::atomic<uint32_t> graceful_disconnections{0};
    std::atomic<uint32_t> unexpected_disconnections{0};
    std::atomic<uint32_t> keepalive_successes{0};
    std::atomic<uint32_t> keepalive_failures{0};
    
    // Methods
    void on_connection_start();
    void on_retransmission();
    void on_window_zero_event();
    void on_graceful_disconnection();
    void on_unexpected_disconnection();
    void on_keepalive_success();
    void on_keepalive_failure();
    Duration get_connection_duration() const;
};

/**
 * Filtering and routing statistics
 */
struct FilteringStats {
    std::atomic<uint32_t> messages_filtered_by_msg_id{0};
    std::atomic<uint32_t> messages_filtered_by_src_comp{0};
    std::atomic<uint32_t> messages_filtered_by_src_sys{0};
    std::atomic<uint32_t> messages_accepted{0};
    std::atomic<uint32_t> messages_rejected{0};
    std::atomic<uint32_t> group_messages_shared{0};
    std::atomic<uint32_t> messages_deduplicated{0};
    
    // Methods
    void on_message_filtered(const std::string& filter_type);
    void on_message_accepted();
    void on_message_rejected();
    void on_group_message_shared();
    void on_message_deduplicated();
    double get_acceptance_rate() const;
};

/**
 * Resource utilization statistics
 */
struct ResourceStats {
    std::atomic<size_t> memory_usage_bytes{0};
    std::atomic<int> file_descriptor_count{0};
    std::atomic<uint64_t> cpu_time_us{0};
    std::atomic<bool> near_fd_limit{false};
    std::atomic<bool> near_memory_limit{false};
    
    // Methods
    void update_memory_usage(size_t bytes);
    void update_fd_count(int count);
    void add_cpu_time(uint64_t time_us);
    void check_resource_limits();
};

/**
 * Main endpoint statistics container
 */
class EndpointStatistics {
public:
    explicit EndpointStatistics(const std::string& endpoint_name);
    explicit EndpointStatistics(const std::string& endpoint_name, const StatsConfig& config);
    ~EndpointStatistics() = default;

    // Core statistics components
    ConnectionHealth connection_health;
    MessageStats message_stats;
    PerformanceMetrics performance;
    FilteringStats filtering;
    ResourceStats resources;
    
    // Endpoint-specific statistics (only one will be used per endpoint)
    std::unique_ptr<UartStats> uart_stats;
    std::unique_ptr<UdpStats> udp_stats;
    std::unique_ptr<TcpStats> tcp_stats;
    
    // Error diagnostics
    static constexpr size_t MAX_ERROR_HISTORY = 50;
    std::deque<ErrorEvent> error_history;
    mutable std::mutex error_history_mutex;
    
    // Methods
    void initialize_uart_stats();
    void initialize_udp_stats(); 
    void initialize_tcp_stats();
    
    void log_endpoint_error(ErrorCategory category, const std::string& description, int error_code = 0);
    void update_periodic_stats();
    void reset_all_stats();
    
    // JSON export for external monitoring
    std::string to_json() const;
    std::string to_detailed_json() const;
    
    // JSON file output
    void write_json_to_file() const;
    void write_json_to_file(const std::string& file_path) const;
    
    // Statistics reporting
    void print_summary() const;
    void print_detailed() const;
    
    // Rate calculations
    double get_error_rate(std::chrono::minutes window = std::chrono::minutes(5)) const;
    double get_recovery_success_rate() const;
    
    // Configuration management
    void update_config(const StatsConfig& config);
    const StatsConfig& get_config() const;
    bool is_category_enabled(const std::string& category) const;
    
private:
    std::string endpoint_name_;
    TimePoint creation_time_;
    StatsConfig stats_config_;
    mutable TimePoint last_json_file_write_{};
    
    void add_error_event(const ErrorEvent& event);
    void cleanup_old_errors();
};

} // namespace EndpointStats
