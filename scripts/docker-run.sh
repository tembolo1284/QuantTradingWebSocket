#!/bin/bash

# Function to stop and remove existing containers
cleanup() {
    echo "Cleaning up existing containers..."
    docker-compose down
    docker network rm trading-net 2>/dev/null || true
    docker system prune -f
}

# Function to ensure network exists
ensure_network() {
    if ! docker network inspect trading-net >/dev/null 2>&1; then
        echo "Creating trading-net network..."
        docker network create trading-net
    fi
}

# Function to build the Docker image
build_image() {
    echo "Building Docker image..."
    ensure_network
    docker-compose build
}

# Function to run tests
run_tests() {
    echo "Running tests..."
    ensure_network
    docker-compose run --rm \
        -e CMAKE_BUILD_TYPE=Debug \
        -e BUILD_TESTS=ON \
        quant_trading \
        bash -c "cmake -B build -G Ninja -DBUILD_TESTS=ON && \
                 cmake --build build && \
                 cd build && \
                 ctest --output-on-failure"
}

# Function to run the server
run_server() {
    echo "Starting trading server..."
    ensure_network
    docker-compose up quant_trading
}

# Function to run the client
run_client() {
    echo "Starting market client..."
    ensure_network
    docker-compose run --rm --service-ports market_client
}

# Function to run development environment
run_dev() {
    echo "Starting development environment..."
    ensure_network
    docker-compose run --rm \
        -e CMAKE_BUILD_TYPE=Debug \
        quant_trading \
        /bin/bash
}

# Main script
case "$1" in
    "build")
        build_image
        ;;
    "server")
        cleanup
        run_server
        ;;
    "client")
        run_client
        ;;
    "test")
        run_tests
        ;;
    "dev")
        run_dev
        ;;
    "clean")
        cleanup
        ;;
    *)
        echo "Usage: $0 {build|server|client|test|dev|clean}"
        echo "  build  - Build the Docker image"
        echo "  server - Run the trading server"
        echo "  client - Run the market client"
        echo "  test   - Run all tests"
        echo "  dev    - Start development environment"
        echo "  clean  - Clean up containers"
        exit 1
        ;;
esac
