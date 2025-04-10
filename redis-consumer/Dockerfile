# redis-consumer/Dockerfile

FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential cmake git wget \
    pkg-config libssl-dev libhiredis-dev \
    librdkafka-dev nlohmann-json3-dev

# Install redis-plus-plus from source
RUN git clone https://github.com/sewenew/redis-plus-plus.git && \
    cd redis-plus-plus && \
    mkdir build && cd build && \
    cmake .. -DREDIS_PLUS_PLUS_BUILD_STATIC=OFF && \
    make && make install

# Create working directory
WORKDIR /app

# Copy code
COPY ./src /app/src
COPY CMakeLists.txt /app

# Build the consumer
RUN cmake . && make

CMD ["./redis_consumer"]
