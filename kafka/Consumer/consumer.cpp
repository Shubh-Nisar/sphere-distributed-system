#include "consumer.h"
#include "../Constants/constants.h"
#include <iostream>
#include <csignal>

Consumer::Consumer(const std::string& broker, const Topic& topic, const std::string& groupId)
    : brokers(broker), topic(topic), consumer(Properties({{"bootstrap.servers", brokers}, {"group.id", groupId}})) {
    consumer.subscribe({topic});
}

void Consumer::subscribeAndPoll() {
    while (running) {
        auto records = consumer.poll(std::chrono::milliseconds(100));
        for (const auto& record : records) {
            if (!record.error()) {
                messages.push_back(handleMessage(record));
            } else {
                std::cerr << "Error: " << record.error().message() << std::endl;
            }
        }
    }
}

void Consumer::stop() {
    running = false;
    consumer.close();
}

std::vector<std::map<std::string, std::string>> Consumer::collectMessages() {
    return messages;  // Return and potentially clear the internal vector if needed
}

std::map<std::string, std::string> Consumer::handleMessage(const ConsumerRecord& record) {
    std::map<std::string, std::string> messageDetails;
    messageDetails["Topic"] = record.topic();
    messageDetails["Partition"] = std::to_string(record.partition());
    messageDetails["Offset"] = std::to_string(record.offset());
    messageDetails["Timestamp"] = record.timestamp().toString();
    messageDetails["Key"] = record.key().toString();
    messageDetails["Value"] = record.value().toString();
    return messageDetails;
}
