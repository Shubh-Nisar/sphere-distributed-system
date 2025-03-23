#ifndef PRODUCER_H
#define PRODUCER_H

#include <kafka/KafkaProducer.h>
#include <string>
#include <atomic>

using namespace kafka;
using namespace kafka::clients::producer;

class Producer {
public:
    Producer(const std::string& broker, const Topic& topic);
    ~Producer();

    // Generic method to send any key-value data as strings
    void send(const std::string& key, const std::string& value);

private:
    // Kafka configuration
    const std::string brokers;
    const Topic topic;
    KafkaProducer producer;
};

#endif // PRODUCER_H
