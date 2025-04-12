import numpy as np
import time
import os
import json
from collections import deque
import matplotlib.pyplot as plt
import sys
# from kafka import KafkaProducer
from tensorflow.keras.models import load_model
from tensorflow.keras.optimizers import Adam
import tensorflow as tf

# Enable eager execution to aid in debugging incremental updates.
tf.config.run_functions_eagerly(True)

# ----- Parameters -----
sequence_length = 30          # Number of time steps in the sliding window
forecast_horizon = 12         # Number of steps to forecast (~2 minutes ahead if data every 10 sec)
update_epochs = 1             # Number of epochs for incremental training
update_batch_size = 32        # Batch size for retraining
min_buffer_size = 10          # Minimum new data points to trigger an update
log_filename = "ml_data.log"  # CSV log file to read from

# ----- Load Pre-trained Model -----
try:
    model = load_model("lstm_autoscaler.h5")
except Exception as e:
    sys.exit(f"Error loading model 'lstm_autoscaler.h5': {e}")

# Recompile the model with a fresh optimizer so it's ready for further training.
model.compile(optimizer=Adam(learning_rate=0.001), loss='mse')

# ----- Kafka Producer Setup -----
# producer = KafkaProducer(
#    bootstrap_servers='10.40.130.98:9092',  # Replace with your Kafka server
#    value_serializer=lambda v: json.dumps(v).encode('utf-8')
# )
# topic = "predictions"

# ----- Utility Functions -----
def get_initial_window(filename, seq_length):
    """Reads the CSV log file and extracts the last seq_length CPU usage percentages."""
    if not os.path.exists(filename):
        print(f"{filename} does not exist. Initializing window with zeros.")
        return np.zeros(seq_length)
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading {filename}: {e}. Initializing window with zeros.")
        return np.zeros(seq_length)
    
    if lines and lines[0].startswith("TIMESTAMP"):
        lines = lines[1:]
    
    window_values = []
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
    """Mimics Unix 'tail -f' behavior."""
    while not os.path.exists(filename):
        print(f"Waiting for {filename} to exist...")
        time.sleep(1)
    with open(filename, 'r') as f:
        f.seek(0, os.SEEK_END)
        while True:
            line = f.readline()
            if not line:
                time.sleep(1)
                continue
            yield line

def create_sequence_dataset(data, seq_length):
    """Creates input sequences and targets for training."""
    X, y = [], []
    for i in range(len(data) - seq_length):
        X.append(data[i:i + seq_length])
        y.append(data[i + seq_length])
    X = np.array(X)
    y = np.array(y)
    return X.reshape((X.shape[0], seq_length, 1)), y.reshape(-1, 1)

def update_model_with_new_data(new_data_raw):
    """Incrementally updates the model with new data."""
    global new_data_buffer, window
    for value in new_data_raw:
        window.append(value)
        new_data_buffer.append(value)
    
    if len(new_data_buffer) >= min_buffer_size:
        recent_history = np.array(list(window))
        retrain_data = np.concatenate([recent_history, np.array(new_data_buffer)])
        X_retrain, y_retrain = create_sequence_dataset(retrain_data, sequence_length)
        print(f"Retraining on {len(new_data_buffer)} new data points. Training samples: {X_retrain.shape[0]}")
        model.fit(X_retrain, y_retrain, epochs=update_epochs, batch_size=update_batch_size, verbose=1)
        print("Model updated with new data.")
        new_data_buffer = []
    else:
        print(f"Accumulated {len(new_data_buffer)} new data points; waiting for at least {min_buffer_size}.")

def multi_step_forecast(steps_ahead=forecast_horizon):
    """Generates a forecast of future CPU usage values."""
    current_window = np.array(window).reshape(-1, 1)
    current_input = current_window.reshape(1, sequence_length, 1)
    predictions = []
    for _ in range(steps_ahead):
        pred = model.predict(current_input, verbose=0)[0, 0]
        predictions.append(pred)
        current_input = np.append(current_input[:, 1:, :], [[[pred]]], axis=1)
    return np.array(predictions)

# Global variables: initialize sliding window and incremental buffer.
initial_window = get_initial_window(log_filename, sequence_length)
window = deque(initial_window, maxlen=sequence_length)
new_data_buffer = []

def main():
    print(f"Listening for new data in {log_filename}...")
    line_generator = tail_f(log_filename)
    
    for line in line_generator:
        line = line.strip()
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
        print(f"New CPU usage received: {cpu_value}%")
        update_model_with_new_data([cpu_value])
        forecast = multi_step_forecast()
        print(f"Forecast for the next {forecast_horizon} steps: {forecast}")
        
        # Send the prediction to Kafka topic
  #      prediction = {
  #          "timestamp": int(time.time()),
  #          "cpu_forecast": forecast.tolist(),
  #          "unit": "percent"
  #      }
  #      print(f"[Producer] Sending: {prediction}")
  #      producer.send(topic, prediction)
        
        # Optionally: visualize the current window and forecast
        plt.figure(figsize=(10, 4))
        plt.plot(np.arange(-sequence_length, 0), list(window), label="Recent CPU Usage")
        plt.plot(np.arange(1, forecast_horizon+1), forecast, marker='o', linestyle='--', label="Forecasted CPU")
        plt.xlabel("Time Steps (relative)")
        plt.ylabel("CPU Usage (%)")
        plt.title("Real-Time Forecast after Incremental Update")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.savefig("latest_forecast.png")
        plt.close()

if __name__ == '__main__':
    main()
