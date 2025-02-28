#!/bin/bash
# Minikube setup script for Linux

echo "Starting Minikube with predefined resources..."
minikube start --cpus=4 --memory=4096 --driver=docker

echo "Enabling Metrics Server..."
minikube addons enable metrics-server
kubectl get nodes -o wide
kubectl get pods -o wide

echo "Verifying Metrics Server..."
kubectl get deployment metrics-server -n kube-system

