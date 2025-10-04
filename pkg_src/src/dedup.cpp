/*
 * This file is part of the MAVLink Router project
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

#include "dedup.h"

#include <chrono>
#include <queue>
#include <string>
#include <unordered_set>
#include <mutex>

class DedupImpl {
    using hash_t = uint64_t;
    using time_t = uint32_t;

public:
    DedupImpl()
        : _start_time(std::chrono::system_clock::now())
    {
    }

    // Thread-safe packet checking with internal locking
    bool check_packet(const uint8_t *buffer, uint32_t size, uint32_t dedup_period_ms)
    {
        std::lock_guard<std::mutex> lock(_impl_mutex);
        
        using namespace std::chrono;
        time_t timestamp
            = duration_cast<milliseconds>(std::chrono::system_clock::now() - _start_time).count();
        
        // Clean up expired entries
        while (!_time_hash_queue.empty()
               && timestamp > _time_hash_queue.front().first + dedup_period_ms) {
            hash_t hash_to_delete = _time_hash_queue.front().second;
            auto it = _packet_hash_set.find(hash_to_delete);
            if (it != _packet_hash_set.end()) {
                _packet_hash_set.erase(it);
            }
            _time_hash_queue.pop();
        }

        // Hash the buffer
        _hash_buffer.assign((const char *)buffer, (uint64_t)size);
        hash_t hash = std::hash<std::string>{}(_hash_buffer);

        // Check if this is a new packet
        bool new_packet_hash = true;
        if (_packet_hash_set.find(hash) == _packet_hash_set.end()) {
            // New packet: add to tracking structures
            _packet_hash_set.insert(hash);
            _time_hash_queue.emplace(timestamp, hash);
        } else {
            // Duplicate packet
            new_packet_hash = false;
        }

        return new_packet_hash;
    }

    // Clear all deduplication data (thread-safe)
    void clear()
    {
        std::lock_guard<std::mutex> lock(_impl_mutex);
        while (!_time_hash_queue.empty()) {
            _time_hash_queue.pop();
        }
        _packet_hash_set.clear();
        _hash_buffer.clear();
    }

private:
    const std::chrono::time_point<std::chrono::system_clock> _start_time;

    std::queue<std::pair<time_t, hash_t>> _time_hash_queue;
    std::unordered_set<hash_t> _packet_hash_set;
    std::string _hash_buffer;
    
    // Mutex protecting all internal data structures
    std::mutex _impl_mutex;
};

Dedup::Dedup(uint32_t dedup_period_ms)
    : _dedup_period_ms(dedup_period_ms)
    , _impl(new DedupImpl())
    , _mutex()
{
}

Dedup::~Dedup()
{
    // Lock to ensure no operations are in progress
    std::lock_guard<std::mutex> lock(_mutex);
    // explicit d-tor is needed to make the unique_ptr work
}

Dedup::Dedup(Dedup&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other._mutex);
    _dedup_period_ms = other._dedup_period_ms;
    _impl = std::move(other._impl);
}

Dedup& Dedup::operator=(Dedup&& other) noexcept
{
    if (this != &other) {
        // Lock both mutexes in a consistent order to prevent deadlock
        std::lock(_mutex, other._mutex);
        std::lock_guard<std::mutex> lock1(_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(other._mutex, std::adopt_lock);
        
        _dedup_period_ms = other._dedup_period_ms;
        _impl = std::move(other._impl);
    }
    return *this;
}

void Dedup::set_dedup_period(uint32_t dedup_period_ms)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _dedup_period_ms = dedup_period_ms;
}

Dedup::PacketStatus Dedup::check_packet(const uint8_t *buffer, uint32_t size)
{
    // First check if dedup is disabled (lock-free fast path)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_dedup_period_ms == 0) {
            return PacketStatus::NEW_PACKET_OR_TIMED_OUT;
        }
    }

    // Perform the actual deduplication check
    // Note: _impl has its own internal mutex, so we don't need to hold _mutex here
    uint32_t period_ms;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        period_ms = _dedup_period_ms;
        if (!_impl) {
            // Safety check: if impl is null, treat as new packet
            return PacketStatus::NEW_PACKET_OR_TIMED_OUT;
        }
    }

    if (_impl->check_packet(buffer, size, period_ms)) {
        return PacketStatus::NEW_PACKET_OR_TIMED_OUT;
    }
    return PacketStatus::ALREADY_EXISTS_IN_BUFFER;
}
