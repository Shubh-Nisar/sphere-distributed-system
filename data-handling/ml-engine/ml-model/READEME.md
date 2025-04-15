# LSTM Autoscaler

This application uses a pre-trained LSTM model to predict CPU usage based on historical data. It continuously monitors CPU metrics, updates the model incrementally, and generates multi-step forecasts.

## Overview

The LSTM Autoscaler:

- Reads CPU usage data from a log file (`ml_data.log`)
- Incrementally updates a pre-trained LSTM model as new data arrives
- Generates forecasts for future CPU usage (12 steps ahead)
- Writes predictions to an output log file (`ml_predictions.log`)
- Works with Kafka to enable real-time metrics processing


## Prerequisites

- Docker installed and running
- Metrics consumer service running (consuming from the metrics Kafka topic)
- Log files properly configured:
    - `ml_data.log` - Contains CPU and memory usage data
    - `ml_predictions.log` - Will store the model's predictions


## File Format Requirements

### Input Log File (`ml_data.log`)

The input file should contain rows in the format:

```
TIMESTAMP,CPU(%),MEMORY(%)
```


### Output Log File (`ml_predictions.log`)

The application will write predictions in the format:

```
TIMESTAMP,PRED_1,PRED_2,...,PRED_12
```


## Setup and Installation

1. **Ensure log files exist**:

```bash
# Create empty log files if they don't exist
touch ./ml_data.log
touch ./ml_predictions.log
```

2. **Verify metrics consumer is running**:
Make sure the metrics-consumer service is active and consuming data from the metrics Kafka topic.
3. **Build the Docker container**:

```bash
docker build -t lstm-autoscaler .
```

4. **Run the container**:

```bash
docker run --rm -it \
  -v "$(pwd)/../ml_data.log:/app/ml_data.log" \
  -v "$(pwd)/../ml_predictions.log:/app/ml_predictions.log" \
  lstm-autoscaler
```


## How It Works

1. The application loads a pre-trained LSTM model (`lstm_autoscaler.h5`)
2. It initializes a sliding window with historical CPU usage data
3. As new data arrives in `ml_data.log`, it:
    - Updates the sliding window
    - Buffers new data for incremental model updates
    - Retrains the model when enough new data is collected
    - Generates forecasts every 10 seconds
    - Writes predictions to `ml_predictions.log`

## Configuration Parameters

The application uses several configurable parameters:

- `sequence_length`: 30 (time steps in sliding window)
- `forecast_horizon`: 12 (steps to forecast ahead)
- `update_epochs`: 1 (epochs for incremental training)
- `update_batch_size`: 32 (batch size for retraining)
- `min_buffer_size`: 10 (minimum new data points to trigger an update)


## Troubleshooting

- If predictions aren't being generated, check that `ml_data.log` is being updated with new data
- Verify both log files are properly mounted in the Docker container
- Ensure the metrics consumer is running and connected to the Kafka topic
- Check Docker logs for any errors or exceptions


## Notes

- The model is designed to make predictions every 10 seconds
- The forecast horizon is approximately 2 minutes ahead (assuming data points every 10 seconds)
- The model performs best with consistent data flow

<div>⁂</div>

[^1]: https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/30930770/0253ab09-8678-4879-b321-93a7d26bc259/paste.txt

