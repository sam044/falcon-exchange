#pragma once

#include "order.hpp"
#include "order_book.hpp"
#include "event_queue.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <chrono>

namespace falcon {

struct Trade {
    uint64_t trade_id;
    std::string symbol;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    int64_t price;
    int64_t quantity;
    std::chrono::microseconds timestamp;
    
    Trade(uint64_t trade_id, const std::string& symbol, 
          uint64_t buy_order_id, uint64_t sell_order_id,
          int64_t price, int64_t quantity)
        : trade_id(trade_id), symbol(symbol),
          buy_order_id(buy_order_id), sell_order_id(sell_order_id),
          price(price), quantity(quantity),
          timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())) {}
};

// MatchingEngine processes orders and matches them according to price-time priority
class MatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;
    using OrderUpdateCallback = std::function<void(const Order&)>;
    
    MatchingEngine(const std::string& symbol)
        : symbol_(symbol), running_(false), trade_id_counter_(0) {
        order_book_ = std::make_unique<OrderBook>(symbol);
    }
    
    ~MatchingEngine() {
        stop();
    }
    
    // Start the matching engine thread
    void start() {
        if (running_.load()) {
            return;
        }
        running_.store(true);
        engine_thread_ = std::thread(&MatchingEngine::run, this);
    }
    
    // Stop the matching engine
    void stop() {
        if (!running_.load()) {
            return;
        }
        running_.store(false);
        
        // Send shutdown event
        OrderEvent shutdown_event;
        shutdown_event.type = EventType::SHUTDOWN;
        event_queue_.push(shutdown_event);
        
        if (engine_thread_.joinable()) {
            engine_thread_.join();
        }
    }
    
    // Submit new order
    bool submit_order(std::shared_ptr<Order> order) {
        if (order->symbol != symbol_) {
            return false;
        }
        
        OrderEvent event;
        event.type = EventType::NEW_ORDER;
        event.order = order;
        
        return event_queue_.push(event);
    }
    
    // Cancel order
    bool cancel_order(uint64_t order_id) {
        OrderEvent event;
        event.type = EventType::CANCEL_ORDER;
        event.cancel_order_id = order_id;
        
        return event_queue_.push(event);
    }
    
    // Replace order (cancel old, add new)
    bool replace_order(uint64_t old_order_id, std::shared_ptr<Order> new_order) {
        OrderEvent event;
        event.type = EventType::REPLACE_ORDER;
        event.cancel_order_id = old_order_id;
        event.order = new_order;
        
        return event_queue_.push(event);
    }
    
    // Register callbacks
    void set_trade_callback(TradeCallback callback) {
        trade_callback_ = callback;
    }
    
    void set_order_update_callback(OrderUpdateCallback callback) {
        order_update_callback_ = callback;
    }
    
    // Get order book reference (read-only access)
    const OrderBook& get_order_book() const {
        return *order_book_;
    }
    
    // Get statistics
    struct Statistics {
        uint64_t orders_processed;
        uint64_t trades_executed;
        uint64_t orders_cancelled;
    };
    
    Statistics get_statistics() const {
        return stats_;
    }
    
private:
    void run() {
        OrderEvent event;
        
        while (running_.load() || !event_queue_.empty()) {
            if (event_queue_.pop(event)) {
                process_event(event);
            } else {
                // Yield CPU when queue is empty
                std::this_thread::yield();
            }
        }
    }
    
    void process_event(const OrderEvent& event) {
        switch (event.type) {
            case EventType::NEW_ORDER:
                process_new_order(event.order);
                break;
            case EventType::CANCEL_ORDER:
                process_cancel_order(event.cancel_order_id);
                break;
            case EventType::REPLACE_ORDER:
                process_replace_order(event.cancel_order_id, event.order);
                break;
            case EventType::SHUTDOWN:
                // Handled by run() loop
                break;
        }
    }
    
    void process_new_order(std::shared_ptr<Order> order) {
        stats_.orders_processed++;
        
        if (order->type == OrderType::MARKET) {
            match_market_order(order);
        } else {
            match_limit_order(order);
        }
        
        if (order_update_callback_) {
            order_update_callback_(*order);
        }
    }
    
    void match_limit_order(std::shared_ptr<Order> order) {
        // Try to match immediately against opposite side
        while (order->is_active() && can_match(order)) {
            auto match_result = try_match(order);
            if (!match_result.matched) {
                break;
            }
        }
        
        // If still has quantity, add to book
        if (order->is_active() && order->remaining_quantity() > 0) {
            order_book_->add_order(order);
        }
    }
    
    void match_market_order(std::shared_ptr<Order> order) {
        // Market orders match immediately against opposite side
        while (order->is_active() && can_match(order)) {
            auto match_result = try_match(order);
            if (!match_result.matched) {
                // No more liquidity
                order->status = OrderStatus::REJECTED;
                break;
            }
        }
        
        // Market orders never go into the book
        if (order->remaining_quantity() > 0) {
            order->status = OrderStatus::REJECTED;
        }
    }
    
    bool can_match(const std::shared_ptr<Order>& order) const {
        if (order->side == OrderSide::BUY) {
            auto best_ask = order_book_->best_ask();
            if (!best_ask) return false;
            // Buy order can match if price >= best ask (or market order)
            return order->type == OrderType::MARKET || order->price >= *best_ask;
        } else {
            auto best_bid = order_book_->best_bid();
            if (!best_bid) return false;
            // Sell order can match if price <= best bid (or market order)
            return order->type == OrderType::MARKET || order->price <= *best_bid;
        }
    }
    
    struct MatchResult {
        bool matched;
        int64_t price;
        int64_t quantity;
        std::shared_ptr<Order> resting_order;
    };
    
    MatchResult try_match(std::shared_ptr<Order> incoming_order) {
        MatchResult result = {false, 0, 0, nullptr};
        
        if (incoming_order->side == OrderSide::BUY) {
            auto* ask_level = order_book_->get_best_ask_level();
            if (!ask_level || ask_level->empty()) {
                return result;
            }
            
            auto best_ask = ask_level->price();
            if (incoming_order->type == OrderType::LIMIT && 
                incoming_order->price < best_ask) {
                return result;
            }
            
            auto resting_order = ask_level->front_order();
            if (!resting_order || !resting_order->is_active()) {
                return result;
            }
            
            // Match at resting order price (price-time priority)
            int64_t match_price = resting_order->price;
            int64_t match_quantity = std::min(
                incoming_order->remaining_quantity(),
                resting_order->remaining_quantity()
            );
            
            // Execute trade
            execute_trade(incoming_order, resting_order, match_price, match_quantity);
            
            // Update price level
            ask_level->update_total_quantity(-match_quantity);
            
            // Remove resting order if filled
            if (resting_order->is_filled()) {
                order_book_->remove_order(resting_order);
                if (ask_level->empty()) {
                    // Level will be removed by remove_order
                }
            }
            
            result = {true, match_price, match_quantity, resting_order};
            
        } else {  // SELL
            auto* bid_level = order_book_->get_best_bid_level();
            if (!bid_level || bid_level->empty()) {
                return result;
            }
            
            auto best_bid = bid_level->price();
            if (incoming_order->type == OrderType::LIMIT && 
                incoming_order->price > best_bid) {
                return result;
            }
            
            auto resting_order = bid_level->front_order();
            if (!resting_order || !resting_order->is_active()) {
                return result;
            }
            
            // Match at resting order price (price-time priority)
            int64_t match_price = resting_order->price;
            int64_t match_quantity = std::min(
                incoming_order->remaining_quantity(),
                resting_order->remaining_quantity()
            );
            
            // Execute trade
            execute_trade(resting_order, incoming_order, match_price, match_quantity);
            
            // Update price level
            bid_level->update_total_quantity(-match_quantity);
            
            // Remove resting order if filled
            if (resting_order->is_filled()) {
                order_book_->remove_order(resting_order);
                if (bid_level->empty()) {
                    // Level will be removed by remove_order
                }
            }
            
            result = {true, match_price, match_quantity, resting_order};
        }
        
        return result;
    }
    
    void execute_trade(std::shared_ptr<Order> buy_order, 
                      std::shared_ptr<Order> sell_order,
                      int64_t price, int64_t quantity) {
        // Update order quantities
        buy_order->filled_quantity += quantity;
        sell_order->filled_quantity += quantity;
        
        // Update order status
        if (buy_order->is_filled()) {
            buy_order->status = OrderStatus::FILLED;
        } else {
            buy_order->status = OrderStatus::PARTIALLY_FILLED;
        }
        
        if (sell_order->is_filled()) {
            sell_order->status = OrderStatus::FILLED;
        } else {
            sell_order->status = OrderStatus::PARTIALLY_FILLED;
        }
        
        // Create trade
        Trade trade(++trade_id_counter_, symbol_, 
                   buy_order->id, sell_order->id,
                   price, quantity);
        
        stats_.trades_executed++;
        
        // Notify callback
        if (trade_callback_) {
            trade_callback_(trade);
        }
    }
    
    void process_cancel_order(uint64_t order_id) {
        if (order_book_->cancel_order(order_id)) {
            stats_.orders_cancelled++;
        }
    }
    
    void process_replace_order(uint64_t old_order_id, std::shared_ptr<Order> new_order) {
        // Cancel old order
        process_cancel_order(old_order_id);
        // Add new order
        process_new_order(new_order);
    }
    
    std::string symbol_;
    std::unique_ptr<OrderBook> order_book_;
    EventQueue event_queue_;
    std::thread engine_thread_;
    std::atomic<bool> running_;
    
    TradeCallback trade_callback_;
    OrderUpdateCallback order_update_callback_;
    
    std::atomic<uint64_t> trade_id_counter_;
    Statistics stats_ = {0, 0, 0};
};

} // namespace falcon

