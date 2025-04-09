#include "../kafka/Producer/producer.h"
#include "../kafka/Constants/constants.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <ctime>
#include <thread>
#include <chrono>
#include <regex>

// Function to get current timestamp
std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buffer);
}

// Function to get total CPU capacity of the node
int getTotalCPUCapacity() {
    std::string command = "kubectl get nodes -n sock-shop --no-headers -o custom-columns=\":status.capacity.cpu\" > temp_cpu_capacity.txt";
    system(command.c_str());

    std::ifstream inputFile("temp_cpu_capacity.txt");
    int totalCPU = -1;
    if (inputFile) {
        std::string line;
        if (std::getline(inputFile, line)) {
            try {
                totalCPU = std::stoi(line); // Convert CPU cores to an integer
            } catch (...) {
                std::cerr << "Error parsing CPU capacity.\n";
            }
        }
    }
    inputFile.close();
    std::remove("temp_cpu_capacity.txt");
    return totalCPU;
}

// Function to get total memory capacity of the node in Ki
long getTotalMemoryCapacity() {
    std::string command = "kubectl get nodes -n sock-shop --no-headers -o custom-columns=\":status.capacity.memory\" > temp_memory_capacity.txt";
    system(command.c_str());

    std::ifstream inputFile("temp_memory_capacity.txt");
    long totalMemory = -1;
    if (inputFile) {
        std::string line;
        if (std::getline(inputFile, line)) {
            try {
                line = std::regex_replace(line, std::regex("[^0-9]"), ""); // Remove non-numeric characters
                totalMemory = std::stol(line);
            } catch (...) {
                std::cerr << "Error parsing memory capacity.\n";
            }
        }
    }
    inputFile.close();
    std::remove("temp_memory_capacity.txt");
    return totalMemory;
}

// Function to convert memory from Mi/Gi to Ki
long convertMemoryToKi(const std::string &memoryStr) {
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

// Function to fetch Kubernetes pod metrics and stream them
void streamPodMetrics(Producer& producer, int intervalSeconds) {
    std::string command = "kubectl top pods -n sock-shop --no-headers --selector=name=front-end";
    while (true) {
        int totalCPU = getTotalCPUCapacity();
        long totalMemory = getTotalMemoryCapacity();

        if (totalCPU == -1 || totalMemory == -1) {
            std::cerr << "Error: Unable to determine total CPU or memory capacity." << std::endl;
            continue;
        }

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cerr << "Error executing command." << std::endl;
            continue;
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

            // Convert CPU usage to percentage
            int cpuMilliCores = std::stoi(cpuUsage.substr(0, cpuUsage.find('m'))); // Remove 'm'
            double cpuPercentage = (cpuMilliCores / (totalCPU * 1000.0)) * 100;

            // Convert memory usage to Ki
            long memoryUsedKi = convertMemoryToKi(memoryUsage);
            double memoryPercentage = (memoryUsedKi / static_cast<double>(totalMemory)) * 100;

            // Format the message value
            std::stringstream valueStream;
            valueStream << "PRODUCER | " << "LOG_TIME | " << timestamp << " | Pod: " << podName
                        << ", CPU: " << cpuPercentage << "%, Memory: " << memoryPercentage << "%";
            std::string value = valueStream.str();

            // Send the data
            producer.send(podName, value);
            std::cout << "Streaming: " << value << std::endl;
        }
        pclose(pipe);

        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
    }
}

int main() {
    const std::string brokers = constants::KAFKA_HOST;
    const Topic topic = constants::KAFKA_METRICS_TOPIC;

    Producer producer(brokers, topic);

    int intervalSeconds = 10;  // Streaming interval
    streamPodMetrics(producer, intervalSeconds);

    return 0;
}
