# Prometheus

### Creating Docker Volume
```
docker volume create prometheus-data
```

### Creating Docker PushGateWay
```
docker run -d \
  --name pushgateway \
  -p 9091:9091 \
  prom/pushgateway
```

### Promethus Docker Container
```
docker run -d \ 
  --name prometheus \ 
  -p 9090:9090 \ 
  -v prometheus-data:/prometheus \ 
  -v $(pwd)/prometheus.yml:/etc/prometheus/prometheus.yml \ 
  --link pushgateway \ 
  prom/prometheus
```
