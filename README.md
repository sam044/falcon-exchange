# Falcon Exchange

**Low-Latency Limit Order Book + Matching Engine**

A high-performance C++ implementation of a central limit order book (CLOB) matching engine, designed to demonstrate expertise in:
- Market microstructure and exchange mechanics
- Low-latency systems programming
- Concurrent data structures
- Cache-friendly memory layouts

## Features

- **Price-Time Priority Matching**: Orders matched by best price first, then by time of arrival
- **Order Types**: Limit and market orders with partial fills
- **Order Management**: Cancel and replace operations
- **Lock-Free Event Queue**: Single-producer single-consumer queue for thread communication
- **Symbol-Per-Core Model**: One matching engine thread per instrument (ready for sharding)
- **Market Data Publishing**: Top-of-book and full depth snapshots
- **High-Resolution Timestamps**: Microsecond precision using `std::chrono::steady_clock`

## Architecture

### Core Components

1. **Order**: Represents a single order with id, symbol, side, price, quantity, and status
2. **PriceLevel**: Aggregates orders at a single price point (FIFO queue for time priority)
3. **OrderBook**: Maintains separate bid/ask trees using `std::map` (O(log n) operations)
4. **MatchingEngine**: Processes orders, matches them, and emits trade events
5. **EventQueue**: Lock-free ring buffer for passing events to engine threads
6. **MarketDataPublisher**: Publishes market data updates (top-of-book, depth, trades)

### Matching Rules

- **Price Priority**: Best bid/ask prices matched first
- **Time Priority**: At the same price, earlier orders matched first
- **Limit Orders**: Match immediately if possible, otherwise rest in book
- **Market Orders**: Match immediately against best available price, never rest in book

## Building

### Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Run

```bash
# Run demo
./falcon-exchange

# Run tests
ctest

# Run benchmarks
./bench_throughput [num_orders]
```

## Performance Considerations

### Current Implementation

- Uses `std::map` for price levels (O(log n) insert/lookup)
- Lock-free queue with cache-line aligned head/tail pointers
- Single-threaded matching per symbol (no contention)

### Production Optimizations (Future Work)

1. **Custom Skip List**: Replace `std::map` with cache-friendly skip list for O(1) average case
2. **Custom Allocators**: Pool allocation for orders/price levels to reduce heap fragmentation
3. **Structure of Arrays (SoA)**: Store orders in separate arrays by field for better cache locality
4. **SIMD Operations**: Use vectorized operations for batch processing
5. **NUMA Awareness**: Pin threads to specific CPU cores
6. **Zero-Copy Networking**: Direct memory mapping for order entry

## Benchmark Results

Example output from `bench_throughput`:

```
=== Throughput Benchmark ===
Orders to process: 100000
  Throughput: 500000 orders/sec
  Average latency: 2.0 microseconds
  P99 latency: 5.0 microseconds
```

## Project Structure

```
falcon-exchange/
  src/
    order.hpp              # Order data structure
    price_level.hpp        # Price level aggregation
    order_book.hpp         # Central limit order book
    matching_engine.hpp   # Matching logic
    event_queue.hpp        # Lock-free queue
    market_data_publisher.hpp  # Market data publishing
    main.cpp               # Demo application
  tests/
    test_order_book.cpp    # OrderBook unit tests
    test_matching_engine.cpp  # MatchingEngine tests
  benchmarks/
    bench_throughput.cpp   # Performance benchmarks
  docs/
    architecture.md        # Detailed architecture docs
    matching_rules.md      # Matching algorithm details
  CMakeLists.txt
  README.md
```

## Usage Example

```cpp
#include "matching_engine.hpp"

using namespace falcon;

// Create matching engine
MatchingEngine engine("AAPL");
engine.set_trade_callback([](const Trade& trade) {
    std::cout << "Trade: " << trade.price 
              << "@" << trade.quantity << std::endl;
});

engine.start();

// Submit orders
auto buy_order = std::make_shared<Order>(
    1, "AAPL", OrderSide::BUY, OrderType::LIMIT, 15000, 100
);
engine.submit_order(buy_order);

// ... process more orders ...

engine.stop();
```

## Design Decisions

1. **Price Representation**: Uses `int64_t` for prices (fixed-point, e.g., cents for USD)
2. **Threading Model**: One engine thread per symbol (can be extended to sharding)
3. **Memory Management**: Uses `std::shared_ptr` for orders (could be optimized with custom allocators)
4. **Event Processing**: Asynchronous event queue to decouple order submission from matching

## License

MIT License - feel free to use this as a portfolio project or learning resource.

## Author

Built to demonstrate systems programming and quantitative finance expertise.

