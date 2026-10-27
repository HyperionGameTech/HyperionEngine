#!/bin/bash
# 
#   Tools/Scripts/BuildHyperion.sh distribution tracy ninja regenerate
#   Tools/Scripts/BuildHyperion.sh sdk ninja regenerate
#   Tools/Scripts/PackageEditorLinux.sh [output dir]

set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN_DIR="$ROOT_DIR/Binaries/Linux/Distribution"
OUTPUT_DIR="${1:-$ROOT_DIR/PackagedBuilds/EditorPreview}"

step() { printf '==> %s\n' "$1"; }
warn() { printf 'WARNING: %s\n' "$1" >&2; }

version_component() {
    sed -nE "s/^set\(HYP_VERSION_$1[[:space:]]+([0-9]+)\).*/\1/p" "$ROOT_DIR/Source/CMakeLists.txt" | head -n 1
}

VERSION="$(version_component MAJOR).$(version_component MINOR).$(version_component PATCH)"
STAGE_NAME="Hyperion-Editor-$VERSION-linux-x64"
STAGE_DIR="$OUTPUT_DIR/$STAGE_NAME"

if [[ ! -e "$BIN_DIR/Hyperion.Editor" ]]; then
    echo "Hyperion.Editor not found in $BIN_DIR. Run 'Tools/Scripts/BuildHyperion.sh distribution ninja regenerate' first." >&2
    exit 1
fi

if ! command -v patchelf >/dev/null; then
    echo "patchelf is required to rewrite the libraries' RUNPATH (apt install patchelf)" >&2
    exit 1
fi

step "Packaging Hyperion editor preview $VERSION (Linux x64)"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

# --- Managed editor ---------------------------------------------------------------------------------------------------

# self-contained, so running the editor doesn't need a .NET install (same as the Windows package)
step "Publishing self-contained editor"

EDITOR_PROJECT="$ROOT_DIR/Build/CSharpProjects-Distribution/Hyperion.Editor/Hyperion.Editor.csproj"
if [[ ! -f "$EDITOR_PROJECT" ]]; then
    echo "Editor project not found at $EDITOR_PROJECT" >&2
    exit 1
fi

PUBLISH_ROOT="$OUTPUT_DIR/publish-intermediate"
rm -rf "$PUBLISH_ROOT"

env -u MSBuildSdksPath dotnet publish "$EDITOR_PROJECT" --configuration Release --runtime linux-x64 --self-contained true --disable-build-servers \
    -p:DebugType=none "-p:OutputPath=$PUBLISH_ROOT/build/" --output "$PUBLISH_ROOT/publish"

cp -R "$PUBLISH_ROOT/publish/." "$STAGE_DIR/"
rm -rf "$PUBLISH_ROOT"

# loaded at runtime by ScriptSystem, so the editor's publish doesn't pick it up
cp "$BIN_DIR/Hyperion.NET.Scripting.dll" "$STAGE_DIR/"

# script projects compile against these; at runtime the published copies are used
mkdir -p "$STAGE_DIR/ref"
for name in Hyperion.NET.Shared.dll Hyperion.NET.Runtime.dll Hyperion.NET.Interop.dll Hyperion.NET.Generators.dll; do
    cp "$BIN_DIR/$name" "$STAGE_DIR/ref/"
done
for plugin_assembly in "$BIN_DIR"/Hyperion.Plugin.*.dll; do
    if [[ -f "$plugin_assembly" ]]; then cp "$plugin_assembly" "$STAGE_DIR/ref/"; fi
done

# --- Native binaries --------------------------------------------------------------------------------------------------

step "Staging native libraries"

is_elf() { file -b "$1" | grep -q "^ELF"; }

# -P keeps the libz.so -> libz.so.1 -> libz.so.1.x symlink chain
for path in "$BIN_DIR"/*.so "$BIN_DIR"/*.so.*; do
    cp -P "$path" "$STAGE_DIR/"
done

native_libraries() {
    find "$STAGE_DIR" -maxdepth 1 -type f -name "*.so*" | while read -r candidate; do
        if is_elf "$candidate"; then echo "$candidate"; fi
    done
}

# the build RUNPATH points into the source tree, and debug info names the build machine's object files
while read -r library; do
    chmod u+w "$library"
    strip --strip-debug "$library"
    if [[ -n "$(patchelf --print-rpath "$library")" ]]; then
        patchelf --set-rpath '$ORIGIN' "$library"
    fi
done < <(native_libraries)

step "Staging built-in plugins"

# a built-in plugin's module libraries and descriptor are in Plugins/<Plugin>, with its content; the editor loads them when a project enables
# the plugin. Test plugins and editor-wide library plugins (MCPBridge) aren't part of the editor
for plugin_bin_dir in "$BIN_DIR"/Plugins/*/; do
    plugin_name="$(basename "$plugin_bin_dir")"
    plugin_source_dir="$ROOT_DIR/Source/Plugins/$plugin_name"

    if [[ ! -d "$plugin_source_dir" ]] || ! compgen -G "$plugin_bin_dir/libhyperion-plugin-*.so" > /dev/null; then
        continue
    fi

    plugin_stage_dir="$STAGE_DIR/Plugins/$plugin_name"
    mkdir -p "$plugin_stage_dir"
    cp "$plugin_bin_dir"/libhyperion-plugin-*.so "$plugin_bin_dir"/*.hypplugin "$plugin_stage_dir/"

    if [[ -d "$plugin_source_dir/Content" ]]; then
        cp -R "$plugin_source_dir/Content" "$plugin_stage_dir/"
    fi

    for library in "$plugin_stage_dir"/libhyperion-plugin-*.so; do
        chmod u+w "$library"
        strip --strip-debug "$library"
        patchelf --set-rpath '$ORIGIN/../..' "$library"

        # the plugin's C#, which the editor loads by name when the plugin loads
        module="$(basename "$library" .so)"
        module="${module#libhyperion-plugin-}"

        if [[ -f "$BIN_DIR/Hyperion.Plugin.$module.dll" ]]; then
            cp "$BIN_DIR/Hyperion.Plugin.$module.dll" "$STAGE_DIR/"
        fi
    done
done

# --- Config -----------------------------------------------------------------------------------------------------------

step "Staging config"

mkdir -p "$STAGE_DIR/Config"

# mirrors Source/Sample/CopyPlatformConfigs.cmake for PLATFORM=Linux
for config_file in "$ROOT_DIR"/Config/*; do
    [[ -f "$config_file" ]] || continue
    name="$(basename "$config_file")"

    case "$name" in
        *.IOS.json|*.Android.json|*.Windows.json|*.Mac.json|*.Linux.json|*.Web.json) continue ;;
        *.json)
            linux_variant="$ROOT_DIR/Config/${name%.json}.Linux.json"
            if [[ -f "$linux_variant" ]]; then cp "$linux_variant" "$STAGE_DIR/Config/$name"; else cp "$config_file" "$STAGE_DIR/Config/$name"; fi
            ;;
        *) cp "$config_file" "$STAGE_DIR/Config/$name" ;;
    esac
done

# basedir points at the source tree, trace/cache servers are dev-only localhost endpoints
GLOBAL_CONFIG="$STAGE_DIR/Config/GlobalConfig.json"
if [[ -f "$GLOBAL_CONFIG" ]]; then
    sed -i -E 's# ?--(basedir|traceserver|cacheserver)=[^ "]*##g; s#("Args"[[:space:]]*:[[:space:]]*")#\1--basedir=./ #' "$GLOBAL_CONFIG"
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

step "Staging project templates"
copy_tracked Source/Templates Source/Engine/DotNET/Sdk

if [[ -d "$ROOT_DIR/Data/Scripts/Strata" ]]; then
    step "Staging generated Strata bindings"
    mkdir -p "$STAGE_DIR/Data/Scripts"
    cp -R "$ROOT_DIR/Data/Scripts/Strata" "$STAGE_DIR/Data/Scripts/"
else
    warn "Data/Scripts/Strata not found; Strata scripts won't resolve engine modules"
fi

SDK_CMAKE_FILE="$BIN_DIR/Sdk/CMake/HyperionSdk.cmake"
SDK_STAGE_DIR="$STAGE_DIR/Sdk"

if [[ -f "$SDK_CMAKE_FILE" ]]; then
    step "Staging SDK"

    header_count=0
    while IFS= read -r -d '' relative_path; do
        case "$relative_path" in *.hpp|*.h|*.inl|*.inc) ;; *) continue ;; esac
        [[ -f "$ROOT_DIR/$relative_path" ]] || continue
        destination="$SDK_STAGE_DIR/Include/${relative_path#Source/}"
        mkdir -p "$(dirname "$destination")"
        cp "$ROOT_DIR/$relative_path" "$destination"
        header_count=$((header_count + 1))
    done < <(cd "$ROOT_DIR" && git -c core.quotepath=off ls-files -z -- Source/Core Source/Engine)

    # CodeGen output isn't tracked
    mkdir -p "$SDK_STAGE_DIR/Include/Generated"
    for generated_file in "$ROOT_DIR"/Source/Generated/*.hpp "$ROOT_DIR"/Source/Generated/*.h "$ROOT_DIR"/Source/Generated/*.inl "$ROOT_DIR"/Source/Generated/*.inc; do
        cp "$generated_file" "$SDK_STAGE_DIR/Include/Generated/"
        header_count=$((header_count + 1))
    done
    echo "    $header_count headers"

    if [[ -f "$ROOT_DIR/Source/Generated/EngineTypes.json" && -f "$ROOT_DIR/Build/hyperion-codegen" ]]; then
        cp "$ROOT_DIR/Source/Generated/EngineTypes.json" "$SDK_STAGE_DIR/Include/Generated/"
        mkdir -p "$SDK_STAGE_DIR/Bin"
        cp "$ROOT_DIR/Build/hyperion-codegen" "$SDK_STAGE_DIR/Bin/hyperion-codegen"
    else
        warn "CodeGen not staged (needs Build/hyperion-codegen and Source/Generated/EngineTypes.json); C++ game projects can't be built from this package"
    fi

    third_party_dirs="$(sed -nE 's/^set\(HYPERION_SDK_THIRD_PARTY_DIRS "([^"]*)"\)/\1/p' "$SDK_CMAKE_FILE")"
    for third_party_dir in ${third_party_dirs//;/ }; do
        # External/ThirdParty/Source, or a submodule (tracy/public)
        third_party_source="$ROOT_DIR/External/ThirdParty/Source/$third_party_dir"
        [[ -d "$third_party_source" ]] || third_party_source="$ROOT_DIR/External/Submodules/$third_party_dir"
        mkdir -p "$SDK_STAGE_DIR/ThirdParty/$third_party_dir"
        cp -R "$third_party_source/." "$SDK_STAGE_DIR/ThirdParty/$third_party_dir/"
    done

    mkdir -p "$SDK_STAGE_DIR/CMake"
    cp "$SDK_CMAKE_FILE" "$SDK_STAGE_DIR/CMake/HyperionSdk.cmake"
    cp "$BIN_DIR/Sdk/CMake/HyperionSdkPlatform.cmake" "$SDK_STAGE_DIR/CMake/HyperionSdkPlatform.cmake"

    if [[ -f "$BIN_DIR/Sdk/RuntimeLibraries.txt" ]]; then
        cp "$BIN_DIR/Sdk/RuntimeLibraries.txt" "$SDK_STAGE_DIR/RuntimeLibraries.txt"
    else
        warn "Sdk/RuntimeLibraries.txt is missing so all .so files will be copied without whitelist!"
    fi

    if grep -q -F "$ROOT_DIR" "$SDK_STAGE_DIR/CMake/HyperionSdk.cmake"; then
        warn "Sdk/CMake/HyperionSdk.cmake has the source tree path ($ROOT_DIR) in it"
    fi

    if ! grep -q -F "TRACY_ENABLE" "$SDK_STAGE_DIR/CMake/HyperionSdk.cmake"; then
        warn "The editor was built without Tracy, so games built against it can't be profiled. Rebuild with 'Tools/Scripts/BuildHyperion.sh distribution tracy ninja regenerate'"
    fi

    # Sdk/Shipping: the static HYP_SHIPPING engine Package Game links into a game
    step "Staging shipping SDK"
    source "$SCRIPT_DIR/StageShippingSdk.sh"
    stage_shipping_sdk "$ROOT_DIR/Binaries/Linux/ShippingSdk" "$SDK_STAGE_DIR" "$ROOT_DIR" "*.so*"

    if [[ ! -f "$SDK_STAGE_DIR/Include/Engine/HyperionPch.hpp" || ! -f "$SDK_STAGE_DIR/Include/Generated/CodeGenOutput.inc" ]]; then
        echo "SDK headers are incomplete (HyperionPch.hpp or CodeGen output missing); run CodeGen and rebuild" >&2
        exit 1
    fi
else
    # without it the editor can't build native game projects, so don't ship a package that looks fine but isn't
    echo "SDK not staged: $SDK_CMAKE_FILE not found. Rebuild with 'Tools/Scripts/BuildHyperion.sh distribution ninja regenerate'" >&2
    exit 1
fi

# --- Legal ------------------------------------------------------------------------------------------------------------

cp "$ROOT_DIR/LICENSE" "$STAGE_DIR/LICENSE"
[[ -f "$ROOT_DIR/THIRD_PARTY_NOTICES.md" ]] && cp "$ROOT_DIR/THIRD_PARTY_NOTICES.md" "$STAGE_DIR/"
[[ -d "$ROOT_DIR/Documentation/ThirdPartyLicenses" ]] && cp -R "$ROOT_DIR/Documentation/ThirdPartyLicenses" "$STAGE_DIR/Licenses"

# --- Checks and archive -----------------------------------------------------------------------------------------------

step "Checking for paths that only exist on the build machine"
while read -r library; do
    rpath="$(patchelf --print-rpath "$library")"
    if [[ -n "$rpath" && "$rpath" != '$ORIGIN' ]]; then
        warn "$(basename "$library") still has RUNPATH $rpath"
    fi

    missing="$(ldd "$library" | awk '/not found/ {print $1}')"
    if [[ -n "$missing" ]]; then
        warn "$(basename "$library") links libraries that aren't on this machine or in the package: $(echo $missing)"
    fi
done < <(native_libraries)

if grep -q -a -F "$ROOT_DIR" "$STAGE_DIR"/libhyperion*.so 2>/dev/null; then
    warn "the engine library has the source tree path ($ROOT_DIR) baked in; build with the 'distribution' argument"
fi

step "Creating $STAGE_NAME.tar.gz"
tar -czf "$OUTPUT_DIR/$STAGE_NAME.tar.gz" -C "$OUTPUT_DIR" "$STAGE_NAME"

echo
echo "Staged $STAGE_NAME ($(du -sh "$STAGE_DIR" | cut -f1)) at $STAGE_DIR"
