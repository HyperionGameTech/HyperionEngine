#!/bin/bash

# Script to move C bindings from .NET-specific directories to generalized scripting/bindings directory
# This makes the bindings available for multiple scripting language integrations

set -e  # Exit on any error

# Define source and destination directories
DOTNET_RUNTIME_DIR="src/dotnet/runtime"
DOTNET_CORE_DIR="src/dotnet/core"
DEST_DIR="src/scripting/bindings"

# Create destination directory if it doesn't exist
echo "Creating destination directory: $DEST_DIR"
mkdir -p "$DEST_DIR"

# Function to move .cpp files while preserving directory structure
move_cpp_files() {
    local src_dir="$1"
    local relative_path="$2"
    
    if [ ! -d "$src_dir" ]; then
        echo "Warning: Source directory $src_dir does not exist, skipping..."
        return
    fi
    
    echo "Processing directory: $src_dir"
    
    # Find all .cpp files in the source directory and its subdirectories
    find "$src_dir" -name "*.cpp" -type f | while read -r cpp_file; do
        # Get the relative path from the source directory
        rel_path="${cpp_file#$src_dir/}"
        
        # Create the destination directory structure preserving subdirectories
        dest_file="$DEST_DIR/$relative_path/$rel_path"
        dest_dir=$(dirname "$dest_file")
        
        echo "Moving: $cpp_file -> $dest_file"
        mkdir -p "$dest_dir"
        mv "$cpp_file" "$dest_file"
    done
}

echo "Starting migration of C bindings to scripting/bindings directory..."

# Move files from dotnet/runtime
if [ -d "$DOTNET_RUNTIME_DIR" ]; then
    move_cpp_files "$DOTNET_RUNTIME_DIR" "runtime"
else
    echo "Warning: $DOTNET_RUNTIME_DIR directory not found"
fi

# Move files from dotnet/core  
if [ -d "$DOTNET_CORE_DIR" ]; then
    move_cpp_files "$DOTNET_CORE_DIR" "core"
else
    echo "Warning: $DOTNET_CORE_DIR directory not found"
fi

echo "Migration completed successfully!"
echo "C binding files have been moved to: $DEST_DIR"

echo ""
echo "Next steps:"
echo "1. Update your build system (CMakeLists.txt, Makefile, etc.) to reference the new paths"
echo "2. Update any #include statements in other files that reference these bindings"
echo "3. Consider updating the file headers to reflect their new generalized purpose"