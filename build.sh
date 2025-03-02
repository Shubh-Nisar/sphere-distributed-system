#!/bin/bash
echo "Compiling fetch_metrics.cpp..."
g++ -o fetch_metrics fetch_metrics.cpp -lcurl
echo "Build complete. Run ./fetch_metrics to fetch Kubernetes metrics!"

