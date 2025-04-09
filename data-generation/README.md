# Custom Load Generation (Locust and Sock Shop)

### Collect and Log metrics (front-end pod)
	bash metrics.sh

### Run Locust Load
	locust -f sim-locust.py --host=http://192.168.49.2:30001
> Click START on Locust Web UI
