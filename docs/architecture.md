# Falcon Exchange Architecture

## Overview

Falcon Exchange is a low-latency matching engine implementing a central limit order book (CLOB). The design prioritizes:
1. **Latency**: Sub-microsecond order processing
2. **Throughput**: Millions of orders per second
3. **Correctness**: Price-time priority matching
4. **Scalability**: Symbol-per-core threading model

## System Architecture

```
┌─────────────┐
│   Client    │
│  (Orders)   │
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│  Event Queue    │  (Lock-free SPSC)
│  (Ring Buffer)  │
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│ Matching Engine │  (Single-threaded per symbol)
│     Thread      │
└──────┬──────────┘
       │
       ├──► OrderBook (Bid/Ask Trees)
       │
       ├──► Trade Events
       │
       └──► Market Data Publisher
```

## Component Details

### Order

Represents a single order in the system.

**Fields:**
- `id`: Unique order identifier
- `symbol`: Trading instrument
- `side`: BUY or SELL
- `type`: LIMIT or MARKET
- `price`: Price in ticks (int64_t for fixed-point)
- `quantity`: Total order quantity
- `filled_quantity`: Quantity already filled
- `status`: NEW, PARTIALLY_FILLED, FILLED, CANCELLED, REJECTED
- `timestamp`: Microsecond-resolution timestamp
- `sequence_number`: For price-time priority

**Design Notes:**
- Uses `int64_t` for prices to avoid floating-point rounding errors
- Timestamps use `std::chrono::steady_clock` for monotonic time

### PriceLevel

Aggregates all orders at a single price point.

**Data Structure:**
- `std::deque<std::shared_ptr<Order>>`: FIFO queue for time priority
- `total_quantity`: Sum of all order quantities at this level

**Operations:**
- `add_order()`: O(1) append to deque
- `remove_order()`: O(n) linear search (could be optimized with hash map)
- `front_order()`: O(1) access to oldest order

**Design Notes:**
- Deque chosen for O(1) push/pop at both ends
- Maintains total quantity for fast depth queries

### OrderBook

Maintains separate bid and ask sides of the book.

**Data Structure:**
- `std::map<int64_t, PriceLevel> bids_`: Ordered by price (ascending)
- `std::map<int64_t, PriceLevel> asks_`: Ordered by price (ascending)

**Operations:**
- `add_order()`: O(log n) map lookup + O(1) price level insert
- `best_bid()`: O(1) access to highest bid (rbegin)
- `best_ask()`: O(1) access to lowest ask (begin)
- `cancel_order()`: O(n) search through price levels

**Design Notes:**
- `std::map` provides ordered iteration for depth snapshots
- For ultra-low latency, could replace with:
  - Custom skip list (O(1) average case)
  - Sorted array with binary search (cache-friendly)
  - Hash map + sorted price list (hybrid)

### MatchingEngine

Core matching logic running in a dedicated thread.

**Threading Model:**
- One thread per symbol (symbol-per-core)
- Lock-free event queue for order submission
- Single-threaded matching (no locks needed)

**Matching Algorithm:**

```
1. Receive order event
2. If MARKET order:
   - Match immediately against best opposite price
   - Reject if no liquidity
3. If LIMIT order:
   - Try to match against opposite side
   - If remaining quantity, add to book
4. Emit trade events for matches
5. Update order status
```

**Price-Time Priority:**
1. **Price Priority**: Best bid/ask matched first
2. **Time Priority**: At same price, oldest order matched first

### EventQueue

Lock-free single-producer single-consumer queue.

**Implementation:**
- Ring buffer with atomic head/tail pointers
- Cache-line aligned to avoid false sharing
- Power-of-2 size for efficient modulo

**Memory Ordering:**
- Producer: `relaxed` load tail, `release` store tail
- Consumer: `relaxed` load head, `acquire` load tail

**Design Notes:**
- SPSC model eliminates need for compare-and-swap
- Wait-free for both producer and consumer

### MarketDataPublisher

Publishes market data updates.

**Update Types:**
- Top-of-book: Best bid/ask with quantities
- Depth snapshot: Full order book depth
- Trade: Executed trade information

**Design Notes:**
- Callback-based for flexibility
- In production, would publish via:
  - FIX protocol
  - Binary protocols (ITCH, proprietary)
  - WebSocket for web clients

## Performance Optimizations

### Current

1. **Lock-Free Queue**: Eliminates mutex contention
2. **Cache-Line Alignment**: Prevents false sharing
3. **Single-Threaded Matching**: No synchronization overhead
4. **Microsecond Timestamps**: High-resolution timing

### Future Optimizations

1. **Custom Allocators**:
   ```cpp
   // Pool allocation for orders
   class OrderPool {
       std::vector<Order> pool_;
       std::stack<Order*> free_list_;
   };
   ```

2. **Structure of Arrays (SoA)**:
   ```cpp
   // Instead of array of Order structs
   struct OrderBookSoA {
       std::vector<uint64_t> ids;
       std::vector<int64_t> prices;
       std::vector<int64_t> quantities;
   };
   ```

3. **Custom Skip List**:
   - O(1) average case insert/lookup
   - Better cache locality than std::map
   - Predictable memory layout

4. **SIMD Operations**:
   - Batch process multiple orders
   - Vectorized price comparisons

5. **NUMA Awareness**:
   - Pin threads to specific CPU cores
   - Allocate memory on local NUMA node

## Threading Model

### Current: Symbol-Per-Core

```
Symbol: AAPL  →  Thread 0  →  CPU Core 0
Symbol: MSFT  →  Thread 1  →  CPU Core 1
Symbol: GOOGL →  Thread 2  →  CPU Core 2
```

**Benefits:**
- No contention between symbols
- Predictable performance
- Easy to scale horizontally

**Limitations:**
- One symbol per thread (could shard by price range)

### Future: Sharding

```
Symbol: AAPL
  Price Range 0-100  →  Thread 0
  Price Range 100-200 → Thread 1
```

## Memory Layout

### Order Structure (Array of Structs - AoS)

```
[Order][Order][Order]...
```

**Pros:**
- Easy to work with
- Good for single order access

**Cons:**
- Poor cache utilization when accessing single fields
- Cache misses when iterating over prices

### Future: Structure of Arrays (SoA)

```
ids:      [id1][id2][id3]...
prices:   [p1][p2][p3]...
quantities: [q1][q2][q3]...
```

**Pros:**
- Better cache locality for field-based operations
- SIMD-friendly

**Cons:**
- More complex code
- Poor for single order access

## Latency Breakdown

Typical order processing latency:

1. **Event Queue Push**: ~50-100 ns (lock-free)
2. **Event Queue Pop**: ~50-100 ns (lock-free)
3. **Order Matching**: ~200-500 ns (depends on book depth)
4. **Trade Event**: ~50-100 ns (callback)

**Total**: ~350-800 ns per order (in ideal conditions)

## Scalability

### Vertical Scaling

- Single symbol: Limited by CPU core speed
- Multiple symbols: Linear scaling with CPU cores

### Horizontal Scaling

- Shard by symbol: Each server handles subset of symbols
- Shard by price range: Distribute price levels across servers

## Testing Strategy

1. **Unit Tests**: Individual component correctness
2. **Integration Tests**: End-to-end order flow
3. **Performance Tests**: Throughput and latency benchmarks
4. **Stress Tests**: High order rate, large book depth

## Production Considerations

1. **Order Persistence**: Write-ahead log for recovery
2. **Risk Checks**: Position limits, order size limits
3. **Network Protocol**: FIX adapter for order entry
4. **Monitoring**: Latency metrics, order rate, book depth
5. **Failover**: Hot standby matching engine

