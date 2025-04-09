#!/bin/bash

# Set variables
POD_PATTERN="front-end"
NAMESPACE="sock-shop"
OUTPUT_FILE="front-end-metrics.log"
COLLECTION_INTERVAL=1  # Collect every 1 second

# Create header for the log file (including TOTAL_PODS column)
echo "TIMESTAMP,POD,CPU(m),CPU(%),MEMORY(Mi),MEMORY(%),TOTAL_PODS,RPM" > $OUTPUT_FILE

# Check if metrics-server is enabled by testing one pod
FIRST_POD=$(kubectl get pods -n $NAMESPACE -o jsonpath='{.items[0].metadata.name}')
if ! kubectl top pod -n $NAMESPACE "$FIRST_POD" &>/dev/null; then
  echo "Metrics Server not available. Enabling it in Minikube..."
  minikube addons enable metrics-server
  echo "Waiting for Metrics Server to become available..."
  sleep 30
fi

echo "Starting metrics collection for pods starting with '$POD_PATTERN' in namespace $NAMESPACE every $COLLECTION_INTERVAL second(s)..."
echo "Metrics are being saved to $OUTPUT_FILE"

while true; do
  TIMESTAMP=$(date +%s)
  # Get a list of all pods whose names start with the pattern
  PODS=$(kubectl get pods -n $NAMESPACE -o jsonpath='{.items[*].metadata.name}' | tr ' ' '\n' | grep "^$POD_PATTERN")
  TOTAL_PODS=$(echo "$PODS" | wc -l)
  for POD in $PODS; do
    METRICS=$(kubectl top pod $POD -n $NAMESPACE --no-headers 2>/dev/null)
    if [ $? -eq 0 ]; then
      # Extract CPU and memory values
      CPU_USAGE=$(echo $METRICS | awk '{print $2}' | sed 's/m//')
      MEM_USAGE=$(echo $METRICS | awk '{print $3}' | sed 's/Mi//')
      
      # Get resource limits for the pod
      CPU_LIMIT=$(kubectl get pod $POD -n $NAMESPACE -o jsonpath='{.spec.containers[0].resources.limits.cpu}' 2>/dev/null | sed 's/[^0-9]*//g')
      MEM_LIMIT=$(kubectl get pod $POD -n $NAMESPACE -o jsonpath='{.spec.containers[0].resources.limits.memory}' 2>/dev/null | sed 's/Mi//g' | sed 's/Gi/000/g')
      if [ -z "$CPU_LIMIT" ]; then
        CPU_LIMIT=$(kubectl get pod $POD -n $NAMESPACE -o jsonpath='{.spec.containers[0].resources.requests.cpu}' 2>/dev/null | sed 's/[^0-9]*//g')
      fi
      if [ -z "$MEM_LIMIT" ]; then
        MEM_LIMIT=$(kubectl get pod $POD -n $NAMESPACE -o jsonpath='{.spec.containers[0].resources.requests.memory}' 2>/dev/null | sed 's/Mi//g' | sed 's/Gi/000/g')
      fi
      if [ -z "$CPU_LIMIT" ]; then
        NODE=$(kubectl get pod $POD -n $NAMESPACE -o jsonpath='{.spec.nodeName}')
        CPU_LIMIT=$(kubectl get node $NODE -o jsonpath='{.status.allocatable.cpu}' | sed 's/[^0-9]*//g')
      fi
      if [ -z "$MEM_LIMIT" ]; then
        NODE=$(kubectl get pod $POD -n $NAMESPACE -o jsonpath='{.spec.nodeName}')
        MEM_LIMIT=$(kubectl get node $NODE -o jsonpath='{.status.allocatable.memory}' | sed 's/Ki//g')
        MEM_LIMIT=$((MEM_LIMIT / 1024))
      fi
      
      if [ -n "$CPU_LIMIT" ] && [ "$CPU_LIMIT" -gt 0 ]; then
        CPU_PERCENT=$(echo "scale=2; ($CPU_USAGE / $CPU_LIMIT) * 100" | bc)
      else
        CPU_PERCENT="N/A"
      fi
      
      if [ -n "$MEM_LIMIT" ] && [ "$MEM_LIMIT" -gt 0 ]; then
        MEM_PERCENT=$(echo "scale=2; ($MEM_USAGE / $MEM_LIMIT) * 100" | bc)
      else
        MEM_PERCENT="N/A"
      fi
      
      # Log metrics for this pod, including total number of pods
      HTTP_REQUESTS=$(kubectl logs -n $NAMESPACE $POD --since=${COLLECTION_INTERVAL}s | grep -c "GET")
      echo "$TIMESTAMP,$POD,$CPU_USAGE,$CPU_PERCENT,$MEM_USAGE,$MEM_PERCENT,$TOTAL_PODS, $HTTP_REQUESTS" >> $OUTPUT_FILE
      echo "$(date +'%Y-%m-%d %H:%M:%S')    POD: $POD    CPU: ${CPU_USAGE}m (${CPU_PERCENT}%)    Memory: ${MEM_USAGE}Mi (${MEM_PERCENT}%)    Total Pods: $TOTAL_PODS    RPM: $HTTP_REQUESTS"
      echo "-----------------------------------------------------------------------------------------------------"
    else
      echo "$(date +'%Y-%m-%d %H:%M:%S') - Error retrieving metrics for pod $POD, retrying..."
    fi
  done
  sleep $COLLECTION_INTERVAL
done
