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

#pragma once

#include <memory>
#include <mutex>

class DedupImpl;

/*
 * De-duplication of raw buffers - Thread-Safe Implementation
 *
 * When presented with a buffer, this class calculates a hash value of the
 * data and determines whether that hash was already seen in a configureable
 * time period (dedup_period). A known hash value will reset the time for that
 * value. Old hash values will be cleaned up each time check_packet is called.
 *
 * This class is thread-safe and can be used concurrently from multiple threads.
 * Each Mainloop instance should have its own Dedup instance to avoid contention.
 *
 * A dedup_period of 0 will disable all de-duplication checks.
 */
class Dedup {
public:
    enum class PacketStatus { NEW_PACKET_OR_TIMED_OUT, ALREADY_EXISTS_IN_BUFFER };

    Dedup(uint32_t dedup_period_ms = 0);
    ~Dedup();

    // Thread-safe: can be called from any thread
    void set_dedup_period(uint32_t dedup_period_ms);

    // Thread-safe: can be called concurrently from multiple threads
    PacketStatus check_packet(const uint8_t *buffer, uint32_t size);

    // Disable copy constructor and assignment operator
    Dedup(const Dedup&) = delete;
    Dedup& operator=(const Dedup&) = delete;

    // Enable move constructor and assignment operator
    Dedup(Dedup&& other) noexcept;
    Dedup& operator=(Dedup&& other) noexcept;

private:
    uint32_t _dedup_period_ms; ///< how long (protected by mutex)
    std::unique_ptr<DedupImpl> _impl; ///< implementation (protected by mutex)
    mutable std::mutex _mutex; ///< protects all member variables
};
