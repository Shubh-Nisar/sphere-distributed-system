#include "../../kafka/Consumer/consumer.h"
#include "../../kafka/Constants/constants.h"
#include <iostream>
#include <kafka/KafkaConsumer.h>
#include <regex>
#include <curl/curl.h>
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>

// Helper function to extract metrics using regex
bool extractMetrics(const std::string& message, std::string& podName, double& cpu, double& memory) {
    std::regex pattern("Pod: ([\\w-]+), CPU: ([0-9.]+)%, Memory: ([0-9.]+)%");
    std::smatch matches;
    
    if (std::regex_search(message, matches, pattern) && matches.size() == 4) {
        podName = matches[1].str();
        cpu = std::stod(matches[2].str());
        memory = std::stod(matches[3].str());
        return true;
    }
    return false;
}

// Callback function for CURL
size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc& e) {
        return 0;
    }
}

// Function to post metrics to Prometheus API
bool postToPrometheus(const std::string& podName, double cpuUsage, double memoryUsage) {
    std::string command = "echo \"# HELP pod_cpu_usage CPU usage of the pod in cores\n"
                          "# TYPE pod_cpu_usage gauge\n"
                          "pod_cpu_usage{pod_name=\\\"" + podName + "\\\"} " + std::to_string(cpuUsage) + "\n"
                          "# HELP pod_memory_usage_bytes Memory usage of the pod in bytes\n"
                          "# TYPE pod_memory_usage_bytes gauge\n"
                          "pod_memory_usage_bytes{pod_name=\\\"" + podName + "\\\"} " + std::to_string(memoryUsage) + 
                          "\" | curl --data-binary @- http://localhost:9091/metrics/job/kubernetes_metrics/instance/" + podName;
    
    // Execute the command
    int result = system(command.c_str());
    
    if (result == 0) {
        std::cout << "Metrics successfully posted to Prometheus." << std::endl;
    } else {
        std::cerr << "Failed to post metrics to Prometheus. Error code: " << result << std::endl;
    }
    return result == 0;
}

int main() {
    const std::string brokers = constants::KAFKA_HOST;
    const Topic topic = constants::KAFKA_METRICS_TOPIC;
    const std::string groupId = "prometheus_group";

    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_ALL);

    // Prepare the configuration using the PropertiesMap explicitly
    kafka::Properties::PropertiesMap configMap;
    configMap["bootstrap.servers"] = std::string(brokers);
    configMap["group.id"] = std::string(groupId);
    configMap["auto.offset.reset"] = std::string("latest");

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
                
                // Extract the readable part of the message
                std::string value = record.value().toString();
                size_t pos = value.find("IME |");
                if (pos != std::string::npos) {
                    std::string metricsPart = value.substr(pos);
                    
                    // Extract metrics
                    std::string podName;
                    double cpu, memory;
                    if (extractMetrics(metricsPart, podName, cpu, memory)) {
                        std::cout << "Extracted metrics - Pod: " << podName 
                                  << ", CPU: " << cpu << "%, Memory: " << memory << "%" << std::endl;
                        
                        // Post to Prometheus
                        postToPrometheus(podName, cpu, memory);
                    } else {
                        std::cerr << "Failed to extract metrics from message" << std::endl;
                    }
                }
            } else {
                std::cerr << "Error: " << record.error().message() << std::endl;
            }
        }
    }

    // Cleanup
    consumer.close();
    curl_global_cleanup();
    return 0;
}
