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

// Helper function to execute a shell command and return its output as a string
std::string execCommand(const std::string& command) {
    const std::string tempFile = "temp_output.txt";
    std::string fullCommand = command + " > " + tempFile;
    system(fullCommand.c_str());
    
    std::ifstream file(tempFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::remove(tempFile.c_str());
    return buffer.str();
}

// Helper function to remove non-digit characters (mimics: sed 's/[^0-9]*//g')
std::string removeNonDigits(const std::string &input) {
    return std::regex_replace(input, std::regex("[^0-9]"), "");
}

// Function to get CPU capacity of the front-end pod using the same logic as the bash file
int getFrontEndPodCPUCapacity() {
    int podCPU = -1;
    std::string cpuStr;
    
    // Try limits first
    std::string command = "kubectl get pod -n sock-shop -l name=front-end -o jsonpath='{.items[0].spec.containers[0].resources.limits.cpu}'";
    cpuStr = execCommand(command);
    std::cout << "CPU limit raw: " << cpuStr << std::endl;
    cpuStr = removeNonDigits(cpuStr);
    
    if (!cpuStr.empty()) {
        try {
            podCPU = std::stoi(cpuStr);
        } catch (...) {
            std::cerr << "Error parsing CPU limit value: " << cpuStr << std::endl;
            podCPU = -1;
        }
    }
    
    // If limits are not available, try requests
    if (podCPU <= 0) {
        command = "kubectl get pod -n sock-shop -l name=front-end -o jsonpath='{.items[0].spec.containers[0].resources.requests.cpu}'";
        cpuStr = execCommand(command);
        std::cout << "CPU request raw: " << cpuStr << std::endl;
        cpuStr = removeNonDigits(cpuStr);
        if (!cpuStr.empty()) {
            try {
                podCPU = std::stoi(cpuStr);
            } catch (...) {
                std::cerr << "Error parsing CPU request value: " << cpuStr << std::endl;
                podCPU = -1;
            }
        }
    }
    
    // If still not available, fallback to node's allocatable CPU
    if (podCPU <= 0) {
        std::string nodeName = execCommand("kubectl get pod -n sock-shop -l name=front-end -o jsonpath='{.items[0].spec.nodeName}'");
        // Clean node name (remove quotes or newline characters)
        nodeName = std::regex_replace(nodeName, std::regex("['\n]"), "");
        std::cout << "Fallback: Node name is: " << nodeName << std::endl;
        if (!nodeName.empty()) {
            command = "kubectl get node " + nodeName + " -o jsonpath='{.status.allocatable.cpu}'";
            cpuStr = execCommand(command);
            std::cout << "Node allocatable CPU raw: " << cpuStr << std::endl;
            cpuStr = removeNonDigits(cpuStr);
            if (!cpuStr.empty()) {
                try {
                    podCPU = std::stoi(cpuStr);
                } catch (...) {
                    std::cerr << "Error parsing node CPU value: " << cpuStr << std::endl;
                    podCPU = 1; // Default value
                }
            }
        }
    }
    
    std::cout << "Final CPU capacity value used: " << podCPU << std::endl;
    return podCPU > 0 ? podCPU : 1;
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

// Function to get memory capacity of the front-end pod in Ki
long getFrontEndPodMemoryCapacity() {
    std::string command = "kubectl get pod -n sock-shop -l name=front-end -o jsonpath='{.items[0].spec.containers[0].resources.limits.memory}'";
    std::string memStr = execCommand(command);
    std::cout << "Memory limit raw: " << memStr << std::endl;
    long podMemory = convertMemoryToKi(memStr);
    
    // If limits not available or invalid, try requests
    if (podMemory <= 0) {
        command = "kubectl get pod -n sock-shop -l name=front-end -o jsonpath='{.items[0].spec.containers[0].resources.requests.memory}'";
        memStr = execCommand(command);
        std::cout << "Memory request raw: " << memStr << std::endl;
        podMemory = convertMemoryToKi(memStr);
    }
    
    return podMemory > 0 ? podMemory : 1048576; // Default to 1Gi (1024*1024 Ki) if invalid
}

// Function to fetch Kubernetes pod metrics and stream them
void streamPodMetrics(Producer& producer, int intervalSeconds) {
    std::string command = "kubectl top pods -n sock-shop --no-headers --selector=name=front-end";
    while (true) {
        // Get the CPU and memory capacity based on pod-specific resources
        int podCPULimit = getFrontEndPodCPUCapacity();
        long podMemoryLimit = getFrontEndPodMemoryCapacity();

        std::cout << "Front-end CPU capacity (limit): " << podCPULimit << std::endl;
        std::cout << "Front-end Memory capacity: " << podMemoryLimit << " Ki" << std::endl;

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cerr << "Error executing kubectl top command." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
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

            // Extract CPU usage in millicores (strip the trailing 'm')
            int cpuMilliCores = 0;
            try {
                cpuMilliCores = std::stoi(cpuUsage.substr(0, cpuUsage.find('m')));
            } catch (...) {
                std::cerr << "Error parsing CPU usage: " << cpuUsage << std::endl;
                cpuMilliCores = 0;
            }
            
            std::cout << "CPU usage: " << cpuMilliCores << "m, Pod CPU Limit: " << podCPULimit << std::endl;
            // Calculate CPU percentage exactly as the bash script does
            double cpuPercentage = (podCPULimit > 0) ? (cpuMilliCores / static_cast<double>(podCPULimit)) * 100 : 0;

            // Convert memory usage (e.g., "100Mi") to Ki
            long memoryUsedKi = convertMemoryToKi(memoryUsage);
            if (memoryUsedKi <= 0) {
                std::cerr << "Error converting memory usage: " << memoryUsage << std::endl;
                memoryUsedKi = 0;
            }
            double memoryPercentage = (podMemoryLimit > 0) ? (memoryUsedKi / static_cast<double>(podMemoryLimit)) * 100 : 0;

            // Format the message value
            std::stringstream valueStream;
            valueStream << "PRODUCER | LOG_TIME | " << timestamp << " | Pod: " << podName
                        << ", CPU: " << cpuPercentage << "%, Memory: " << memoryPercentage << "%";
            std::string value = valueStream.str();

            // Send the data via Kafka
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

    int intervalSeconds = 10;  // Streaming interval in seconds
    streamPodMetrics(producer, intervalSeconds);

    return 0;
}

