cmake_minimum_required(VERSION 3.10)
project(RedisConsumer)

set(CMAKE_CXX_STANDARD 17)

# Find required libraries
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)

find_path(REDIS_PLUS_PLUS_INCLUDE_DIR sw/redis++/redis++.h)
find_library(REDIS_PLUS_PLUS_LIBRARY redis++)

# Include directories
include_directories(
    ${HIREDIS_INCLUDE_DIR}
    ${REDIS_PLUS_PLUS_INCLUDE_DIR}
    "/app/kafka/modern-cpp-kafka/include/"
)

# Add libraries
add_library(constants /app/kafka/Constants/constants.cpp /app/kafka/Constants/constants.h)
add_library(consumer /app/kafka/Consumer/consumer.cpp /app/kafka/Consumer/consumer.h)

# Add executable
add_executable(redis_consumer redis_consumer.cpp)

# Link libraries
target_link_libraries(redis_consumer
    PRIVATE
    constants
    consumer
    rdkafka++
    rdkafka
    pthread
    ssl
    crypto
    z
    curl
    ${HIREDIS_LIBRARY}
    ${REDIS_PLUS_PLUS_LIBRARY}
)
