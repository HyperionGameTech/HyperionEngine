#!/bin/bash
# Stages a relocatable macOS editor preview (Hyperion Editor.app) from Binaries/Mac/Distribution and archives it.
#
#   Tools/Scripts/BuildHyperion.sh distribution tracy ninja regenerate
#   Tools/Scripts/BuildHyperion.sh sdk ninja regenerate
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
BUNDLE_ID="io.github.ajmd17.HyperionEditor"

# the engine resolves Config, Content and Data next to its executable, so they all live in Contents/MacOS
APP_DIR="$STAGE_DIR/Hyperion Editor.app"
APP_ROOT="$APP_DIR/Contents/MacOS"
APP_RESOURCES="$APP_DIR/Contents/Resources"

if [[ ! -e "$BIN_DIR/Hyperion.Editor" ]]; then
    echo "Hyperion.Editor not found in $BIN_DIR. Run 'Tools/Scripts/BuildHyperion.sh distribution ninja regenerate' first." >&2
    exit 1
fi

step "Packaging Hyperion editor preview $VERSION (macOS arm64)"

rm -rf "$STAGE_DIR"
mkdir -p "$APP_ROOT" "$APP_RESOURCES"

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

env -u MSBuildSdksPath dotnet publish "$EDITOR_PROJECT" --configuration Release --runtime osx-arm64 --self-contained true --disable-build-servers \
    -p:DebugType=none "-p:OutputPath=$PUBLISH_ROOT/build/" --output "$PUBLISH_ROOT/publish"

cp -R "$PUBLISH_ROOT/publish/." "$APP_ROOT/"
rm -rf "$PUBLISH_ROOT"

# loaded at runtime by ScriptSystem, so the editor's publish doesn't pick it up
cp "$BIN_DIR/Hyperion.NET.Scripting.dll" "$APP_ROOT/"

# script projects compile against these; at runtime the published copies are used
mkdir -p "$APP_ROOT/ref"
for name in Hyperion.NET.Shared.dll Hyperion.NET.Runtime.dll Hyperion.NET.Interop.dll Hyperion.NET.Generators.dll; do
    cp "$BIN_DIR/$name" "$APP_ROOT/ref/"
done
for plugin_assembly in "$BIN_DIR"/Hyperion.Plugin.*.dll; do
    if [[ -f "$plugin_assembly" ]]; then cp "$plugin_assembly" "$APP_ROOT/ref/"; fi
done

# --- Native binaries --------------------------------------------------------------------------------------------------

step "Staging native libraries"

is_macho() { file -b "$1" | grep -q "Mach-O"; }

for path in "$BIN_DIR"/*.dylib; do
    cp "$path" "$APP_ROOT/"
    # the debug map names the build machine's object files
    strip -S "$APP_ROOT/$(basename "$path")" 2> >(grep -v "invalidate the code signature" >&2)
done

# a built-in plugin's module libraries and descriptor are in Plugins/<Plugin>, with its content; the editor loads them when a project enables
# the plugin. Test plugins and editor-wide library plugins (MCPBridge) aren't part of the editor
for plugin_bin_dir in "$BIN_DIR"/Plugins/*/; do
    plugin_name="$(basename "$plugin_bin_dir")"
    plugin_source_dir="$ROOT_DIR/Source/Plugins/$plugin_name"

    if [[ ! -d "$plugin_source_dir" ]] || ! compgen -G "$plugin_bin_dir/libhyperion-plugin-*.dylib" > /dev/null; then
        continue
    fi

    plugin_stage_dir="$APP_ROOT/Plugins/$plugin_name"
    mkdir -p "$plugin_stage_dir"
    cp "$plugin_bin_dir"/libhyperion-plugin-*.dylib "$plugin_bin_dir"/*.hypplugin "$plugin_stage_dir/"

    if [[ -d "$plugin_source_dir/Content" ]]; then
        cp -R "$plugin_source_dir/Content" "$plugin_stage_dir/"
    fi

    for library in "$plugin_stage_dir"/libhyperion-plugin-*.dylib; do
        strip -S "$library" 2> >(grep -v "invalidate the code signature" >&2)
        install_name_tool -add_rpath "@loader_path/../.." "$library" 2>/dev/null || true

        # the plugin's C#, which the editor loads by name when the plugin loads
        module="$(basename "$library" .dylib)"
        module="${module#libhyperion-plugin-}"

        if [[ -f "$BIN_DIR/Hyperion.Plugin.$module.dll" ]]; then
            cp "$BIN_DIR/Hyperion.Plugin.$module.dll" "$APP_ROOT/"
        fi
    done
done

# --- Bundle non-system libraries --------------------------------------------------------------------------------------

step "Bundling library dependencies"

macho_files() {
    find "$APP_ROOT" -type f \( -name "*.dylib" -o -perm -u+x \) -not -path "*/ref/*" | while read -r candidate; do
        if is_macho "$candidate"; then echo "$candidate"; fi
    done
}

# install_name_tool warns on every edit that the signature is now invalid; everything is re-signed below
relink() {
    install_name_tool "$@" 2> >(grep -v "invalidate the code signature" >&2)
}

# otool prints a header line per architecture of a universal binary, and lists a dylib's own install name first
linked_libraries() {
    local own_id
    own_id="$(otool -D "$1" | grep -v ':$' | head -n 1)"
    otool -L "$1" | grep -v ':$' | awk '{print $1}' | sort -u | grep -v -x -F "${own_id:-/dev/null}" || true
}

external_dependencies() {
    linked_libraries "$1" | grep -Ev '^(/usr/lib/|/System/|@rpath/|@loader_path/|@executable_path/)' || true
}

rpath_dependencies() {
    linked_libraries "$1" | sed -n 's#^@rpath/\([^/]*\)$#\1#p'
}

all_rpaths() {
    otool -l "$1" | awk '/LC_RPATH/ { getline; getline; print $2 }'
}

absolute_rpaths() {
    all_rpaths "$1" | grep -v '^@' || true
}

# where each bundled library was copied from, so its @loader_path rpaths (common in Homebrew) can be followed
ORIGINS_DIR="$(mktemp -d)"
trap 'rm -rf "$ORIGINS_DIR"' EXIT

original_dir() {
    local origin_file="$ORIGINS_DIR/$(basename "$1")"
    if [[ -f "$origin_file" ]]; then cat "$origin_file"; else echo "$BIN_DIR"; fi
}

rpath_search_dirs() {
    local rpath
    for rpath in $(all_rpaths "$1"); do
        case "$rpath" in
            @loader_path*) echo "$(original_dir "$1")${rpath#@loader_path}" ;;
            @*) ;;
            *) echo "$rpath" ;;
        esac
    done
}

stage_library() {
    echo "    $2  (from $1)"
    cp -L "$1" "$APP_ROOT/$2"
    chmod u+w "$APP_ROOT/$2"
    dirname "$1" > "$ORIGINS_DIR/$2"
}

# copying a library in can pull in more, so repeat until nothing external is left
for pass in 1 2 3 4 5 6; do
    changed=0

    while read -r binary; do
        for dependency in $(external_dependencies "$binary"); do
            dependency_name="$(basename "$dependency")"

            if [[ ! -e "$APP_ROOT/$dependency_name" ]]; then
                if [[ ! -e "$dependency" ]]; then
                    warn "$(basename "$binary") links $dependency, which does not exist on this machine"
                    continue
                fi
                stage_library "$dependency" "$dependency_name"
                changed=1
            fi

            relink -change "$dependency" "@rpath/$dependency_name" "$binary"
        done

        # @rpath/ libraries already resolve next to the binary, but have to be found through the build machine's rpaths first
        for dependency_name in $(rpath_dependencies "$binary"); do
            [[ -e "$APP_ROOT/$dependency_name" ]] && continue

            for rpath in $(rpath_search_dirs "$binary"); do
                if [[ -e "$rpath/$dependency_name" ]]; then
                    stage_library "$rpath/$dependency_name" "$dependency_name"
                    changed=1
                    break
                fi
            done

            [[ -e "$APP_ROOT/$dependency_name" ]] || warn "$(basename "$binary") links @rpath/$dependency_name, which is not in any of its rpaths"
        done
    done < <(macho_files)

    [[ $changed -eq 1 ]] || break
done

while read -r binary; do
    # the package is arm64 only, so drop the x86_64 half of universal libraries
    if [[ "$(lipo -archs "$binary")" == *" "* ]]; then
        lipo -thin arm64 "$binary" -output "$binary.thin" && mv "$binary.thin" "$binary"
    fi

    if [[ "$binary" == *.dylib ]]; then
        relink -id "@rpath/$(basename "$binary")" "$binary"
    fi

    # resolve @rpath/ next to the binary itself, and drop the build machine's absolute search paths
    for rpath in $(absolute_rpaths "$binary"); do
        relink -delete_rpath "$rpath" "$binary" 2>/dev/null || true
    done
    relink -add_rpath "@loader_path" "$binary" 2>/dev/null || true
done < <(macho_files)

# --- Vulkan driver ----------------------------------------------------------------------------------------------------

# The Vulkan loader looks for driver manifests in the app bundle's Resources/vulkan/icd.d, so the bundled MoltenVK is
# found without setting VK_DRIVER_FILES. Prefer the MoltenVK from the Vulkan install the engine was built against;
# the checked-in copy lags behind.
MOLTENVK_SOURCE=""
for candidate in "$APP_ROOT/libMoltenVK.dylib" "${VULKAN_SDK:-/nonexistent}/lib/libMoltenVK.dylib" /usr/local/lib/libMoltenVK.dylib /opt/homebrew/lib/libMoltenVK.dylib "$ROOT_DIR/External/ThirdParty/Binaries/Mac/Release/libMoltenVK.dylib"; do
    if [[ -e "$candidate" ]]; then MOLTENVK_SOURCE="$candidate"; break; fi
done

if [[ -n "$MOLTENVK_SOURCE" ]]; then
    [[ -e "$APP_ROOT/libMoltenVK.dylib" ]] || cp -L "$MOLTENVK_SOURCE" "$APP_ROOT/libMoltenVK.dylib"
    mkdir -p "$APP_RESOURCES/vulkan/icd.d"
    cat > "$APP_RESOURCES/vulkan/icd.d/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../MacOS/libMoltenVK.dylib",
        "api_version": "1.2.0",
        "is_portability_driver": true
    }
}
EOF
else
    warn "libMoltenVK.dylib not found; the package will rely on a Vulkan driver installed on the user's machine"
fi

# --- Bundle metadata --------------------------------------------------------------------------------------------------

step "Writing Info.plist and icon"

cat > "$APP_DIR/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Hyperion Editor</string>
    <key>CFBundleDisplayName</key>
    <string>Hyperion Editor</string>
    <key>CFBundleIdentifier</key>
    <string>$BUNDLE_ID</string>
    <key>CFBundleExecutable</key>
    <string>Hyperion.Editor</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>LSArchitecturePriority</key>
    <array>
        <string>arm64</string>
    </array>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF
plutil -lint "$APP_DIR/Contents/Info.plist" >/dev/null

ICONSET_DIR="$OUTPUT_DIR/AppIcon.iconset"
ICON_SOURCE="$ROOT_DIR/Source/Editor/Managed/Assets/Branding/hyperion-icon.ico"
rm -rf "$ICONSET_DIR"
mkdir -p "$ICONSET_DIR"
for size in 16 32 128 256; do
    sips -s format png -z $size $size "$ICON_SOURCE" --out "$ICONSET_DIR/icon_${size}x${size}.png" >/dev/null
    if [[ $size -le 128 ]]; then
        sips -s format png -z $((size * 2)) $((size * 2)) "$ICON_SOURCE" --out "$ICONSET_DIR/icon_${size}x${size}@2x.png" >/dev/null
    fi
done
iconutil -c icns "$ICONSET_DIR" -o "$APP_RESOURCES/AppIcon.icns"
rm -rf "$ICONSET_DIR"

# --- Config -----------------------------------------------------------------------------------------------------------

step "Staging config"

mkdir -p "$APP_ROOT/Config"

# mirrors Source/Sample/CopyPlatformConfigs.cmake for PLATFORM=Mac
for config_file in "$ROOT_DIR"/Config/*; do
    [[ -f "$config_file" ]] || continue
    name="$(basename "$config_file")"

    case "$name" in
        *.IOS.json|*.Android.json|*.Windows.json|*.Mac.json|*.Linux.json|*.Web.json) continue ;;
        *.json)
            mac_variant="$ROOT_DIR/Config/${name%.json}.Mac.json"
            if [[ -f "$mac_variant" ]]; then cp "$mac_variant" "$APP_ROOT/Config/$name"; else cp "$config_file" "$APP_ROOT/Config/$name"; fi
            ;;
        *) cp "$config_file" "$APP_ROOT/Config/$name" ;;
    esac
done

# basedir points at the source tree, trace/cache servers are dev-only localhost endpoints
GLOBAL_CONFIG="$APP_ROOT/Config/GlobalConfig.json"
if [[ -f "$GLOBAL_CONFIG" ]]; then
    sed -i '' -E 's# ?--(basedir|traceserver|cacheserver)=[^ "]*##g; s#("Args"[[:space:]]*:[[:space:]]*")#\1--basedir=./ #' "$GLOBAL_CONFIG"
fi

# --- Content, shaders, scripts ----------------------------------------------------------------------------------------

copy_tracked() {
    local count=0
    while IFS= read -r -d '' relative_path; do
        [[ -f "$ROOT_DIR/$relative_path" ]] || continue
        mkdir -p "$APP_ROOT/$(dirname "$relative_path")"
        cp "$ROOT_DIR/$relative_path" "$APP_ROOT/$relative_path"
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
    mkdir -p "$APP_ROOT/Data/Scripts"
    cp -R "$ROOT_DIR/Data/Scripts/Strata" "$APP_ROOT/Data/Scripts/"
else
    warn "Data/Scripts/Strata not found; Strata scripts won't resolve engine modules"
fi

SDK_CMAKE_FILE="$BIN_DIR/Sdk/CMake/HyperionSdk.cmake"
SDK_STAGE_DIR="$APP_ROOT/Sdk"

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

    # a C++ game runs CodeGen over its own headers, against the engine types the engine's CodeGen run listed
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
        warn "Sdk/RuntimeLibraries.txt is missing so all .dylib files will be copied without whitelist!"
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
    stage_shipping_sdk "$ROOT_DIR/Binaries/Mac/ShippingSdk" "$SDK_STAGE_DIR" "$ROOT_DIR" "*.dylib"

    if [[ ! -f "$SDK_STAGE_DIR/Include/Engine/HyperionPch.hpp" || ! -f "$SDK_STAGE_DIR/Include/Generated/CodeGenOutput.inc" ]]; then
        echo "SDK headers are incomplete (HyperionPch.hpp or CodeGen output missing); run CodeGen and rebuild" >&2
        exit 1
    fi
else
    # without it the editor can't build native game projects, so don't ship a package that looks fine but isn't
    echo "SDK not staged: $SDK_CMAKE_FILE not found. Rebuild with 'Tools/Scripts/BuildHyperion.sh distribution ninja regenerate'" >&2
    exit 1
fi

# --- Sign -------------------------------------------------------------------------------------------------------------

# codesign expects only code in Contents/MacOS, so the data folders move to Resources and keep their old paths as
# symlinks (the engine resolves them next to its executable). The .NET files have to stay beside the executable for the
# host to find them, and are signed one by one, which stores their signature in extended attributes.
step "Moving data folders to Resources"
for directory in "$APP_ROOT"/*/; do
    name="$(basename "$directory")"
    mv "$APP_ROOT/$name" "$APP_RESOURCES/$name"
    ln -s "../Resources/$name" "$APP_ROOT/$name"
done

# Apple Silicon refuses to run a binary whose signature no longer matches, and install_name_tool invalidates it.
# The signature is ad-hoc: Gatekeeper still treats the download as unidentified, but sees an intact app rather than a
# damaged one.
step "Ad-hoc signing"
for path in "$APP_ROOT"/*; do
    # the main executable is signed along with the bundle
    [[ -L "$path" || "$path" == "$APP_ROOT/Hyperion.Editor" ]] && continue
    codesign --force --sign - "$path" 2>/dev/null || warn "codesign failed for $(basename "$path")"
done

codesign --force --sign - "$APP_DIR"
codesign --verify --deep --strict "$APP_DIR"

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

    for dependency_name in $(rpath_dependencies "$binary"); do
        [[ -e "$APP_ROOT/$dependency_name" ]] || warn "$(basename "$binary") links @rpath/$dependency_name, which is not in the package"
    done
done < <(macho_files)

if grep -q -a -F "$ROOT_DIR" "$APP_ROOT"/libhyperion*.dylib 2>/dev/null; then
    warn "the engine library has the source tree path ($ROOT_DIR) baked in; build with the 'distribution' argument"
fi

step "Creating $STAGE_NAME.tar.gz"
tar -czf "$OUTPUT_DIR/$STAGE_NAME.tar.gz" -C "$OUTPUT_DIR" "$STAGE_NAME"

echo
echo "Staged $STAGE_NAME ($(du -sh "$STAGE_DIR" | cut -f1)) at $STAGE_DIR"
