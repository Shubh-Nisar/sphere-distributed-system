# Redis Consumer Docker Setup

This repository contains the implementation for consuming messages from a Kafka topic (`predictions`) and storing them into Redis.

## Prerequisites

Before running the `redis-consumer` container, ensure the following services are set up:

1. **Kafka Docker Container** - Running on port 9092
2. **Redis Docker Container** - Running on port 6379

## Quick Start

### 1. Running Redis Server

```bash
docker pull redis
docker run -d --name redis-server -p 6379:6379 redis
```


### 2. Running Kafka Container

```bash
docker pull wurstmeister/kafka
docker run -d \
    --name=kafka \
    -p 9092:9092 \
    -e KAFKA_ADVERTISED_LISTENER=PLAINTEXT://localhost:9092 \
    -e KAFKA_LISTENER_SECURITY_PROTOCOL=PLAINTEXT \
    -e KAFKA_LISTENER_PORT=9092 \
    -e KAFKA_LISTENER_NAME=PLAINTEXT \
    wurstmeister/kafka
```


### 3. Build and Run the Redis Consumer

```bash
cd ~/sphere-distributed-system
docker build -t redis-consumer -f redis-consumer/Dockerfile .
docker run --network="host" redis-consumer
```


### 4. Verify Data in Redis

```bash
docker exec -it redis-server redis-cli
keys *
get prediction:123
```


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

