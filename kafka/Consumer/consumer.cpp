//
// Created by SHUBH NISAR on 2/26/25.
//

//
// Created by SHUBH NISAR on 2/26/25.
//

#include "consumer.h"
#include "../Constants/constants.h"

std::atomic_bool running = {true};

void stopRunning(int sig) {
    if (sig != SIGINT) return;

    if (running) {
        running = false;
    } else {
        // Restore the signal handler, -- to avoid stuck with this handler
        signal(SIGINT, SIG_IGN); // NOLINT
    }
}

Consumer::Consumer(const std:: string brokers, const Topic topic):
    brokers(brokers), topic(topic){}

int main()
{
    // Use Ctrl-C to terminate the program
    signal(SIGINT, stopRunning);    // NOLINT

    // E.g. KAFKA_BROKER_LIST: "192.168.0.1:9092,192.168.0.2:9092,192.168.0.3:9092"
    const std::string brokers = constants::KAFKA_HOST;
    const Topic topic = constants::KAFKA_METRICS_TOPIC;

    Consumer consumer(brokers, topic);

    // Prepare the configuration
    const Properties props({{"bootstrap.servers", {brokers}}});

    // Create a consumer instance
    KafkaConsumer Consumer(props);

    // Subscribe to topics
    Consumer.subscribe({topic});

    while (running) {
        // Poll messages from Kafka brokers
        auto records = Consumer.poll(std::chrono::milliseconds(100));

        for (const auto& record: records) {
            if (!record.error()) {
                std::cout << "Got a new message..." << std::endl;
                std::cout << "    Topic    : " << record.topic() << std::endl;
                std::cout << "    Partition: " << record.partition() << std::endl;
                std::cout << "    Offset   : " << record.offset() << std::endl;
                std::cout << "    Timestamp: " << record.timestamp().toString() << std::endl;
                std::cout << "    Headers  : " << toString(record.headers()) << std::endl;
                std::cout << "    Key      : " << record.key().toString() << std::endl;
                std::cout << "    Value    : " << record.value().toString() << std::endl;


            } else {
                std::cerr << record.toString() << std::endl;
            }
        }
    }

    // No explicit close is needed, RAII will take care of it
    Consumer.close();
}
