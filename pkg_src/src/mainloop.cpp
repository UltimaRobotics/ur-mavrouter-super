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
#include "mainloop.h"

#include <assert.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <atomic>
#include <memory>

#include <common/log.h>
#include <common/util.h>

#include "autolog.h"
#include "tlog.h"

Mainloop Mainloop::_instance{};
bool Mainloop::_initialized = false;

// Thread-local storage for extension thread mainloop instances
// Main thread uses nullptr (uses singleton)
static thread_local Mainloop* _thread_local_instance = nullptr;

static void exit_signal_handler(int signum)
{
    // Only signal exit for the main router singleton
    Mainloop::get_instance().request_exit(0);
}

static void setup_signal_handlers()
{
    struct sigaction sa = {};

    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = exit_signal_handler;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
}

Mainloop &Mainloop::init()
{
    assert(_initialized == false);

    _initialized = true;

    return _instance;
}

Mainloop* Mainloop::create_extension_instance()
{
    log_info("Creating new Mainloop instance for extension thread");
    
    // Allocate a new Mainloop instance on the heap
    Mainloop* extension_loop = new Mainloop();
    
    // Initialize its state (similar to what the singleton does, but independent)
    extension_loop->epollfd = -1;
    extension_loop->g_tcp_fd = -1;
    extension_loop->should_process_tcp_hangups = false;
    extension_loop->_timeouts = nullptr;
    extension_loop->_retcode = -1;
    extension_loop->_errors_aggregate.msg_to_unknown = 0;
    
    // CRITICAL: Initialize the per-instance exit flag
    extension_loop->_should_exit.store(false, std::memory_order_relaxed);
    
    // IMPORTANT: Each extension mainloop has its own independent Dedup instance
    // This is already initialized by the default constructor, but we document it here
    // extension_loop->_msg_dedup is a separate instance with its own mutex
    
    log_info("Created independent Mainloop instance at %p for extension thread", extension_loop);
    log_info("Extension mainloop has independent exit flag and dedup system");
    
    return extension_loop;
}

void Mainloop::destroy_extension_instance(Mainloop* instance)
{
    if (!instance) {
        log_warning("Attempted to destroy null Mainloop extension instance");
        return;
    }
    
    log_info("Destroying Mainloop extension instance at %p - using proper cleanup sequence", instance);
    
    // STEP 1: Close all endpoint FDs properly (this will untrack them)
    log_info("Closing %zu endpoints", instance->g_endpoints.size());
    for (auto &endpoint : instance->g_endpoints) {
        if (endpoint && endpoint->fd >= 0) {
            log_debug("Closing endpoint '%s' FD %d", endpoint->get_name().c_str(), endpoint->fd);
            
            // Remove from epoll first if epollfd is still valid
            if (instance->epollfd >= 0) {
                instance->remove_fd(endpoint->fd);
            }
            
            // Untrack the FD
            instance->untrack_fd(endpoint->fd);
            
            // Close the endpoint's socket fd
            close(endpoint->fd);
            endpoint->fd = -1;
        }
    }
    instance->g_endpoints.clear();
    log_info("Cleared endpoints vector");
    
    // STEP 2: Reset log endpoint
    instance->_log_endpoint.reset();
    
    // STEP 3: Clean up all timeouts (untrack their FDs)
    int freedTimeouts = 0;
    while (instance->_timeouts != nullptr) {
        Timeout *current = instance->_timeouts;
        instance->_timeouts = current->next;
        if (current->fd >= 0) {
            if (instance->epollfd >= 0) {
                instance->remove_fd(current->fd);
            }
            instance->untrack_fd(current->fd);
            close(current->fd);
        }
        delete current;
        freedTimeouts++;
    }
    log_info("Freed %d timeout structures", freedTimeouts);
    
    // STEP 4: Close TCP server if it's open
    if (instance->g_tcp_fd >= 0) {
        if (instance->epollfd >= 0) {
            instance->remove_fd(instance->g_tcp_fd);
        }
        instance->untrack_fd(instance->g_tcp_fd);
        close(instance->g_tcp_fd);
        instance->g_tcp_fd = -1;
        log_info("Closed TCP server FD");
    }
    
    // STEP 5: Close epoll file descriptor (do this last)
    if (instance->epollfd >= 0) {
        instance->untrack_fd(instance->epollfd);
        close(instance->epollfd);
        instance->epollfd = -1;
        log_info("Closed epoll FD");
    }
    
    // STEP 6: Force close any remaining tracked FDs (should be none at this point)
    instance->force_close_all_tracked_fds();
    
    // STEP 7: Delete the instance
    delete instance;
    
    log_info("Destroyed Mainloop extension instance - all resources properly cleaned up");
}

void Mainloop::teardown()
{
    // CRITICAL: Reset the instance exit flag to allow restart
    _instance._should_exit.store(false, std::memory_order_relaxed);
    
    // Close all endpoint file descriptors before clearing them
    for (auto &endpoint : _instance.g_endpoints) {
        if (endpoint && endpoint->fd >= 0) {
            // Remove from epoll first if epollfd is still valid
            if (_instance.epollfd >= 0) {
                _instance.remove_fd(endpoint->fd);
            }
            // Close the endpoint's socket fd
            close(endpoint->fd);
            endpoint->fd = -1;
        }
    }
    
    // Clear all endpoints
    _instance.g_endpoints.clear();
    
    // Reset log endpoint
    _instance._log_endpoint.reset();
    
    // Close TCP server socket if it's open
    if (_instance.g_tcp_fd >= 0) {
        if (_instance.epollfd >= 0) {
            _instance.remove_fd(_instance.g_tcp_fd);
        }
        _instance.untrack_fd(_instance.g_tcp_fd);
        close(_instance.g_tcp_fd);
        _instance.g_tcp_fd = -1;
    }
    
    // Clean up all timeouts
    while (_instance._timeouts != nullptr) {
        Timeout *current = _instance._timeouts;
        _instance._timeouts = current->next;
        if (current->fd >= 0) {
            if (_instance.epollfd >= 0) {
                _instance.remove_fd(current->fd);
            }
            _instance.untrack_fd(current->fd);
            close(current->fd);
        }
        delete current;
    }
    
    // Close epoll file descriptor if it's open (do this last)
    if (_instance.epollfd >= 0) {
        _instance.untrack_fd(_instance.epollfd);
        close(_instance.epollfd);
    }
    
    // CRITICAL: Reset epollfd to -1 to allow subsequent open() calls
    _instance.epollfd = -1;
    
    // Reset error aggregates
    _instance._errors_aggregate.msg_to_unknown = 0;
    
    // Reset return code
    _instance._retcode = -1;
    
    // Reset process hangups flag
    _instance.should_process_tcp_hangups = false;
    
    _initialized = false;
}

Mainloop &Mainloop::instance()
{
    // If this is an extension thread with its own instance, return that
    if (_thread_local_instance != nullptr) {
        return *_thread_local_instance;
    }
    
    // Otherwise return the singleton (main thread)
    return _instance;
}

void Mainloop::set_thread_instance(Mainloop* instance)
{
    _thread_local_instance = instance;
}

void Mainloop::request_exit(int retcode)
{
    // Each mainloop instance has its own _should_exit flag
    // This ensures extension threads don't affect the main router
    if (this == &_instance) {
        log_info("Main router requesting exit (retcode=%d)", retcode);
    } else {
        log_info("Extension mainloop %p requesting exit (retcode=%d)", this, retcode);
    }
    
    _retcode = retcode;
    _should_exit.store(true, std::memory_order_relaxed);
    
    log_info("Exit flag set for mainloop instance %p", this);
}

int Mainloop::open()
{
    _retcode = -1;

    if (epollfd != -1) {
        return -EBUSY;
    }

    epollfd = epoll_create1(EPOLL_CLOEXEC);

    if (epollfd == -1) {
        log_error("%m");
        return -1;
    }
    
    track_fd(epollfd, "EPOLL");

    _retcode = 0;

    return 0;
}

int Mainloop::mod_fd(int fd, void *data, int events) const
{
    struct epoll_event epev = {};

    epev.events = events;
    epev.data.ptr = data;

    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epev) < 0) {
        log_error("Could not mod fd (%m)");
        return -1;
    }

    return 0;
}

int Mainloop::add_fd(int fd, void *data, int events) const
{
    struct epoll_event epev = {};

    epev.events = events;
    epev.data.ptr = data;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epev) < 0) {
        log_error("Could not add fd to epoll (%m)");
        return -1;
    }

    return 0;
}

int Mainloop::remove_fd(int fd) const
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        log_error("Could not remove fd from epoll (%m)");
        return -1;
    }

    return 0;
}

int Mainloop::write_msg(const std::shared_ptr<Endpoint> &e, const struct buffer *buf) const
{
    int r = e->write_msg(buf);

    /*
     * If endpoint would block, add EPOLLOUT event to get notified when it's
     * possible to write again
     */
    if (r == -EAGAIN) {
        mod_fd(e->fd, e.get(), EPOLLIN | EPOLLOUT);
    }

    return r;
}

void Mainloop::route_msg(struct buffer *buf)
{
    bool unknown = true;

    for (const auto &e : this->g_endpoints) {
        auto acceptState = e->accept_msg(buf);

        switch (acceptState) {
        case Endpoint::AcceptState::Accepted:
            log_trace("Endpoint [%d] accepted message %u to %d/%d from %u/%u",
                      e->fd,
                      buf->curr.msg_id,
                      buf->curr.target_sysid,
                      buf->curr.target_compid,
                      buf->curr.src_sysid,
                      buf->curr.src_compid);
            if (write_msg(e, buf) == -EPIPE) { // only TCP endpoints should return -EPIPE
                should_process_tcp_hangups = true;
            }
            unknown = false;
            break;
        case Endpoint::AcceptState::Filtered:
            log_trace("Endpoint [%d] filtered out message %u to %d/%d from %u/%u",
                      e->fd,
                      buf->curr.msg_id,
                      buf->curr.target_sysid,
                      buf->curr.target_compid,
                      buf->curr.src_sysid,
                      buf->curr.src_compid);
            unknown = false;
            break;
        case Endpoint::AcceptState::Rejected:
            // fall through
        default:
            break; // do nothing (will count as unknown)
        }
    }

    if (unknown) {
        _errors_aggregate.msg_to_unknown++;
        log_trace("Message %u to unknown sysid/compid: %d/%d",
                  buf->curr.msg_id,
                  buf->curr.target_sysid,
                  buf->curr.target_compid);
    }
}

void Mainloop::process_tcp_hangups()
{
    // Remove endpoints which are invalid and can't be reconnected
    for (auto it = g_endpoints.begin(); it != g_endpoints.end();) {
        auto endpoint = it->get();

        if (!endpoint->is_valid()) {
            if (endpoint->get_type() == ENDPOINT_TYPE_TCP) {
                auto *tcp_endpoint = static_cast<TcpEndpoint *>(endpoint);
                if (!tcp_endpoint->should_retry_connection()) {
                    log_info("Removing TCP endpoint %s (no retry)", tcp_endpoint->get_name().c_str());
                    it = g_endpoints.erase(it);
                    continue;
                }
            } else if (endpoint->get_type() == ENDPOINT_TYPE_UART) {
                // UART endpoints with reconnection support stay in the list
                log_debug("UART endpoint %s marked invalid, reconnection will be attempted", endpoint->get_name().c_str());
            } else if (endpoint->get_type() == ENDPOINT_TYPE_UDP) {
                // UDP endpoints with reconnection support stay in the list  
                log_debug("UDP endpoint %s marked invalid, reconnection will be attempted", endpoint->get_name().c_str());
            }
        }
        ++it;
    }

    should_process_tcp_hangups = false;
}

void Mainloop::handle_tcp_connection()
{
    log_debug("TCP Server: New client");

    auto *tcp = new TcpEndpoint{"dynamic"};

    int fd = tcp->accept(g_tcp_fd);
    if (fd == -1) {
        goto accept_error;
    }

    g_endpoints.emplace_back(tcp);
    this->add_fd(g_endpoints.back()->fd, g_endpoints.back().get(), EPOLLIN);

    return;

accept_error:
    log_error("TCP Server: Could not accept TCP connection (%m)");
    delete tcp;
}

int Mainloop::loop()
{
    const int max_events = 8;
    struct epoll_event events[max_events];
    int r;

    if (epollfd < 0) {
        return -EINVAL;
    }

    setup_signal_handlers();

    add_timeout(LOG_AGGREGATE_INTERVAL_SEC * MSEC_PER_SEC,
                std::bind(&Mainloop::_log_aggregate_timeout, this, std::placeholders::_1),
                this);

    while (!_should_exit.load(std::memory_order_relaxed)) {
        int i;
        
        // CRITICAL: Check instance-specific exit flag BEFORE blocking on epoll_wait
        // Each mainloop instance has its own flag, ensuring isolation
        if (_should_exit.load(std::memory_order_relaxed)) {
            log_info("Mainloop %p exiting before epoll_wait due to exit request", this);
            break;
        }

        // Use a timeout for epoll_wait so we can check _should_exit periodically
        // This allows the thread to exit gracefully when request_exit() is called
        r = epoll_wait(epollfd, events, max_events, 100); // 100ms timeout
        if (r < 0 && errno == EINTR) {
            continue;
        }
        
        // If timeout (r == 0), loop will continue and check _should_exit
        if (r == 0) {
            continue;
        }

        for (i = 0; i < r; i++) {
            if (events[i].data.ptr == &g_tcp_fd) {
                handle_tcp_connection();
                continue;
            }

            auto *p = static_cast<Pollable *>(events[i].data.ptr);

            if (events[i].events & EPOLLIN) {
                int rd = p->handle_read();
                if (rd < 0 && !p->is_valid()) {
                    // Any endpoint may become invalid after a read
                    should_process_tcp_hangups = true;
                }
            }

            if (events[i].events & EPOLLOUT) {
                if (!p->handle_canwrite()) {
                    mod_fd(p->fd, p, EPOLLIN);
                }
            }

            if (events[i].events & EPOLLERR) {
                log_error("poll error for fd %i", p->fd);

                if (p->is_critical()) {
                    log_error("Critical fd %i got error, exiting", p->fd);
                    request_exit(EXIT_FAILURE);
                } else {
                    log_warning("Non-critical fd %i got error, handling gracefully", p->fd);

                    // Special handling for UART endpoints to prevent infinite error loops
                    auto *endpoint = dynamic_cast<Endpoint*>(p);
                    if (endpoint && endpoint->get_type() == ENDPOINT_TYPE_UART) {
                        auto *uart_endpoint = static_cast<UartEndpoint*>(endpoint);
                        uart_endpoint->_handle_epoll_error();
                    } else {
                        // Mark for cleanup/reconnection for other endpoint types
                        should_process_tcp_hangups = true;
                    }
                }
            }
        }

        if (should_process_tcp_hangups) {
            process_tcp_hangups();
        }

        _del_timeouts();
    }

    if (_log_endpoint != nullptr) {
        _log_endpoint->stop();
    }

    clear_endpoints();

    // free all remaning Timeouts
    while (_timeouts != nullptr) {
        Timeout *current = _timeouts;
        _timeouts = current->next;
        remove_fd(current->fd);
        delete current;
    }

    return _retcode;
}

bool Mainloop::_log_aggregate_timeout(void *data)
{
    if (_errors_aggregate.msg_to_unknown > 0) {
        log_warning("%u messages to unknown endpoints in the last %d seconds",
                    _errors_aggregate.msg_to_unknown,
                    LOG_AGGREGATE_INTERVAL_SEC);
        _errors_aggregate.msg_to_unknown = 0;
    }

    for (const auto &e : g_endpoints) {
        e->log_aggregate(LOG_AGGREGATE_INTERVAL_SEC);
        e->update_statistics_periodic(); // Update enhanced statistics
    }
    return true;
}

void Mainloop::print_statistics()
{
    for (const auto &e : g_endpoints) {
        e->print_statistics();

        // Print enhanced statistics if available
        if (auto* stats = e->get_statistics()) {
            stats->print_summary();
        }
    }
}

static bool _print_statistics_timeout_cb(void *data)
{
    auto *mainloop = static_cast<Mainloop *>(data);
    mainloop->print_statistics();
    return true;
}

bool Mainloop::dedup_check_msg(const buffer *buf)
{
    // Use this mainloop instance's dedup system
    // Each mainloop (main thread or extension thread) has its own dedup instance
    // This ensures thread-safety without contention between threads
    return _msg_dedup.check_packet(buf->data, buf->len)
        == Dedup::PacketStatus::NEW_PACKET_OR_TIMED_OUT;
}

bool Mainloop::add_endpoints(const Configuration &config)
{
    // Debug: Log the configuration being passed to mainloop
    log_info("Mainloop::add_endpoints() - Configuration received: %zu UART, %zu UDP, %zu TCP endpoints",
            config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());
    
    // Debug: Log details of each endpoint configuration
    for (size_t i = 0; i < config.uart_configs.size(); i++) {
        log_debug("Mainloop::add_endpoints() - UART[%zu]: name='%s', device='%s', baudrates=%zu",
                 i, config.uart_configs[i].name.c_str(), config.uart_configs[i].device.c_str(), 
                 config.uart_configs[i].baudrates.size());
    }
    for (size_t i = 0; i < config.udp_configs.size(); i++) {
        log_debug("Mainloop::add_endpoints() - UDP[%zu]: name='%s', address='%s', port=%lu, mode=%d",
                 i, config.udp_configs[i].name.c_str(), config.udp_configs[i].address.c_str(), 
                 config.udp_configs[i].port, (int)config.udp_configs[i].mode);
    }
    for (size_t i = 0; i < config.tcp_configs.size(); i++) {
        log_debug("Mainloop::add_endpoints() - TCP[%zu]: name='%s', address='%s', port=%lu, retry_timeout=%d",
                 i, config.tcp_configs[i].name.c_str(), config.tcp_configs[i].address.c_str(), 
                 config.tcp_configs[i].port, config.tcp_configs[i].retry_timeout);
    }

    // Create UART and UDP endpoints
    if (config.sniffer_sysid != 0) {
        Endpoint::sniffer_sysid = config.sniffer_sysid;
        log_info("An endpoint with sysid %u on it will sniff all messages",
                 Endpoint::sniffer_sysid);
    }
    for (const auto &conf : config.uart_configs) {
        log_info("Mainloop::add_endpoints() - Setting up UART endpoint: %s on %s", 
                conf.name.c_str(), conf.device.c_str());
        auto uart = std::make_shared<UartEndpoint>(conf.name);

        if (!uart->setup(conf)) {
            log_error("Mainloop::add_endpoints() - Failed to setup UART endpoint: %s on %s", 
                     conf.name.c_str(), conf.device.c_str());
            return false;
        }

        g_endpoints.push_back(uart);
        auto endpoint = g_endpoints.back();
        if (endpoint->fd >= 0) {
            this->add_fd(endpoint->fd, endpoint.get(), EPOLLIN);
            this->track_fd(endpoint->fd, "UART:" + conf.name);
            log_info("Mainloop::add_endpoints() - Successfully added UART endpoint: %s [fd=%d]", 
                    conf.name.c_str(), endpoint->fd);
        } else {
            log_warning("Mainloop::add_endpoints() - UART endpoint %s has invalid fd, but keeping for reconnection", 
                       conf.name.c_str());
        }
    }

    for (const auto &conf : config.udp_configs) {
        log_info("Mainloop::add_endpoints() - Setting up UDP endpoint: %s at %s:%lu", 
                conf.name.c_str(), conf.address.c_str(), conf.port);
        auto udp = std::make_shared<UdpEndpoint>(conf.name);

        if (!udp->setup(conf)) {
            log_error("Mainloop::add_endpoints() - Failed to setup UDP endpoint: %s at %s:%lu", 
                     conf.name.c_str(), conf.address.c_str(), conf.port);
            return false;
        }

        g_endpoints.emplace_back(udp);
        auto endpoint = g_endpoints.back();
        if (endpoint->fd >= 0) {
            this->add_fd(endpoint->fd, endpoint.get(), EPOLLIN);
            this->track_fd(endpoint->fd, "UDP:" + conf.name);
            log_info("Mainloop::add_endpoints() - Successfully added UDP endpoint: %s [fd=%d]", 
                    conf.name.c_str(), endpoint->fd);
        } else {
            log_warning("Mainloop::add_endpoints() - UDP endpoint %s has invalid fd, but keeping for reconnection", 
                       conf.name.c_str());
        }
    }

    // Create TCP endpoints
    for (const auto &conf : config.tcp_configs) {
        log_info("Mainloop::add_endpoints() - Setting up TCP endpoint: %s at %s:%lu", 
                conf.name.c_str(), conf.address.c_str(), conf.port);
        auto tcp = std::make_shared<TcpEndpoint>(conf.name);

        if (!tcp->setup(conf)) { // handles reconnect and add_fd
            log_error("Mainloop::add_endpoints() - Failed to setup TCP endpoint: %s at %s:%lu", 
                     conf.name.c_str(), conf.address.c_str(), conf.port);
            return false;        // only on fatal errors
        }

        g_endpoints.emplace_back(tcp);
        if (tcp->fd >= 0) {
            this->track_fd(tcp->fd, "TCP:" + conf.name);
        }
        log_info("Mainloop::add_endpoints() - Successfully added TCP endpoint: %s [fd=%d]", 
                conf.name.c_str(), tcp->fd);
    }

    // Link grouped endpoints together
    for (auto e : g_endpoints) {
        if (e->get_group_name().empty()) {
            continue;
        }

        for (auto other : g_endpoints) { // find other endpoints in group
            if (other != e && e->get_group_name() == e->get_group_name()) {
                e->link_group_member(other);
            }
        }
    }

    // Create TCP server
    if (config.tcp_port != 0u) {
        g_tcp_fd = tcp_open(config.tcp_port);
    }

    // Create Log endpoint
    auto conf = config.log_config;
    if (!conf.logs_dir.empty()) {
        switch (conf.mavlink_dialect) {
        case LogOptions::MavDialect::Ardupilotmega:
            this->_log_endpoint = std::make_shared<BinLog>(conf);
            break;

        case LogOptions::MavDialect::Common:
            this->_log_endpoint = std::make_shared<ULog>(conf);
            break;

        case LogOptions::MavDialect::Auto:
            this->_log_endpoint = std::make_shared<AutoLog>(conf);
            break;

            // no default case on purpose
        }
        this->_log_endpoint->mark_unfinished_logs();
        g_endpoints.emplace_back(this->_log_endpoint);

        if (conf.log_telemetry) {
            auto tlog_endpoint = std::make_shared<TLog>(conf);
            tlog_endpoint->mark_unfinished_logs();
            g_endpoints.emplace_back(tlog_endpoint);
        }
    }

    // Apply other options
    if (config.report_msg_statistics) {
        add_timeout(MSEC_PER_SEC, _print_statistics_timeout_cb, this);
    }

    if (config.dedup_period_ms > 0) {
        log_info("Message de-duplication enabled: %ld ms period", config.dedup_period_ms);
        _msg_dedup.set_dedup_period(config.dedup_period_ms);
    }

    log_info("Mainloop::add_endpoints() - Completed endpoint setup - Total active endpoints: %zu", g_endpoints.size());

    // Count endpoints by type for final verification
    size_t uart_count = 0, udp_count = 0, tcp_count = 0, log_count = 0;
    for (const auto &endpoint : g_endpoints) {
        if (endpoint->get_type() == ENDPOINT_TYPE_UART) uart_count++;
        else if (endpoint->get_type() == ENDPOINT_TYPE_UDP) udp_count++;
        else if (endpoint->get_type() == ENDPOINT_TYPE_TCP) tcp_count++;
        else if (endpoint->get_type() == ENDPOINT_TYPE_LOG) log_count++;
    }

    log_info("Mainloop::add_endpoints() - Final endpoint count: %zu UART, %zu UDP, %zu TCP, %zu Log endpoints", 
            uart_count, udp_count, tcp_count, log_count);

    return true;
}

void Mainloop::clear_endpoints()
{
    g_endpoints.clear();
}

void Mainloop::track_fd(int fd, const std::string& description)
{
    if (fd < 0) return;
    
    std::lock_guard<std::mutex> lock(_tracked_fds_mutex);
    _tracked_fds[fd] = description;
    log_debug("Tracked FD %d: %s", fd, description.c_str());
}

void Mainloop::untrack_fd(int fd)
{
    if (fd < 0) return;
    
    std::lock_guard<std::mutex> lock(_tracked_fds_mutex);
    auto it = _tracked_fds.find(fd);
    if (it != _tracked_fds.end()) {
        log_debug("Untracked FD %d: %s", fd, it->second.c_str());
        _tracked_fds.erase(it);
    } else {
        log_debug("Attempted to untrack FD %d but it was not tracked", fd);
    }
}

void Mainloop::force_close_all_tracked_fds()
{
    std::lock_guard<std::mutex> lock(_tracked_fds_mutex);
    
    log_info("Force closing %zu tracked file descriptors", _tracked_fds.size());
    
    for (auto& pair : _tracked_fds) {
        int fd = pair.first;
        const std::string& desc = pair.second;
        
        log_info("Force closing FD %d (%s)", fd, desc.c_str());
        
        // Check if FD is valid before attempting to close
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1) {
            log_debug("FD %d already closed or invalid, skipping", fd);
            continue;
        }
        
        // Remove from epoll first if epollfd is valid and not the FD being closed
        if (epollfd >= 0 && fd != epollfd) {
            int ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
            if (ret < 0 && errno != ENOENT && errno != EBADF) {
                log_debug("Could not remove FD %d from epoll: %m", fd);
            }
        }
        
        // Force close the file descriptor
        if (close(fd) == 0) {
            log_info("Successfully closed FD %d", fd);
        } else {
            log_warning("Failed to close FD %d: %m (may have been closed already)", fd);
        }
    }
    
    _tracked_fds.clear();
    log_info("All tracked FDs closed and cleared");
}

int Mainloop::tcp_open(unsigned long tcp_port)
{
    int fd;
    struct sockaddr_in6 sockaddr = {};
    int val = 1;

    fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd == -1) {
        log_error("TCP Server: Could not create tcp socket (%m)");
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr.sin6_family = AF_INET6;
    sockaddr.sin6_port = htons(tcp_port);
    sockaddr.sin6_addr = in6addr_any;

    if (bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        log_error("TCP Server: Could not bind to tcp socket (%m)");
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        log_error("TCP Server: Could not listen on tcp socket (%m)");
        close(fd);
        return -1;
    }

    add_fd(fd, &g_tcp_fd, EPOLLIN);
    track_fd(fd, "TCP_SERVER");

    log_info("Opened TCP Server [%d] [::]:%lu", fd, tcp_port);

    return fd;
}

Timeout *Mainloop::add_timeout(uint32_t timeout_msec, std::function<bool(void *)> cb,
                               const void *data)
{
    auto *t = new Timeout(cb, data);

    assert_or_return(t, nullptr);

    t->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (t->fd < 0) {
        log_error("Unable to create timerfd: %m");
        goto error;
    }

    mod_timeout(t, timeout_msec);

    if (add_fd(t->fd, t, EPOLLIN) < 0) {
        goto error;
    }
    
    track_fd(t->fd, "TIMEOUT");

    t->next = _timeouts;
    _timeouts = t;

    return t;

error:
    delete t;
    return nullptr;
}

void Mainloop::del_timeout(Timeout *t)
{
    t->remove_me = true;
}

void Mainloop::mod_timeout(Timeout *t, uint32_t timeout_msec)
{
    struct itimerspec ts;

    ts.it_interval.tv_sec = timeout_msec / MSEC_PER_SEC;
    ts.it_interval.tv_nsec = (timeout_msec % MSEC_PER_SEC) * NSEC_PER_MSEC;
    ts.it_value.tv_sec = ts.it_interval.tv_sec;
    ts.it_value.tv_nsec = ts.it_interval.tv_nsec;

    timerfd_settime(t->fd, 0, &ts, nullptr);
}

void Mainloop::_del_timeouts()
{
    // Guarantee one valid Timeout on the beginning of the list
    while ((_timeouts != nullptr) && _timeouts->remove_me) {
        Timeout *next = _timeouts->next;
        remove_fd(_timeouts->fd);
        untrack_fd(_timeouts->fd);
        delete _timeouts;
        _timeouts = next;
    }

    // Remove all other Timeouts
    if (_timeouts != nullptr) {
        Timeout *prev = _timeouts;
        Timeout *current = _timeouts->next;
        while (current != nullptr) {
            if (current->remove_me) {
                prev->next = current->next;
                remove_fd(current->fd);
                untrack_fd(current->fd);
                delete current;
                current = prev->next;
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
}