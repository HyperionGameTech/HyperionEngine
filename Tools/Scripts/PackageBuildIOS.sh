#!/bin/bash
set -e
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HYP_ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "Running shipping build (BuildHyperion.sh shipping IOS ninja regenerate)..."
"$SCRIPT_DIR/BuildHyperion.sh" shipping IOS ninja regenerate

BIN_DIR="$HYP_ROOT_DIR/Binaries/IOS/Shipping"

# Commandlets can't run on-device, and aren't built for shipping anyway, so shader
# precompilation and cooking use the Mac host tools (built via a normal Release build).
BIN_DIR_RELEASE="$HYP_ROOT_DIR/Binaries/Mac/Release"

if [[ ! -d "$BIN_DIR_RELEASE" ]]; then
    echo "ERROR: Mac Release build not found at $BIN_DIR_RELEASE. Build a non-shipping Mac Release config first (needed for commandlet tools)."
    exit 1
fi

echo "Running PrecompileShaders commandlet..."

# Compile shaders for only iOS
"$BIN_DIR_RELEASE/PrecompileShaders" --platform=ios

echo "Running Cook commandlet..."

read -r -p "Enter the project name (folder under Projects/): " PROJECT_NAME
if [[ -z "$PROJECT_NAME" ]]; then
    echo "No project name entered, aborting packaged build."
    exit 1
fi


TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$HYP_ROOT_DIR/PackagedBuilds/IOS/Build_$TIMESTAMP"

echo "Creating packaged build at: $OUT_DIR"

"$BIN_DIR_RELEASE/BlobStorageCookCommandlet" --project=Projects/"$PROJECT_NAME" --out-cache="$OUT_DIR/Cache" --out-content="$OUT_DIR/Content"

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
for f in "$HYP_ROOT_DIR"/Config/*Config.IOS.json; do
    cp "$f" "$OUT_DIR/"
done

echo "Updating GlobalConfig.json for packaged build..."
if [[ -f "$OUT_DIR/GlobalConfig.json" ]]; then
    sed -i '' -E 's#--basedir=[A-Za-z0-9/._-]+#--basedir=./#' "$OUT_DIR/GlobalConfig.json"
fi

echo "Copying content and cache..."

echo "Copying Content..."
if [[ -d "$BIN_DIR/Content" ]]; then
    cp -R "$BIN_DIR/Content" "$OUT_DIR/Content"
fi

echo "Copying Cache..."
if [[ -d "$BIN_DIR/Cache" ]]; then
    cp -R "$BIN_DIR/Cache" "$OUT_DIR/Cache"
fi

echo "Copying Shaders.hmf..."
cp "$HYP_ROOT_DIR/Config/Shaders.hmf" "$OUT_DIR/Config/Shaders.hmf"

echo "Done! Packaged build created at: $OUT_DIR"
echo "NOTE: this is a raw Ninja build output, not a signed .app/.ipa. Installing on a"
echo "      device or the simulator still requires taking this through Xcode (or"
echo "      xcodebuild/codesign) with a valid provisioning profile."
