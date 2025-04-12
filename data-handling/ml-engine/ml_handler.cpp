#include "../../kafka/Consumer/consumer.h"
#include "../../kafka/Constants/constants.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <sys/stat.h>  // For checking file existence
#include <kafka/KafkaConsumer.h>
#include <sstream>
#include <string>

// Helper function: trim whitespace from both ends of a string.
std::string trim(const std::string &str) {
    const std::string whitespace = " \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

// Function to parse a Kafka record's value and extract the desired fields.
// Expected format:
//   [binary data]IME | <TIMESTAMP> | Pod: <pod>, CPU: <cpu>% , Memory: <memory>%
// On success, the function returns true and sets outTimestamp, outCpu, and outMemory.
bool parseRecordValue(const std::string &rawValue, std::string &outTimestamp, std::string &outCpu, std::string &outMemory) {
    // Split the raw value on " | "
    size_t pos1 = rawValue.find(" | ");
    if (pos1 == std::string::npos) return false;
    size_t pos2 = rawValue.find(" | ", pos1 + 3);
    if (pos2 == std::string::npos) return false;

    // The timestamp is the portion between the first and second delimiter.
    outTimestamp = trim(rawValue.substr(pos1 + 3, pos2 - (pos1 + 3)));

    // The rest (from pos2 + 3 onward) should contain Pod, CPU, and Memory information.
    std::string part3 = rawValue.substr(pos2 + 3);
    // Example part3: "Pod: front-end-55c969566c-rp8ds, CPU: 0.075%, Memory: 0.716332%"
    
    // Find the CPU value.
    size_t cpuPos = part3.find("CPU:");
    if (cpuPos == std::string::npos) return false;
    size_t cpuPctPos = part3.find("%", cpuPos);
    if (cpuPctPos == std::string::npos) return false;
    outCpu = trim(part3.substr(cpuPos + 4, cpuPctPos - (cpuPos + 4)));
    
    // Find the Memory value.
    size_t memPos = part3.find("Memory:");
    if (memPos == std::string::npos) return false;
    size_t memPctPos = part3.find("%", memPos);
    if (memPctPos == std::string::npos) return false;
    outMemory = trim(part3.substr(memPos + 7, memPctPos - (memPos + 7)));
    
    return true;
}

int main() {
    // Kafka configuration.
    const std::string brokers = constants::KAFKA_HOST;
    const Topic topic = constants::KAFKA_METRICS_TOPIC;
    const std::string groupId = "ml_group";

    kafka::Properties::PropertiesMap configMap;
    configMap["bootstrap.servers"] = std::string{brokers};
    configMap["group.id"] = std::string{groupId};
    configMap["auto.offset.reset"] = std::string{"latest"};

    kafka::Properties props(configMap);
    KafkaConsumer consumer(props);
    consumer.subscribe({topic});

    // Use an explicit path for the log file (here in the current directory).
    const std::string LOG_FILE_PATH = "../ml_data.log";

    // Check if the file exists.
    bool fileExists = false;
    {
        std::ifstream infile(LOG_FILE_PATH);
        fileExists = infile.good();
    }

    // Open the file in append mode.
    std::ofstream logfile(LOG_FILE_PATH, std::ios_base::app);
    if (!logfile.is_open()) {
        std::cerr << "Error: Unable to open " << LOG_FILE_PATH << std::endl;
        return 1;
    }

    // If file is new, write the CSV header.
    if (!fileExists) {
        logfile << "TIMESTAMP,CPU(%),MEMORY(%)" << std::endl;
        logfile.flush();
    }

    // (Optional) Write a test row to confirm file I/O.
    logfile << "TEST,TEST,TEST" << std::endl;
    logfile.flush();
    std::cout << "Test row written to " << LOG_FILE_PATH << std::endl;

    std::cout << "Listening for new messages..." << std::endl;

    while (true) {
        auto records = consumer.poll(std::chrono::milliseconds(100));
        for (const auto &record : records) {
            if (!record.error()) {
                std::string rawValue = record.value().toString();
                std::cout << "Got a new message..." << std::endl;
                std::cout << "    Topic    : " << record.topic() << std::endl;
                std::cout << "    Partition: " << record.partition() << std::endl;
                std::cout << "    Offset   : " << record.offset() << std::endl;
                std::cout << "    Timestamp: " << record.timestamp().toString() << std::endl;
                std::cout << "    Key      : " << record.key().toString() << std::endl;
                std::cout << "    Value    : " << rawValue << std::endl;

                // Parse the record value.
                std::string parsedTimestamp, cpuValue, memValue;
                bool success = parseRecordValue(rawValue, parsedTimestamp, cpuValue, memValue);
                
                // Use the parsed timestamp if parsing is successful; otherwise, use the Kafka record timestamp.
                std::string finalTimestamp = success ? parsedTimestamp : record.timestamp().toString();
                
                // Build the CSV line.
                std::ostringstream oss;
                oss << finalTimestamp << "," << cpuValue << "," << memValue;
                std::string csvLine = oss.str();

                logfile << csvLine << std::endl;
                logfile.flush();

                std::cout << "Wrote CSV line: " << csvLine << std::endl;
            } else {
                std::string errorEntry = "Error: " + record.error().message();
                std::cerr << errorEntry << std::endl;
                logfile << errorEntry << std::endl;
                logfile.flush();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup (never reached in this infinite loop).
    consumer.close();
    logfile.close();
    return 0;
}

