#pragma once

#include <cstdint>
#include <chrono>
#include <string>

namespace falcon {

enum class OrderSide : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1
};

enum class OrderStatus : uint8_t {
    NEW = 0,
    PARTIALLY_FILLED = 1,
    FILLED = 2,
    CANCELLED = 3,
    REJECTED = 4
};

struct Order {
    uint64_t id;
    std::string symbol;
    OrderSide side;
    OrderType type;
    int64_t price;  // In ticks (e.g., cents for USD, or use fixed-point)
    int64_t quantity;
    int64_t filled_quantity;
    OrderStatus status;
    std::chrono::microseconds timestamp;
    
    // For price-time priority queue
    uint64_t sequence_number;
    
    Order() = default;
    
    Order(uint64_t id, const std::string& symbol, OrderSide side, OrderType type,
          int64_t price, int64_t quantity)
        : id(id), symbol(symbol), side(side), type(type), price(price),
          quantity(quantity), filled_quantity(0), status(OrderStatus::NEW),
          timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())),
          sequence_number(0) {}
    
    int64_t remaining_quantity() const {
        return quantity - filled_quantity;
    }
    
    bool is_filled() const {
        return filled_quantity >= quantity;
    }
    
    bool is_active() const {
        return status == OrderStatus::NEW || status == OrderStatus::PARTIALLY_FILLED;
    }
};

} // namespace falcon

