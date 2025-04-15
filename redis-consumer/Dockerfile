FROM ubuntu:20.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    pkg-config \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install a specific version of librdkafka
RUN git clone https://github.com/edenhill/librdkafka.git \
    && cd librdkafka \
    && git checkout v2.4.0 \
    && ./configure \
    && make \
    && make install \
    && ldconfig

# Install hiredis
RUN git clone https://github.com/redis/hiredis.git \
    && cd hiredis \
    && make \
    && make install \
    && ldconfig

# Install redis-plus-plus
RUN git clone https://github.com/sewenew/redis-plus-plus.git \
    && cd redis-plus-plus \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make \
    && make install \
    && ldconfig

# Create app directory
WORKDIR /app

# Copy necessary files
COPY redis-consumer/ /app/redis-consumer/
COPY kafka/ /app/kafka/

# Build the application
WORKDIR /app/redis-consumer
RUN rm -rf build && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    make

# Run the application
CMD ["./build/redis_consumer"]
