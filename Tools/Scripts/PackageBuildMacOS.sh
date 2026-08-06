#!/bin/bash
set -e
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HYP_ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "Running shipping build (BuildHyperion.sh shipping ninja regenerate)..."
"$SCRIPT_DIR/BuildHyperion.sh" shipping ninja regenerate

BIN_DIR="$HYP_ROOT_DIR/Binaries/Mac/Shipping"

# Commandlets not included / won't work for shipping so we use release
BIN_DIR_RELEASE="$HYP_ROOT_DIR/Binaries/Mac/Release"

if [[ ! -d "$BIN_DIR_RELEASE" ]]; then
    echo "ERROR: Release build not found at $BIN_DIR_RELEASE. Build a non-shipping Release config first (needed for commandlet tools)."
    exit 1
fi

echo "Running PrecompileShaders commandlet..."

# Compile shaders for only Mac
"$BIN_DIR_RELEASE/PrecompileShaders" --platform=mac

echo "Running Cook commandlet..."

read -r -p "Enter the project name (folder under Projects/): " PROJECT_NAME
if [[ -z "$PROJECT_NAME" ]]; then
    echo "No project name entered, aborting packaged build."
    exit 1
fi

echo "Cooking project: $PROJECT_NAME"

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$HYP_ROOT_DIR/PackagedBuilds/Mac/Build_$TIMESTAMP"
echo "Creating packaged build at: $OUT_DIR"

"$BIN_DIR_RELEASE/BlobStorageCookCommandlet" --content=Projects/"$PROJECT_NAME" --CacheDir="$OUT_DIR/Cache"


mkdir -p "$OUT_DIR/Source/Shaders"

echo "Copying executable..."
find "$BIN_DIR" -maxdepth 1 -type f -perm -u+x ! -name "*.dylib" -exec cp {} "$OUT_DIR/" \;

echo "Copying dylibs..."
for f in "$BIN_DIR"/*.dylib; do
    cp "$f" "$OUT_DIR/"
done

echo "Copying config files..."
for f in "$HYP_ROOT_DIR"/Config/*Config.json; do
    cp "$f" "$OUT_DIR/"
done
for f in "$HYP_ROOT_DIR"/Config/*Config.Mac.json; do
    cp "$f" "$OUT_DIR/"
done

echo "Updating GlobalConfig.json for packaged build..."
if [[ -f "$OUT_DIR/GlobalConfig.json" ]]; then
    sed -i '' -E 's#-BaseDir=[A-Za-z0-9/._-]+#-BaseDir=./#' "$OUT_DIR/GlobalConfig.json"
fi

echo "Copying Shaders.ini..."
cp "$HYP_ROOT_DIR/Source/Shaders/Shaders.ini" "$OUT_DIR/Source/Shaders/"

# For development build, copy the game actions vdf and steam app id
echo "Copying development steam files..."
# This will need to not be hardcoded obviously!
cp "$HYP_ROOT_DIR/Source/Sample/DefaultGame/game_actions_480.vdf" "$OUT_DIR/game_actions_480.vdf"
cp "$HYP_ROOT_DIR/Source/Sample/DefaultGame/steam_appid.txt" "$OUT_DIR/steam_appid.txt"

echo "Done! Packaged build created at: $OUT_DIR"
