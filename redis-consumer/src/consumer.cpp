#include <iostream>
#include <librdkafka/rdkafkacpp.h>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <chrono>
#include <thread>

using json = nlohmann::json;

int main() {
    std::string brokers = "localhost:9092";
    std::string topic = "predictions";
    std::string errstr;

    // Kafka config
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    conf->set("bootstrap.servers", brokers, errstr);
    conf->set("group.id", "redis-consumer-group", errstr);
    conf->set("auto.offset.reset", "earliest", errstr);

    // Kafka consumer
    RdKafka::KafkaConsumer *consumer = RdKafka::KafkaConsumer::create(conf, errstr);
    consumer->subscribe({topic});

    std::cout << "[INFO] Subscribed to Kafka topic: " << topic << std::endl;

    // Redis client
    auto redis = sw::redis::Redis("tcp://localhost:6379"); //Ensure that redis container is running

    while (true) {
        RdKafka::Message *msg = consumer->consume(1000);
        if (msg->err() == RdKafka::ERR_NO_ERROR) {
            std::string payload = static_cast<const char *>(msg->payload());
            try {
                json j = json::parse(payload);
                long timestamp = j["timestamp"].get<long>();
                std::string key = "prediction:" + std::to_string(timestamp);
                std::string value = j.dump();
                redis.set(key, value);
                std::cout << "[REDIS] Stored key: " << key << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "[ERROR] Failed to parse message or store to Redis: " << e.what() << std::endl;
            }
        }
        delete msg;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    consumer->close();
    delete consumer;
    delete conf;

    return 0;
}
