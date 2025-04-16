cmake_minimum_required(VERSION 3.10)
project(MLHandler)

set(CMAKE_CXX_STANDARD 17)  

include_directories(
    "/usr/local/include"  
    "/usr/include"
    "../../../kafka/modern-cpp-kafka/include/"
)

link_directories(
    "/usr/local/lib" 
    "/usr/lib" 
)

add_library(constants ../../../kafka/Constants/constants.cpp ../../../kafka/Constants/constants.h)

add_library(consumer
    ../../../kafka/Consumer/consumer.cpp 
    ../../../kafka/Consumer/consumer.h
)

add_executable(ml_handler ml_handler.cpp)

target_link_libraries(ml_handler
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
)
