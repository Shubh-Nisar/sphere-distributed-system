echo "Creating NGINX Deployment"
kubectl apply -f nginx-deployment.yaml

echo "Exposing NGINX Service"
kubectl expose deployment nginx-deployment --port=80 --type=NodePort

echo "Fetching Service Details"
kubectl get svc
