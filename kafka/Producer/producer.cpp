//
// Created by SHUBH NISAR on 2/24/25.
//

#include "producer.h"
#include "../Constants/constants.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <thread>
#include <chrono>
#include <regex>
#include <csignal>
#include <atomic>

// Global producer pointer for signal handler
Producer* globalProducer = nullptr;

// Signal handler for clean shutdown
void signalHandler(int sig) {
    if (sig == SIGINT && globalProducer) {
        std::cout << "\nReceived SIGINT. Stopping producer..." << std::endl;
        globalProducer->stop();
    }
}

Producer::Producer(const std::string broker, const Topic topic) :
    brokers(broker), 
    topic(topic),
    producer(Properties({{"bootstrap.servers", broker}})),
    running(false) {
}

Producer::~Producer() {
    stop();
    producer.close();
}

void Producer::stop() {
    running = false;
    producer.flush(); // Flush any pending messages
}

std::string Producer::getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buffer);
}

int Producer::getTotalCPUCapacity() {
    std::string command = "timeout 5 kubectl get nodes --no-headers -o custom-columns=\":status.capacity.cpu\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error executing command." << std::endl;
        return -1;
    }
    
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != nullptr)
            result += buffer;
    }
    pclose(pipe);
    
    try {
        return std::stoi(result);
    } catch (...) {
        std::cerr << "Error parsing CPU capacity." << std::endl;
        return -1;
    }
}

long Producer::getTotalMemoryCapacity() {
    std::string command = "timeout 5 kubectl get nodes --no-headers -o custom-columns=\":status.capacity.memory\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error executing command." << std::endl;
        return -1;
    }
    
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != nullptr)
            result += buffer;
    }
    pclose(pipe);
    
    try {
        result = std::regex_replace(result, std::regex("[^0-9]"), "");
        return std::stol(result);
    } catch (...) {
        std::cerr << "Error parsing memory capacity." << std::endl;
        return -1;
    }
}

long Producer::convertMemoryToKi(const std::string &memoryStr) {
    std::regex numberRegex(R"(\d+)");
    std::smatch match;

    if (std::regex_search(memoryStr, match, numberRegex)) {
        long value = std::stol(match.str());
        if (memoryStr.find("Gi") != std::string::npos)
            return value * 1024 * 1024; // Convert Gi to Ki
        else if (memoryStr.find("Mi") != std::string::npos)
            return value * 1024; // Convert Mi to Ki
        else
            return value; // Assume it's already in Ki
    }
    return -1; // Invalid value
}

void Producer::fetchAndStreamPodMetrics() {
    int totalCPU = getTotalCPUCapacity();
    long totalMemory = getTotalMemoryCapacity();

    if (totalCPU == -1 || totalMemory == -1) {
        std::cerr << "Error: Unable to determine total CPU or memory capacity." << std::endl;
        return;
    }

    std::string command = "timeout 5 kubectl top pods -n default --no-headers";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error executing command." << std::endl;
        return;
    }
    
    char buffer[256];
    std::string timestamp = getCurrentTimestamp();
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        std::istringstream iss(line);
        std::string podName, cpuUsage, memoryUsage;

        if (!(iss >> podName >> cpuUsage >> memoryUsage)) {
            continue;
        }

        try {
            // Convert CPU usage to percentage
            int cpuMilliCores = std::stoi(cpuUsage.substr(0, cpuUsage.find('m'))); // Remove 'm'
            double cpuPercentage = (cpuMilliCores / (totalCPU * 1000.0)) * 100;

            // Convert memory usage to Ki
            long memoryUsedKi = convertMemoryToKi(memoryUsage);
            double memoryPercentage = (memoryUsedKi / static_cast<double>(totalMemory)) * 100;

            // Format the message value
            std::stringstream valueStream;
            valueStream << "PRODUCER | " << "LOG | " << "POD | " << timestamp << " | CPU: " << cpuPercentage << "% | Memory: " << memoryPercentage << "%";
            std::string value = valueStream.str();
	    std::cout << value << std::endl;

            // Create and send Kafka record
            ProducerRecord record(topic, 
                                 Key(podName.c_str(), podName.size()), 
                                 Value(value.c_str(), value.size()));

            // Prepare delivery callback
            auto deliveryCb = [podName](const RecordMetadata& metadata, const Error& error) {
                if (!error) {
                    std::cout << "Metrics for pod " << podName << " delivered: " << metadata.toString() << std::endl;
                } else {
                    std::cerr << "Failed to deliver metrics for pod " << podName << ": " << error.message() << std::endl;
                }
            };

            // Send the message
            producer.send(record, deliveryCb);
            
            std::cout << "Streaming: Pod: " << podName
                      << ", CPU: " << cpuPercentage << "%, Memory: " << memoryPercentage << "%\n";
                      
        } catch (...) {
            std::cerr << "Error processing line: " << line << std::endl;
        }
    }
    
    pclose(pipe);
}

void Producer::streamPodMetrics(int intervalSeconds) {
    running = true;
    
    std::cout << "Streaming Kubernetes pod metrics to Kafka topic '" << topic 
              << "' every " << intervalSeconds << " seconds. Press Ctrl+C to stop.\n";
              
    while (running) {
        fetchAndStreamPodMetrics();
        
        // Replace the single sleep with a loop of shorter sleeps
        // This allows checking the running flag more frequently
        for (int i = 0; i < intervalSeconds && running; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int main() {
    // Set up signal handler
    signal(SIGINT, signalHandler);
    
    // Kafka configuration
    const std::string brokers = constants::KAFKA_HOST;
    const Topic topic = constants::KAFKA_METRICS_TOPIC;
    
    // Create producer
    Producer producer(brokers, topic);
    globalProducer = &producer;
    
    // Start streaming metrics (10 second interval)
    producer.streamPodMetrics(10);
    
    std::cout << "Producer stopped. Exiting." << std::endl;
    return 0;
}
