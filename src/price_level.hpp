#pragma once

#include "order.hpp"
#include <deque>
#include <memory>
#include <cstdint>

namespace falcon {

// PriceLevel aggregates all orders at a single price point
// Uses deque for O(1) push/pop at both ends (FIFO for price-time priority)
class PriceLevel {
public:
    PriceLevel(int64_t price) : price_(price), total_quantity_(0) {}
    
    // Add order to this price level (FIFO)
    void add_order(std::shared_ptr<Order> order) {
        orders_.push_back(order);
        total_quantity_ += order->remaining_quantity();
    }
    
    // Remove order (typically from front for FIFO)
    void remove_order(std::shared_ptr<Order> order) {
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if ((*it)->id == order->id) {
                total_quantity_ -= (*it)->remaining_quantity();
                orders_.erase(it);
                break;
            }
        }
    }
    
    // Update order quantity (for cancel/replace)
    void update_order_quantity(std::shared_ptr<Order> order, int64_t new_quantity) {
        for (auto& o : orders_) {
            if (o->id == order->id) {
                total_quantity_ -= o->remaining_quantity();
                o->quantity = new_quantity;
                o->filled_quantity = 0;  // Reset on replace
                total_quantity_ += o->remaining_quantity();
                break;
            }
        }
    }
    
    // Get front order (oldest at this price)
    std::shared_ptr<Order> front_order() const {
        return orders_.empty() ? nullptr : orders_.front();
    }
    
    // Check if empty
    bool empty() const {
        return orders_.empty();
    }
    
    // Get total quantity at this price level
    int64_t total_quantity() const {
        return total_quantity_;
    }
    
    // Get price
    int64_t price() const {
        return price_;
    }
    
    // Get number of orders at this level
    size_t order_count() const {
        return orders_.size();
    }
    
    // Update total quantity (called when order is filled)
    void update_total_quantity(int64_t delta) {
        total_quantity_ += delta;
    }
    
    // Find order by ID (returns shared_ptr if found, nullptr otherwise)
    std::shared_ptr<Order> find_order(uint64_t order_id) const {
        for (const auto& order : orders_) {
            if (order->id == order_id) {
                return order;
            }
        }
        return nullptr;
    }
    
    // Get all orders (for iteration)
    const std::deque<std::shared_ptr<Order>>& get_orders() const {
        return orders_;
    }
    
private:
    int64_t price_;
    int64_t total_quantity_;
    std::deque<std::shared_ptr<Order>> orders_;  // FIFO queue for price-time priority
};

} // namespace falcon

