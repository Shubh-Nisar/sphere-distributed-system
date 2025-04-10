cmake_minimum_required(VERSION 3.10)
project(redis_consumer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find packages
find_package(nlohmann_json REQUIRED)

# Add include directories
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# Add executable
add_executable(redis_consumer src/consumer.cpp)

# Link required libraries
target_link_libraries(redis_consumer
    rdkafka++
    redis++
    hiredis
    nlohmann_json::nlohmann_json
)
