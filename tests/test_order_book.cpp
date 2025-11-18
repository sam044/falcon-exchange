#include "../src/order_book.hpp"
#include "../src/order.hpp"
#include <cassert>
#include <iostream>

using namespace falcon;

void test_basic_order_book() {
    std::cout << "Testing basic order book operations..." << std::endl;
    
    OrderBook book("AAPL");
    
    // Add buy orders
    auto buy1 = std::make_shared<Order>(1, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15000, 100);
    auto buy2 = std::make_shared<Order>(2, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15100, 200);
    
    assert(book.add_order(buy1));
    assert(book.add_order(buy2));
    
    // Add sell orders
    auto sell1 = std::make_shared<Order>(3, "AAPL", OrderSide::SELL, OrderType::LIMIT, 15200, 150);
    auto sell2 = std::make_shared<Order>(4, "AAPL", OrderSide::SELL, OrderType::LIMIT, 15300, 100);
    
    assert(book.add_order(sell1));
    assert(book.add_order(sell2));
    
    // Check best bid/ask
    assert(book.best_bid() == 15100);  // Highest buy price
    assert(book.best_ask() == 15200);  // Lowest sell price
    
    // Check spread
    auto spread = book.spread();
    assert(spread && *spread == 100);
    
    std::cout << "  ✓ Basic operations passed" << std::endl;
}

void test_cancel_order() {
    std::cout << "Testing order cancellation..." << std::endl;
    
    OrderBook book("AAPL");
    
    auto buy1 = std::make_shared<Order>(1, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15000, 100);
    auto buy2 = std::make_shared<Order>(2, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15100, 200);
    
    book.add_order(buy1);
    book.add_order(buy2);
    
    assert(book.best_bid() == 15100);
    
    // Cancel order 2
    assert(book.cancel_order(2));
    assert(book.best_bid() == 15000);
    
    // Cancel non-existent order
    assert(!book.cancel_order(999));
    
    std::cout << "  ✓ Order cancellation passed" << std::endl;
}

void test_top_of_book() {
    std::cout << "Testing top of book..." << std::endl;
    
    OrderBook book("AAPL");
    
    auto buy1 = std::make_shared<Order>(1, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15000, 100);
    auto buy2 = std::make_shared<Order>(2, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15000, 50);
    auto sell1 = std::make_shared<Order>(3, "AAPL", OrderSide::SELL, OrderType::LIMIT, 15100, 75);
    
    book.add_order(buy1);
    book.add_order(buy2);
    book.add_order(sell1);
    
    auto top = book.get_top_of_book();
    assert(top.bid_price == 15000);
    assert(top.bid_quantity == 150);  // 100 + 50
    assert(top.ask_price == 15100);
    assert(top.ask_quantity == 75);
    
    std::cout << "  ✓ Top of book passed" << std::endl;
}

void test_depth_snapshot() {
    std::cout << "Testing depth snapshot..." << std::endl;
    
    OrderBook book("AAPL");
    
    // Add multiple price levels
    for (int i = 0; i < 5; ++i) {
        auto buy = std::make_shared<Order>(i + 1, "AAPL", OrderSide::BUY, 
                                          OrderType::LIMIT, 15000 - i * 10, 100);
        book.add_order(buy);
    }
    
    auto bid_depth = book.get_bid_depth(3);
    assert(bid_depth.size() == 3);
    assert(bid_depth[0].price == 15000);  // Best bid (highest)
    assert(bid_depth[1].price == 14990);
    assert(bid_depth[2].price == 14980);
    
    std::cout << "  ✓ Depth snapshot passed" << std::endl;
}

int main() {
    std::cout << "=== OrderBook Tests ===" << std::endl;
    
    test_basic_order_book();
    test_cancel_order();
    test_top_of_book();
    test_depth_snapshot();
    
    std::cout << "\n✓ All OrderBook tests passed!" << std::endl;
    return 0;
}

