#!/bin/bash

# link all source files to object files
# Debug log-level use as default
cmake -DCMAKE_BUILD_TYPE=Debug ..

# delete all old binary files and make new
make clean
make -j


# run program with default params
./toyota_remote_controller


