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
    docker-compose build \
        --build-arg BUILD_TYPE=${CMAKE_BUILD_TYPE:-Release} \
        --build-arg BUILD_TESTS=${BUILD_TESTS:-OFF}
}

# Function to run tests
run_tests() {
    echo "Running tests..."
    ensure_network
    docker-compose run --rm \
        -e CMAKE_BUILD_TYPE=Debug \
        -e BUILD_TESTS=ON \
        quant_trading \
        bash -c "ldconfig && \
                 cmake -B build -G Ninja \
                       -DCMAKE_BUILD_TYPE=Debug \
                       -DBUILD_TESTS=ON && \
                 cmake --build build && \
                 cd build && \
                 ctest --output-on-failure"
}

# Function to run the server
run_server() {
    echo "Starting trading server..."
    ensure_network
    docker-compose up -d market_server
    echo "Server running on port 8080"
    container_name=$(docker-compose ps -q market_server)
    docker logs -f "$container_name"
}

# Function to run the client
run_client() {
    echo "Starting market client..."
    ensure_network
    docker-compose run --rm \
        -it \
        market_client
}

# Function to run development environment
run_dev() {
    echo "Starting development environment..."
    ensure_network
    docker-compose run --rm \
        -e CMAKE_BUILD_TYPE=Debug \
        -v ${PWD}:/app \
        -it \
        quant_trading \
        bash -c "ldconfig && /bin/bash"
}

# Function to check dependencies
check_deps() {
    echo "Checking dependencies..."
    local deps=(docker docker-compose)
    for dep in "${deps[@]}"; do
        if ! command -v $dep &> /dev/null; then
            echo "Error: $dep is required but not installed"
            exit 1
        fi
    done
}

# Function to show logs
show_logs() {
    local service=$1
    if [ -z "$service" ]; then
        docker-compose logs -f
    else
        docker-compose logs -f $service
    fi
}

# Main script
check_deps

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
    "logs")
        show_logs $2
        ;;
    *)
        echo "Usage: $0 {build|server|client|test|dev|clean|logs}"
        echo "  build  - Build the Docker image"
        echo "  server - Run the trading server"
        echo "  client - Run the market client"
        echo "  test   - Run all tests"
        echo "  dev    - Start development environment"
        echo "  clean  - Clean up containers"
        echo "  logs   - Show container logs (optional: specify service name)"
        exit 1
        ;;
esac
