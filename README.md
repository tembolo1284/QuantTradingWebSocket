# High-Performance Quantitative Trading System

A low-latency order execution and market data management system implemented in pure C, focusing on minimizing latency through custom implementations of critical components.

## System Architecture

### Core Components

#### 1. Order Book Implementation
- **Data Structure**: AVL trees for price levels
  - Why AVL Trees?
    - O(log n) lookup, insertion, and deletion operations
    - Self-balancing ensures consistent performance regardless of order patterns
    - Perfect for price-level indexing where we need quick access to best bid/ask
    - Maintains both price and time priority efficiently

#### 2. WebSocket Communication
- Uses libwebsockets for reliable WebSocket implementation
- Key features:
  - Non-blocking I/O for high throughput
  - Efficient message handling
  - Built-in support for fragmentation
  - Automatic ping/pong handling

#### 3. Trading Engine
- Price-time priority matching
- Efficient order management
- Real-time market data distribution
- Support for order cancellation and modification

### Prerequisites

```bash
# Install basic build tools
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git

# Install required libraries
sudo apt-get install libssl-dev libncurses-dev

# Install libwebsockets from source
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets
mkdir build && cd build
cmake -DLWS_WITH_SSL=ON ..
make
sudo make install
sudo ldconfig

# Install readline
wget https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz
tar xvf readline-8.2.tar.gz
cd readline-8.2
./configure
make
sudo make install
sudo ldconfig

# Clone and set up Unity testing framework
cd third_party
git clone https://github.com/ThrowTheSwitch/Unity.git
cd Unity
mkdir build && cd build
cmake ..
make
sudo make install

## Building

# Configure with Ninja
cmake -B build -G Ninja

# Build everything
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

## Project Structure

QuantTradingWebSocket/

├── include/

│   ├── client/

│   │   ├── client_commands.h

│   │   ├── command_line.h

│   │   ├── market_monitor.h

│   │   ├── order_entry.h

│   │   ├── trade_history.h

│   │   └── ws_client.h

│   ├── protocol/

│   │   ├── json_protocol.h

│   │   ├── message_types.h

│   │   ├── protocol_constants.h

│   │   └── protocol_validation.h

│   ├── server/

│   │   ├── market_data.h

│   │   ├── server_handlers.h

│   │   ├── session_manager.h

│   │   └── ws_server.h

│   ├── trading_engine/

│   │   ├── avl_tree.h

│   │   ├── order_book.h

│   │   ├── order.h

│   │   ├── trade.h

│   │   └── trader.h

│   └── utils/

│       ├── logging.h

│       └── order_loader.h

├── src/

│   ├── client/

│   ├── protocol/

│   ├── server/

│   ├── trading_engine/

│   └── utils/

├── tests/

│   ├── trading_engine/

│   └── utils/

└── third_party/

    ├── cJSON/

    ├── libwebsockets/

    ├── readline-8.2/

    └── Unity/

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

## Features

### Trading Engine

- Price-time priority matching

- Real-time order book updates

- Support for market data subscriptions

- Order cancellation functionality

### Client Interface

- Interactive command-line interface

- Real-time market data display

- Order entry and management

- Trade history tracking

### Server Features

- WebSocket-based communication

- Session management

- Market data distribution

- Order book management

## Build System

### CMake Configuration

The project uses CMake with support for multiple compilers and platforms:

```bash

# Configure with GCC (default)
cmake -B build -G Ninja

# Configure with Clang
cmake -B build -G Ninja -DUSE_CLANG=ON

# Build
cmake --build build

## Key CMake features:

- Multi-compiler support (GCC, Clang)

- Platform-specific optimizations

- Configurable build options:

    - BUILD_TESTS: Enable/disable test builds

    - ENABLE_ASAN: Enable Address Sanitizer

    - USE_CLANG: Switch to Clang compiler

### Docker Support

## Building with Docker

```bash

# Build using provided Dockerfile
docker build -t market_server .

# Run the server
docker run -p 8080:8080 market_server /market_server

# Run the client
docker run -it market_server /market_client

## Docker Run Script

A convenient `docker-run.sh` script provides common operations:

```bash
./docker-run.sh [command]

Commands:
  build   - Build the Docker image
  server  - Run the trading server
  client  - Run the market client
  test    - Run all tests
  dev     - Start development environment
  clean   - Clean up containers

## Continuous Integration

The project uses GitHub Actions for CI/CD with the following workflow:

## Matrix Testing

    - Ubuntu with GCC and Clang

## Automated Checks

    - Compilation across all platforms
    - Unit test execution
    - Sanitizer checks
    - Docker build verification
    - Artifact generation

## Dependencies

CI automatically handles:

    - Unity test framework setup
    - cJSON library integration
    - Platform-specific dependencies
    - Compiler-specific configurations

To enable CI:

1. Place .github/workflows/ci.yml in your repository

2. Ensure third_party/ directory exists

3. Push to GitHub
