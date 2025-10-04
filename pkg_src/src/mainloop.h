/*
 * This file is part of the MAVLink Router project
 *
 * Copyright (C) 2016  Intel Corporation. All rights reserved.
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

#include <memory>
#include <string>
#include <vector>

#include "common/log.h"

#include "binlog.h"
#include "comm.h"
#include "config.h"  // Use the canonical Configuration definition from config.h
#include "dedup.h"
#include "endpoint.h"
#include "endpoint_stats.h"
#include "timeout.h"
#include "ulog.h"
#include <map>

// Configuration struct is defined in config.h to avoid ABI mismatches between translation units

struct endpoint_entry {
    struct endpoint_entry *next;
    TcpEndpoint *endpoint;
};

class Mainloop {
public:
    int open();
    int add_fd(int fd, void *data, int events) const;
    int mod_fd(int fd, void *data, int events) const;
    int remove_fd(int fd) const;
    int loop();
    void route_msg(struct buffer *buf);
    void handle_tcp_connection();
    int write_msg(const std::shared_ptr<Endpoint> &e, const struct buffer *buf) const;
    void process_tcp_hangups();
    Timeout *add_timeout(uint32_t timeout_msec, std::function<bool(void *)> cb, const void *data);
    void del_timeout(Timeout *t);
    void mod_timeout(Timeout *t, uint32_t timeout_msec);

    bool add_endpoints(const Configuration &config);
    void clear_endpoints();
    
    // FD tracking for extension threads
    void track_fd(int fd, const std::string& description);
    void untrack_fd(int fd);
    void force_close_all_tracked_fds();

    /*
     * Returns true, if the message was already received earlier
     */
    bool dedup_check_msg(const buffer *buf);

    void print_statistics();

    int epollfd = -1;
    bool should_process_tcp_hangups = false;
    
    // Per-instance exit flag for independent mainloop control
    std::atomic<bool> _should_exit{false};
    
    // Make endpoints accessible to ExtensionManager for cleanup
    std::vector<std::shared_ptr<Endpoint>> g_endpoints{};

    /*
     * Return singleton for this class, tied to the main thread. It needds to
     * be called after a call to Mainloop::init().
     */
    static Mainloop &get_instance()
    {
        assert(_initialized);
        return _instance;
    }

    /*
     * Initialize and return singleton (for main thread).
     */
    static Mainloop &init();

    /*
     * De-initialize singleton so we can start a fresh on the same
     * thread
     */
    static void teardown();

    /*
     * Returns the current thread's mainloop instance.
     * For the main thread, returns the singleton.
     * For extension threads, returns their thread-local instance.
     * This is the CORRECT method to use for thread-safe access.
     */
    static Mainloop &instance();

    /*
     * Create a new independent Mainloop instance for extension threads.
     * This bypasses the singleton pattern and allows multiple instances.
     * Each extension thread gets its own dedup system to prevent contention.
     */
    static Mainloop* create_extension_instance();

    /*
     * Destroy an extension Mainloop instance created with create_extension_instance().
     */
    static void destroy_extension_instance(Mainloop* instance);
    
    /*
     * Set the current thread's mainloop instance.
     * Called by extension threads to register their mainloop.
     */
    static void set_thread_instance(Mainloop* instance);

    /*
     * Request that loop exits on next iteration.
     */
    void request_exit(int retcode);

private:
    static const unsigned int LOG_AGGREGATE_INTERVAL_SEC = 5;

    int g_tcp_fd = -1; ///< for TCP server
    std::shared_ptr<LogEndpoint> _log_endpoint{nullptr};

    Timeout *_timeouts = nullptr;

    Dedup _msg_dedup{0}; // disabled by default

    struct {
        uint32_t msg_to_unknown = 0;
    } _errors_aggregate;

    int _retcode;

    int tcp_open(unsigned long tcp_port);
    void _del_timeouts();
    bool _retry_timeout_cb(void *data);
    bool _log_aggregate_timeout(void *data);
    
    // FD tracking for extension threads
    std::map<int, std::string> _tracked_fds;
    std::mutex _tracked_fds_mutex;

    Mainloop() = default;
    Mainloop(const Mainloop &) = delete;
    Mainloop &operator=(const Mainloop &) = delete;

    static Mainloop _instance;
    static bool _initialized;
};
