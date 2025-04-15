#include <iostream>
#include <librdkafka/rdkafkacpp.h>
#include <sw/redis++/redis++.h>
#include <chrono>
#include <thread>
#include <csignal>
#include <string>
#include <cctype>

/**
 * @file consumer.cpp
 * 
 * @brief Kafka Consumer to consume messages from the Kafka "predictions" topic and store them in Redis.
 *
 * This C++ program connects to a Kafka broker, subscribes to the "predictions" topic, consumes messages
 * from Kafka, sanitizes the payload, and stores the cleaned data in a Redis database.
 * The consumer gracefully shuts down when receiving termination signals (SIGINT, SIGTERM).
 * The program utilizes librdkafka to interface with Kafka and sw/redis++ to interact with Redis.
 *
 */

volatile sig_atomic_t run = 1;  // Global flag for graceful termination

/**
 * @brief Signal handler for graceful shutdown on receiving SIGINT or SIGTERM.
 *
 * This function is triggered when a termination signal (SIGINT or SIGTERM) is received.
 * It sets the global 'run' flag to 0, allowing the consumer to gracefully shut down.
 * 
 * @param signum The signal number.
 */
void sigterm_handler(int signum) {
    run = 0;
    std::cout << "[INFO] Graceful shutdown initiated..." << std::endl;
}

/**
 * @brief Helper function to sanitize the payload by removing non-printable characters.
 *
 * This function ensures that only printable characters and spaces are kept in the payload. 
 * It removes any non-printable characters to avoid issues when storing the data in Redis.
 *
 * @param payload The raw message from Kafka.
 * @return A sanitized string containing only printable characters.
 */
std::string sanitize_payload(const std::string& payload) {
    std::string sanitized;
    for (char c : payload) {
        // Add only printable characters or spaces to the sanitized payload
        if (std::isprint(static_cast<unsigned char>(c)) || std::isspace(static_cast<unsigned char>(c))) {
            sanitized += c;
        }
    }
    return sanitized;
}

int main() {
    std::string brokers = "localhost:9092";  // Kafka broker address
    std::string topic = "predictions";  // Kafka topic to consume messages from
    std::string errstr;  // Error string for configuration validation

    // Set up signal handler for graceful shutdown (e.g., Ctrl+C)
    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    // Kafka configuration object
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    conf->set("bootstrap.servers", brokers, errstr);  // Kafka broker list
    conf->set("group.id", "redis-consumer-group", errstr);  // Consumer group ID
    conf->set("auto.offset.reset", "earliest", errstr);  // Set the consumer to start from the beginning if no offset is available

    // Create Kafka consumer instance
    RdKafka::KafkaConsumer *consumer = RdKafka::KafkaConsumer::create(conf, errstr);
    if (!consumer) {
        std::cerr << "[ERROR] Failed to create Kafka consumer: " << errstr << std::endl;
        return 1;
    }

    // Subscribe to Kafka topic
    consumer->subscribe({topic});
    std::cout << "[INFO] Subscribed to Kafka topic: " << topic << std::endl;

    // Redis client setup
    auto redis = sw::redis::Redis("tcp://localhost:6379");  // Ensure Redis is running on localhost:6379

    // Main consumer loop
    while (run) {
        // Consume Kafka message with a timeout of 1000ms (1 second)
        RdKafka::Message *msg = consumer->consume(1000);

        if (msg->err() == RdKafka::ERR_NO_ERROR) {
            std::cout << "[INFO] Message consumed from topic: " << topic << std::endl;
            std::cout << "[INFO] Message Offset: " << msg->offset() << std::endl;
            std::cout << "[INFO] Message Key: " << msg->key() << std::endl;

            // Get the payload size and cast it to a string
            size_t payload_size = msg->len();
            const char *payload = static_cast<const char *>(msg->payload());

            // Skip empty messages
            if (payload_size == 0) {
                std::cerr << "[ERROR] Received empty message, skipping..." << std::endl;
                continue;
            }

            // Convert payload to string
            std::string payload_str(payload, payload_size);
            std::cout << "[INFO] Raw Payload: " << payload_str << std::endl;

            // Sanitize the payload to remove any non-printable characters
            payload_str = sanitize_payload(payload_str);

            // Check if sanitized payload is empty
            if (payload_str.empty()) {
                std::cerr << "[ERROR] Empty sanitized payload, skipping..." << std::endl;
                continue;
            }

            // Generate Redis key (using offset as key or timestamp if available)
            std::string key = "prediction:" + std::to_string(msg->offset());  // Use message offset as key for uniqueness
            
            // Store the sanitized payload in Redis
            redis.set(key, payload_str);

            std::cout << "[REDIS] Stored key: " << key << std::endl;
        } else if (msg->err() != RdKafka::ERR__TIMED_OUT) {
            // Log Kafka consumer errors
            std::cerr << "[ERROR] Kafka consumer error: " << msg->errstr() << std::endl;
        }

        // Free message memory
        delete msg;

        // Allow graceful shutdown with a short delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!run) break;  // Check if the run flag is set to 0 for shutdown
    }

    // Clean up Kafka consumer and Redis client
    consumer->close();
    delete consumer;
    delete conf;

    std::cout << "[INFO] Consumer and Redis client closed gracefully." << std::endl;
    return 0;
}
