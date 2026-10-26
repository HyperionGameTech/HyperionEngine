#!/bin/bash
# Stages the static shipping engine that Package Game links games against (BuildHyperion.sh sdk) as Sdk/Shipping of an editor package.
# It shares the headers and CodeGen of the Sdk folder it goes in, which has to be staged first. Sourced by PackageEditorLinux.sh and
# PackageEditorMacOS.sh:
#   stage_shipping_sdk <shipping bin dir> <Sdk stage dir> <source tree> <native library glob, eg. *.so*>

stage_shipping_sdk() {
    local shipping_bin_dir="$1"
    local sdk_stage_dir="$2"
    local root_dir="$3"
    local library_glob="$4"

    local shipping_cmake_file="$shipping_bin_dir/Sdk/CMake/HyperionSdk.cmake"
    local static_libraries_file="$shipping_bin_dir/Sdk/StaticLibraries.txt"
    local shipping_stage_dir="$sdk_stage_dir/Shipping"

    if [[ ! -f "$shipping_cmake_file" || ! -f "$static_libraries_file" ]]; then
        echo "Shipping SDK not staged: $shipping_cmake_file or $static_libraries_file not found. Build it with 'Tools/Scripts/BuildHyperion.sh ninja sdk regenerate'" >&2
        exit 1
    fi

    if ! grep -q -F "set(HYPERION_SDK_SHIPPING ON)" "$shipping_cmake_file"; then
        echo "$shipping_cmake_file isn't a shipping SDK" >&2
        exit 1
    fi

    local third_party_dirs third_party_dir
    third_party_dirs="$(sed -nE 's/^set\(HYPERION_SDK_THIRD_PARTY_DIRS "([^"]*)"\)/\1/p' "$shipping_cmake_file")"
    for third_party_dir in ${third_party_dirs//;/ }; do
        if [[ ! -d "$sdk_stage_dir/ThirdParty/$third_party_dir" ]]; then
            echo "The shipping SDK includes ThirdParty/$third_party_dir, which the editor's SDK doesn't stage" >&2
            exit 1
        fi
    done

    mkdir -p "$shipping_stage_dir/CMake" "$shipping_stage_dir/Lib" "$shipping_stage_dir/Bin"

    local file
    for file in HyperionSdk.cmake HyperionSdkPlatform.cmake HyperionCopyRuntimeDependencies.cmake; do
        cp "$shipping_bin_dir/Sdk/CMake/$file" "$shipping_stage_dir/CMake/$file"
    done

    # archives go in Lib and are listed by file name; system libraries and link flags stay as they are
    local line library_name library_count=0
    : > "$shipping_stage_dir/StaticLibraries.txt"

    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ -n "$line" ]] || continue

        if [[ "$line" != /* ]]; then
            echo "$line" >> "$shipping_stage_dir/StaticLibraries.txt"
            continue
        fi

        if [[ ! -f "$line" ]]; then
            echo "The shipping SDK links $line, which doesn't exist" >&2
            exit 1
        fi

        library_name="$(basename "$line")"

        if [[ -f "$shipping_stage_dir/Lib/$library_name" ]] && ! cmp -s "$line" "$shipping_stage_dir/Lib/$library_name"; then
            echo "Two shipping SDK libraries are named $library_name" >&2
            exit 1
        fi

        cp "$line" "$shipping_stage_dir/Lib/$library_name"
        echo "$library_name" >> "$shipping_stage_dir/StaticLibraries.txt"
        library_count=$((library_count + 1))
    done < "$static_libraries_file"

    echo "    $library_count libraries"

    # each built-in plugin module's registration object, which a game links for the plugins it uses; they share a file name, so they're renamed
    local plugin_module_objects_file="$shipping_bin_dir/Sdk/PluginModuleObjects.txt"
    local plugin_module plugin_module_object plugin_module_object_name
    : > "$shipping_stage_dir/PluginModuleObjects.txt"

    if [[ -f "$plugin_module_objects_file" ]]; then
        while IFS= read -r line || [[ -n "$line" ]]; do
            [[ "$line" == *=* ]] || continue

            plugin_module="${line%%=*}"
            plugin_module_object="${line#*=}"

            if [[ ! -f "$plugin_module_object" ]]; then
                echo "The shipping SDK's plugin module $plugin_module has no object at $plugin_module_object" >&2
                exit 1
            fi

            plugin_module_object_name="$plugin_module.PluginModuleRegister.${plugin_module_object##*.}"

            cp "$plugin_module_object" "$shipping_stage_dir/Lib/$plugin_module_object_name"
            echo "$plugin_module=$plugin_module_object_name" >> "$shipping_stage_dir/PluginModuleObjects.txt"
        done < "$plugin_module_objects_file"
    fi

    # the assemblies a C# game is compiled against, and what a game may load from the shipping build
    for file in Hyperion.NET.Shared.dll Hyperion.NET.Runtime.dll Hyperion.NET.Interop.dll Hyperion.NET.Generators.dll; do
        if [[ ! -f "$shipping_bin_dir/$file" ]]; then
            echo "Missing $file in $shipping_bin_dir" >&2
            exit 1
        fi

        cp "$shipping_bin_dir/$file" "$shipping_stage_dir/Bin/$file"
    done

    for file in "$shipping_bin_dir"/$library_glob; do
        cp -P "$file" "$shipping_stage_dir/Bin/"
    done

    if grep -q -F "$root_dir" "$shipping_stage_dir/CMake/HyperionSdk.cmake"; then
        echo "WARNING: Sdk/Shipping/CMake/HyperionSdk.cmake has the source tree path ($root_dir) in it" >&2
    fi

    if ! grep -q -F "HYP_SHIPPING=1" "$shipping_stage_dir/CMake/HyperionSdk.cmake"; then
        echo "WARNING: the shipping SDK doesn't define HYP_SHIPPING" >&2
    fi
}
