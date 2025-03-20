//
// Created by SHUBH NISAR on 2/24/25.
//

#ifndef PRODUCER_H
#define PRODUCER_H

#include <kafka/KafkaProducer.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <atomic>
#include <csignal>

using namespace kafka;
using namespace kafka::clients::producer;

class Producer {
public:
    Producer(const std::string broker, const Topic topic);
    
    // Start streaming pod metrics to Kafka
    void streamPodMetrics(int intervalSeconds);
    
    // Stop streaming metrics
    void stop();
    
    // Destructor to ensure proper cleanup
    ~Producer();
    
private:
    // Kafka configuration
    const std::string brokers;
    const Topic topic;
    KafkaProducer producer;
    
    // Control flag for streaming loop
    std::atomic<bool> running;
    
    // Helper methods for pod metrics collection
    int getTotalCPUCapacity();
    long getTotalMemoryCapacity();
    long convertMemoryToKi(const std::string &memoryStr);
    std::string getCurrentTimestamp();
    void fetchAndStreamPodMetrics();
};

#endif //PRODUCER_H
