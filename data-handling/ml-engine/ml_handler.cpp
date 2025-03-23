#include "../../kafka/Consumer/consumer.h"
#include "../../kafka/Constants/constants.h"
#include <iostream>
#include <kafka/KafkaConsumer.h>

int main() {
    const std::string brokers = constants::KAFKA_HOST;
    const Topic topic = constants::KAFKA_METRICS_TOPIC;
    const std::string groupId = "ml_group";

    // Prepare the configuration using the PropertiesMap explicitly
    kafka::Properties::PropertiesMap configMap;
    configMap["bootstrap.servers"] = std::string(brokers); // Convert to std::string explicitly
    configMap["group.id"] = std::string(groupId); // Convert to std::string explicitly
    configMap["auto.offset.reset"] = std::string("latest"); // Convert to std::string explicitly

    // Create a properties object from the map
    kafka::Properties props(configMap);

    // Create a consumer instance
    KafkaConsumer consumer(props);

    // Subscribe to the topic
    consumer.subscribe({topic});

    std::cout << "Listening for new messages...\n";
    while (true) {
        auto records = consumer.poll(std::chrono::milliseconds(100));
        for (const auto& record : records) {
            if (!record.error()) {
                std::cout << "Got a new message..." << std::endl;
                std::cout << "    Topic    : " << record.topic() << std::endl;
                std::cout << "    Partition: " << record.partition() << std::endl;
                std::cout << "    Offset   : " << record.offset() << std::endl;
		std::cout << "    Timestamp: " << record.timestamp().toString() << std::endl;
                std::cout << "    Key      : " << record.key().toString() << std::endl;
                std::cout << "    Value    : " << record.value().toString() << std::endl;
            } else {
                std::cerr << "Error: " << record.error().message() << std::endl;
            }
        }
    }

    // Cleanup
    consumer.close();
    return 0;
}
