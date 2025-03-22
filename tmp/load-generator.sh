#!/bin/bash

# Configuration
MINIKUBE_IP=$(minikube ip)
URL="http://${MINIKUBE_IP}:31622"
MAX_CONCURRENT=50       # Maximum number of concurrent requests
INTERVAL_SECONDS=10     # Time between batches
MIN_REQUESTS=1          # Minimum requests per batch
MAX_REQUESTS=100000     # Maximum requests per batch

# Function to generate a random number between min and max
random_number() {
    local min=$1
    local max=$2
    # Use /dev/urandom for better randomness with large ranges
    echo $(( ($(od -An -N4 -tu4 /dev/urandom) % ($max - $min + 1)) + $min ))
}

# Function to send requests with controlled concurrency
send_controlled_requests() {
    local total=$1
    local concurrent=$2
    local completed=0
    local start_time=$(date +%s.%N)
    local progress=0
    local last_progress_update=0

    # Create a named pipe for job control
    local pipe=$(mktemp -u)
    mkfifo $pipe

    # Start background process to manage the pipe
    exec 3<>$pipe
    rm $pipe

    # Initialize the semaphore
    for ((i=0; i<concurrent; i++)); do
        echo >&3
    done

    # Process all requests with controlled concurrency
    for ((i=1; i<=total; i++)); do
        # Wait for a slot to become available
        read -u 3

        # Update progress display
        progress=$((i * 100 / total))
        current_time=$(date +%s.%N)
        elapsed=$(echo "$current_time - $start_time" | bc)
        
        if [ $((i % 100)) -eq 0 ] || [ $i -eq $total ] || [ $(echo "$elapsed - $last_progress_update > 0.5" | bc) -eq 1 ]; then
            rate=$(echo "scale=1; $i / $elapsed" | bc 2>/dev/null || echo "calculating...")
            echo -ne "Progress: $i/$total ($progress%) - Rate: $rate req/sec\r"
            last_progress_update=$elapsed
        fi

        # Execute the request in background
        {
            curl -s -o /dev/null --connect-timeout 2 --max-time 5 "$URL" >/dev/null 2>&1
            # Signal that this job is done
            echo >&3
        } &
    done

    # Wait for all background jobs to complete
    wait
    exec 3>&-

    # Calculate final statistics
    end_time=$(date +%s.%N)
    total_time=$(echo "$end_time - $start_time" | bc)
    requests_per_second=$(echo "scale=2; $total / $total_time" | bc)
    
    echo -e "\nCompleted $total requests in $total_time seconds ($requests_per_second req/sec)"
}

echo "Starting load test against $URL"
echo "Every $INTERVAL_SECONDS seconds, a random number ($MIN_REQUESTS-$MAX_REQUESTS) of requests will be sent"
echo "Maximum concurrent requests: $MAX_CONCURRENT"
echo "Press Ctrl+C to stop"

# Counter for total requests
total_requests=0

# Start time for the entire test
start_time=$(date +%s)

# Trap Ctrl+C to show summary before exiting
trap 'echo -e "\n\nTest summary: Sent $total_requests requests over $(($(date +%s) - start_time)) seconds"; exit' INT

while true; do
    # Generate random number of requests
    num_requests=$(random_number $MIN_REQUESTS $MAX_REQUESTS)
    
    echo "---"
    echo "Starting batch: $num_requests requests"
    
    # Send the requests with controlled concurrency
    batch_start=$(date +%s)
    send_controlled_requests $num_requests $MAX_CONCURRENT
    batch_end=$(date +%s)
    
    # Update total requests
    total_requests=$((total_requests + num_requests))
    
    # Calculate elapsed time since start
    current_time=$(date +%s)
    total_elapsed=$((current_time - start_time))
    
    # Calculate overall requests per second
    overall_rate=$(echo "scale=2; $total_requests / $total_elapsed" | bc)
    
    echo "Total: $total_requests requests in $total_elapsed seconds ($overall_rate req/sec average)"
    
    # Calculate time to next interval
    next_interval=$((batch_start + INTERVAL_SECONDS))
    now=$(date +%s)
    if [ $now -lt $next_interval ]; then
        wait_time=$((next_interval - now))
        echo "Waiting $wait_time seconds for next batch..."
        sleep $wait_time
    else
        echo "Batch processing took longer than interval, starting next batch immediately"
    fi
done
