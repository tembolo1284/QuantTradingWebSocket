# Build stage
FROM ubuntu:22.04 as builder

# Prevent timezone prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libssl-dev \
    git \
    clang \
    && rm -rf /var/lib/apt/lists/*

# Set work directory
WORKDIR /app

# Copy source code
COPY . .

# Create build directory and run CMake
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    && cmake --build build

# Production stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash trader

# Copy built binaries from builder stage
COPY --from=builder /app/build/quant_trading /usr/local/bin/
COPY --from=builder /app/build/market_client /usr/local/bin/

# Switch to non-root user
USER trader

# Set default command (can be overridden)
CMD ["quant_trading"]
