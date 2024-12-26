# Build stage
FROM ubuntu:22.04 AS builder

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
    libncurses-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Set work directory
WORKDIR /app

# Build and install libwebsockets
RUN git clone https://github.com/warmcat/libwebsockets.git && \
    cd libwebsockets && \
    mkdir build && \
    cd build && \
    cmake -DLWS_WITH_SSL=ON .. && \
    make && \
    make install && \
    cd ../.. && \
    rm -rf libwebsockets

# Build and install readline
RUN wget https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz && \
    tar xvf readline-8.2.tar.gz && \
    cd readline-8.2 && \
    ./configure && \
    make && \
    make install && \
    cd .. && \
    rm -rf readline-8.2 readline-8.2.tar.gz

# Update library cache
RUN ldconfig

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
    libncurses6 \
    && rm -rf /var/lib/apt/lists/*

# Copy libwebsockets from builder
COPY --from=builder /usr/local/lib/libwebsockets.so* /usr/local/lib/

# Copy readline from builder
COPY --from=builder /usr/local/lib/libreadline.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libhistory.so* /usr/local/lib/

# Update library cache
RUN ldconfig

# Create non-root user
RUN useradd -m -s /bin/bash trader

# Copy built binaries from builder stage
COPY --from=builder /app/build/market_server /usr/local/bin/
COPY --from=builder /app/build/market_client /usr/local/bin/

# Create directory for any runtime data
RUN mkdir -p /var/lib/trading && chown trader:trader /var/lib/trading

# Switch to non-root user
USER trader
WORKDIR /home/trader

# Set default command (can be overridden)
CMD ["market_server"]
