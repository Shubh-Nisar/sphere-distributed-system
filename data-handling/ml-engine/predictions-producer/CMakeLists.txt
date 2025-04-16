cmake_minimum_required(VERSION 3.10)
project(predictions_producer_project)

set(CMAKE_CXX_STANDARD 20)

# Include directories for the project
include_directories(
    "/usr/include"
    "/usr/local/include"
    "../../../kafka/modern-cpp-kafka/include/"
)

link_directories(
    "/usr/lib"
    "/usr/local/lib"
)

# Building the producer as a library
add_library(producer ../../../kafka/Producer/producer.cpp ../../../kafka/Producer/producer.h)

# Constants library, assuming it is used across multiple files
add_library(constants ../../../kafka/Constants/constants.cpp ../../../kafka/Constants/constants.h)

# Executable for fetching and streaming pod metrics
add_executable(ml_producer ml_producer.cpp)

# Linking the ml_producer executable with producer_lib and other dependencies
target_link_libraries(ml_producer
    PRIVATE
    producer
    constants
    rdkafka++  # C++ wrapper for Kafka (MUST come first)
    rdkafka    # Core C library for Kafka
    pthread ssl crypto z curl  # Required dependencies for system and Kafka
)

