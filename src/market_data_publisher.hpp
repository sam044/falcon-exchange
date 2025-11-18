#pragma once

#include "order_book.hpp"
#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace falcon {

// MarketDataPublisher publishes market data updates
// In production, this would publish to subscribers via various protocols
// (FIX, ITCH, proprietary binary, etc.)

struct MarketDataUpdate {
    enum class UpdateType {
        TOP_OF_BOOK,
        DEPTH_SNAPSHOT,
        TRADE
    };
    
    UpdateType type;
    std::string symbol;
    std::chrono::microseconds timestamp;
    
    // For TOP_OF_BOOK
    std::optional<int64_t> bid_price;
    std::optional<int64_t> bid_quantity;
    std::optional<int64_t> ask_price;
    std::optional<int64_t> ask_quantity;
    
    // For DEPTH_SNAPSHOT
    std::vector<OrderBook::DepthLevel> bid_depth;
    std::vector<OrderBook::DepthLevel> ask_depth;
    
    // For TRADE
    int64_t trade_price;
    int64_t trade_quantity;
};

class MarketDataPublisher {
public:
    using UpdateCallback = std::function<void(const MarketDataUpdate&)>;
    
    MarketDataPublisher() = default;
    
    // Register callback for market data updates
    void set_update_callback(UpdateCallback callback) {
        update_callback_ = callback;
    }
    
    // Publish top of book update
    void publish_top_of_book(const OrderBook& book) {
        MarketDataUpdate update;
        update.type = MarketDataUpdate::UpdateType::TOP_OF_BOOK;
        update.symbol = book.symbol();
        update.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
        
        auto top = book.get_top_of_book();
        update.bid_price = top.bid_price;
        update.bid_quantity = top.bid_quantity;
        update.ask_price = top.ask_price;
        update.ask_quantity = top.ask_quantity;
        
        if (update_callback_) {
            update_callback_(update);
        }
    }
    
    // Publish full depth snapshot
    void publish_depth_snapshot(const OrderBook& book, size_t max_levels = 10) {
        MarketDataUpdate update;
        update.type = MarketDataUpdate::UpdateType::DEPTH_SNAPSHOT;
        update.symbol = book.symbol();
        update.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
        
        update.bid_depth = book.get_bid_depth(max_levels);
        update.ask_depth = book.get_ask_depth(max_levels);
        
        if (update_callback_) {
            update_callback_(update);
        }
    }
    
    // Publish trade update
    void publish_trade(const std::string& symbol, int64_t price, int64_t quantity) {
        MarketDataUpdate update;
        update.type = MarketDataUpdate::UpdateType::TRADE;
        update.symbol = symbol;
        update.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
        update.trade_price = price;
        update.trade_quantity = quantity;
        
        if (update_callback_) {
            update_callback_(update);
        }
    }
    
private:
    UpdateCallback update_callback_;
};

} // namespace falcon

