#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <ctime>

// Function to get current timestamp
std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buffer);
}

// Function to fetch total CPU capacity of the node
int getTotalCPUCapacity() {
    std::string command = "kubectl get nodes --no-headers -o custom-columns=\":status.capacity.cpu\" > temp_cpu_capacity.txt";
    system(command.c_str());

    std::ifstream inputFile("temp_cpu_capacity.txt");
    int totalCPU = -1;
    if (inputFile) {
        std::string line;
        if (std::getline(inputFile, line)) {
            totalCPU = std::stoi(line); // Convert CPU cores to an integer
        }
    }
    inputFile.close();
    std::remove("temp_cpu_capacity.txt");

    return totalCPU;
}

// Function to fetch total memory capacity of the node (in Ki)
long getTotalMemoryCapacity() {
    std::string command = "kubectl get nodes --no-headers -o custom-columns=\":status.capacity.memory\" > temp_memory_capacity.txt";
    system(command.c_str());

    std::ifstream inputFile("temp_memory_capacity.txt");
    long totalMemory = -1;
    if (inputFile) {
        std::string line;
        if (std::getline(inputFile, line)) {
            totalMemory = std::stol(line.substr(0, line.size() - 2)); // Remove 'Ki' and convert to long
        }
    }
    inputFile.close();
    std::remove("temp_memory_capacity.txt");

    return totalMemory;
}

// Function to fetch Kubernetes pod metrics and store them
void fetchAndStorePodMetrics(const std::string& filename) {
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
        std::string namespaceName, podName, cpuUsage, memoryUsage;

        if (!(iss >> namespaceName >> podName >> cpuUsage >> memoryUsage)) {
            continue;
        }

        // Convert CPU usage to percentage
        int cpuMilliCores = std::stoi(cpuUsage.substr(0, cpuUsage.size() - 1)); // Remove 'm' and convert to int
        double cpuPercentage = (cpuMilliCores / (totalCPU * 1000.0)) * 100;

        // Convert memory usage to percentage
        long memoryUsedKi = std::stol(memoryUsage.substr(0, memoryUsage.size() - 2)); // Remove 'Ki' and convert to long
        double memoryPercentage = (memoryUsedKi / static_cast<double>(totalMemory)) * 100;

        outputFile << timestamp << " | Namespace: " << namespaceName << ", Pod: " << podName
                   << ", CPU: " << cpuPercentage << "%, Memory: " << memoryPercentage << "%" << std::endl;
    }

    inputFile.close();
    outputFile.close();
    std::remove("temp_pod_metrics.txt");

    std::cout << "Pod metrics appended to " << filename << std::endl;
}

int main() {
    std::string filename = "pod_metrics.txt";
    fetchAndStorePodMetrics(filename);
    return 0;
}

