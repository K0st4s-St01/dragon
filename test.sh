#!/bin/bash
set -e

cd "$(dirname "$0")/build"

cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target test_all
ctest --output-on-failure
