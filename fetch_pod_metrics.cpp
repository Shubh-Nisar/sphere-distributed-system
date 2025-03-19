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
    std::string command = "kubectl get nodes --no-headers -o custom-columns=\":status.capacity.cpu\" > temp_cpu_capacity.txt";
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
    std::string command = "kubectl get nodes --no-headers -o custom-columns=\":status.capacity.memory\" > temp_memory_capacity.txt";
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

// Function to fetch Kubernetes pod metrics and store them
void fetchAndStorePodMetrics(const std::string &filename) {
    int totalCPU = getTotalCPUCapacity();
    long totalMemory = getTotalMemoryCapacity();

    if (totalCPU == -1 || totalMemory == -1) {
        std::cerr << "Error: Unable to determine total CPU or memory capacity." << std::endl;
        return;
    }

    std::string command = "kubectl top pods -n default --no-headers > temp_pod_metrics.txt";
    system(command.c_str());

    std::ifstream inputFile("temp_pod_metrics.txt");
    std::ofstream outputFile(filename, std::ios::app); // Append mode

    if (!inputFile || !outputFile) {
        std::cerr << "Error: Unable to open file for reading/writing." << std::endl;
        return;
    }

    std::string line;
    std::string timestamp = getCurrentTimestamp();

    while (std::getline(inputFile, line)) {
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

            outputFile << timestamp << " | Pod: " << podName
                       << ", CPU: " << cpuPercentage << "%, Memory: " << memoryPercentage << "%" << std::endl;

            std::cout << "Logged: " << timestamp << " | Pod: " << podName
                      << ", CPU: " << cpuPercentage << "%, Memory: " << memoryPercentage << "%\n";
        } catch (...) {
            std::cerr << "Error processing line: " << line << std::endl;
        }
    }

    inputFile.close();
    outputFile.close();
    std::remove("temp_pod_metrics.txt");

    std::cout << "Pod metrics appended to " << filename << std::endl;
}

int main() {
    std::string filename = "pod_metrics.txt";
    int interval = 10; // Time interval in seconds

    std::cout << "Fetching Kubernetes pod metrics every " << interval << " seconds. Press Ctrl+C to stop.\n";

    while (true) {
        fetchAndStorePodMetrics(filename);
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
    return 0;
}
