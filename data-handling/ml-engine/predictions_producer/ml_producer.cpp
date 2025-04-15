#include "../../../kafka/Producer/producer.h"
#include "../../../kafka/Constants/constants.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>

// Function to get the last position in the file (for tailing)
// This function is used to determine where we left off in the file, so that we can pick up from the last processed line.
long getFileSize(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    return file.tellg();
}

// Function to trim spaces from a string (useful for the timestamp)
// This helps in ensuring that any unwanted leading/trailing spaces are removed from the timestamp.
std::string trim(const std::string &str) {
    size_t first = str.find_first_not_of(' '); // Find the first non-space character
    if (first == std::string::npos) return ""; // If no non-space character is found, return an empty string.
    size_t last = str.find_last_not_of(' '); // Find the last non-space character
    return str.substr(first, (last - first + 1)); // Return the substring with the spaces removed
}

// Function to process new lines in the log file
// This function reads the log file from the point we left off and processes each line to send it to Kafka.
void processNewData(Producer& producer, const std::string& logFile, long initialPosition) {
    std::ifstream file(logFile);
    file.seekg(initialPosition, std::ios::beg); // Start reading from the last position we read in the file

    std::string line;
    while (std::getline(file, line)) { // Read each line of the log file
        size_t commaPos = line.find(","); // Find the position of the first comma (used to split the timestamp from the rest of the values)

        // Parse the CSV data and extract the timestamp (first part before the comma)
        std::string timestamp = trim(line.substr(0, commaPos)); // Trim the timestamp to remove leading/trailing spaces

        // The rest of the line is the value that we will send to Kafka
        std::string value = line.substr(commaPos + 1); // The remaining part after the comma is the value (predictions data)

        // Set the timestamp as the key for Kafka
        std::string key = timestamp;  // Use the timestamp as the key

        // Format the message value as a string
        std::stringstream valueStream;
        valueStream << "PRODUCER - LOG_TIME | " << timestamp << " | Predictions: " << value;
        std::string message_value = valueStream.str();  // Create the final message string with timestamp and value

        // Send the message to Kafka
        producer.send(key, message_value);  // Send the formatted message to Kafka using the producer
        std::cout << "[INFO] Sent to Kafka: Key=" << key << ", Value=" << message_value << std::endl; // Log the sent message for debugging and tracking
    }
}

int main() {
    const std::string brokers = constants::KAFKA_HOST; // Kafka broker address (configured in constants)
    const Topic topic = constants::KAFKA_PREDICTIONS_TOPIC; // The topic to which we will send predictions data

    // Create the Kafka producer object
    Producer producer(brokers, topic);  // Instantiate the Kafka producer with the broker and topic

    std::string logFile = "../../ml_predictions.log";  // The path to the log file containing the prediction data

    // Get initial position in the file (we want to start from the last line)
    long initialPosition = getFileSize(logFile); // Get the current file size to start from the end of the log file

    // Continuous loop to keep processing the log file and sending data to Kafka
    while (true) {
        // Process new data from the log file
        processNewData(producer, logFile, initialPosition); // Process the new lines added to the log file

        // Update the file's position (for tailing new data)
        initialPosition = getFileSize(logFile);  // Recalculate the position of the file to continue reading from the last point

        // Sleep for a while before checking for new data again (to avoid constant reading and CPU overuse)
        std::this_thread::sleep_for(std::chrono::seconds(1));  // Check for new lines every 1 second
    }

    return 0;  // End of program
}
