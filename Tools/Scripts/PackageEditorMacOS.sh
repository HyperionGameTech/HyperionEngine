#!/bin/bash
# Stages a relocatable macOS editor preview from Binaries/Mac/Distribution and archives it.
#
#   Tools/Scripts/BuildHyperion.sh distribution ninja regenerate
#   Tools/Scripts/PackageEditorMacOS.sh [output dir]
#
# Homebrew and SDK libraries the binaries link against are copied in and their load paths rewritten, so the
# result runs on a Mac without Homebrew or the Vulkan SDK. The archive is a .tar.gz because zip-based
# transports (GitHub artifacts) drop the executable bits.
set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN_DIR="$ROOT_DIR/Binaries/Mac/Distribution"
OUTPUT_DIR="${1:-$ROOT_DIR/PackagedBuilds/EditorPreview}"

step() { printf '==> %s\n' "$1"; }
warn() { printf 'WARNING: %s\n' "$1" >&2; }

version_component() {
    sed -nE "s/^set\(HYP_VERSION_$1[[:space:]]+([0-9]+)\).*/\1/p" "$ROOT_DIR/Source/CMakeLists.txt" | head -n 1
}

VERSION="$(version_component MAJOR).$(version_component MINOR).$(version_component PATCH)"
STAGE_NAME="Hyperion-Editor-$VERSION-macos-arm64"
STAGE_DIR="$OUTPUT_DIR/$STAGE_NAME"

if [[ ! -e "$BIN_DIR/Hyperion.Editor" ]]; then
    echo "Hyperion.Editor not found in $BIN_DIR. Run 'Tools/Scripts/BuildHyperion.sh distribution ninja regenerate' first." >&2
    exit 1
fi

step "Packaging Hyperion editor preview $VERSION (macOS arm64)"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

# --- Binaries ---------------------------------------------------------------------------------------------------------

step "Staging binaries"

is_macho() { file -b "$1" | grep -q "Mach-O"; }

# sample, commandlets and Strata tools are not part of the editor preview
is_excluded_executable() {
    case "$(basename "$1")" in
        hyperion-sample*|CSharpGame|stratac|*Commandlet|PrecompileShaders|GenerateBlueNoiseBlob|*_test|*_demo|opt_bench|strata_tests) return 0 ;;
    esac
    return 1
}

for path in "$BIN_DIR"/*; do
    [[ -f "$path" ]] || continue
    name="$(basename "$path")"

    case "$name" in
        *.dylib|*.dll|*.json) cp "$path" "$STAGE_DIR/" ;;
        *.pdb|*.xml|*.a|*.dSYM) ;;
        *)
            if [[ -x "$path" ]] && is_macho "$path" && ! is_excluded_executable "$path"; then
                cp "$path" "$STAGE_DIR/"
            fi
            ;;
    esac
done

# managed dependencies ship their native libraries (Skia, HarfBuzz, ANGLE) under runtimes/
for rid_dir in "$BIN_DIR"/runtimes/osx "$BIN_DIR"/runtimes/osx-arm64; do
    [[ -d "$rid_dir" ]] || continue
    mkdir -p "$STAGE_DIR/runtimes"
    cp -R "$rid_dir" "$STAGE_DIR/runtimes/"
done

# script projects compile against these (same layout as the Windows package)
mkdir -p "$STAGE_DIR/ref"
for name in Hyperion.NET.Shared.dll Hyperion.NET.Runtime.dll Hyperion.NET.Interop.dll; do
    [[ -f "$BIN_DIR/$name" ]] && cp "$BIN_DIR/$name" "$STAGE_DIR/ref/"
done

# --- Bundle non-system libraries --------------------------------------------------------------------------------------

step "Bundling library dependencies"

macho_files() {
    find "$STAGE_DIR" -type f \( -name "*.dylib" -o -perm -u+x \) -not -path "*/ref/*" | while read -r candidate; do
        if is_macho "$candidate"; then echo "$candidate"; fi
    done
}

external_dependencies() {
    otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -Ev '^(/usr/lib/|/System/|@rpath/|@loader_path/|@executable_path/)' || true
}

# copying a library in can pull in more, so repeat until nothing external is left
for pass in 1 2 3 4 5 6; do
    changed=0

    while read -r binary; do
        for dependency in $(external_dependencies "$binary"); do
            dependency_name="$(basename "$dependency")"

            if [[ "$dependency_name" == "$(basename "$binary")" ]]; then
                continue
            fi

            if [[ ! -e "$STAGE_DIR/$dependency_name" ]]; then
                if [[ ! -e "$dependency" ]]; then
                    warn "$(basename "$binary") links $dependency, which does not exist on this machine"
                    continue
                fi
                echo "    $dependency_name  (from $dependency)"
                cp -L "$dependency" "$STAGE_DIR/$dependency_name"
                chmod u+w "$STAGE_DIR/$dependency_name"
                changed=1
            fi

            install_name_tool -change "$dependency" "@rpath/$dependency_name" "$binary"
        done
    done < <(macho_files)

    [[ $changed -eq 1 ]] || break
done

while read -r binary; do
    if [[ "$binary" == *.dylib ]]; then
        install_name_tool -id "@rpath/$(basename "$binary")" "$binary"
    fi

    # resolve @rpath/ next to the binary itself, and drop the build machine's absolute search paths
    for rpath in $(otool -l "$binary" | awk '/LC_RPATH/ { getline; getline; print $2 }'); do
        if [[ "$rpath" != @* ]]; then
            install_name_tool -delete_rpath "$rpath" "$binary" 2>/dev/null || true
        fi
    done
    install_name_tool -add_rpath "@loader_path" "$binary" 2>/dev/null || true
done < <(macho_files)

# --- Vulkan driver ----------------------------------------------------------------------------------------------------

# The Vulkan loader finds MoltenVK through a driver manifest; point it at the bundled copy from the launcher.
MOLTENVK_SOURCE=""
for candidate in "$STAGE_DIR/libMoltenVK.dylib" "$ROOT_DIR/External/ThirdParty/Binaries/Mac/Release/libMoltenVK.dylib" /opt/homebrew/lib/libMoltenVK.dylib; do
    if [[ -e "$candidate" ]]; then MOLTENVK_SOURCE="$candidate"; break; fi
done

if [[ -n "$MOLTENVK_SOURCE" ]]; then
    [[ -e "$STAGE_DIR/libMoltenVK.dylib" ]] || cp -L "$MOLTENVK_SOURCE" "$STAGE_DIR/libMoltenVK.dylib"
    mkdir -p "$STAGE_DIR/vulkan/icd.d"
    cat > "$STAGE_DIR/vulkan/icd.d/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../libMoltenVK.dylib",
        "api_version": "1.2.0",
        "is_portability_driver": true
    }
}
EOF
else
    warn "libMoltenVK.dylib not found; the package will rely on a Vulkan driver installed on the user's machine"
fi

cat > "$STAGE_DIR/Hyperion Editor.command" <<'EOF'
#!/bin/bash
# Double-click to start the editor.
DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ -f "$DIR/vulkan/icd.d/MoltenVK_icd.json" ]]; then
    export VK_DRIVER_FILES="$DIR/vulkan/icd.d/MoltenVK_icd.json"
    export VK_ICD_FILENAMES="$VK_DRIVER_FILES"
fi
cd "$DIR"
exec "$DIR/Hyperion.Editor" "$@"
EOF
chmod +x "$STAGE_DIR/Hyperion Editor.command"

# --- Re-sign ----------------------------------------------------------------------------------------------------------

# Apple Silicon refuses to run a binary whose signature no longer matches, and install_name_tool invalidates it.
# This is an ad-hoc signature: enough to run, but Gatekeeper still treats the download as unidentified.
step "Ad-hoc signing"
while read -r binary; do
    codesign --force --sign - "$binary" 2>/dev/null || warn "codesign failed for $(basename "$binary")"
done < <(macho_files)

# --- Config -----------------------------------------------------------------------------------------------------------

step "Staging config"

mkdir -p "$STAGE_DIR/Config"

# mirrors Source/Sample/CopyPlatformConfigs.cmake for PLATFORM=Mac
for config_file in "$ROOT_DIR"/Config/*; do
    [[ -f "$config_file" ]] || continue
    name="$(basename "$config_file")"

    case "$name" in
        *.IOS.json|*.Android.json|*.Windows.json|*.Mac.json|*.Linux.json) continue ;;
        *.json)
            mac_variant="$ROOT_DIR/Config/${name%.json}.Mac.json"
            if [[ -f "$mac_variant" ]]; then cp "$mac_variant" "$STAGE_DIR/Config/$name"; else cp "$config_file" "$STAGE_DIR/Config/$name"; fi
            ;;
        *) cp "$config_file" "$STAGE_DIR/Config/$name" ;;
    esac
done

# basedir points at the source tree, trace/cache servers are dev-only localhost endpoints
GLOBAL_CONFIG="$STAGE_DIR/Config/GlobalConfig.json"
if [[ -f "$GLOBAL_CONFIG" ]]; then
    sed -i '' -E 's# ?--(basedir|traceserver|cacheserver)=[^ "]*##g; s#("Args"[[:space:]]*:[[:space:]]*")#\1--basedir=./ #' "$GLOBAL_CONFIG"
fi

# --- Content, shaders, scripts ----------------------------------------------------------------------------------------

copy_tracked() {
    local count=0
    while IFS= read -r -d '' relative_path; do
        [[ -f "$ROOT_DIR/$relative_path" ]] || continue
        mkdir -p "$STAGE_DIR/$(dirname "$relative_path")"
        cp "$ROOT_DIR/$relative_path" "$STAGE_DIR/$relative_path"
        count=$((count + 1))
    done < <(cd "$ROOT_DIR" && git -c core.quotepath=off ls-files -z -- "$@")
    echo "    $count files"
}

step "Staging engine and editor content"
copy_tracked Content/Engine Content/Editor

step "Staging shader sources"
copy_tracked Source/Shaders

if [[ -d "$ROOT_DIR/Data/Scripts/Strata" ]]; then
    step "Staging generated Strata bindings"
    mkdir -p "$STAGE_DIR/Data/Scripts"
    cp -R "$ROOT_DIR/Data/Scripts/Strata" "$STAGE_DIR/Data/Scripts/"
else
    warn "Data/Scripts/Strata not found; Strata scripts won't resolve engine modules"
fi

# --- Legal ------------------------------------------------------------------------------------------------------------

cp "$ROOT_DIR/LICENSE" "$STAGE_DIR/LICENSE"
[[ -f "$ROOT_DIR/THIRD_PARTY_NOTICES.md" ]] && cp "$ROOT_DIR/THIRD_PARTY_NOTICES.md" "$STAGE_DIR/"
[[ -d "$ROOT_DIR/Documentation/ThirdPartyLicenses" ]] && cp -R "$ROOT_DIR/Documentation/ThirdPartyLicenses" "$STAGE_DIR/Licenses"

# --- Checks and archive -----------------------------------------------------------------------------------------------

step "Checking for paths that only exist on the build machine"
while read -r binary; do
    leftover="$(external_dependencies "$binary")"
    if [[ -n "$leftover" ]]; then
        warn "$(basename "$binary") still links: $(echo $leftover)"
    fi
done < <(macho_files)

if grep -q -a -F "$ROOT_DIR" "$STAGE_DIR"/libhyperion*.dylib 2>/dev/null; then
    warn "the engine library has the source tree path ($ROOT_DIR) baked in; build with the 'distribution' argument"
fi

step "Creating $STAGE_NAME.tar.gz"
tar -czf "$OUTPUT_DIR/$STAGE_NAME.tar.gz" -C "$OUTPUT_DIR" "$STAGE_NAME"

echo
echo "Staged $STAGE_NAME ($(du -sh "$STAGE_DIR" | cut -f1)) at $STAGE_DIR"
