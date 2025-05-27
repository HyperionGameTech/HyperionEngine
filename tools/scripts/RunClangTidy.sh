#!/bin/bash

fix_flag=""
files=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix)
            fix_flag="--fix --fix-errors"
            shift
            ;;
        --files)
            shift
            while [[ $# -gt 0 ]]; do
                files+=("$1")
                shift
            done
            ;;
        *)
            # Skip unrecognized parameters
            shift
            ;;
    esac
done
# If no files provided via --files, recursively find all .cpp files in the src directory
if [ ${#files[@]} -eq 0 ]; then
    while IFS= read -r -d $'\0' file; do
        files+=("$file")
    done < <(find src -type f -name '*.cpp' -print0)
fi

clang-tidy -p build $fix_flag -format-style=file "${files[@]}" --header-filter=.* --exclude-header-filter="^(.*(thirdparty).*)|(include\/.*)" -- -I./src -I./include