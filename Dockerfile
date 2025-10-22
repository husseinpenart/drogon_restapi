# Build stage: Use Ubuntu 22.04 with build tools
FROM ubuntu:22.04 AS builder

# Install build dependencies with retry logic
RUN apt-get update --fix-missing || true && \
    for i in 1 2 3 4 5; do apt-get update && break || sleep 5; done && \
    apt-get install -y \
    g++ \
    cmake \
    make \
    git \
    libjsoncpp-dev \
    zlib1g-dev \
    libssl-dev \
    libbrotli-dev \
    libuuid1 uuid-dev \
    libpq-dev \
    libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Install Drogon from source
RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init --recursive && \
    mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install

# Install JWT-CPP (if not vendored)
RUN git clone https://github.com/Thalhammer/jwt-cpp.git && \
    cd jwt-cpp && \
    mkdir build && cd build && \
    cmake -DJWT_CMAKE_FILES_INSTALL_DIR=/usr/local/lib/cmake .. && \
    make -j$(nproc) && \
    make install

# Copy project source code
COPY . .

# Build the project
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install

# Runtime stage: Use slim Ubuntu image
FROM ubuntu:22.04

# Install runtime dependencies with retry logic
RUN apt-get update --fix-missing || true && \
    for i in 1 2 3 4 5; do apt-get update && break || sleep 5; done && \
    apt-get install -y \
    libjsoncpp25 \
    zlib1g \
    libssl3 \
    libbrotli1 \
    libuuid1 \
    libpq5 \
    libfmt8 \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /appictional

# Copy the built binary and config
COPY --from=builder /app/build/drogon_restapi /app/drogon_restapi
COPY --from=builder /app/config.json /app/config.json

# Expose port (Drogon default is 8080)
EXPOSE 8080

# Run the app
CMD ["/app/drogon_restapi"]