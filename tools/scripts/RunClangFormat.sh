#!/bin/bash
# Define the source directory (use current directory if not specified)
SOURCE_DIR="./src"

# Find and run clang-format on all .cpp and .hpp files recursively,
# skipping files inside any src/third_party folder.
find "$SOURCE_DIR" -type f \( -name "*.cpp" -or -name "*.hpp" \) \
    ! -path "third_party/*" \
    -exec clang-format -i -style=file {} \;