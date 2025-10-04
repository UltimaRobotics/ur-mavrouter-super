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

#include "json_config.h"
#include "log.h"
#include "util.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <errno.h>
#include <climits>
#include <stdexcept>
#include <cstring>
#include <sstream>

// Include nlohmann JSON library
#include "../../modules/nholmann/json.hpp"
using json = nlohmann::json;

// Forward declarations for config structures
#include "../logendpoint.h"
#include "../endpoint_stats.h"
#include "../endpoint.h"
#include "../config.h"

// Define default baudrate if not already defined
#ifndef DEFAULT_BAUDRATE
#define DEFAULT_BAUDRATE 115200U
#endif

int JsonConfig::parse(const std::string &filename)
{
    log_info("JsonConfig::parse() - Starting to parse JSON file: '%s'", filename.c_str());

    try {
        log_debug("JsonConfig::parse() - Attempting to open file: '%s'", filename.c_str());
        std::ifstream file(filename);
        if (!file.is_open()) {
            log_error("JsonConfig::parse() - Could not open JSON config file '%s' (%m)", filename.c_str());
            return -errno;
        }
        log_debug("JsonConfig::parse() - File opened successfully");

        // Check file size
        file.seekg(0, std::ios::end);
        std::streampos file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        log_debug("JsonConfig::parse() - File size: %ld bytes", (long)file_size);

        if (file_size == 0) {
            log_error("JsonConfig::parse() - JSON config file '%s' is empty", filename.c_str());
            return -EINVAL;
        }

        // Read file content for debugging
        std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        log_debug("JsonConfig::parse() - File content preview (first 200 chars): '%.200s%s'", 
                 file_content.c_str(), file_content.length() > 200 ? "..." : "");

        // Parse JSON using nlohmann library
        log_debug("JsonConfig::parse() - Starting JSON parsing with nlohmann library");
        try {
            _json_data = json::parse(file_content);
            log_debug("JsonConfig::parse() - JSON parsing completed successfully");
        } catch (const json::parse_error& parse_err) {
            log_error("JsonConfig::parse() - JSON parse error at byte %zu: %s", parse_err.byte, parse_err.what());
            log_error("JsonConfig::parse() - Error context: %s", file_content.substr(
                std::max(0, (int)parse_err.byte - 50), 100).c_str());
            return -EINVAL;
        }

        // Log parsed JSON structure
        log_debug("JsonConfig::parse() - Parsed JSON type: %s", _json_data.type_name());
        if (_json_data.is_object()) {
            log_debug("JsonConfig::parse() - JSON object has %zu top-level keys", _json_data.size());
            for (auto it = _json_data.begin(); it != _json_data.end(); ++it) {
                log_debug("JsonConfig::parse() - Top-level key: '%s' (type: %s)", 
                         it.key().c_str(), it.value().type_name());
            }
        }

        // Validate JSON structure
        log_debug("JsonConfig::parse() - Starting JSON structure validation");
        if (!validate_json_structure()) {
            log_error("JsonConfig::parse() - Invalid JSON structure in file '%s'", filename.c_str());
            return -EINVAL;
        }
        log_debug("JsonConfig::parse() - JSON structure validation passed");

        log_info("JsonConfig::parse() - Successfully parsed and validated JSON config file '%s'", filename.c_str());
        return 0;

    } catch (const std::exception& e) {
        log_error("JsonConfig::parse() - Unexpected error parsing JSON config file '%s': %s", 
                 filename.c_str(), e.what());
        return -EINVAL;
    }
}

int JsonConfig::extract_configuration(Configuration &config)
{
    log_info("JsonConfig::extract_configuration() - Starting configuration extraction");

    try {
        // Initialize configuration with safe defaults
        log_debug("JsonConfig::extract_configuration() - Initializing default configuration");
        _init_default_config(config);
        log_debug("JsonConfig::extract_configuration() - Default configuration initialized successfully");

        // Extract each section
        if (_json_data.contains("general")) {
            log_info("JsonConfig::extract_configuration() - Found 'general' section, extracting...");
            try {
                _extract_general_config(config, _json_data["general"]);
                log_debug("JsonConfig::extract_configuration() - General configuration extracted successfully");
            } catch (const std::exception& e) {
                log_error("JsonConfig::extract_configuration() - Error extracting general config: %s", e.what());
                throw;
            }
        } else {
            log_debug("JsonConfig::extract_configuration() - No 'general' section found, using defaults");
        }

        if (_json_data.contains("log")) {
            log_info("JsonConfig::extract_configuration() - Found 'log' section, extracting...");
            try {
                _extract_log_config(config, _json_data["log"]);
                log_debug("JsonConfig::extract_configuration() - Log configuration extracted successfully");
                log_debug("JsonConfig::extract_configuration() - Log config - logs_dir: '%s', log_mode: %d, fcu_id: %d", 
                         config.log_config.logs_dir.c_str(), (int)config.log_config.log_mode, config.log_config.fcu_id);
            } catch (const std::exception& e) {
                log_error("JsonConfig::extract_configuration() - Error extracting log config: %s", e.what());
                throw;
            }
        } else {
            log_debug("JsonConfig::extract_configuration() - No 'log' section found, using defaults");
        }

        if (_json_data.contains("statistics") || _json_data.contains("stats")) {
            std::string section_name = _json_data.contains("statistics") ? "statistics" : "stats";
            log_info("JsonConfig::extract_configuration() - Found '%s' section, extracting...", section_name.c_str());
            try {
                auto stats_section = _json_data.contains("statistics") ? 
                                    _json_data["statistics"] : _json_data["stats"];
                _extract_stats_config(config.stats_config, stats_section);
                log_debug("JsonConfig::extract_configuration() - Statistics configuration extracted successfully");
            } catch (const std::exception& e) {
                log_error("JsonConfig::extract_configuration() - Error extracting stats config: %s", e.what());
                throw;
            }
        } else {
            log_debug("JsonConfig::extract_configuration() - No 'statistics' or 'stats' section found, using defaults");
        }

        if (_json_data.contains("uart_endpoints")) {
            log_info("JsonConfig::extract_configuration() - Found 'uart_endpoints' section with %zu endpoints", 
                     _json_data["uart_endpoints"].size());
            try {
                _extract_uart_endpoints(config, _json_data["uart_endpoints"]);
                log_debug("JsonConfig::extract_configuration() - UART endpoints extracted successfully: %zu endpoints", 
                         config.uart_configs.size());
            } catch (const std::exception& e) {
                log_error("JsonConfig::extract_configuration() - Error extracting UART endpoints: %s", e.what());
                throw;
            }
        } else {
            log_debug("JsonConfig::extract_configuration() - No 'uart_endpoints' section found");
        }

        if (_json_data.contains("udp_endpoints")) {
            log_info("JsonConfig::extract_configuration() - Found 'udp_endpoints' section with %zu endpoints", 
                     _json_data["udp_endpoints"].size());
            try {
                _extract_udp_endpoints(config, _json_data["udp_endpoints"]);
                log_debug("JsonConfig::extract_configuration() - UDP endpoints extracted successfully: %zu endpoints", 
                         config.udp_configs.size());
            } catch (const std::exception& e) {
                log_error("JsonConfig::extract_configuration() - Error extracting UDP endpoints: %s", e.what());
                throw;
            }
        } else {
            log_debug("JsonConfig::extract_configuration() - No 'udp_endpoints' section found");
        }

        if (_json_data.contains("tcp_endpoints")) {
            log_info("JsonConfig::extract_configuration() - Found 'tcp_endpoints' section with %zu endpoints", 
                     _json_data["tcp_endpoints"].size());
            log_debug("JsonConfig::extract_configuration() - TCP config size before extraction: %zu", 
                     config.tcp_configs.size());
            try {
                _extract_tcp_endpoints(config, _json_data["tcp_endpoints"]);
                log_debug("JsonConfig::extract_configuration() - TCP endpoints extracted successfully: %zu endpoints", 
                         config.tcp_configs.size());
            } catch (const std::exception& e) {
                log_error("JsonConfig::extract_configuration() - Error extracting TCP endpoints: %s", e.what());
                throw;
            }
        } else {
            log_debug("JsonConfig::extract_configuration() - No 'tcp_endpoints' section found");
        }

        log_info("JsonConfig::extract_configuration() - Successfully extracted complete configuration");
        log_info("JsonConfig::extract_configuration() - Final summary: %zu UART, %zu UDP, %zu TCP endpoints", 
                config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());

        // Verify the actual endpoint configurations are populated
        log_debug("JsonConfig::extract_configuration() - Verification: UART configs has %zu entries", config.uart_configs.size());
        for (size_t i = 0; i < config.uart_configs.size(); i++) {
            log_debug("JsonConfig::extract_configuration() - UART[%zu]: name='%s', device='%s'", 
                     i, config.uart_configs[i].name.c_str(), config.uart_configs[i].device.c_str());
        }
        log_debug("JsonConfig::extract_configuration() - Verification: UDP configs has %zu entries", config.udp_configs.size());
        for (size_t i = 0; i < config.udp_configs.size(); i++) {
            log_debug("JsonConfig::extract_configuration() - UDP[%zu]: name='%s', address='%s', port=%lu", 
                     i, config.udp_configs[i].name.c_str(), config.udp_configs[i].address.c_str(), config.udp_configs[i].port);
        }
        log_debug("JsonConfig::extract_configuration() - Verification: TCP configs has %zu entries", config.tcp_configs.size());
        for (size_t i = 0; i < config.tcp_configs.size(); i++) {
            log_debug("JsonConfig::extract_configuration() - TCP[%zu]: name='%s', address='%s', port=%lu", 
                     i, config.tcp_configs[i].name.c_str(), config.tcp_configs[i].address.c_str(), config.tcp_configs[i].port);
        }

        return 0;

    } catch (const std::exception& e) {
        log_error("JsonConfig::extract_configuration() - Error extracting configuration: %s", e.what());
        return -EINVAL;
    }
}

bool JsonConfig::validate_json_structure() const
{
    try {
        // Validate top-level structure
        if (!_json_data.is_object()) {
            log_error("JSON root must be an object");
            return false;
        }

        // Validate general section if present
        if (_json_data.contains("general") && !_json_data["general"].is_object()) {
            log_error("'general' section must be an object");
            return false;
        }

        // Validate log section if present
        if (_json_data.contains("log") && !_json_data["log"].is_object()) {
            log_error("'log' section must be an object");
            return false;
        }

        // Validate endpoint arrays if present
        const std::vector<std::string> endpoint_sections = {
            "uart_endpoints", "udp_endpoints", "tcp_endpoints"
        };

        for (const auto& section : endpoint_sections) {
            if (_json_data.contains(section)) {
                if (!_json_data[section].is_array()) {
                    log_error("'%s' section must be an array", section.c_str());
                    return false;
                }

                for (const auto& endpoint : _json_data[section]) {
                    if (!endpoint.is_object()) {
                        log_error("Each endpoint in '%s' must be an object", section.c_str());
                        return false;
                    }
                }
            }
        }

        // Validate statistics section if present
        if (_json_data.contains("statistics") && !_json_data["statistics"].is_object()) {
            log_error("'statistics' section must be an object");
            return false;
        }

        if (_json_data.contains("stats") && !_json_data["stats"].is_object()) {
            log_error("'stats' section must be an object");
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        log_error("JSON validation error: %s", e.what());
        return false;
    }
}

int JsonConfig::extract_stats_config(EndpointStats::StatsConfig &config)
{
    try {
        if (_json_data.contains("statistics")) {
            _extract_stats_config(config, _json_data["statistics"]);
        } else if (_json_data.contains("stats")) {
            _extract_stats_config(config, _json_data["stats"]);
        }
        return 0;
    } catch (const std::exception& e) {
        log_error("Error extracting stats configuration: %s", e.what());
        return -EINVAL;
    }
}

void JsonConfig::_init_default_config(Configuration &config)
{
    log_debug("JsonConfig::_init_default_config() - Starting default configuration initialization");

    try {
        // Initialize log config fields with defaults
        config.log_config.logs_dir = "/tmp/mavlink-logs";
        config.log_config.log_mode = LogMode::disabled;
        config.log_config.mavlink_dialect = LogOptions::MavDialect::Auto;
        config.log_config.fcu_id = 1;
        config.log_config.min_free_space = 100;
        config.log_config.max_log_files = 10;
        config.log_config.log_telemetry = false;
        
        // Initialize extension_conf_dir to an empty string as a default
        config.extension_conf_dir = "";

        log_debug("JsonConfig::_init_default_config() - Default configuration initialized successfully");
    } catch (const std::exception& e) {
        log_error("JsonConfig::_init_default_config() - Error during default config initialization: %s", e.what());
        throw;
    }
}

void JsonConfig::_extract_general_config(Configuration &config, const json &general_section)
{
    try {
        if (general_section.contains("tcp_server_port") || general_section.contains("TcpServerPort")) {
            auto key = general_section.contains("tcp_server_port") ? "tcp_server_port" : "TcpServerPort";
            config.tcp_port = general_section[key].get<unsigned long>();
        }

        if (general_section.contains("report_stats") || general_section.contains("ReportStats")) {
            auto key = general_section.contains("report_stats") ? "report_stats" : "ReportStats";
            config.report_msg_statistics = general_section[key].get<bool>();
        }

        if (general_section.contains("debug_log_level") || general_section.contains("DebugLogLevel")) {
            auto key = general_section.contains("debug_log_level") ? "debug_log_level" : "DebugLogLevel";
            std::string level = general_section[key].get<std::string>();

            if (level == "error") config.debug_log_level = Log::Level::ERROR;
            else if (level == "warning") config.debug_log_level = Log::Level::WARNING;
            else if (level == "info") config.debug_log_level = Log::Level::INFO;
            else if (level == "debug") config.debug_log_level = Log::Level::DEBUG;
            else if (level == "trace") config.debug_log_level = Log::Level::TRACE;
            else log_warning("Invalid debug log level: %s, using default", level.c_str());
        }

        if (general_section.contains("deduplication_period") || general_section.contains("DeduplicationPeriod")) {
            auto key = general_section.contains("deduplication_period") ? "deduplication_period" : "DeduplicationPeriod";
            config.dedup_period_ms = general_section[key].get<unsigned long>();
        }

        if (general_section.contains("sniffer_sysid") || general_section.contains("SnifferSysid")) {
            auto key = general_section.contains("sniffer_sysid") ? "sniffer_sysid" : "SnifferSysid";
            config.sniffer_sysid = general_section[key].get<unsigned long>();
        }

        if (general_section.contains("extension_conf_dir")) {
            config.extension_conf_dir = general_section["extension_conf_dir"].get<std::string>();
        }

        log_debug("JsonConfig::extract_configuration() - General configuration extracted");
    } catch (const std::exception& e) {
        log_error("Error extracting general configuration: %s", e.what());
        throw;
    }
}

void JsonConfig::_extract_log_config(Configuration &config, const json &log_section)
{
    log_debug("JsonConfig::_extract_log_config() - Starting log configuration extraction");

    try {
        log_debug("JsonConfig::_extract_log_config() - Log section type: %s, size: %zu", 
                 log_section.type_name(), log_section.size());

        if (log_section.contains("logs_dir")) {
            std::string logs_dir = log_section["logs_dir"].get<std::string>();
            log_debug("JsonConfig::_extract_log_config() - Found logs_dir: '%s'", logs_dir.c_str());
            config.log_config.logs_dir = logs_dir;
            log_debug("JsonConfig::_extract_log_config() - Set logs_dir successfully");
        } else {
            log_debug("JsonConfig::_extract_log_config() - No logs_dir found, keeping default: '%s'", 
                     config.log_config.logs_dir.c_str());
        }

        if (log_section.contains("log_mode") || log_section.contains("LogMode")) {
            auto key = log_section.contains("log_mode") ? "log_mode" : "LogMode";
            log_debug("JsonConfig::_extract_log_config() - Found log mode key: '%s'", key);
            std::string mode = log_section[key].get<std::string>();
            log_debug("JsonConfig::_extract_log_config() - Log mode value: '%s'", mode.c_str());

            if (mode == "always") {
                config.log_config.log_mode = LogMode::always;
                log_debug("JsonConfig::_extract_log_config() - Set log_mode to always");
            } else if (mode == "while_armed") {
                config.log_config.log_mode = LogMode::while_armed;
                log_debug("JsonConfig::_extract_log_config() - Set log_mode to while_armed");
            } else if (mode == "disabled") {
                config.log_config.log_mode = LogMode::disabled;
                log_debug("JsonConfig::_extract_log_config() - Set log_mode to disabled");
            } else {
                log_warning("JsonConfig::_extract_log_config() - Invalid log mode: %s, using disabled", mode.c_str());
                config.log_config.log_mode = LogMode::disabled;
            }
        } else {
            log_debug("JsonConfig::_extract_log_config() - No log_mode found, keeping default");
        }

        if (log_section.contains("mavlink_dialect") || log_section.contains("MavlinkDialect")) {
            auto key = log_section.contains("mavlink_dialect") ? "mavlink_dialect" : "MavlinkDialect";
            log_debug("JsonConfig::_extract_log_config() - Found mavlink dialect key: '%s'", key);
            std::string dialect = log_section[key].get<std::string>();
            log_debug("JsonConfig::_extract_log_config() - Mavlink dialect value: '%s'", dialect.c_str());

            if (dialect == "Auto") {
                config.log_config.mavlink_dialect = LogOptions::MavDialect::Auto;
                log_debug("JsonConfig::_extract_log_config() - Set mavlink_dialect to Auto");
            } else if (dialect == "Common") {
                config.log_config.mavlink_dialect = LogOptions::MavDialect::Common;
                log_debug("JsonConfig::_extract_log_config() - Set mavlink_dialect to Common");
            } else if (dialect == "ArduPilot") {
                config.log_config.mavlink_dialect = LogOptions::MavDialect::Ardupilotmega;
                log_debug("JsonConfig::_extract_log_config() - Set mavlink_dialect to ArduPilot");
            } else {
                log_warning("JsonConfig::_extract_log_config() - Invalid mavlink dialect: %s, using Auto", dialect.c_str());
                config.log_config.mavlink_dialect = LogOptions::MavDialect::Auto;
            }
        } else {
            log_debug("JsonConfig::_extract_log_config() - No mavlink_dialect found, keeping default");
        }

        if (log_section.contains("log_system_id") || log_section.contains("LogSystemId")) {
            auto key = log_section.contains("log_system_id") ? "log_system_id" : "LogSystemId";
            log_debug("JsonConfig::_extract_log_config() - Found log system id key: '%s'", key);
            int fcu_id = log_section[key].get<int>();
            log_debug("JsonConfig::_extract_log_config() - Log system id value: %d", fcu_id);
            config.log_config.fcu_id = fcu_id;
            log_debug("JsonConfig::_extract_log_config() - Set fcu_id successfully");
        } else {
            log_debug("JsonConfig::_extract_log_config() - No log_system_id found, keeping default: %d", 
                     config.log_config.fcu_id);
        }

        if (log_section.contains("min_free_space") || log_section.contains("MinFreeSpace")) {
            auto key = log_section.contains("min_free_space") ? "min_free_space" : "MinFreeSpace";
            log_debug("JsonConfig::_extract_log_config() - Found min free space key: '%s'", key);
            unsigned long min_free_space = log_section[key].get<unsigned long>();
            log_debug("JsonConfig::_extract_log_config() - Min free space value: %lu", min_free_space);
            config.log_config.min_free_space = min_free_space;
            log_debug("JsonConfig::_extract_log_config() - Set min_free_space successfully");
        } else {
            log_debug("JsonConfig::_extract_log_config() - No min_free_space found, keeping default: %lu", 
                     config.log_config.min_free_space);
        }

        if (log_section.contains("max_log_files") || log_section.contains("MaxLogFiles")) {
            auto key = log_section.contains("max_log_files") ? "max_log_files" : "MaxLogFiles";
            log_debug("JsonConfig::_extract_log_config() - Found max log files key: '%s'", key);
            unsigned long max_log_files = log_section[key].get<unsigned long>();
            log_debug("JsonConfig::_extract_log_config() - Max log files value: %lu", max_log_files);
            config.log_config.max_log_files = max_log_files;
            log_debug("JsonConfig::_extract_log_config() - Set max_log_files successfully");
        } else {
            log_debug("JsonConfig::_extract_log_config() - No max_log_files found, keeping default: %lu", 
                     config.log_config.max_log_files);
        }

        if (log_section.contains("log_telemetry") || log_section.contains("LogTelemetry")) {
            auto key = log_section.contains("log_telemetry") ? "log_telemetry" : "LogTelemetry";
            log_debug("JsonConfig::_extract_log_config() - Found log telemetry key: '%s'", key);
            bool log_telemetry = log_section[key].get<bool>();
            log_debug("JsonConfig::_extract_log_config() - Log telemetry value: %s", log_telemetry ? "true" : "false");
            config.log_config.log_telemetry = log_telemetry;
            log_debug("JsonConfig::_extract_log_config() - Set log_telemetry successfully");
        } else {
            log_debug("JsonConfig::_extract_log_config() - No log_telemetry found, keeping default: %s", 
                     config.log_config.log_telemetry ? "true" : "false");
        }

        log_debug("JsonConfig::_extract_log_config() - Log configuration extraction completed successfully");

    } catch (const std::exception& e) {
        log_error("JsonConfig::_extract_log_config() - Error extracting log configuration: %s", e.what());
        throw;
    }
}

void JsonConfig::_extract_stats_config(EndpointStats::StatsConfig &config, const json &stats_section)
{
    try {
        if (stats_section.contains("enable_connection_health")) {
            config.enable_connection_health = stats_section["enable_connection_health"].get<bool>();
        }
        if (stats_section.contains("enable_message_stats")) {
            config.enable_message_stats = stats_section["enable_message_stats"].get<bool>();
        }
        if (stats_section.contains("enable_performance_metrics")) {
            config.enable_performance_metrics = stats_section["enable_performance_metrics"].get<bool>();
        }
        if (stats_section.contains("enable_filtering_stats")) {
            config.enable_filtering_stats = stats_section["enable_filtering_stats"].get<bool>();
        }
        if (stats_section.contains("enable_resource_stats")) {
            config.enable_resource_stats = stats_section["enable_resource_stats"].get<bool>();
        }
        if (stats_section.contains("enable_uart_stats")) {
            config.enable_uart_stats = stats_section["enable_uart_stats"].get<bool>();
        }
        if (stats_section.contains("enable_udp_stats")) {
            config.enable_udp_stats = stats_section["enable_udp_stats"].get<bool>();
        }
        if (stats_section.contains("enable_tcp_stats")) {
            config.enable_tcp_stats = stats_section["enable_tcp_stats"].get<bool>();
        }
        if (stats_section.contains("periodic_collection_interval_ms")) {
            config.periodic_collection_interval_ms = stats_section["periodic_collection_interval_ms"].get<unsigned long>();
        }
        if (stats_section.contains("error_cleanup_interval_ms")) {
            config.error_cleanup_interval_ms = stats_section["error_cleanup_interval_ms"].get<unsigned long>();
        }
        if (stats_section.contains("statistics_report_interval_ms")) {
            config.statistics_report_interval_ms = stats_section["statistics_report_interval_ms"].get<unsigned long>();
        }
        if (stats_section.contains("resource_check_interval_ms")) {
            config.resource_check_interval_ms = stats_section["resource_check_interval_ms"].get<unsigned long>();
        }

        // JSON file output configuration
        if (stats_section.contains("enable_json_file_output")) {
            config.enable_json_file_output = stats_section["enable_json_file_output"].get<bool>();
            log_info("JsonConfig::_extract_stats_config() - JSON file output enabled: %s", 
                     config.enable_json_file_output ? "true" : "false");
        }
        if (stats_section.contains("json_output_file_path")) {
            config.json_output_file_path = stats_section["json_output_file_path"].get<std::string>();
            log_info("JsonConfig::_extract_stats_config() - JSON output file path: '%s'", 
                     config.json_output_file_path.c_str());

            // Validate file path
            if (config.json_output_file_path.empty()) {
                log_warning("JsonConfig::_extract_stats_config() - JSON output file path is empty");
            } else if (config.json_output_file_path.find("..") != std::string::npos) {
                log_warning("JsonConfig::_extract_stats_config() - JSON output file path contains '..' which may be unsafe: '%s'", 
                           config.json_output_file_path.c_str());
            }
        }
        if (stats_section.contains("json_file_write_interval_ms")) {
            config.json_file_write_interval_ms = stats_section["json_file_write_interval_ms"].get<unsigned long>();
            log_info("JsonConfig::_extract_stats_config() - JSON file write interval: %lu ms (%.1f seconds)", 
                     config.json_file_write_interval_ms, config.json_file_write_interval_ms / 1000.0);

            if (config.json_file_write_interval_ms < 1000) {
                log_warning("JsonConfig::_extract_stats_config() - JSON file write interval is very frequent (%lu ms), this may impact performance", 
                           config.json_file_write_interval_ms);
            }
        }

        // Log complete JSON file output configuration
        if (config.enable_json_file_output) {
            log_info("JsonConfig::_extract_stats_config() - JSON File Output Configuration Summary:");
            log_info("  - Enabled: true");
            log_info("  - File Path: '%s'", config.json_output_file_path.c_str());
            log_info("  - Write Interval: %lu ms", config.json_file_write_interval_ms);
        } else {
            log_debug("JsonConfig::_extract_stats_config() - JSON file output is disabled");
        }

    } catch (const std::exception& e) {
        log_error("Error extracting stats configuration: %s", e.what());
        throw;
    }
}

void JsonConfig::_extract_uart_endpoints(Configuration &config, const json &uart_endpoints)
{
    try {
        for (const auto &endpoint : uart_endpoints) {
            UartEndpointConfig uart_config = {};

            // Initialize with safe defaults
            uart_config.name = "";
            uart_config.device = "";
            uart_config.flowcontrol = false;
            uart_config.group = "";

            if (endpoint.contains("name")) {
                uart_config.name = endpoint["name"].get<std::string>();
            }

            if (endpoint.contains("device")) {
                uart_config.device = endpoint["device"].get<std::string>();
            }

            if (endpoint.contains("baud") || endpoint.contains("baudrates")) {
                auto key = endpoint.contains("baud") ? "baud" : "baudrates";
                auto baud_value = endpoint[key];

                if (baud_value.is_array()) {
                    for (const auto &rate : baud_value) {
                        uart_config.baudrates.push_back(rate.get<uint32_t>());
                    }
                } else if (baud_value.is_string()) {
                    // Parse comma-separated string
                    std::string baud_str = baud_value.get<std::string>();
                    std::stringstream ss(baud_str);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        uart_config.baudrates.push_back(std::stoul(item));
                    }
                } else {
                    uart_config.baudrates.push_back(baud_value.get<uint32_t>());
                }
            }

            if (endpoint.contains("flow_control") || endpoint.contains("FlowControl")) {
                auto key = endpoint.contains("flow_control") ? "flow_control" : "FlowControl";
                uart_config.flowcontrol = endpoint[key].get<bool>();
            }

            if (endpoint.contains("group")) {
                uart_config.group = endpoint["group"].get<std::string>();
            }

            // Extract filter configurations
            _extract_filter_config(endpoint, uart_config);

            // Validate and set defaults
            if (uart_config.name.empty()) {
                uart_config.name = "json_uart_" + std::to_string(config.uart_configs.size());
            }

            if (uart_config.baudrates.empty()) {
                uart_config.baudrates.push_back(DEFAULT_BAUDRATE);
            }

            if (!uart_config.device.empty()) {
                config.uart_configs.push_back(uart_config);
                log_info("Added UART endpoint: %s on %s", uart_config.name.c_str(), uart_config.device.c_str());
                log_debug("JsonConfig::_extract_uart_endpoints() - Total UART endpoints after addition: %zu", 
                         config.uart_configs.size());
            } else {
                log_warning("Skipping UART endpoint with empty device");
            }
        }

    } catch (const std::exception& e) {
        log_error("Error extracting UART endpoints: %s", e.what());
        throw;
    }
}

void JsonConfig::_extract_udp_endpoints(Configuration &config, const json &udp_endpoints)
{
    try {
        for (const auto &endpoint : udp_endpoints) {
            UdpEndpointConfig udp_config = {};
            udp_config.mode = UdpEndpointConfig::Mode::Client; // Default

            // Initialize with safe defaults
            udp_config.name = "";
            udp_config.address = "";
            udp_config.port = 0;
            udp_config.group = "";

            if (endpoint.contains("name")) {
                udp_config.name = endpoint["name"].get<std::string>();
            }

            if (endpoint.contains("address")) {
                udp_config.address = endpoint["address"].get<std::string>();
            }

            if (endpoint.contains("port")) {
                udp_config.port = endpoint["port"].get<unsigned long>();
            }

            if (endpoint.contains("mode")) {
                std::string mode = endpoint["mode"].get<std::string>();
                if (mode == "client" || mode == "normal") {
                    udp_config.mode = UdpEndpointConfig::Mode::Client;
                } else if (mode == "server" || mode == "eavesdropping") {
                    udp_config.mode = UdpEndpointConfig::Mode::Server;
                } else {
                    log_warning("Invalid UDP mode: %s, using client", mode.c_str());
                    udp_config.mode = UdpEndpointConfig::Mode::Client;
                }
            }

            if (endpoint.contains("group")) {
                udp_config.group = endpoint["group"].get<std::string>();
            }

            // Extract filter configurations
            _extract_filter_config(endpoint, udp_config);

            // Validate and set defaults
            if (udp_config.name.empty()) {
                udp_config.name = "json_udp_" + std::to_string(config.udp_configs.size());
            }

            if (!udp_config.address.empty() && udp_config.port > 0) {
                config.udp_configs.push_back(udp_config);
                log_info("Added UDP endpoint: %s at %s:%lu", udp_config.name.c_str(), 
                        udp_config.address.c_str(), udp_config.port);
                log_debug("JsonConfig::_extract_udp_endpoints() - Total UDP endpoints after addition: %zu", 
                         config.udp_configs.size());
            } else {
                log_warning("Skipping UDP endpoint with invalid address or port: '%s':%lu", 
                           udp_config.address.c_str(), udp_config.port);
            }
        }

    } catch (const std::exception& e) {
        log_error("Error extracting UDP endpoints: %s", e.what());
        throw;
    }
}

void JsonConfig::_extract_tcp_endpoints(Configuration &config, const json &tcp_endpoints)
{
    try {
        for (const auto &endpoint : tcp_endpoints) {
            TcpEndpointConfig tcp_config = {};

            // Initialize with safe defaults
            tcp_config.name = "";
            tcp_config.address = "";
            tcp_config.port = 0;
            tcp_config.retry_timeout = 5000;
            tcp_config.group = "";

            if (endpoint.contains("name")) {
                tcp_config.name = endpoint["name"].get<std::string>();
            }

            if (endpoint.contains("address")) {
                tcp_config.address = endpoint["address"].get<std::string>();
            }

            if (endpoint.contains("port")) {
                tcp_config.port = endpoint["port"].get<unsigned long>();
            }

            if (endpoint.contains("retry_timeout") || endpoint.contains("RetryTimeout")) {
                auto key = endpoint.contains("retry_timeout") ? "retry_timeout" : "RetryTimeout";
                tcp_config.retry_timeout = endpoint[key].get<unsigned long>();
            }

            if (endpoint.contains("group")) {
                tcp_config.group = endpoint["group"].get<std::string>();
            }

            // Extract filter configurations
            _extract_filter_config(endpoint, tcp_config);

            // Validate and set defaults
            if (tcp_config.name.empty()) {
                tcp_config.name = "json_tcp_" + std::to_string(config.tcp_configs.size());
            }

            if (!tcp_config.address.empty() && tcp_config.port > 0) {
                config.tcp_configs.push_back(tcp_config);
                log_info("Added TCP endpoint: %s at %s:%lu", tcp_config.name.c_str(), 
                        tcp_config.address.c_str(), tcp_config.port);
                log_debug("JsonConfig::_extract_tcp_endpoints() - Total TCP endpoints after addition: %zu", 
                         config.tcp_configs.size());
            } else {
                log_warning("Skipping TCP endpoint with invalid address or port: '%s':%lu", 
                           tcp_config.address.c_str(), tcp_config.port);
            }
        }

    } catch (const std::exception& e) {
        log_error("Error extracting TCP endpoints: %s", e.what());
        throw;
    }
}

template<typename EndpointConfig>
void JsonConfig::_extract_filter_config(const json &endpoint, EndpointConfig &config)
{
    // Extract message ID filters
    _extract_vector_field(endpoint, "allow_msg_id_out", "AllowMsgIdOut", config.allow_msg_id_out);
    _extract_vector_field(endpoint, "block_msg_id_out", "BlockMsgIdOut", config.block_msg_id_out);
    _extract_vector_field(endpoint, "allow_msg_id_in", "AllowMsgIdIn", config.allow_msg_id_in);
    _extract_vector_field(endpoint, "block_msg_id_in", "BlockMsgIdIn", config.block_msg_id_in);

    // Extract component ID filters (using proper type casting)
    std::vector<uint32_t> temp_vec;

    _extract_vector_field(endpoint, "allow_src_comp_out", "AllowSrcCompOut", temp_vec);
    for (auto val : temp_vec) config.allow_src_comp_out.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    _extract_vector_field(endpoint, "block_src_comp_out", "BlockSrcCompOut", temp_vec);
    for (auto val : temp_vec) config.block_src_comp_out.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    _extract_vector_field(endpoint, "allow_src_comp_in", "AllowSrcCompIn", temp_vec);
    for (auto val : temp_vec) config.allow_src_comp_in.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    _extract_vector_field(endpoint, "block_src_comp_in", "BlockSrcCompIn", temp_vec);
    for (auto val : temp_vec) config.block_src_comp_in.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    // Extract system ID filters
    _extract_vector_field(endpoint, "allow_src_sys_out", "AllowSrcSysOut", temp_vec);
    for (auto val : temp_vec) config.allow_src_sys_out.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    _extract_vector_field(endpoint, "block_src_sys_out", "BlockSrcSysOut", temp_vec);
    for (auto val : temp_vec) config.block_src_sys_out.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    _extract_vector_field(endpoint, "allow_src_sys_in", "AllowSrcSysIn", temp_vec);
    for (auto val : temp_vec) config.allow_src_sys_in.push_back(static_cast<uint8_t>(val));
    temp_vec.clear();

    _extract_vector_field(endpoint, "block_src_sys_in", "BlockSrcSysIn", temp_vec);
    for (auto val : temp_vec) config.block_src_sys_in.push_back(static_cast<uint8_t>(val));
}

void JsonConfig::_extract_vector_field(const json &endpoint, const std::string &snake_case_key, 
                                      const std::string &camel_case_key, std::vector<uint32_t> &target)
{
    std::string key;
    if (endpoint.contains(snake_case_key)) {
        key = snake_case_key;
    } else if (endpoint.contains(camel_case_key)) {
        key = camel_case_key;
    } else {
        return; // Key not found
    }

    try {
        auto value = endpoint[key];
        if (value.is_array()) {
            for (const auto &item : value) {
                if (item.is_number_unsigned()) {
                    target.push_back(item.get<uint32_t>());
                } else if (item.is_string()) {
                    target.push_back(std::stoul(item.get<std::string>()));
                }
            }
        } else if (value.is_string()) {
            // Parse comma-separated string
            std::string str_value = value.get<std::string>();
            std::stringstream ss(str_value);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // Trim whitespace
                item.erase(0, item.find_first_not_of(" \t"));
                item.erase(item.find_last_not_of(" \t") + 1);
                if (!item.empty()) {
                    target.push_back(std::stoul(item));
                }
            }
        } else if (value.is_number()) {
            target.push_back(value.get<uint32_t>());
        }
    } catch (const std::exception& e) {
        log_warning("Error parsing vector field '%s': %s", key.c_str(), e.what());
    }
}

template<typename T>
T JsonConfig::_safe_get_value(const json &obj, const std::string &key, const T &default_value)
{
    try {
        if (obj.contains(key)) {
            return obj[key].get<T>();
        }
    } catch (const std::exception& e) {
        log_warning("Error getting value for key '%s': %s, using default", key.c_str(), e.what());
    }
    return default_value;
}

std::string JsonConfig::_safe_get_string(const json &obj, const std::string &snake_key, 
                                        const std::string &camel_key, const std::string &default_value)
{
    if (obj.contains(snake_key)) {
        return _safe_get_value(obj, snake_key, default_value);
    } else if (obj.contains(camel_key)) {
        return _safe_get_value(obj, camel_key, default_value);
    }
    return default_value;
}

template<typename T>
T JsonConfig::_safe_get_number(const json &obj, const std::string &snake_key, 
                              const std::string &camel_key, const T &default_value)
{
    if (obj.contains(snake_key)) {
        return _safe_get_value(obj, snake_key, default_value);
    } else if (obj.contains(camel_key)) {
        return _safe_get_value(obj, camel_key, default_value);
    }
    return default_value;
}