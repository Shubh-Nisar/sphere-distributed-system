#!/bin/bash

cd build
cmake ..
make
./fetch_pod_metrics
