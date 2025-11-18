#include "../src/matching_engine.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <thread>
#include <iomanip>
#include <algorithm>

using namespace falcon;

class ThroughputBenchmark {
public:
    void run(uint64_t num_orders) {
        std::cout << "=== Throughput Benchmark ===" << std::endl;
        std::cout << "Orders to process: " << num_orders << std::endl;
        
        MatchingEngine engine("AAPL");
        uint64_t trades_executed = 0;
        
        engine.set_trade_callback([&trades_executed](const Trade&) {
            trades_executed++;
        });
        
        engine.start();
        
        // Generate orders
        std::vector<std::shared_ptr<Order>> orders;
        orders.reserve(num_orders);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int64_t> price_dist(14900, 15100);
        std::uniform_int_distribution<int64_t> qty_dist(1, 100);
        std::uniform_int_distribution<int> side_dist(0, 1);
        
        for (uint64_t i = 0; i < num_orders; ++i) {
            OrderSide side = (side_dist(gen) == 0) ? OrderSide::BUY : OrderSide::SELL;
            int64_t price = price_dist(gen);
            int64_t quantity = qty_dist(gen);
            
            auto order = std::make_shared<Order>(
                i + 1, "AAPL", side, OrderType::LIMIT, price, quantity
            );
            orders.push_back(order);
        }
        
        // Benchmark: submit all orders
        auto start = std::chrono::high_resolution_clock::now();
        
        for (auto& order : orders) {
            engine.submit_order(order);
        }
        
        // Wait for all orders to be processed
        while (true) {
            auto stats = engine.get_statistics();
            if (stats.orders_processed >= num_orders) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        auto stats = engine.get_statistics();
        
        // Calculate metrics
        double orders_per_sec = (num_orders * 1e6) / duration.count();
        double avg_latency_us = duration.count() / static_cast<double>(num_orders);
        
        std::cout << "\nResults:" << std::endl;
        std::cout << "  Total time: " << duration.count() << " microseconds" << std::endl;
        std::cout << "  Orders processed: " << stats.orders_processed << std::endl;
        std::cout << "  Trades executed: " << stats.trades_executed << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << orders_per_sec << " orders/sec" << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(2)
                  << avg_latency_us << " microseconds" << std::endl;
        
        engine.stop();
    }
    
    void latency_test(uint64_t num_orders) {
        std::cout << "\n=== Latency Benchmark ===" << std::endl;
        std::cout << "Orders to process: " << num_orders << std::endl;
        
        MatchingEngine engine("AAPL");
        std::vector<std::chrono::microseconds> latencies;
        latencies.reserve(num_orders);
        
        engine.start();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int64_t> price_dist(14900, 15100);
        std::uniform_int_distribution<int64_t> qty_dist(1, 100);
        std::uniform_int_distribution<int> side_dist(0, 1);
        
        for (uint64_t i = 0; i < num_orders; ++i) {
            OrderSide side = (side_dist(gen) == 0) ? OrderSide::BUY : OrderSide::SELL;
            int64_t price = price_dist(gen);
            int64_t quantity = qty_dist(gen);
            
            auto order = std::make_shared<Order>(
                i + 1, "AAPL", side, OrderType::LIMIT, price, quantity
            );
            
            auto submit_time = std::chrono::steady_clock::now();
            engine.submit_order(order);
            
            // Wait for order to be processed
            while (order->status == OrderStatus::NEW) {
                std::this_thread::yield();
            }
            
            auto process_time = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                process_time - submit_time
            );
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(latency));
        }
        
        // Calculate percentiles
        std::sort(latencies.begin(), latencies.end());
        
        auto p50 = latencies[latencies.size() * 0.50].count();
        auto p95 = latencies[latencies.size() * 0.95].count();
        auto p99 = latencies[latencies.size() * 0.99].count();
        auto p999 = latencies[latencies.size() * 0.999].count();
        auto max = latencies.back().count();
        
        std::cout << "\nLatency Percentiles (microseconds):" << std::endl;
        std::cout << "  P50:  " << p50 << std::endl;
        std::cout << "  P95:  " << p95 << std::endl;
        std::cout << "  P99:  " << p99 << std::endl;
        std::cout << "  P99.9: " << p999 << std::endl;
        std::cout << "  Max:  " << max << std::endl;
        
        engine.stop();
    }
};

int main(int argc, char* argv[]) {
    uint64_t num_orders = 100000;
    
    if (argc > 1) {
        num_orders = std::stoull(argv[1]);
    }
    
    ThroughputBenchmark benchmark;
    benchmark.run(num_orders);
    benchmark.latency_test(std::min(num_orders, 10000ULL));  // Limit latency test
    
    return 0;
}

