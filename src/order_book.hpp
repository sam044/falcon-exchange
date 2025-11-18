#pragma once

#include "order.hpp"
#include "price_level.hpp"
#include <map>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>

namespace falcon {

// OrderBook maintains separate bid and ask sides
// Uses std::map for ordered price levels (O(log n) operations)
// For ultra-low latency, could be replaced with custom skip list
class OrderBook {
public:
    OrderBook(const std::string& symbol) : symbol_(symbol), sequence_counter_(0) {}
    
    // Add order to the book
    bool add_order(std::shared_ptr<Order> order) {
        if (order->symbol != symbol_) {
            return false;
        }
        
        order->sequence_number = ++sequence_counter_;
        
        if (order->side == OrderSide::BUY) {
            return add_to_side(bids_, order);
        } else {
            return add_to_side(asks_, order);
        }
    }
    
    // Remove order from the book
    bool remove_order(std::shared_ptr<Order> order) {
        if (order->side == OrderSide::BUY) {
            return remove_from_side(bids_, order);
        } else {
            return remove_from_side(asks_, order);
        }
    }
    
    // Cancel order
    bool cancel_order(uint64_t order_id) {
        // Search bid side
        for (auto it = bids_.begin(); it != bids_.end(); ++it) {
            auto order = it->second.find_order(order_id);
            if (order) {
                order->status = OrderStatus::CANCELLED;
                it->second.remove_order(order);
                if (it->second.empty()) {
                    bids_.erase(it);
                }
                return true;
            }
        }
        
        // Search ask side
        for (auto it = asks_.begin(); it != asks_.end(); ++it) {
            auto order = it->second.find_order(order_id);
            if (order) {
                order->status = OrderStatus::CANCELLED;
                it->second.remove_order(order);
                if (it->second.empty()) {
                    asks_.erase(it);
                }
                return true;
            }
        }
        
        return false;
    }
    
    // Get best bid (highest buy price)
    std::optional<int64_t> best_bid() const {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.rbegin()->first;  // Highest price
    }
    
    // Get best ask (lowest sell price)
    std::optional<int64_t> best_ask() const {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first;  // Lowest price
    }
    
    // Get spread
    std::optional<int64_t> spread() const {
        auto bid = best_bid();
        auto ask = best_ask();
        if (bid && ask) {
            return *ask - *bid;
        }
        return std::nullopt;
    }
    
    // Get mid price
    std::optional<double> mid_price() const {
        auto bid = best_bid();
        auto ask = best_ask();
        if (bid && ask) {
            return (*bid + *ask) / 2.0;
        }
        return std::nullopt;
    }
    
    // Get top of book (best bid/ask with quantities)
    struct TopOfBook {
        std::optional<int64_t> bid_price;
        std::optional<int64_t> bid_quantity;
        std::optional<int64_t> ask_price;
        std::optional<int64_t> ask_quantity;
    };
    
    TopOfBook get_top_of_book() const {
        TopOfBook top;
        
        if (!bids_.empty()) {
            const auto& best_bid_level = bids_.rbegin()->second;
            top.bid_price = best_bid_level.price();
            top.bid_quantity = best_bid_level.total_quantity();
        }
        
        if (!asks_.empty()) {
            const auto& best_ask_level = asks_.begin()->second;
            top.ask_price = best_ask_level.price();
            top.ask_quantity = best_ask_level.total_quantity();
        }
        
        return top;
    }
    
    // Get price level for matching (best bid/ask)
    PriceLevel* get_best_bid_level() {
        if (bids_.empty()) return nullptr;
        return &bids_.rbegin()->second;
    }
    
    PriceLevel* get_best_ask_level() {
        if (asks_.empty()) return nullptr;
        return &asks_.begin()->second;
    }
    
    // Get full depth snapshot
    struct DepthLevel {
        int64_t price;
        int64_t quantity;
        size_t order_count;
    };
    
    std::vector<DepthLevel> get_bid_depth(size_t max_levels = 10) const {
        std::vector<DepthLevel> depth;
        auto it = bids_.rbegin();
        for (size_t i = 0; i < max_levels && it != bids_.rend(); ++i, ++it) {
            depth.push_back({it->second.price(), it->second.total_quantity(), 
                            it->second.order_count()});
        }
        return depth;
    }
    
    std::vector<DepthLevel> get_ask_depth(size_t max_levels = 10) const {
        std::vector<DepthLevel> depth;
        auto it = asks_.begin();
        for (size_t i = 0; i < max_levels && it != asks_.end(); ++i, ++it) {
            depth.push_back({it->second.price(), it->second.total_quantity(), 
                            it->second.order_count()});
        }
        return depth;
    }
    
    const std::string& symbol() const {
        return symbol_;
    }
    
    size_t bid_levels() const {
        return bids_.size();
    }
    
    size_t ask_levels() const {
        return asks_.size();
    }
    
private:
    std::string symbol_;
    std::map<int64_t, PriceLevel> bids_;  // Ordered by price (ascending)
    std::map<int64_t, PriceLevel> asks_;  // Ordered by price (ascending)
    uint64_t sequence_counter_;
    
    bool add_to_side(std::map<int64_t, PriceLevel>& side, std::shared_ptr<Order> order) {
        auto it = side.find(order->price);
        if (it == side.end()) {
            side.emplace(order->price, PriceLevel(order->price));
            it = side.find(order->price);
        }
        it->second.add_order(order);
        return true;
    }
    
    bool remove_from_side(std::map<int64_t, PriceLevel>& side, std::shared_ptr<Order> order) {
        auto it = side.find(order->price);
        if (it != side.end()) {
            it->second.remove_order(order);
            if (it->second.empty()) {
                side.erase(it);
            }
            return true;
        }
        return false;
    }
    
};

} // namespace falcon

