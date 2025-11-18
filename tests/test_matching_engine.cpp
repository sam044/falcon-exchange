#include "../src/matching_engine.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace falcon;

std::vector<Trade> captured_trades;
std::vector<Order> captured_order_updates;

void trade_callback(const Trade& trade) {
    captured_trades.push_back(trade);
}

void order_update_callback(const Order& order) {
    captured_order_updates.push_back(order);
}

void test_basic_matching() {
    std::cout << "Testing basic order matching..." << std::endl;
    
    captured_trades.clear();
    captured_order_updates.clear();
    
    MatchingEngine engine("AAPL");
    engine.set_trade_callback(trade_callback);
    engine.set_order_update_callback(order_update_callback);
    engine.start();
    
    // Add a sell order to the book
    auto sell1 = std::make_shared<Order>(1, "AAPL", OrderSide::SELL, 
                                        OrderType::LIMIT, 15000, 100);
    engine.submit_order(sell1);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Submit a buy order that matches
    auto buy1 = std::make_shared<Order>(2, "AAPL", OrderSide::BUY, 
                                       OrderType::LIMIT, 15000, 50);
    engine.submit_order(buy1);
    
    // Wait for matching
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should have one trade
    assert(captured_trades.size() == 1);
    assert(captured_trades[0].price == 15000);
    assert(captured_trades[0].quantity == 50);
    assert(captured_trades[0].buy_order_id == 2);
    assert(captured_trades[0].sell_order_id == 1);
    
    // Check order statuses
    assert(buy1->status == OrderStatus::FILLED);
    assert(sell1->status == OrderStatus::PARTIALLY_FILLED);
    assert(sell1->filled_quantity == 50);
    
    engine.stop();
    
    std::cout << "  ✓ Basic matching passed" << std::endl;
}

void test_price_time_priority() {
    std::cout << "Testing price-time priority..." << std::endl;
    
    captured_trades.clear();
    
    MatchingEngine engine("AAPL");
    engine.set_trade_callback(trade_callback);
    engine.start();
    
    // Add two sell orders at same price (time priority)
    auto sell1 = std::make_shared<Order>(1, "AAPL", OrderSide::SELL, 
                                        OrderType::LIMIT, 15000, 100);
    auto sell2 = std::make_shared<Order>(2, "AAPL", OrderSide::SELL, 
                                        OrderType::LIMIT, 15000, 100);
    
    engine.submit_order(sell1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    engine.submit_order(sell2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Submit buy order that matches both
    auto buy1 = std::make_shared<Order>(3, "AAPL", OrderSide::BUY, 
                                       OrderType::LIMIT, 15000, 150);
    engine.submit_order(buy1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should match sell1 first (time priority)
    assert(captured_trades.size() >= 1);
    assert(captured_trades[0].sell_order_id == 1);
    
    engine.stop();
    
    std::cout << "  ✓ Price-time priority passed" << std::endl;
}

void test_market_order() {
    std::cout << "Testing market orders..." << std::endl;
    
    captured_trades.clear();
    
    MatchingEngine engine("AAPL");
    engine.set_trade_callback(trade_callback);
    engine.start();
    
    // Add limit sell order
    auto sell1 = std::make_shared<Order>(1, "AAPL", OrderSide::SELL, 
                                        OrderType::LIMIT, 15000, 100);
    engine.submit_order(sell1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Submit market buy order
    auto buy1 = std::make_shared<Order>(2, "AAPL", OrderSide::BUY, 
                                       OrderType::MARKET, 0, 50);
    engine.submit_order(buy1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should match
    assert(captured_trades.size() == 1);
    assert(buy1->status == OrderStatus::FILLED);
    
    engine.stop();
    
    std::cout << "  ✓ Market orders passed" << std::endl;
}

void test_cancel_order() {
    std::cout << "Testing order cancellation..." << std::endl;
    
    MatchingEngine engine("AAPL");
    engine.start();
    
    // Add order
    auto buy1 = std::make_shared<Order>(1, "AAPL", OrderSide::BUY, 
                                       OrderType::LIMIT, 15000, 100);
    engine.submit_order(buy1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Cancel it
    assert(engine.cancel_order(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(buy1->status == OrderStatus::CANCELLED);
    
    engine.stop();
    
    std::cout << "  ✓ Order cancellation passed" << std::endl;
}

int main() {
    std::cout << "=== MatchingEngine Tests ===" << std::endl;
    
    test_basic_matching();
    test_price_time_priority();
    test_market_order();
    test_cancel_order();
    
    std::cout << "\n✓ All MatchingEngine tests passed!" << std::endl;
    return 0;
}

