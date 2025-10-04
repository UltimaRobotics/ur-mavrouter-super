
#include "endpoint_stats.h"
#include <common/log.h>
#include <common/util.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <fstream>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstring>

namespace EndpointStats {

// RollingAverage Implementation
RollingAverage::RollingAverage(size_t window_size)
    : window_size_(window_size), sum_(0.0) {}

void RollingAverage::add_sample(double value) {
    samples_.push_back(value);
    sum_ += value;
    
    if (samples_.size() > window_size_) {
        sum_ -= samples_.front();
        samples_.pop_front();
    }
}

double RollingAverage::get_average() const {
    return samples_.empty() ? 0.0 : sum_ / samples_.size();
}

void RollingAverage::reset() {
    samples_.clear();
    sum_ = 0.0;
}

// RateCalculator Implementation
RateCalculator::RateCalculator(Duration window)
    : window_duration_(window), last_cleanup_(std::chrono::steady_clock::now()) {}

void RateCalculator::add_event(size_t count) {
    auto now = std::chrono::steady_clock::now();
    events_.push_back({now, count});
    
    // Periodic cleanup to prevent unbounded growth
    if (now - last_cleanup_ > window_duration_) {
        cleanup_old_events();
        last_cleanup_ = now;
    }
}

double RateCalculator::get_rate() const {
    cleanup_old_events();
    
    if (events_.empty()) {
        return 0.0;
    }
    
    size_t total_count = 0;
    for (const auto& event : events_) {
        total_count += event.count;
    }
    
    auto window_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(window_duration_).count() / 1000.0;
    return static_cast<double>(total_count) / window_seconds;
}

void RateCalculator::reset() {
    events_.clear();
    last_cleanup_ = std::chrono::steady_clock::now();
}

void RateCalculator::cleanup_old_events() const {
    auto cutoff = std::chrono::steady_clock::now() - window_duration_;
    
    // Use const_cast to modify mutable member in const method
    auto& mutable_events = const_cast<std::deque<Event>&>(events_);
    while (!mutable_events.empty() && mutable_events.front().timestamp < cutoff) {
        mutable_events.pop_front();
    }
}

// ConnectionHealth Implementation
void ConnectionHealth::set_state(ConnectionState new_state) {
    auto now = std::chrono::steady_clock::now();
    ConnectionState old_state = state.exchange(new_state);
    
    if (old_state == ConnectionState::CONNECTED && new_state != ConnectionState::CONNECTED) {
        // Connection lost
        if (connection_start_time.time_since_epoch().count() > 0) {
            total_uptime += now - connection_start_time;
        }
    } else if (old_state != ConnectionState::CONNECTED && new_state == ConnectionState::CONNECTED) {
        // Connection established
        connection_start_time = now;
        last_connection_time = now;
    }
}

void ConnectionHealth::on_connection_established() {
    set_state(ConnectionState::CONNECTED);
}

void ConnectionHealth::on_connection_lost() {
    connection_drops++;
    set_state(ConnectionState::DISCONNECTED);
}

void ConnectionHealth::on_reconnection_attempt() {
    reconnection_attempts++;
    set_state(ConnectionState::RECONNECTING);
}

void ConnectionHealth::on_successful_reconnection() {
    successful_reconnections++;
    set_state(ConnectionState::CONNECTED);
}

double ConnectionHealth::get_stability_ratio() const {
    auto total_time = total_uptime + total_downtime;
    if (total_time.count() == 0) {
        return 0.0;
    }
    
    auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_uptime).count();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_time).count();
    
    return static_cast<double>(uptime_ms) / static_cast<double>(total_ms) * 100.0;
}

Duration ConnectionHealth::get_current_uptime() const {
    if (state.load() == ConnectionState::CONNECTED && connection_start_time.time_since_epoch().count() > 0) {
        return std::chrono::steady_clock::now() - connection_start_time;
    }
    return Duration(0);
}

std::string ConnectionHealth::get_state_string() const {
    switch (state.load()) {
        case ConnectionState::DISCONNECTED: return "DISCONNECTED";
        case ConnectionState::CONNECTING: return "CONNECTING";
        case ConnectionState::CONNECTED: return "CONNECTED";
        case ConnectionState::RECONNECTING: return "RECONNECTING";
        case ConnectionState::ERROR_STATE: return "ERROR";
        default: return "UNKNOWN";
    }
}

// MessageStats Implementation
void MessageStats::on_message_received(size_t size, bool is_v2) {
    message_rate.add_event(1);
    byte_rate.add_event(size);
    message_size_avg.add_sample(static_cast<double>(size));
    
    if (is_v2) {
        mavlink_v2_count++;
    } else {
        mavlink_v1_count++;
    }
    
    update_peaks();
}

void MessageStats::on_malformed_packet() {
    malformed_packets++;
}

void MessageStats::on_buffer_overrun() {
    buffer_overruns++;
}

void MessageStats::on_timeout_error() {
    timeout_errors++;
}

void MessageStats::update_peaks() {
    double current_msg_rate = message_rate.get_rate();
    double current_byte_rate = byte_rate.get_rate();
    
    double expected_peak_msg = peak_message_rate.load();
    while (current_msg_rate > expected_peak_msg && 
           !peak_message_rate.compare_exchange_weak(expected_peak_msg, current_msg_rate)) {}
    
    double expected_peak_byte = peak_byte_rate.load();
    while (current_byte_rate > expected_peak_byte && 
           !peak_byte_rate.compare_exchange_weak(expected_peak_byte, current_byte_rate)) {}
}

double MessageStats::get_protocol_v2_ratio() const {
    uint32_t v1 = mavlink_v1_count.load();
    uint32_t v2 = mavlink_v2_count.load();
    uint32_t total = v1 + v2;
    
    return total > 0 ? (static_cast<double>(v2) / static_cast<double>(total)) * 100.0 : 0.0;
}

// PerformanceMetrics Implementation
void PerformanceMetrics::record_latency(uint64_t latency_us) {
    uint64_t current_min = min_latency_us.load();
    while (latency_us < current_min && 
           !min_latency_us.compare_exchange_weak(current_min, latency_us)) {}
    
    uint64_t current_max = max_latency_us.load();
    while (latency_us > current_max && 
           !max_latency_us.compare_exchange_weak(current_max, latency_us)) {}
    
    avg_latency.add_sample(static_cast<double>(latency_us));
}

void PerformanceMetrics::update_buffer_utilization(size_t rx_used, size_t rx_total, 
                                                    size_t tx_used, size_t tx_total) {
    if (rx_total > 0) {
        rx_buffer_utilization.store((static_cast<double>(rx_used) / rx_total) * 100.0);
    }
    if (tx_total > 0) {
        tx_buffer_utilization.store((static_cast<double>(tx_used) / tx_total) * 100.0);
    }
}

void PerformanceMetrics::record_processing_time(uint64_t processing_time_us) {
    processing_time_avg.add_sample(static_cast<double>(processing_time_us));
}

// UART-specific stats
void UartStats::on_baudrate_change(uint32_t new_baudrate) {
    current_baudrate.store(new_baudrate);
    baudrate_changes++;
}

void UartStats::on_flow_control_event() {
    flow_control_events++;
}

void UartStats::on_hardware_error() {
    hardware_errors++;
}

void UartStats::on_device_scan() {
    device_scan_count++;
}

void UartStats::add_device_path(const std::string& path) {
    if (std::find(device_path_history.begin(), device_path_history.end(), path) == device_path_history.end()) {
        device_path_history.push_back(path);
        // Keep history limited
        if (device_path_history.size() > 10) {
            device_path_history.erase(device_path_history.begin());
        }
    }
}

// UDP-specific stats
void UdpStats::on_address_change() {
    address_changes++;
}

void UdpStats::on_socket_error() {
    socket_errors++;
}

void UdpStats::on_multicast_packet() {
    multicast_packets++;
}

void UdpStats::on_broadcast_packet() {
    broadcast_packets++;
}

void UdpStats::on_icmp_error() {
    icmp_errors++;
}

void UdpStats::on_out_of_order_packet() {
    out_of_order_packets++;
}

void UdpStats::on_packet_loss() {
    packet_loss_rate.add_event(1);
}

double UdpStats::get_packet_loss_rate() const {
    return packet_loss_rate.get_rate();
}

// TCP-specific stats
void TcpStats::on_connection_start() {
    connection_start_time = std::chrono::steady_clock::now();
}

void TcpStats::on_retransmission() {
    retransmissions++;
}

void TcpStats::on_window_zero_event() {
    window_zero_events++;
}

void TcpStats::on_graceful_disconnection() {
    graceful_disconnections++;
}

void TcpStats::on_unexpected_disconnection() {
    unexpected_disconnections++;
}

void TcpStats::on_keepalive_success() {
    keepalive_successes++;
}

void TcpStats::on_keepalive_failure() {
    keepalive_failures++;
}

Duration TcpStats::get_connection_duration() const {
    if (connection_start_time.time_since_epoch().count() > 0) {
        return std::chrono::steady_clock::now() - connection_start_time;
    }
    return Duration(0);
}

// FilteringStats Implementation
void FilteringStats::on_message_filtered(const std::string& filter_type) {
    if (filter_type == "msg_id") {
        messages_filtered_by_msg_id++;
    } else if (filter_type == "src_comp") {
        messages_filtered_by_src_comp++;
    } else if (filter_type == "src_sys") {
        messages_filtered_by_src_sys++;
    }
}

void FilteringStats::on_message_accepted() {
    messages_accepted++;
}

void FilteringStats::on_message_rejected() {
    messages_rejected++;
}

void FilteringStats::on_group_message_shared() {
    group_messages_shared++;
}

void FilteringStats::on_message_deduplicated() {
    messages_deduplicated++;
}

double FilteringStats::get_acceptance_rate() const {
    uint32_t total = messages_accepted.load() + messages_rejected.load();
    return total > 0 ? (static_cast<double>(messages_accepted.load()) / total) * 100.0 : 0.0;
}

// ResourceStats Implementation
void ResourceStats::update_memory_usage(size_t bytes) {
    memory_usage_bytes.store(bytes);
}

void ResourceStats::update_fd_count(int count) {
    file_descriptor_count.store(count);
    check_resource_limits();
}

void ResourceStats::add_cpu_time(uint64_t time_us) {
    cpu_time_us.fetch_add(time_us);
}

void ResourceStats::check_resource_limits() {
    struct rlimit fd_limit;
    if (getrlimit(RLIMIT_NOFILE, &fd_limit) == 0) {
        int current_fds = file_descriptor_count.load();
        near_fd_limit.store(current_fds > (int)(fd_limit.rlim_cur * 0.8));
    }
    
    // Memory limit checking would require more system-specific code
    // For now, just flag if we're using more than 100MB
    size_t memory_bytes = memory_usage_bytes.load();
    near_memory_limit.store(memory_bytes > (100 * 1024 * 1024));
}

// EndpointStatistics Implementation
EndpointStatistics::EndpointStatistics(const std::string& endpoint_name)
    : endpoint_name_(endpoint_name)
    , creation_time_(std::chrono::steady_clock::now()) {
}

EndpointStatistics::EndpointStatistics(const std::string& endpoint_name, const StatsConfig& config)
    : endpoint_name_(endpoint_name)
    , creation_time_(std::chrono::steady_clock::now())
    , stats_config_(config) {
}

void EndpointStatistics::initialize_uart_stats() {
    uart_stats = std::make_unique<UartStats>();
}

void EndpointStatistics::initialize_udp_stats() {
    udp_stats = std::make_unique<UdpStats>();
}

void EndpointStatistics::initialize_tcp_stats() {
    tcp_stats = std::make_unique<TcpStats>();
}

void EndpointStatistics::log_endpoint_error(ErrorCategory category, const std::string& description, int error_code) {
    ErrorEvent event(category, description, error_code);
    add_error_event(event);
}

void EndpointStatistics::add_error_event(const ErrorEvent& event) {
    std::lock_guard<std::mutex> lock(error_history_mutex);
    error_history.push_back(event);
    
    if (error_history.size() > MAX_ERROR_HISTORY) {
        error_history.pop_front();
    }
}

void EndpointStatistics::cleanup_old_errors() {
    std::lock_guard<std::mutex> lock(error_history_mutex);
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::hours(24);
    
    while (!error_history.empty() && error_history.front().timestamp < cutoff) {
        error_history.pop_front();
    }
}

void EndpointStatistics::update_periodic_stats() {
    message_stats.update_peaks();
    cleanup_old_errors();
    resources.check_resource_limits();
    
    // Write JSON file if enabled and interval has passed
    if (stats_config_.enable_json_file_output && !stats_config_.json_output_file_path.empty()) {
        auto now = std::chrono::steady_clock::now();
        auto interval = std::chrono::milliseconds(stats_config_.json_file_write_interval_ms);
        
        if (last_json_file_write_.time_since_epoch().count() == 0 || 
            (now - last_json_file_write_) >= interval) {
            
            log_info("EndpointStatistics::update_periodic_stats() - JSON file output triggered for endpoint '%s'", 
                     endpoint_name_.c_str());
            log_debug("EndpointStatistics::update_periodic_stats() - Target file: '%s', Write interval: %u ms", 
                      stats_config_.json_output_file_path.c_str(), stats_config_.json_file_write_interval_ms);
            
            write_json_to_file();
            last_json_file_write_ = now;
        } else {
            auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_json_file_write_).count();
            log_trace("EndpointStatistics::update_periodic_stats() - JSON file write skipped, time since last: %ld ms (interval: %u ms)", 
                      time_since_last, stats_config_.json_file_write_interval_ms);
        }
    } else {
        if (!stats_config_.enable_json_file_output) {
            log_trace("EndpointStatistics::update_periodic_stats() - JSON file output disabled for endpoint '%s'", 
                      endpoint_name_.c_str());
        } else if (stats_config_.json_output_file_path.empty()) {
            log_warning("EndpointStatistics::update_periodic_stats() - JSON file output enabled but no file path configured for endpoint '%s'", 
                        endpoint_name_.c_str());
        }
    }
}

void EndpointStatistics::reset_all_stats() {
    // Reset ConnectionHealth atomics individually
    connection_health.state.store(ConnectionState::DISCONNECTED);
    connection_health.reconnection_attempts.store(0);
    connection_health.successful_reconnections.store(0);
    connection_health.connection_drops.store(0);
    connection_health.connection_start_time = TimePoint{};
    connection_health.last_connection_time = TimePoint{};
    connection_health.total_uptime = Duration{0};
    connection_health.total_downtime = Duration{0};
    
    // Reset MessageStats
    message_stats.message_rate = RateCalculator{};
    message_stats.byte_rate = RateCalculator{};
    message_stats.message_size_avg = RollingAverage{};
    message_stats.peak_message_rate.store(0.0);
    message_stats.peak_byte_rate.store(0.0);
    message_stats.mavlink_v1_count.store(0);
    message_stats.mavlink_v2_count.store(0);
    message_stats.malformed_packets.store(0);
    message_stats.buffer_overruns.store(0);
    message_stats.timeout_errors.store(0);
    
    // Reset PerformanceMetrics
    performance.min_latency_us.store(UINT64_MAX);
    performance.max_latency_us.store(0);
    performance.avg_latency = RollingAverage{};
    performance.rx_buffer_utilization.store(0.0);
    performance.tx_buffer_utilization.store(0.0);
    performance.processing_time_avg = RollingAverage{};
    performance.queue_depth.store(0);
    
    // Reset FilteringStats
    filtering.messages_filtered_by_msg_id.store(0);
    filtering.messages_filtered_by_src_comp.store(0);
    filtering.messages_filtered_by_src_sys.store(0);
    filtering.messages_accepted.store(0);
    filtering.messages_rejected.store(0);
    filtering.group_messages_shared.store(0);
    filtering.messages_deduplicated.store(0);
    
    // Reset ResourceStats
    resources.memory_usage_bytes.store(0);
    resources.file_descriptor_count.store(0);
    resources.cpu_time_us.store(0);
    resources.near_fd_limit.store(false);
    resources.near_memory_limit.store(false);
    
    std::lock_guard<std::mutex> lock(error_history_mutex);
    error_history.clear();
}

double EndpointStatistics::get_error_rate(std::chrono::minutes window) const {
    std::lock_guard<std::mutex> lock(error_history_mutex);
    auto cutoff = std::chrono::steady_clock::now() - window;
    
    size_t recent_errors = 0;
    for (const auto& error : error_history) {
        if (error.timestamp >= cutoff) {
            recent_errors++;
        }
    }
    
    auto window_seconds = std::chrono::duration_cast<std::chrono::seconds>(window).count();
    return static_cast<double>(recent_errors) / window_seconds;
}

double EndpointStatistics::get_recovery_success_rate() const {
    uint32_t attempts = connection_health.reconnection_attempts.load();
    uint32_t successes = connection_health.successful_reconnections.load();
    
    return attempts > 0 ? (static_cast<double>(successes) / attempts) * 100.0 : 0.0;
}

std::string EndpointStatistics::to_json() const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    
    json << "{\n";
    json << "  \"endpoint_name\": \"" << endpoint_name_ << "\",\n";
    json << "  \"connection_health\": {\n";
    json << "    \"state\": \"" << connection_health.get_state_string() << "\",\n";
    json << "    \"stability_ratio\": " << connection_health.get_stability_ratio() << ",\n";
    json << "    \"reconnection_attempts\": " << connection_health.reconnection_attempts.load() << ",\n";
    json << "    \"connection_drops\": " << connection_health.connection_drops.load() << "\n";
    json << "  },\n";
    
    json << "  \"message_stats\": {\n";
    json << "    \"message_rate\": " << message_stats.message_rate.get_rate() << ",\n";
    json << "    \"peak_message_rate\": " << message_stats.peak_message_rate.load() << ",\n";
    json << "    \"avg_message_size\": " << message_stats.message_size_avg.get_average() << ",\n";
    json << "    \"protocol_v2_ratio\": " << message_stats.get_protocol_v2_ratio() << ",\n";
    json << "    \"malformed_packets\": " << message_stats.malformed_packets.load() << "\n";
    json << "  },\n";
    
    json << "  \"performance\": {\n";
    json << "    \"avg_latency_us\": " << performance.avg_latency.get_average() << ",\n";
    json << "    \"rx_buffer_util\": " << performance.rx_buffer_utilization.load() << ",\n";
    json << "    \"tx_buffer_util\": " << performance.tx_buffer_utilization.load() << "\n";
    json << "  },\n";
    
    json << "  \"filtering\": {\n";
    json << "    \"acceptance_rate\": " << filtering.get_acceptance_rate() << ",\n";
    json << "    \"messages_filtered\": " << (filtering.messages_filtered_by_msg_id.load() + 
                                               filtering.messages_filtered_by_src_comp.load() + 
                                               filtering.messages_filtered_by_src_sys.load()) << "\n";
    json << "  }\n";
    
    json << "}";
    return json.str();
}

std::string EndpointStatistics::to_detailed_json() const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    json << "{\n";
    json << "  \"timestamp\": " << timestamp << ",\n";
    json << "  \"endpoint_name\": \"" << endpoint_name_ << "\",\n";
    json << "  \"enabled_categories\": {\n";
    json << "    \"connection_health\": " << (stats_config_.enable_connection_health ? "true" : "false") << ",\n";
    json << "    \"message_stats\": " << (stats_config_.enable_message_stats ? "true" : "false") << ",\n";
    json << "    \"performance_metrics\": " << (stats_config_.enable_performance_metrics ? "true" : "false") << ",\n";
    json << "    \"filtering_stats\": " << (stats_config_.enable_filtering_stats ? "true" : "false") << ",\n";
    json << "    \"resource_stats\": " << (stats_config_.enable_resource_stats ? "true" : "false") << ",\n";
    json << "    \"uart_stats\": " << (stats_config_.enable_uart_stats ? "true" : "false") << ",\n";
    json << "    \"udp_stats\": " << (stats_config_.enable_udp_stats ? "true" : "false") << ",\n";
    json << "    \"tcp_stats\": " << (stats_config_.enable_tcp_stats ? "true" : "false") << "\n";
    json << "  },\n";
    
    if (stats_config_.enable_connection_health) {
        json << "  \"connection_health\": {\n";
        json << "    \"state\": \"" << connection_health.get_state_string() << "\",\n";
        json << "    \"stability_ratio\": " << connection_health.get_stability_ratio() << ",\n";
        json << "    \"reconnection_attempts\": " << connection_health.reconnection_attempts.load() << ",\n";
        json << "    \"successful_reconnections\": " << connection_health.successful_reconnections.load() << ",\n";
        json << "    \"connection_drops\": " << connection_health.connection_drops.load() << ",\n";
        auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(connection_health.get_current_uptime()).count();
        json << "    \"current_uptime_ms\": " << uptime_ms << "\n";
        json << "  },\n";
    }
    
    if (stats_config_.enable_message_stats) {
        json << "  \"message_stats\": {\n";
        json << "    \"message_rate\": " << message_stats.message_rate.get_rate() << ",\n";
        json << "    \"byte_rate\": " << message_stats.byte_rate.get_rate() << ",\n";
        json << "    \"peak_message_rate\": " << message_stats.peak_message_rate.load() << ",\n";
        json << "    \"peak_byte_rate\": " << message_stats.peak_byte_rate.load() << ",\n";
        json << "    \"avg_message_size\": " << message_stats.message_size_avg.get_average() << ",\n";
        json << "    \"protocol_v2_ratio\": " << message_stats.get_protocol_v2_ratio() << ",\n";
        json << "    \"mavlink_v1_count\": " << message_stats.mavlink_v1_count.load() << ",\n";
        json << "    \"mavlink_v2_count\": " << message_stats.mavlink_v2_count.load() << ",\n";
        json << "    \"malformed_packets\": " << message_stats.malformed_packets.load() << ",\n";
        json << "    \"buffer_overruns\": " << message_stats.buffer_overruns.load() << ",\n";
        json << "    \"timeout_errors\": " << message_stats.timeout_errors.load() << "\n";
        json << "  },\n";
    }
    
    if (stats_config_.enable_performance_metrics) {
        json << "  \"performance\": {\n";
        json << "    \"min_latency_us\": " << performance.min_latency_us.load() << ",\n";
        json << "    \"max_latency_us\": " << performance.max_latency_us.load() << ",\n";
        json << "    \"avg_latency_us\": " << performance.avg_latency.get_average() << ",\n";
        json << "    \"rx_buffer_utilization\": " << performance.rx_buffer_utilization.load() << ",\n";
        json << "    \"tx_buffer_utilization\": " << performance.tx_buffer_utilization.load() << ",\n";
        json << "    \"avg_processing_time_us\": " << performance.processing_time_avg.get_average() << ",\n";
        json << "    \"queue_depth\": " << performance.queue_depth.load() << "\n";
        json << "  },\n";
    }
    
    if (stats_config_.enable_filtering_stats) {
        json << "  \"filtering\": {\n";
        json << "    \"acceptance_rate\": " << filtering.get_acceptance_rate() << ",\n";
        json << "    \"messages_filtered_by_msg_id\": " << filtering.messages_filtered_by_msg_id.load() << ",\n";
        json << "    \"messages_filtered_by_src_comp\": " << filtering.messages_filtered_by_src_comp.load() << ",\n";
        json << "    \"messages_filtered_by_src_sys\": " << filtering.messages_filtered_by_src_sys.load() << ",\n";
        json << "    \"messages_accepted\": " << filtering.messages_accepted.load() << ",\n";
        json << "    \"messages_rejected\": " << filtering.messages_rejected.load() << ",\n";
        json << "    \"group_messages_shared\": " << filtering.group_messages_shared.load() << ",\n";
        json << "    \"messages_deduplicated\": " << filtering.messages_deduplicated.load() << "\n";
        json << "  },\n";
    }
    
    if (stats_config_.enable_resource_stats) {
        json << "  \"resources\": {\n";
        json << "    \"memory_usage_bytes\": " << resources.memory_usage_bytes.load() << ",\n";
        json << "    \"file_descriptor_count\": " << resources.file_descriptor_count.load() << ",\n";
        json << "    \"cpu_time_us\": " << resources.cpu_time_us.load() << ",\n";
        json << "    \"near_fd_limit\": " << (resources.near_fd_limit.load() ? "true" : "false") << ",\n";
        json << "    \"near_memory_limit\": " << (resources.near_memory_limit.load() ? "true" : "false") << "\n";
        json << "  },\n";
    }
    
    if (uart_stats && stats_config_.enable_uart_stats) {
        json << "  \"uart_stats\": {\n";
        json << "    \"current_baudrate\": " << uart_stats->current_baudrate.load() << ",\n";
        json << "    \"baudrate_changes\": " << uart_stats->baudrate_changes.load() << ",\n";
        json << "    \"flow_control_events\": " << uart_stats->flow_control_events.load() << ",\n";
        json << "    \"hardware_errors\": " << uart_stats->hardware_errors.load() << ",\n";
        json << "    \"device_scan_count\": " << uart_stats->device_scan_count.load() << "\n";
        json << "  },\n";
    }
    
    if (udp_stats && stats_config_.enable_udp_stats) {
        json << "  \"udp_stats\": {\n";
        json << "    \"address_changes\": " << udp_stats->address_changes.load() << ",\n";
        json << "    \"socket_errors\": " << udp_stats->socket_errors.load() << ",\n";
        json << "    \"multicast_packets\": " << udp_stats->multicast_packets.load() << ",\n";
        json << "    \"broadcast_packets\": " << udp_stats->broadcast_packets.load() << ",\n";
        json << "    \"icmp_errors\": " << udp_stats->icmp_errors.load() << ",\n";
        json << "    \"out_of_order_packets\": " << udp_stats->out_of_order_packets.load() << ",\n";
        json << "    \"packet_loss_rate\": " << udp_stats->get_packet_loss_rate() << "\n";
        json << "  },\n";
    }
    
    if (tcp_stats && stats_config_.enable_tcp_stats) {
        json << "  \"tcp_stats\": {\n";
        auto duration = tcp_stats->get_connection_duration();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        json << "    \"connection_duration_ms\": " << duration_ms << ",\n";
        json << "    \"retransmissions\": " << tcp_stats->retransmissions.load() << ",\n";
        json << "    \"window_zero_events\": " << tcp_stats->window_zero_events.load() << ",\n";
        json << "    \"graceful_disconnections\": " << tcp_stats->graceful_disconnections.load() << ",\n";
        json << "    \"unexpected_disconnections\": " << tcp_stats->unexpected_disconnections.load() << ",\n";
        json << "    \"keepalive_successes\": " << tcp_stats->keepalive_successes.load() << ",\n";
        json << "    \"keepalive_failures\": " << tcp_stats->keepalive_failures.load() << "\n";
        json << "  },\n";
    }
    
    // Error summary
    {
        std::lock_guard<std::mutex> lock(error_history_mutex);
        json << "  \"error_summary\": {\n";
        json << "    \"total_errors\": " << error_history.size() << ",\n";
        json << "    \"error_rate_per_minute\": " << get_error_rate(std::chrono::minutes(5)) * 60 << ",\n";
        json << "    \"recovery_success_rate\": " << get_recovery_success_rate() << "\n";
        json << "  }\n";
    }
    
    json << "}";
    return json.str();
}

void EndpointStatistics::write_json_to_file() const {
    if (!stats_config_.enable_json_file_output) {
        log_debug("EndpointStatistics::write_json_to_file() - JSON file output disabled for endpoint '%s'", 
                  endpoint_name_.c_str());
        return;
    }
    
    if (stats_config_.json_output_file_path.empty()) {
        log_warning("EndpointStatistics::write_json_to_file() - JSON file output enabled but file path is empty for endpoint '%s'", 
                    endpoint_name_.c_str());
        return;
    }
    
    log_debug("EndpointStatistics::write_json_to_file() - Using configured file path: '%s'", 
              stats_config_.json_output_file_path.c_str());
    write_json_to_file(stats_config_.json_output_file_path);
}

void EndpointStatistics::write_json_to_file(const std::string& file_path) const {
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        log_info("EndpointStatistics::write_json_to_file() - Starting JSON statistics write for endpoint '%s'", 
                 endpoint_name_.c_str());
        log_debug("EndpointStatistics::write_json_to_file() - Output file path: '%s'", file_path.c_str());
        
        // Generate JSON data first to measure size
        std::string json_data = to_detailed_json();
        size_t data_size_bytes = json_data.length();
        
        log_info("EndpointStatistics::write_json_to_file() - Generated JSON data: %zu bytes (%.2f KB)", 
                 data_size_bytes, data_size_bytes / 1024.0);
        
        // Check if file exists first
        std::ifstream test_file(file_path);
        bool file_exists = test_file.good();
        test_file.close();
        
        if (!file_exists) {
            log_info("EndpointStatistics::write_json_to_file() - File '%s' does not exist, will create it", file_path.c_str());
            
            // Extract directory path and create directories if needed
            size_t last_slash = file_path.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                std::string dir_path = file_path.substr(0, last_slash);
                log_debug("EndpointStatistics::write_json_to_file() - Creating directory path: '%s'", dir_path.c_str());
                
                // Create directories recursively (simple implementation)
                std::string current_path = "";
                std::istringstream path_stream(dir_path);
                std::string segment;
                
                while (std::getline(path_stream, segment, '/')) {
                    if (segment.empty()) continue;
                    
                    current_path += "/" + segment;
                    
                    // Try to create directory (will fail silently if it exists)
                    if (mkdir(current_path.c_str(), 0755) == 0) {
                        log_debug("EndpointStatistics::write_json_to_file() - Created directory: '%s'", current_path.c_str());
                    } else if (errno != EEXIST) {
                        log_warning("EndpointStatistics::write_json_to_file() - Failed to create directory '%s': %s", 
                                   current_path.c_str(), strerror(errno));
                    }
                }
            }
        } else {
            log_debug("EndpointStatistics::write_json_to_file() - File '%s' exists, will overwrite", file_path.c_str());
        }
        
        // Attempt to open file for writing
        std::ofstream file(file_path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            log_error("EndpointStatistics::write_json_to_file() - Failed to open/create file: %s (%s)", 
                     file_path.c_str(), strerror(errno));
            return;
        }
        
        log_debug("EndpointStatistics::write_json_to_file() - File opened successfully, writing data...");
        
        // Write data to file
        file << json_data << std::endl;
        
        if (file.fail()) {
            log_error("EndpointStatistics::write_json_to_file() - Error occurred during file write operation: %s", 
                     strerror(errno));
            file.close();
            return;
        }
        
        file.close();
        
        // Calculate write duration
        auto end_time = std::chrono::steady_clock::now();
        auto write_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        if (!file_exists) {
            log_info("EndpointStatistics::write_json_to_file() - Successfully created and wrote %zu bytes to new file '%s' in %ld μs", 
                     data_size_bytes, file_path.c_str(), write_duration);
        } else {
            log_info("EndpointStatistics::write_json_to_file() - Successfully wrote %zu bytes to existing file '%s' in %ld μs", 
                     data_size_bytes, file_path.c_str(), write_duration);
        }
        log_debug("EndpointStatistics::write_json_to_file() - Write rate: %.2f KB/s", 
                  (data_size_bytes / 1024.0) / (write_duration / 1000000.0));
        
        // Log statistics summary for this write
        log_info("EndpointStatistics::write_json_to_file() - Statistics summary - Endpoint: %s, Connection: %s, Msg Rate: %.1f/s, Error Rate: %.2f/min", 
                 endpoint_name_.c_str(), 
                 connection_health.get_state_string().c_str(),
                 message_stats.message_rate.get_rate(),
                 get_error_rate(std::chrono::minutes(5)) * 60);
        
    } catch (const std::exception& e) {
        log_error("EndpointStatistics::write_json_to_file() - Exception during file write to %s: %s", 
                  file_path.c_str(), e.what());
    }
}

void EndpointStatistics::print_summary() const {
    log_info("=== %s Statistics Summary ===", endpoint_name_.c_str());
    log_info("Connection State: %s", connection_health.get_state_string().c_str());
    log_info("Stability: %.1f%%", connection_health.get_stability_ratio());
    log_info("Message Rate: %.1f msg/s", message_stats.message_rate.get_rate());
    log_info("Byte Rate: %.1f KB/s", message_stats.byte_rate.get_rate() / 1024.0);
    log_info("Protocol v2 Ratio: %.1f%%", message_stats.get_protocol_v2_ratio());
    log_info("Average Latency: %.1f μs", performance.avg_latency.get_average());
    log_info("Error Rate: %.2f errors/min", get_error_rate(std::chrono::minutes(5)) * 60);
    log_info("==============================");
}

void EndpointStatistics::update_config(const StatsConfig& config) {
    stats_config_ = config;
}

const StatsConfig& EndpointStatistics::get_config() const {
    return stats_config_;
}

bool EndpointStatistics::is_category_enabled(const std::string& category) const {
    if (category == "connection_health") return stats_config_.enable_connection_health;
    if (category == "message_stats") return stats_config_.enable_message_stats;
    if (category == "performance_metrics") return stats_config_.enable_performance_metrics;
    if (category == "filtering_stats") return stats_config_.enable_filtering_stats;
    if (category == "resource_stats") return stats_config_.enable_resource_stats;
    if (category == "uart_stats") return stats_config_.enable_uart_stats;
    if (category == "udp_stats") return stats_config_.enable_udp_stats;
    if (category == "tcp_stats") return stats_config_.enable_tcp_stats;
    return true; // Default to enabled for unknown categories
}

void EndpointStatistics::print_detailed() const {
    print_summary();
    
    log_info("=== Detailed Statistics ===");
    
    // Connection details (only if enabled)
    if (stats_config_.enable_connection_health) {
        log_info("Reconnection Attempts: %u", connection_health.reconnection_attempts.load());
        log_info("Connection Drops: %u", connection_health.connection_drops.load());
        log_info("Recovery Success Rate: %.1f%%", get_recovery_success_rate());
    }
    
    // Message details (only if enabled)
    if (stats_config_.enable_message_stats) {
        log_info("Peak Message Rate: %.1f msg/s", message_stats.peak_message_rate.load());
        log_info("Average Message Size: %.1f bytes", message_stats.message_size_avg.get_average());
        log_info("Malformed Packets: %u", message_stats.malformed_packets.load());
        log_info("Buffer Overruns: %u", message_stats.buffer_overruns.load());
        log_info("Timeout Errors: %u", message_stats.timeout_errors.load());
    }
    
    // Performance details (only if enabled)
    if (stats_config_.enable_performance_metrics) {
        log_info("Min/Max Latency: %lu/%lu μs", 
                 performance.min_latency_us.load(), 
                 performance.max_latency_us.load());
        log_info("RX Buffer Utilization: %.1f%%", performance.rx_buffer_utilization.load());
        log_info("TX Buffer Utilization: %.1f%%", performance.tx_buffer_utilization.load());
    }
    
    // Filtering details (only if enabled)
    if (stats_config_.enable_filtering_stats) {
        log_info("Acceptance Rate: %.1f%%", filtering.get_acceptance_rate());
        log_info("Messages Filtered (ID/Comp/Sys): %u/%u/%u", 
                 filtering.messages_filtered_by_msg_id.load(),
                 filtering.messages_filtered_by_src_comp.load(), 
                 filtering.messages_filtered_by_src_sys.load());
    }
    
    // Resource usage (only if enabled)
    if (stats_config_.enable_resource_stats) {
        log_info("Memory Usage: %.1f KB", resources.memory_usage_bytes.load() / 1024.0);
        log_info("File Descriptors: %d", resources.file_descriptor_count.load());
        log_info("CPU Time: %.1f ms", resources.cpu_time_us.load() / 1000.0);
    }
    
    // Endpoint-specific stats (only if enabled)
    if (uart_stats && stats_config_.enable_uart_stats) {
        log_info("=== UART Specific ===");
        log_info("Current Baudrate: %u", uart_stats->current_baudrate.load());
        log_info("Baudrate Changes: %u", uart_stats->baudrate_changes.load());
        log_info("Hardware Errors: %u", uart_stats->hardware_errors.load());
        log_info("Device Scans: %u", uart_stats->device_scan_count.load());
    }
    
    if (udp_stats && stats_config_.enable_udp_stats) {
        log_info("=== UDP Specific ===");
        log_info("Address Changes: %u", udp_stats->address_changes.load());
        log_info("Socket Errors: %u", udp_stats->socket_errors.load());
        log_info("Packet Loss Rate: %.2f loss/s", udp_stats->get_packet_loss_rate());
        log_info("ICMP Errors: %u", udp_stats->icmp_errors.load());
    }
    
    if (tcp_stats && stats_config_.enable_tcp_stats) {
        log_info("=== TCP Specific ===");
        auto duration = tcp_stats->get_connection_duration();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        log_info("Connection Duration: %ld ms", duration_ms);
        log_info("Retransmissions: %u", tcp_stats->retransmissions.load());
        log_info("Graceful/Unexpected Disconnections: %u/%u", 
                 tcp_stats->graceful_disconnections.load(),
                 tcp_stats->unexpected_disconnections.load());
    }
    
    log_info("===========================");
}

} // namespace EndpointStats
