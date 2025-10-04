
/*
 * This file is part of the MAVLink Router project
 *
 * Copyright (C) 2024  Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <string>
#include <vector>
#include <memory>

// Include nlohmann JSON library
#include "../../modules/nholmann/json.hpp"

// Forward declarations
struct Configuration;
struct UartEndpointConfig;
struct UdpEndpointConfig;
struct TcpEndpointConfig;
struct LogOptions;
enum class LogMode;

// Forward declare EndpointStats namespace
namespace EndpointStats {
    struct StatsConfig;
}

// Include headers that define the complete types
#include "../logendpoint.h"
#include "../endpoint_stats.h"

/**
 * JSON Configuration Parser for ur-mavrouter
 * 
 * Uses nlohmann/json library for robust JSON parsing
 * Supports loading configuration from JSON files with structure
 * corresponding to all options documented in configuration.md
 */
class JsonConfig {
public:
    JsonConfig() = default;
    ~JsonConfig() = default;

    /**
     * Load and parse a JSON configuration file
     * 
     * @param filename Path to JSON configuration file
     * @return 0 on success, negative error code on failure
     */
    int parse(const std::string &filename);

    /**
     * Extract configuration into the main Configuration structure
     * 
     * @param config Configuration structure to populate
     * @return 0 on success, negative error code on failure
     */
    int extract_configuration(Configuration &config);

    /**
     * Extract only statistics configuration
     * 
     * @param config Statistics configuration structure to populate
     * @return 0 on success, negative error code on failure
     */
    int extract_stats_config(EndpointStats::StatsConfig &config);

    /**
     * Validate JSON structure and format
     * 
     * @return true if JSON structure is valid, false otherwise
     */
    bool validate_json_structure() const;

private:
    nlohmann::json _json_data;

    // Configuration extraction helpers
    void _init_default_config(Configuration &config);
    void _extract_general_config(Configuration &config, const nlohmann::json &general_section);
    void _extract_log_config(Configuration &config, const nlohmann::json &log_section);
    void _extract_stats_config(EndpointStats::StatsConfig &config, const nlohmann::json &stats_section);
    void _extract_uart_endpoints(Configuration &config, const nlohmann::json &uart_endpoints);
    void _extract_udp_endpoints(Configuration &config, const nlohmann::json &udp_endpoints);
    void _extract_tcp_endpoints(Configuration &config, const nlohmann::json &tcp_endpoints);

    // Filter configuration helpers
    template<typename EndpointConfig>
    void _extract_filter_config(const nlohmann::json &endpoint, EndpointConfig &config);
    
    void _extract_vector_field(const nlohmann::json &endpoint, const std::string &snake_case_key, 
                               const std::string &camel_case_key, std::vector<uint32_t> &target);
    
    // Safe JSON value extraction helpers
    template<typename T>
    T _safe_get_value(const nlohmann::json &obj, const std::string &key, const T &default_value);
    
    std::string _safe_get_string(const nlohmann::json &obj, const std::string &snake_key, 
                                const std::string &camel_key, const std::string &default_value = "");
    
    template<typename T>
    T _safe_get_number(const nlohmann::json &obj, const std::string &snake_key, 
                      const std::string &camel_key, const T &default_value);
};
