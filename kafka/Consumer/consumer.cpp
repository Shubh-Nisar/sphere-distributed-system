#include "consumer.h"
#include "../Constants/constants.h"
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include <regex>

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

Consumer::Consumer(const std::string brokers, const Topic topic):
    brokers(brokers), topic(topic){}

int main()
{
    // Set up Prometheus
    auto registry = std::make_shared<prometheus::Registry>();
    prometheus::Exposer exposer{"0.0.0.0:8080"};
    exposer.RegisterCollectable(registry);

    // Create Prometheus metrics
    auto& cpu_gauge = prometheus::BuildGauge()
        .Name("pod_cpu_usage")
        .Help("CPU usage of the POD")
        .Register(*registry);
    auto& memory_gauge = prometheus::BuildGauge()
        .Name("pod_memory_usage")
        .Help("Memory usage of the POD")
        .Register(*registry);

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

                // Extract pod name from the key
                std::string pod_name = record.key().toString();

                // Extract and process the value
                std::string value = record.value().toString();
                std::regex pattern(R"(POD \| (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) \| CPU: ([\d.]+)% \| Memory: ([\d.]+)%)");
                std::smatch matches;

                if (std::regex_search(value, matches, pattern)) {
                    std::string timestamp = matches[1];
                    double cpu_usage = std::stod(matches[2]);
                    double memory_usage = std::stod(matches[3]);

                    // Update Prometheus metrics with pod name as a label
                    cpu_gauge.Add({{"pod", pod_name}, {"timestamp", timestamp}}).Set(cpu_usage);
                    memory_gauge.Add({{"pod", pod_name}, {"timestamp", timestamp}}).Set(memory_usage);

                    std::cout << "Extracted data:" << std::endl;
                    std::cout << "  Pod Name: " << pod_name << std::endl;
                    std::cout << "  Timestamp: " << timestamp << std::endl;
                    std::cout << "  CPU Usage: " << cpu_usage << "%" << std::endl;
                    std::cout << "  Memory Usage: " << memory_usage << "%" << std::endl;
                } else {
                    std::cerr << "Failed to extract data from the message" << std::endl;
                }
            } else {
                std::cerr << record.toString() << std::endl;
            }
        }
    }

    // No explicit close is needed, RAII will take care of it
    Consumer.close();
}
