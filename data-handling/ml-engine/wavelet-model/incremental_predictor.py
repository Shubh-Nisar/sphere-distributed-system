#!/usr/bin/env python3
import os, sys, time, json
import numpy as np
import pywt
import matplotlib.pyplot as plt
from collections import deque
from datetime import datetime, timedelta
from tensorflow.keras.models import Sequential, load_model
from tensorflow.keras.layers import Dense, Dropout
from tensorflow.keras.optimizers import Adam

# ===== Configuration =====
sequence_length   = 30
forecast_horizon  = 15
min_buffer_size   = 10
initial_train_sec = 10 * 60
wavelet           = 'haar'
level             = 4
log_filename      = "ml_data.log"
model_dir         = "model"
model_path        = os.path.join(model_dir, "wavelet_autoscaler.h5")
scaler_path       = os.path.join(model_dir, "scaler.json")
predictions_file  = "ml_predictions.log"

# ===== Globals =====
window        = deque(maxlen=sequence_length)
new_data_buf  = []

# ===== Utility Functions =====
def load_scaler():
    if os.path.exists(scaler_path):
        return json.load(open(scaler_path))
    return None

def save_scaler(minv, maxv):
    json.dump({"min": minv, "max": maxv}, open(scaler_path, "w"))

def normalize(x):
    return (x - scaler["min"]) / (scaler["max"] - scaler["min"] + 1e-6)

def denormalize(x):
    return x * (scaler["max"] - scaler["min"] + 1e-6) + scaler["min"]

def get_initial_window(fname):
    if not os.path.exists(fname):
        print(f"{fname} not found; initializing with zeros.")
        return np.zeros(sequence_length)
    lines = [L for L in open(fname) if "TEST,TEST,TEST" not in L]
    vals = []
    for line in lines[-sequence_length:]:
        parts = line.strip().split(",")
        if len(parts) != 3:
            continue
        try:
            cpu = float(parts[1])
            vals.append(normalize(cpu))
        except:
            pass
    if len(vals) < sequence_length:
        vals = [0.0] * (sequence_length - len(vals)) + vals
    return np.array(vals)

def tail_f(fname):
    while not os.path.exists(fname):
        time.sleep(0.1)
    f = open(fname)
    f.seek(0, os.SEEK_END)
    while True:
        line = f.readline()
        if not line:
            time.sleep(0.1)
            continue
        yield line

def decompose(sig):
    return pywt.wavedec(sig, wavelet, level=level)

def prepare_wavelet(seq):
    coeffs = decompose(seq)
    flat   = np.concatenate(coeffs)
    return flat, coeffs

def reconstruct(flat, template):
    coeffs = []
    idx = 0
    for t in template:
        length = len(t)
        coeffs.append(flat[idx:idx+length])
        idx += length
    return pywt.waverec(coeffs, wavelet)

def append_predictions(forecast_norm):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    vals = [f"{denormalize(x):.2f}" for x in forecast_norm]
    line = ts + "," + ",".join(vals) + "\n"
    with open(predictions_file, "a") as f:
        f.write(line)

# ===== Model Functions =====
def build_model(input_dim, output_dim):
    m = Sequential([
        Dense(256, activation='relu', input_dim=input_dim),
        Dropout(0.2),
        Dense(128, activation='relu'),
        Dropout(0.2),
        Dense(64,  activation='relu'),
        Dense(output_dim)
    ])
    m.compile(optimizer=Adam(5e-4), loss='mse')
    return m

def bootstrap_initial_train():
    print("⏳ Starting 10 min bootstrap training…")
    cpu_vals = []
    start_ts = None
    cutoff   = None

    # 1) Read existing lines
    for L in open(log_filename):
        if "TEST,TEST,TEST" in L: continue
        p = L.strip().split(",")
        if len(p)!=3: continue
        ts = datetime.fromisoformat(p[0])
        if start_ts is None:
            start_ts = ts
            cutoff   = start_ts + timedelta(seconds=initial_train_sec)
        if ts > cutoff:
            break
        try:
            cpu_vals.append(float(p[1]))
        except:
            pass

    # 2) Tail until we have enough
    tail = tail_f(log_filename)
    while True:
        if len(cpu_vals) >= sequence_length + forecast_horizon and ts > cutoff:
            break
        L = next(tail)
        if "TEST,TEST,TEST" in L: continue
        p = L.strip().split(",")
        if len(p)!=3: continue
        ts = datetime.fromisoformat(p[0])
        if ts <= cutoff or len(cpu_vals) < sequence_length + forecast_horizon:
            try:
                cpu_vals.append(float(p[1]))
            except:
                pass

    # 3) Compute and save scaler
    mn, mx = min(cpu_vals), max(cpu_vals)
    save_scaler(mn, mx)
    print(f"🔢 Scaler saved: min={mn:.2f}, max={mx:.2f}")

    # 4) Build windows
    X, y = [], []
    for i in range(len(cpu_vals) - sequence_length - forecast_horizon + 1):
        X.append(cpu_vals[i:i+sequence_length])
        y.append(cpu_vals[i+sequence_length:i+sequence_length+forecast_horizon])
    X, y = np.array(X), np.array(y)

    # 5) Normalize
    X = (X - mn) / (mx - mn + 1e-6)
    y = (y - mn) / (mx - mn + 1e-6)

    # 6) Wavelet transform
    Xw, yw = [], []
    for i in range(len(X)):
        fx, _ = prepare_wavelet(X[i])
        fy, _ = prepare_wavelet(y[i])
        Xw.append(fx); yw.append(fy)
    Xw, yw = np.array(Xw), np.array(yw)

    # 7) Train & save
    model = build_model(Xw.shape[1], yw.shape[1])
    model.fit(Xw, yw, epochs=100, batch_size=16, verbose=1)
    os.makedirs(model_dir, exist_ok=True)
    model.save(model_path)
    print("✅ Bootstrap training complete; model saved.")
    return model

def update_online(new_vals):
    global new_data_buf, window, model
    for v in new_vals:
        window.append(v)
        new_data_buf.append(v)

    if len(new_data_buf) < min_buffer_size:
        print(f"Accumulated {len(new_data_buf)} new data points; waiting for at least {min_buffer_size}.")
        return

    # Build combined series
    recent = np.array(window)
    comb   = np.concatenate([recent, np.array(new_data_buf)])
    if len(comb) < sequence_length + forecast_horizon:
        return

    # Create X/y windows
    Xn, yn = [], []
    for i in range(len(comb) - sequence_length - forecast_horizon + 1):
        Xn.append(comb[i:i+sequence_length])
        yn.append(comb[i+sequence_length:i+sequence_length+forecast_horizon])
    Xn, yn = np.array(Xn), np.array(yn)

    # Normalize
    Xn = normalize(Xn)
    yn = normalize(yn)

    # Wavelet transform
    Xw, yw = [], []
    for i in range(len(Xn)):
        fx, _ = prepare_wavelet(Xn[i])
        fy, _ = prepare_wavelet(yn[i])
        Xw.append(fx); yw.append(fy)
    Xw, yw = np.array(Xw), np.array(yw)

    print(f"Incremental training on {len(new_data_buf)} new data points; {Xw.shape[0]} training samples generated.")
    model.fit(Xw, yw, epochs=1, batch_size=32, verbose=1)
    new_data_buf.clear()

def multi_step_forecast():
    flat_in, _ = prepare_wavelet(np.array(window))
    pred_flat  = model.predict(flat_in.reshape(1,-1), verbose=0)[0]
    _, tpl     = prepare_wavelet(np.zeros(forecast_horizon))
    rec        = reconstruct(pred_flat, tpl)[:forecast_horizon]
    return rec

# ===== Main =====
if __name__ == "__main__":
    # 1) Load scaler & model or bootstrap
    scaler = load_scaler()
    if os.path.exists(model_path) and scaler:
        print("🔄 Loading existing model and scaler…")
        model = load_model(model_path)
    else:
        model = bootstrap_initial_train()
        scaler = load_scaler()

    model.compile(optimizer=Adam(5e-4), loss='mse')

    # 2) Seed window
    init = get_initial_window(log_filename)
    window = deque(init, maxlen=sequence_length)
    print("Initial sliding window (normalized):")
    print([round(x,4) for x in window])

    # 3) Online loop
    for line in tail_f(log_filename):
        line = line.strip()
        if not line or "TEST,TEST,TEST" in line:
            continue
        parts = line.split(",")
        if len(parts)!=3:
            continue
        try:
            cpu_raw = float(parts[1])
            cpu     = normalize(cpu_raw)
        except:
            continue

        print(f"New CPU usage received: {cpu_raw:.2f}%")
        update_online([cpu])

        forecast_norm = multi_step_forecast()
        forecast_pct  = [denormalize(x) for x in forecast_norm]
        print("Forecast for next %d steps: %s" % (forecast_horizon, 
              ", ".join(f"{v:.2f}%" for v in forecast_pct)))
        append_predictions(forecast_norm)

        # Optional plotting
        plt.figure(figsize=(10,4))
        plt.plot(np.arange(-sequence_length,0), [denormalize(x) for x in window], label="Recent CPU")
        plt.plot(np.arange(1,forecast_horizon+1), forecast_pct, marker='o', linestyle='--', label="Forecast")
        plt.xlabel("Time Steps (relative)")
        plt.ylabel("CPU Usage (%)")
        plt.title("Real-Time Forecast after Incremental Update")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.savefig("latest_forecast.png")
        plt.close()

