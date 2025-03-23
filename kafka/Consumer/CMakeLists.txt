cmake_minimum_required(VERSION 3.10)
project(kafka_consumer)

set(CMAKE_CXX_STANDARD 20)

# Use system paths for Ubuntu instead of Homebrew
include_directories(
    "/usr/include"
    "/usr/local/include"
    "../modern-cpp-kafka/include/"
)

link_directories(
    "/usr/lib"
    "/usr/local/lib"
)

add_library(consumer consumer.cpp
    consumer.h)

add_library(constants ../Constants/constants.cpp ../Constants/constants.h)

target_link_libraries(consumer
    PRIVATE
    constants
    rdkafka++  # C++ wrapper (MUST come first)
    rdkafka    # Core C library
    pthread ssl crypto z curl  # Required dependencies
)
