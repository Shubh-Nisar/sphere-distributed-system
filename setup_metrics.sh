#!/bin/bash

# Apply the Metrics Server YAML from GitHub
echo "Installing Kubernetes Metrics Server..."
kubectl apply -f https://github.com/kubernetes-sigs/metrics-server/releases/latest/download/components.yaml

# Wait for Metrics Server to be ready
echo "Waiting for Metrics Server to be ready..."
kubectl wait --namespace kube-system --for=condition=Ready pod -l k8s-app=metrics-server --timeout=60s

# Verify installation
echo "Checking Metrics Server status..."
kubectl get deployment metrics-server -n kube-system

echo "Metrics Server installation complete!"

