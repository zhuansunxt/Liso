#!/bin/bash
# Start Liso HTTP server
./lisod 9999 10000 $(pwd)/log $(pwd)/lisod.lock $(pwd)/../www - - -
