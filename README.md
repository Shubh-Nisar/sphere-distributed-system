
# SPHERE: Scalable Proactive Handling for Efficient Resource Expansion

A framework that addresses the challenges of resource allocation in distributed systems by integrating real-time monitoring, predictive analytics, and intelligent orchestration.

## Data Generation

### Kafka Docker Image
```bash
docker pull apache/kafka:3.9.0
```

### Run Kafka Container
```bash
docker run -p 9092:9092 apache/kafka:3.9.0
```

### Run Locust
```bash
cd data-generation
locust -f sim-locust.py --host=http://192.168.49.2:30001
```

## Data Collection

### Fetch Pod Metrics (Producer, topic :: metrics)
```bash
cd data-collection
mkdir build
bash build.sh
```

## Data Handling

### ML Engine (Consumer, topic :: metrics)
```bash
cd data-handling/ml-engine/metrics-consumer
mkdir build
bash build.sh
```
