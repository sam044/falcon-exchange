#pragma once

#include "order.hpp"
#include <atomic>
#include <memory>
#include <array>
#include <cstdint>

namespace falcon {

enum class EventType : uint8_t {
    NEW_ORDER = 0,
    CANCEL_ORDER = 1,
    REPLACE_ORDER = 2,
    SHUTDOWN = 3
};

struct OrderEvent {
    EventType type;
    std::shared_ptr<Order> order;
    uint64_t cancel_order_id;  // For cancel events
    int64_t new_price;         // For replace events
    int64_t new_quantity;      // For replace events
};

// Lock-free single-producer single-consumer queue
// Based on ring buffer with atomic head/tail pointers
template<size_t Size>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {
        // Ensure power of 2 for efficient modulo
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    }
    
    bool push(const OrderEvent& event) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & (Size - 1);
        
        // Check if queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        buffer_[current_tail] = event;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(OrderEvent& event) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Check if queue is empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        
        event = buffer_[current_head];
        head_.store((current_head + 1) & (Size - 1), std::memory_order_release);
        return true;
    }
    
    bool empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (tail - head) & (Size - 1);
    }
    
private:
    alignas(64) std::atomic<size_t> head_;  // Cache line aligned
    alignas(64) std::atomic<size_t> tail_;  // Cache line aligned
    std::array<OrderEvent, Size> buffer_;
};

// EventQueue is an alias for the lock-free queue
// Default size: 65536 (2^16) events
using EventQueue = LockFreeQueue<65536>;

} // namespace falcon

