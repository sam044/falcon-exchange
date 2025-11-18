#include "matching_engine.hpp"
#include "market_data_publisher.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

using namespace falcon;

void print_trade(const Trade& trade) {
    std::cout << "[TRADE] ID: " << trade.trade_id
              << " Symbol: " << trade.symbol
              << " Price: " << trade.price
              << " Quantity: " << trade.quantity
              << " Buy Order: " << trade.buy_order_id
              << " Sell Order: " << trade.sell_order_id
              << std::endl;
}

void print_order_update(const Order& order) {
    std::cout << "[ORDER] ID: " << order.id
              << " Symbol: " << order.symbol
              << " Side: " << (order.side == OrderSide::BUY ? "BUY" : "SELL")
              << " Price: " << order.price
              << " Quantity: " << order.quantity
              << " Filled: " << order.filled_quantity
              << " Status: ";
    
    switch (order.status) {
        case OrderStatus::NEW:
            std::cout << "NEW";
            break;
        case OrderStatus::PARTIALLY_FILLED:
            std::cout << "PARTIALLY_FILLED";
            break;
        case OrderStatus::FILLED:
            std::cout << "FILLED";
            break;
        case OrderStatus::CANCELLED:
            std::cout << "CANCELLED";
            break;
        case OrderStatus::REJECTED:
            std::cout << "REJECTED";
            break;
    }
    std::cout << std::endl;
}

void print_market_data(const MarketDataUpdate& update) {
    switch (update.type) {
        case MarketDataUpdate::UpdateType::TOP_OF_BOOK:
            std::cout << "[TOB] " << update.symbol << " ";
            if (update.bid_price) {
                std::cout << "Bid: " << *update.bid_price 
                          << "@" << *update.bid_quantity << " ";
            }
            if (update.ask_price) {
                std::cout << "Ask: " << *update.ask_price 
                          << "@" << *update.ask_quantity;
            }
            std::cout << std::endl;
            break;
            
        case MarketDataUpdate::UpdateType::DEPTH_SNAPSHOT:
            std::cout << "[DEPTH] " << update.symbol << std::endl;
            std::cout << "  Bids:" << std::endl;
            for (const auto& level : update.bid_depth) {
                std::cout << "    " << level.price << "@" << level.quantity 
                          << " (" << level.order_count << " orders)" << std::endl;
            }
            std::cout << "  Asks:" << std::endl;
            for (const auto& level : update.ask_depth) {
                std::cout << "    " << level.price << "@" << level.quantity 
                          << " (" << level.order_count << " orders)" << std::endl;
            }
            break;
            
        case MarketDataUpdate::UpdateType::TRADE:
            std::cout << "[TRADE UPDATE] " << update.symbol 
                      << " " << update.trade_price 
                      << "@" << update.trade_quantity << std::endl;
            break;
    }
}

int main() {
    std::cout << "=== Falcon Exchange - Low-Latency Matching Engine ===" << std::endl;
    
    // Create matching engine for AAPL
    MatchingEngine engine("AAPL");
    
    // Set up callbacks
    engine.set_trade_callback(print_trade);
    engine.set_order_update_callback(print_order_update);
    
    // Create market data publisher
    MarketDataPublisher publisher;
    publisher.set_update_callback(print_market_data);
    
    // Start the engine
    engine.start();
    
    // Example: Build up some liquidity
    std::cout << "\n--- Building initial book ---" << std::endl;
    
    // Add some buy orders
    for (int i = 0; i < 5; ++i) {
        auto buy_order = std::make_shared<Order>(
            1000 + i, "AAPL", OrderSide::BUY, OrderType::LIMIT,
            15000 - i * 10, 100  // Prices: 150.00, 149.90, 149.80, etc.
        );
        engine.submit_order(buy_order);
    }
    
    // Add some sell orders
    for (int i = 0; i < 5; ++i) {
        auto sell_order = std::make_shared<Order>(
            2000 + i, "AAPL", OrderSide::SELL, OrderType::LIMIT,
            15100 + i * 10, 100  // Prices: 151.00, 151.10, 151.20, etc.
        );
        engine.submit_order(sell_order);
    }
    
    // Wait for orders to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Publish market data
    publisher.publish_top_of_book(engine.get_order_book());
    publisher.publish_depth_snapshot(engine.get_order_book(), 5);
    
    // Example: Matching orders
    std::cout << "\n--- Matching orders ---" << std::endl;
    
    // Submit a buy order that will match
    auto aggressive_buy = std::make_shared<Order>(
        3000, "AAPL", OrderSide::BUY, OrderType::LIMIT,
        15100, 50  // Matches with best ask at 151.00
    );
    engine.submit_order(aggressive_buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Submit a market sell order
    auto market_sell = std::make_shared<Order>(
        3001, "AAPL", OrderSide::SELL, OrderType::MARKET,
        0, 75  // Market order, matches best bid
    );
    engine.submit_order(market_sell);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Publish updated market data
    publisher.publish_top_of_book(engine.get_order_book());
    
    // Example: Cancel order
    std::cout << "\n--- Cancelling order ---" << std::endl;
    engine.cancel_order(1001);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Print final statistics
    std::cout << "\n--- Statistics ---" << std::endl;
    auto stats = engine.get_statistics();
    std::cout << "Orders processed: " << stats.orders_processed << std::endl;
    std::cout << "Trades executed: " << stats.trades_executed << std::endl;
    std::cout << "Orders cancelled: " << stats.orders_cancelled << std::endl;
    
    // Stop the engine
    engine.stop();
    
    std::cout << "\n=== Demo Complete ===" << std::endl;
    
    return 0;
}

