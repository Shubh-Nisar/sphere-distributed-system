import numpy as np
import time
import os
import sys
import pywt
from collections import deque
import matplotlib.pyplot as plt
from tensorflow.keras.models import load_model
from tensorflow.keras.optimizers import Adam

# ----- Configuration Parameters -----
sequence_length = 30          # Window length (input)
forecast_horizon = 12         # How many future steps to forecast
min_buffer_size = 10          # Minimum new data points before retraining
log_filename = "ml_data.log"  # Log file (should be continuously updated)
wavelet = 'haar'              # Wavelet basis (same as in pretraining)
level = 4                     # Decomposition level
predictions_filename = "ml_predictions.log"

# Global sliding window and incremental data buffer
window = deque(maxlen=sequence_length)
new_data_buffer = []

# ----- Utility: Read Initial Window from Log File -----
def get_initial_window(filename, seq_length):
    if not os.path.exists(filename):
        print(f"{filename} not found, using zeros.")
        return np.zeros(seq_length)
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        return np.zeros(seq_length)
    window_values = []
    # Process only the last 'seq_length' valid lines
    for line in lines[-seq_length:]:
        # Exclude any line that is "TEST,TEST,TEST"
        if "TEST,TEST,TEST" in line:
            continue
        fields = line.strip().split(',')
        if len(fields) != 3:
            continue
        try:
            # Extract the CPU percentage from field index 1
            cpu_percent = float(fields[1])
            window_values.append(cpu_percent)
        except Exception as e:
            continue
    if len(window_values) < seq_length:
        # Pad the beginning with zeros if there are insufficient data points
        window_values = [0.0] * (seq_length - len(window_values)) + window_values
    return np.array(window_values)

# ----- Utility: Tail the Log File (similar to "tail -f") -----
def tail_f(filename):
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

# ----- Utility: Wavelet Transform (Decomposition) -----
def decompose_signal(signal, wavelet, level):
    coeffs = pywt.wavedec(signal, wavelet, level=level)
    return coeffs

# For a given sequence, get the flattened wavelet coefficients and keep the structure template for inverse transform.
def prepare_wavelet_sample(sequence, wavelet, level):
    coeffs = decompose_signal(sequence, wavelet, level)
    flat = np.concatenate([c for c in coeffs])
    return flat, coeffs

# Inverse transform: reconstruct the signal from flattened coefficients.
def reconstruct_signal(predicted_flat, coeffs_template, wavelet):
    coeffs = []
    start = 0
    for c in coeffs_template:
        length = len(c)
        coeffs.append(predicted_flat[start:start+length])
        start += length
    signal_reconstructed = pywt.waverec(coeffs, wavelet)
    return signal_reconstructed

# ----- Update Model with New Data -----
def update_model_with_new_data(new_data):
    global new_data_buffer, window, model
    for value in new_data:
        window.append(value)
        new_data_buffer.append(value)
    
    if len(new_data_buffer) >= min_buffer_size:
        # Combine current sliding window and new buffer to form training samples
        recent_history = np.array(window)
        combined = np.concatenate([recent_history, np.array(new_data_buffer)])
        if len(combined) < sequence_length + forecast_horizon:
            return
        X_new, y_new = [], []
        for i in range(len(combined) - sequence_length - forecast_horizon + 1):
            X_new.append(combined[i:i+sequence_length])
            y_new.append(combined[i+sequence_length:i+sequence_length+forecast_horizon])
        X_new = np.array(X_new)
        y_new = np.array(y_new)
        
        # Transform new samples into wavelet domain
        X_wavelet, y_wavelet = [], []
        for i in range(X_new.shape[0]):
            flat_X, _ = prepare_wavelet_sample(X_new[i], wavelet, level)
            flat_y, _ = prepare_wavelet_sample(y_new[i], wavelet, level)
            X_wavelet.append(flat_X)
            y_wavelet.append(flat_y)
        X_wavelet = np.array(X_wavelet)
        y_wavelet = np.array(y_wavelet)
        
        print(f"Incremental training on {len(new_data_buffer)} new data points; {X_wavelet.shape[0]} training samples generated.")
        model.fit(X_wavelet, y_wavelet, epochs=1, batch_size=32, verbose=1)
        new_data_buffer.clear()
    else:
        print(f"Accumulated {len(new_data_buffer)} new data points; waiting for at least {min_buffer_size}.")

# ----- Multi-step Forecast: Predict Future CPU Usage -----
def multi_step_forecast(steps_ahead=forecast_horizon):
    # Prepare the current window for prediction (transform into wavelet domain)
    current_window = np.array(window)
    input_wavelet, _ = prepare_wavelet_sample(current_window, wavelet, level)
    input_wavelet = input_wavelet.reshape(1, -1)
    predicted_wavelet = model.predict(input_wavelet, verbose=0)[0]
    # IMPORTANT: Get the coefficient structure for the forecast signal.
    # Create a dummy signal of length equal to forecast_horizon and decompose it.
    dummy_forecast = np.zeros(forecast_horizon)
    _, coeffs_template = prepare_wavelet_sample(dummy_forecast, wavelet, level)
    predicted_signal = reconstruct_signal(predicted_wavelet, coeffs_template, wavelet)
    return predicted_signal[:steps_ahead]

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

# Global variables: initialize sliding window and incremental buffer.
initial_window = get_initial_window(log_filename, sequence_length)
window = deque(initial_window, maxlen=sequence_length)
new_data_buffer = []

# ----- Main Incremental Training Process -----
def main():
    global model, window, new_data_buffer
    # Load the pre-trained model saved from Colab
    try:
        model = load_model("wavelet_autoscaler.h5", compile=False)
    except Exception as e:
        sys.exit(f"Error loading model: {e}")
    
    # Recompile the model for further training
    model.compile(optimizer=Adam(learning_rate=0.001), loss='mse')
    
    # Initialize the sliding window with the last sequence_length values from the existing log file
    initial_window = get_initial_window(log_filename, sequence_length)
    window = deque(initial_window, maxlen=sequence_length)
    new_data_buffer = []
    print(f"Initial sliding window: {list(window)}")
    
    # Monitor the log file for new entries
    for line in tail_f(log_filename):
        line = line.strip()
        # Skip empty lines and lines containing "TEST,TEST,TEST"
        if not line or "TEST,TEST,TEST" in line:
            continue
        fields = line.split(',')
        if len(fields) != 3:
            continue
        try:
            # Extract CPU percentage from field index 1
            cpu_value = float(fields[1])
        except Exception as e:
            continue
        print(f"New CPU usage received: {cpu_value}%")
        update_model_with_new_data([cpu_value])
        
        forecast = multi_step_forecast()
        print(f"Forecast for next {forecast_horizon} steps: {forecast}")
        append_predictions_to_file(forecast)
        
        # Optionally visualize the current sliding window and forecast
        plt.figure(figsize=(10, 4))
        plt.plot(np.arange(-sequence_length, 0), list(window), label="Recent CPU Usage")
        plt.plot(np.arange(1, forecast_horizon+1), forecast, marker='o', linestyle='--', label="Forecast")
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
