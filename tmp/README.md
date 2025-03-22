# Custom Load Generation (Nginx)

### Create Nginx Deployment
	bash nginx.sh

### Run load
	bash load-generator.sh

### Stop Load
	ps aux | grep load-generator.sh
	kill -9 <PID>
