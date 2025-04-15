#!/usr/bin/env python3
"""
incremental_predictor.py

This script loads a pre-trained LSTM model ("lstm_autoscaler.h5")
and uses a CSV log file (ml_data.log) to continuously update the model
with new CPU usage values. The log file is expected to have rows in the format:

TIMESTAMP,CPU(%),MEMORY(%)

It uses the CPU usage percentage (the second field) to update a sliding window,
trains the model incrementally when enough new data is collected,
and generates a multi-step forecast.

In addition to printing the forecast, each forecast is appended as a CSV row 
to a predictions file (predictions.csv) that is written to a shared volume 
on the host machine. A separate Kafka C++ publisher can then read this file
and push the prediction data to the prediction topic.
"""

import numpy as np
import time
import os
from collections import deque
import matplotlib.pyplot as plt
import sys

from tensorflow.keras.models import load_model
from tensorflow.keras.optimizers import Adam
import tensorflow as tf

# Enable eager execution to help with debugging incremental updates.
tf.config.run_functions_eagerly(True)

# ----- Parameters -----
sequence_length = 30          # Number of time steps in the sliding window.
forecast_horizon = 12         # Number of steps to forecast (~2 minutes ahead if data every 10 sec).
update_epochs = 1             # Number of epochs for incremental training.
update_batch_size = 32        # Batch size for retraining.
min_buffer_size = 10          # Minimum new data points to trigger an update.
log_filename = "ml_data.log"  # CSV log file to read from.
predictions_filename = "ml_predictions.log"  # File to append forecasts for the C++ publisher.

# ----- Load Pre-trained Model -----
try:
    model = load_model("lstm_autoscaler.h5")
except Exception as e:
    sys.exit(f"Error loading model 'lstm_autoscaler.h5': {e}")

# Recompile the model with a fresh optimizer so it's ready for further training.
model.compile(optimizer=Adam(learning_rate=0.001), loss='mse')

# ----- Global Variables -----
window = deque(maxlen=sequence_length)  # Initialize sliding window.
new_data_buffer = []  # Buffer for incremental updates.

# ----- Utility Functions -----
def get_initial_window(filename, seq_length):
    """
    Reads the CSV log file and extracts the last seq_length CPU usage percentages.
    Only rows with exactly 3 fields (new format) are processed.
    Returns a NumPy array of length seq_length.
    """
    if not os.path.exists(filename):
        print(f"{filename} does not exist. Initializing window with zeros.")
        return np.zeros(seq_length)
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading {filename}: {e}. Initializing window with zeros.")
        return np.zeros(seq_length)

    # Skip header if present.
    if lines and lines[0].startswith("TIMESTAMP"):
        lines = lines[1:]
    
    window_values = []
    # Process only rows that have exactly 3 fields.
    for line in lines[-seq_length:]:
        fields = line.strip().split(',')
        if len(fields) != 3:
            print(f"Skipping non-conforming line: {line.strip()}")
            continue
        try:
            cpu_percent = float(fields[1])
            window_values.append(cpu_percent)
        except Exception as e:
            print(f"Error parsing line: {line.strip()}. Error: {e}")
            continue
    if len(window_values) < seq_length:
        window_values = [0.0] * (seq_length - len(window_values)) + window_values
    return np.array(window_values)

def tail_f(filename):
    """
    Generator that mimics Unix "tail -f" behavior.
    Waits for the file to exist (if necessary) and then yields new lines as they are appended.
    """
    while not os.path.exists(filename):
        print(f"Waiting for {filename} to exist...")
        time.sleep(1)
    with open(filename, 'r') as f:
        # Move pointer to the end of the file.
        f.seek(0, os.SEEK_END)
        while True:
            line = f.readline()
            if not line:
                time.sleep(1)
                continue
            yield line

def create_sequence_dataset(data, seq_length):
    """
    Constructs input sequences and targets from a 1D array 'data'.
    Returns:
      X: shape (samples, seq_length, 1)
      y: shape (samples, 1)
    """
    X, y = [], []
    for i in range(len(data) - seq_length):
        X.append(data[i:i+seq_length])
        y.append(data[i+seq_length])
    X = np.array(X)
    y = np.array(y)
    return X.reshape((X.shape[0], seq_length, 1)), y.reshape(-1, 1)

def update_model_with_new_data(new_data_raw):
    """
    Appends new CPU usage values to the sliding window and an incremental buffer.
    When the buffer has at least min_buffer_size new points, constructs a training dataset and 
    fine-tunes the model incrementally.
    """
    global window, new_data_buffer
    for value in new_data_raw:
        window.append(value)
        new_data_buffer.append(value)
    
    if len(new_data_buffer) >= min_buffer_size and len(window) >= sequence_length:
        recent_history = np.array(list(window))
        retrain_data = np.concatenate([recent_history, np.array(new_data_buffer)])
        X_retrain, y_retrain = create_sequence_dataset(retrain_data, sequence_length)
        if X_retrain.shape[0] > 0:  # Only train if there is data to train on
            print(f"Retraining on {len(new_data_buffer)} new data points. Training samples: {X_retrain.shape[0]}")
            model.fit(X_retrain, y_retrain, epochs=update_epochs, batch_size=update_batch_size, verbose=1)
            print("Model updated with new data.")
        else:
            print("No valid training data available.")
        new_data_buffer = []
    else:
        print(f"Accumulated {len(new_data_buffer)} new data points; waiting for at least {min_buffer_size}.")

def multi_step_forecast(steps_ahead=forecast_horizon):
    """
    Generates a forecast of future CPU usage values by recursively predicting one step ahead
    using the current sliding window.
    Returns a NumPy array of forecasted values.
    """
    # Ensure the window has enough data (sequence_length = 30)
    if len(window) < sequence_length:
        print(f"Not enough data for forecasting. Window size: {len(window)}. Waiting for more data.")
        return np.zeros(steps_ahead)  # Return an array of zeros until enough data is available.
    
    current_window = np.array(window).reshape(-1, 1)  # Reshape to match the input shape for LSTM.
    current_input = current_window.reshape(1, sequence_length, 1)  # Reshape to (1, sequence_length, 1)
    predictions = []
    
    for _ in range(steps_ahead):
        pred = model.predict(current_input, verbose=0)[0, 0]
        predictions.append(pred)
        current_input = np.append(current_input[:, 1:, :], [[[pred]]], axis=1)  # Update the input for the next prediction.
    
    return np.array(predictions)

def append_predictions_to_file(forecast):
    """
    Appends the forecast values along with the current time as a CSV row to predictions_filename.
    This ensures only one line per timestamp with all forecasted values.
    """
    # Create a comma-separated string of forecast values.
    forecast_str = ",".join([f"{x:.4f}" for x in forecast])
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")  # Use current timestamp once.
    line = f"{timestamp},{forecast_str}\n"  # Append the timestamp with all forecast values.
    try:
        with open(predictions_filename, "a") as f:
            f.write(line)  # Write the forecast to the file.
    except Exception as e:
        print(f"Error writing to predictions file: {e}")

def main():
    last_prediction_time = None  # Initialize the last prediction time.
    print(f"Listening for new data in {log_filename}...")
    line_generator = tail_f(log_filename)
    
    while True:
        line = line_generator.__next__().strip()
        if not line or line.startswith("TIMESTAMP"):
            continue
        fields = line.split(',')
        if len(fields) != 3:
            print(f"Skipping non-new-format line: {line}")
            continue
        try:
            cpu_value = float(fields[1])
        except Exception as e:
            print(f"Error parsing CPU value from line: {line}. {e}")
            continue

        # Check if it's time for the next prediction (every 10 seconds)
        current_time = time.time()
        if last_prediction_time is None or current_time - last_prediction_time >= 10:
            last_prediction_time = current_time  # Update the time of last prediction.
            print(f"New CPU usage received: {cpu_value}%")
            update_model_with_new_data([cpu_value])  # Update the model with the new data.
            forecast = multi_step_forecast()  # Generate forecast for the next steps.
            print(f"Forecast for the next {forecast_horizon} steps: {forecast}")
            
            # Append the forecast to the predictions file.
            append_predictions_to_file(forecast)  # Store the forecast in the file.
            
         
if __name__ == '__main__':
    main()
