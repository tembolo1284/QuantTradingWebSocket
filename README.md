# High-Performance Quantitative Trading System

A low-latency order execution and market data management system implemented in pure C, focusing on minimizing latency through custom implementations of critical components.

## System Architecture

### Core Components

#### 1. Order Book Implementation
- **Data Structure**: AVL trees for price levels with linked lists for time priority
  - Why AVL Trees?
    - O(log n) lookup, insertion, and deletion operations
    - Self-balancing ensures consistent performance regardless of order patterns
    - Better than Red-Black trees for our use case as they maintain stricter balance
    - Perfect for price-level indexing where we need quick access to best bid/ask
  - Why Linked Lists for Time Priority?
    - O(1) insertion at each price level
    - Natural FIFO ordering preserves time priority
    - Memory efficient for variable number of orders at each price level

#### 2. WebSocket Implementation
- Custom implementation without external dependencies for maximum control
- Key features:
  - Zero-copy buffer management to minimize memory operations
  - Non-blocking I/O for high throughput
  - Efficient frame parsing with minimal memory allocation
  - Built-in support for fragmentation and message continuation

#### 3. Memory Management
- Custom memory pooling for frequently allocated structures
- Zero-copy operations where possible
- Careful alignment considerations for modern CPU architectures
- Minimized heap allocations in critical paths

### Performance Optimizations

1. **Lock-Free Data Structures**
   - Atomic operations for order ID generation
   - Lock-free ring buffers for message passing
   - Designed for multi-threaded market data handling

2. **Cache Optimization**
   - Struct alignment for cache line efficiency
   - Hot/cold data separation
   - Minimized pointer chasing in critical paths

3. **Network Optimization**
   - Custom WebSocket implementation for minimal overhead
   - Zero-copy buffer management
   - Efficient binary message formats

## Building and Testing

### Prerequisites
```bash
# Install basic build tools
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git

# Clone and set up Unity testing framework
cd third_party
git clone https://github.com/ThrowTheSwitch/Unity.git
cd Unity
mkdir build && cd build
cmake ..
make
sudo make install
```

### Building
```bash
# Configure with Ninja
cmake -B build -G Ninja

# Build everything
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## Project Structure
```
QuantTradingWebSocket/

├── include/

│   ├── common.h                 # Common definitions

│   ├── net/

│   │   ├── websocket.h         # WebSocket interface

│   │   ├── frame.h            # Frame parsing

│   │   └── buffer.h           # Zero-copy buffers

│   └── trading/

│       ├── order_book.h        # Order book management

│       └── order.h            # Order handling

├── src/

│   ├── net/                    # Network implementations

│   └── trading/               # Trading logic implementations

├── tests/                     # Unity-based tests

├── examples/

│   ├── market_maker/          # Market making example

│   └── order_book_viewer/     # Book visualization

└── third_party/              # External dependencies

```

## Performance Characteristics

### Order Book Operations
- Order insertion: O(log n) where n is number of price levels
- Order cancellation: O(log n)
- Best bid/ask lookup: O(1)
- Price level lookup: O(log n)
- Time priority maintenance: O(1) within price level

### Memory Usage
- Fixed overhead per price level: ~32 bytes
- Per order overhead: ~48 bytes
- Dynamic memory allocation only during order insertion
- Memory pooling for high-frequency operations

### Network Performance
- Frame parsing: O(1) for control frames
- Message handling: Zero-copy for standard message sizes
- Connection overhead: Minimal with custom WebSocket implementation

## Design Decisions

### 1. AVL Trees vs Alternatives
Considered alternatives:
- **Red-Black Trees**: Slightly faster insertions but less balanced
- **Skip Lists**: More memory overhead, probabilistic balance
- **Hash Tables**: No ordered operations, unsuitable for best bid/ask
- **B-Trees**: Better for disk-based systems, overkill for in-memory

AVL trees chosen because:
- Guaranteed O(log n) operations
- Strict balancing ideal for market data patterns
- Memory efficient for our use case
- Excellent cache locality within nodes

### 2. Custom WebSocket Implementation
Reasons for custom implementation:
- Control over memory allocation
- Minimized latency
- No external dependencies
- Optimized for our specific use case
- Bietter integration with our buffer management

### 3. Memory Management Strategy
- Pooled allocations for orders and price levels
- Arena allocation for network buffers
- Stack allocation for temporary objects
- Careful use of static allocation where appropriate

## Examples

### Market Maker
Simple market making strategy demonstrating:
- Order book interaction
- Price updates handling
- Market data processing
- Basic spread maintenance

### Order Book Viewer
Real-time visualization showing:
- Price level aggregation
- Order book depth
- Market data updates
- Trading activity monitoring

## Future Enhancements

1. **Performance**
   - SIMD optimizations for order matching
   - Custom memory allocator
   - Hardware timestamp support
   - TCP kernel bypass (DPDK/XDP)

2. **Features**
   - Multi-asset support
   - Complex order types
   - Risk management layer
   - FIX protocol support

3. **Monitoring**
   - Latency statistics
   - Memory usage tracking
   - Order flow analytics
   - Performance profiling
