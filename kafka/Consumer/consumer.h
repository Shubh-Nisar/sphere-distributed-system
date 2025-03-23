#ifndef CONSUMER_H
#define CONSUMER_H

#include <kafka/KafkaConsumer.h>
#include <map>
#include <string>
#include <vector>
#include <atomic>

using namespace kafka;
using namespace kafka::clients::consumer;

class Consumer {
public:
    Consumer(const std::string& broker, const Topic& topic, const std::string& groupId);
    void subscribeAndPoll();
    void stop();
    std::vector<std::map<std::string, std::string>> collectMessages();

private:
    std::string brokers;
    Topic topic;
    KafkaConsumer consumer;
    std::atomic_bool running { true };
    std::vector<std::map<std::string, std::string>> messages;

    std::map<std::string, std::string> handleMessage(const ConsumerRecord& record);
};

#endif // CONSUMER_H
