#include "producer.h"
#include <iostream>

Producer::Producer(const std::string& broker, const Topic& topic)
    : brokers(broker), topic(topic), producer(Properties({{"bootstrap.servers", broker}})) {
}

Producer::~Producer() {
    producer.close();
}

void Producer::send(const std::string& key, const std::string& value) {
    ProducerRecord record(topic, Key(key.c_str(), key.size()), Value(value.c_str(), value.size()));

    auto deliveryCb = [key](const RecordMetadata& metadata, const Error& error) {
        if (!error) {
            std::cout << "Data for " << key << " delivered: " << metadata.toString() << std::endl;
        } else {
            std::cerr << "Failed to deliver data for " << key << ": " << error.message() << std::endl;
        }
    };

    producer.send(record, deliveryCb);
}
