#!/bin/bash

# deploy.sh - Syncs all modules from source data folders and executables to deploy folder
# Usage: ./deploy.sh <deploy_folder_path> [build_dir] [client_exec_path] [server_exec_path]

set -e  # Exit on error

# Check if deploy folder path is provided
if [ $# -eq 0 ]; then
    echo "Error: Deploy folder path required"
    echo "Usage: $0 <deploy_folder_path> [build_dir] [client_exec_path] [server_exec_path]"
    exit 1
fi

DEPLOY_DIR="$1"
BUILD_DIR="$2"
CLIENT_EXEC_PATH="$3"
SERVER_EXEC_PATH="$4"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Source module directories
MODULES="$PROJECT_ROOT/data/modules"
DASLIB_SRC="$PROJECT_ROOT/thirdparty/dascript/daslib"

# Executable names
CLIENT_EXEC="game-client"
SERVER_EXEC="game-server"

# Default build directory if not provided
if [ -z "$BUILD_DIR" ]; then
    BUILD_DIR="$PROJECT_ROOT/build"
fi

# Default executable paths if not provided
if [ -z "$CLIENT_EXEC_PATH" ]; then
    CLIENT_EXEC_PATH="$BUILD_DIR/src/client/$CLIENT_EXEC"
fi

if [ -z "$SERVER_EXEC_PATH" ]; then
    SERVER_EXEC_PATH="$BUILD_DIR/src/server/$SERVER_EXEC"
fi

# Destination modules directory
DEPLOY_MODULES="$DEPLOY_DIR/modules"
DEPLOY_DAS_ROOT="$DEPLOY_DIR/daslib"
DEPLOY_DASLIB="$DEPLOY_DAS_ROOT/daslib"

# Check if source directories exist
if [ ! -d "$MODULES" ]; then
    echo "Warning: Source directory not found: $MODULES"
fi

# Create deploy directory if it doesn't exist
mkdir -p "$DEPLOY_DIR"
mkdir -p "$DEPLOY_MODULES"
mkdir -p "$DEPLOY_DASLIB"

# Array to track synced modules
SYNCED_MODULES=()

# Function to sync a module folder
sync_module() {
    local source_dir="$1"
    local module_name="$2"
    local dest_dir="$DEPLOY_MODULES/$module_name"
    
    if [ -d "$source_dir/$module_name" ]; then
        echo "Syncing module: $module_name from $source_dir"
        rsync -av --delete "$source_dir/$module_name/" "$dest_dir/"
        # Add to synced modules list (avoid duplicates)
        if [[ ! " ${SYNCED_MODULES[@]} " =~ " ${module_name} " ]]; then
            SYNCED_MODULES+=("$module_name")
        fi
    fi
}

# Sync modules from data/modules
if [ -d "$MODULES" ]; then
    echo "Syncing modules from: $MODULES"
    for module_dir in "$MODULES"/*; do
        if [ -d "$module_dir" ]; then
            module_name=$(basename "$module_dir")
            sync_module "$MODULES" "$module_name"
        fi
    done
fi

# Create modlist.txt with all synced modules
MODLIST_FILE="$DEPLOY_MODULES/modlist.txt"
if [ ${#SYNCED_MODULES[@]} -gt 0 ]; then
    echo "Creating modlist.txt with ${#SYNCED_MODULES[@]} module(s)..."
    printf '%s\n' "${SYNCED_MODULES[@]}" | sort > "$MODLIST_FILE"
    echo "  modlist.txt created: $MODLIST_FILE"
else
    echo "Warning: No modules were synced, modlist.txt not created"
fi

# Sync daScript runtime daslib files used by script compiler/runtime.
if [ -d "$DASLIB_SRC" ]; then
    echo "Syncing daScript daslib from: $DASLIB_SRC"
    rsync -av --delete "$DASLIB_SRC/" "$DEPLOY_DASLIB/"
else
    echo "Warning: daScript daslib source not found: $DASLIB_SRC"
fi

# Sync executables from build directory
echo ""
echo "Syncing executables from build directory..."

# Track which executables were found
CLIENT_FOUND=0
SERVER_FOUND=0

# Function to sync an executable
sync_executable() {
    local exec_path="$1"
    local exec_name="$2"
    
    if [ -f "$exec_path" ]; then
        if [ -x "$exec_path" ]; then
            echo "Found $exec_name at $exec_path"
            echo "Syncing $exec_name to $DEPLOY_DIR"
            rsync -av "$exec_path" "$DEPLOY_DIR/$exec_name"
            return 0
        else
            echo "Warning: $exec_name found at $exec_path but is not executable"
            return 1
        fi
    else
        echo "Warning: $exec_name not found at $exec_path"
        return 1
    fi
}

# Sync client executable
if sync_executable "$CLIENT_EXEC_PATH" "$CLIENT_EXEC"; then
    CLIENT_FOUND=1
fi

# Sync server executable
if sync_executable "$SERVER_EXEC_PATH" "$SERVER_EXEC"; then
    SERVER_FOUND=1
fi

# If executables not found at specified paths, try common build locations
if [ $CLIENT_FOUND -eq 0 ] || [ $SERVER_FOUND -eq 0 ]; then
    echo ""
    echo "Trying alternative build locations..."
    
    # Try common build directory structures
    ALTERNATIVE_PATHS=(
        "$BUILD_DIR/src/client/$CLIENT_EXEC"
        "$BUILD_DIR/src/server/$SERVER_EXEC"
        "$PROJECT_ROOT/build/src/client/$CLIENT_EXEC"
        "$PROJECT_ROOT/build/src/server/$SERVER_EXEC"
    )
    
    if [ $CLIENT_FOUND -eq 0 ]; then
        for path in "${ALTERNATIVE_PATHS[@]}"; do
            if [[ "$path" == *"$CLIENT_EXEC"* ]] && [ -f "$path" ] && [ -x "$path" ]; then
                echo "Found $CLIENT_EXEC at alternative location: $path"
                rsync -av "$path" "$DEPLOY_DIR/$CLIENT_EXEC"
                CLIENT_FOUND=1
                break
            fi
        done
    fi
    
    if [ $SERVER_FOUND -eq 0 ]; then
        for path in "${ALTERNATIVE_PATHS[@]}"; do
            if [[ "$path" == *"$SERVER_EXEC"* ]] && [ -f "$path" ] && [ -x "$path" ]; then
                echo "Found $SERVER_EXEC at alternative location: $path"
                rsync -av "$path" "$DEPLOY_DIR/$SERVER_EXEC"
                SERVER_FOUND=1
                break
            fi
        done
    fi
fi

# Summary
echo ""
echo "Deployment complete."
echo "  Modules synced to: $DEPLOY_MODULES"
echo "  daScript root synced to: $DEPLOY_DAS_ROOT"
if [ $CLIENT_FOUND -eq 1 ] && [ $SERVER_FOUND -eq 1 ]; then
    echo "  Executables synced to: $DEPLOY_DIR (both found)"
elif [ $CLIENT_FOUND -eq 1 ] || [ $SERVER_FOUND -eq 1 ]; then
    echo "  Executables synced to: $DEPLOY_DIR (partial - some missing)"
    if [ $CLIENT_FOUND -eq 0 ]; then
        echo "    Missing: $CLIENT_EXEC"
    fi
    if [ $SERVER_FOUND -eq 0 ]; then
        echo "    Missing: $SERVER_EXEC"
    fi
else
    echo "  Executables: None found"
    echo "    Build the project first: cmake --build build"
    echo "    Expected locations:"
    echo "      Client: $CLIENT_EXEC_PATH"
    echo "      Server: $SERVER_EXEC_PATH"
fi
