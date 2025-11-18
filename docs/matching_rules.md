# Matching Rules

## Overview

Falcon Exchange implements a **price-time priority** matching algorithm, which is the standard for most electronic exchanges.

## Priority Rules

### 1. Price Priority

Orders are matched first by price:
- **Buy orders**: Highest price matched first
- **Sell orders**: Lowest price matched first

**Example:**
```
Bid Side:
  150.10 @ 100 (Order A)
  150.00 @ 200 (Order B)
  149.90 @ 150 (Order C)

Ask Side:
  150.20 @ 100 (Order D)
  150.30 @ 200 (Order E)
```

If a sell order arrives at 150.05, it will match with Order A (150.10) first, even though Order B was placed earlier.

### 2. Time Priority

At the same price, orders are matched in the order they were received (FIFO).

**Example:**
```
Bid Side at 150.00:
  150.00 @ 100 (Order A, received at 10:00:00.000)
  150.00 @ 200 (Order B, received at 10:00:00.001)
  150.00 @ 150 (Order C, received at 10:00:00.002)
```

If a sell order arrives at 150.00, it will match with Order A first, then Order B, then Order C.

## Order Types

### Limit Orders

A limit order specifies a maximum (buy) or minimum (sell) price.

**Matching Behavior:**
1. Try to match immediately against opposite side
2. If fully matched, order is filled
3. If partially matched, remaining quantity rests in book
4. If not matched, order rests in book at specified price

**Example:**
```
Book State:
  Bid: 150.00 @ 100
  Ask: 150.10 @ 200

Incoming: Buy limit @ 150.05, quantity 50
Result: No match (150.05 < 150.10), order rests in book

Incoming: Buy limit @ 150.10, quantity 50
Result: Matches with ask @ 150.10, fills 50, ask has 150 remaining
```

### Market Orders

A market order matches immediately at the best available price.

**Matching Behavior:**
1. Match immediately against best opposite price
2. Continue matching until order is filled or liquidity exhausted
3. Never rests in book
4. Rejected if no liquidity available

**Example:**
```
Book State:
  Bid: 150.00 @ 100
  Ask: 150.10 @ 200

Incoming: Market buy, quantity 150
Result: Matches with ask @ 150.10
  - Fills 150 at 150.10
  - Ask has 50 remaining
  - Market order is filled
```

## Matching Algorithm

### Pseudocode

```
function match_order(incoming_order):
    while incoming_order.remaining_quantity > 0:
        if incoming_order.side == BUY:
            best_ask = order_book.get_best_ask()
            if best_ask == null:
                if incoming_order.type == MARKET:
                    reject_order(incoming_order)
                else:
                    add_to_book(incoming_order)
                break
            
            if incoming_order.type == LIMIT and 
               incoming_order.price < best_ask.price:
                add_to_book(incoming_order)
                break
            
            resting_order = best_ask.get_front_order()
            match_price = resting_order.price  // Price-time priority
            match_quantity = min(
                incoming_order.remaining_quantity,
                resting_order.remaining_quantity
            )
            
            execute_trade(incoming_order, resting_order, 
                         match_price, match_quantity)
            
            if resting_order.is_filled():
                remove_from_book(resting_order)
        
        else:  // SELL
            best_bid = order_book.get_best_bid()
            if best_bid == null:
                if incoming_order.type == MARKET:
                    reject_order(incoming_order)
                else:
                    add_to_book(incoming_order)
                break
            
            if incoming_order.type == LIMIT and 
               incoming_order.price > best_bid.price:
                add_to_book(incoming_order)
                break
            
            resting_order = best_bid.get_front_order()
            match_price = resting_order.price  // Price-time priority
            match_quantity = min(
                incoming_order.remaining_quantity,
                resting_order.remaining_quantity
            )
            
            execute_trade(resting_order, incoming_order, 
                         match_price, match_quantity)
            
            if resting_order.is_filled():
                remove_from_book(resting_order)
```

## Trade Execution

### Trade Price

The trade price is determined by the **resting order's price** (price-time priority).

**Example:**
```
Book State:
  Bid: 150.00 @ 100 (resting order)

Incoming: Sell limit @ 149.95, quantity 50
Trade Price: 150.00 (resting order's price)
```

This ensures that resting orders get price improvement when possible.

### Partial Fills

Orders can be partially filled:

```
Order: Buy limit @ 150.00, quantity 1000

Match 1: 200 @ 150.00 → 200 filled, 800 remaining
Match 2: 300 @ 150.00 → 500 filled, 500 remaining
Match 3: 500 @ 150.00 → 1000 filled, order complete
```

## Order Status Transitions

```
NEW → PARTIALLY_FILLED → FILLED
NEW → CANCELLED
NEW → REJECTED (market order with no liquidity)
```

## Examples

### Example 1: Simple Match

```
Initial Book:
  Bid: 150.00 @ 100
  Ask: 150.10 @ 200

Order: Buy limit @ 150.10, quantity 50

Result:
  Trade: 50 @ 150.10
  Book:
    Bid: 150.00 @ 100
    Ask: 150.10 @ 150
```

### Example 2: Price Improvement

```
Initial Book:
  Bid: 150.00 @ 100
  Ask: 150.10 @ 200

Order: Sell limit @ 149.95, quantity 50

Result:
  Trade: 50 @ 150.00 (resting order gets price improvement)
  Book:
    Bid: 150.00 @ 50
    Ask: 150.10 @ 200
```

### Example 3: Multiple Matches

```
Initial Book:
  Bid: 150.00 @ 100
  Bid: 149.90 @ 200
  Ask: 150.10 @ 150
  Ask: 150.20 @ 200

Order: Market buy, quantity 200

Result:
  Trade 1: 150 @ 150.10
  Trade 2: 50 @ 150.20
  Book:
    Bid: 150.00 @ 100
    Bid: 149.90 @ 200
    Ask: 150.20 @ 150
```

### Example 4: Time Priority

```
Initial Book (all at 150.00):
  Bid: 150.00 @ 100 (Order A, 10:00:00.000)
  Bid: 150.00 @ 200 (Order B, 10:00:00.001)
  Bid: 150.00 @ 150 (Order C, 10:00:00.002)

Order: Sell limit @ 150.00, quantity 250

Result:
  Trade 1: 100 @ 150.00 (Order A filled)
  Trade 2: 150 @ 150.00 (Order B partially filled, 50 remaining)
  Book:
    Bid: 150.00 @ 50 (Order B remaining)
    Bid: 150.00 @ 150 (Order C)
```

## Edge Cases

### Empty Book

- Limit order: Rests in book
- Market order: Rejected

### No Matchable Price

- Limit buy @ 150.00, best ask @ 150.10: Rests in book
- Limit sell @ 150.10, best bid @ 150.00: Rests in book

### Exact Price Match

- Buy @ 150.00, Ask @ 150.00: Matches immediately

### Market Order Exhaustion

- Market order partially fills, then liquidity exhausted: Remaining quantity rejected

## Performance Considerations

1. **Best Price Lookup**: O(1) with ordered map (rbegin/begin)
2. **Order Insertion**: O(log n) map lookup + O(1) price level insert
3. **Order Matching**: O(k) where k is number of price levels to match through
4. **Time Priority**: O(1) access to front of deque

## Future Enhancements

- **Iceberg Orders**: Hidden quantity, only show display size
- **Stop Orders**: Triggered by price movement
- **Time-in-Force**: IOC (Immediate-or-Cancel), FOK (Fill-or-Kill)
- **Order Types**: Post-only, reduce-only

