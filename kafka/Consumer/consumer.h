#ifndef CONSUMER_H
#define CONSUMER_H

#include <kafka/KafkaConsumer.h>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <string>
using namespace kafka;
using namespace kafka::clients::consumer;

class Consumer {
public:
    Consumer(const std::string broker, const Topic topic);
private:
    // E.g. KAFKA_BROKER_LIST: "192.168.0.1:9092,192.168.0.2:9092,192.168.0.3:9092"
    const std::string brokers;
    const Topic topic;
};



#endif //CONSUMER_H
