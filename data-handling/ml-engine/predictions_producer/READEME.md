# Kafka Log Processor

This application processes machine learning predictions from log files and publishes them to Kafka prediction topic.

## Prerequisites

- Kafka cluster running and accessible
- Docker installed (for running the ML model container)
- Bash shell environment


## Setup Instructions

1. Ensure Kafka is running and properly configured before starting the application.
2. Verify that the ML model Docker container is running. This container should output predictions to the log file at:

```
../../ml_predictions.log
```

3. Create or reset the build directory:

```bash
# Remove existing build directory if present
rm -rf build

# Create fresh build directory
mkdir -p build
```

4. Build the application:

```bash
bash build.sh
```


## Running the Application

After building, the application will monitor the ML predictions log file and publish the data to configured Kafka topics.

## Troubleshooting

- If no data is being processed, check that the `ml_predictions.log` file exists and is being written to by the ML model container.
- Verify Kafka connection settings if messages are not appearing in the expected topics.
- Check application logs for any error messages or connection issues.


## Directory Structure

```
.
├── build/           # Compiled application files
├── build.sh         # Build script
└── README.md        # This file
```


## Notes

- The application depends on the ML model container writing predictions to the specified log file
- Kafka must be running for the application to successfully publish messages

