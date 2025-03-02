#!/bin/bash
echo "Forwarding Kubernetes Metrics Server API to localhost:8443..."
kubectl port-forward -n kube-system service/metrics-server 8443:https

