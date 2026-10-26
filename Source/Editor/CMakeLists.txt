cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-editor)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} library, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

include_directories(SYSTEM
    ${THIRD_PARTY_INCLUDE_DIR}
)
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Generated/Editor
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Editor
)

file(GLOB_RECURSE CURRENT_SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE CURRENT_HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Code" FILES ${CURRENT_SOURCES} ${CURRENT_HEADERS})

set(HYP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/Generated/Editor")
file(GLOB REFLECTION_GENERATED_SOURCES "${HYP_GENERATED_DIR}/*.cpp")
source_group(TREE ${HYP_GENERATED_DIR} PREFIX "Generated Code" FILES ${REFLECTION_GENERATED_SOURCES})

set(CURRENT_SOURCES "${CURRENT_SOURCES}" ${REFLECTION_GENERATED_SOURCES})
set(CURRENT_HEADERS "${CURRENT_HEADERS}" ${REFLECTION_GENERATED_HEADERS})

# Static for now
add_library(${BUILD_NAME} STATIC ${CURRENT_SOURCES} ${CURRENT_HEADERS})
# target_compile_definitions(${BUILD_NAME} PRIVATE HYP_BUILD_EDITOR=1 HYP_EDITOR=1)
target_compile_definitions(${BUILD_NAME} PRIVATE HYP_BUILD_ENGINE=1 HYP_EDITOR=1)

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
  target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

hyp_link_core_library(${BUILD_NAME})
target_link_libraries(${BUILD_NAME} PRIVATE hyperion-scene hyperion-rendering hyperion-asset hyperion-ui hyperion-input hyperion-baking hyperion-plugins)

if(NOT DEFINED CSHARP_PROJECTS_DIR)
  message(FATAL_ERROR "CSHARP_PROJECTS_DIR is not defined. Please define it in the CMakeLists.txt located in src/")
endif()

find_program(DOTNET_EXECUTABLE dotnet)

if(DOTNET_EXECUTABLE)
  include(${CMAKE_SOURCE_DIR}/Engine/DotNET/Generators/Generators.cmake)

  # Generate Hyperion.Editor.csproj
  set(EDITOR_PROJECT_DIR "${CSHARP_PROJECTS_DIR}/Hyperion.Editor")
  file(MAKE_DIRECTORY ${EDITOR_PROJECT_DIR})

  # Generate launchSettings.json to enable native debugging
  set(EDITOR_PROPERTIES_DIR "${EDITOR_PROJECT_DIR}/Properties")
  file(MAKE_DIRECTORY ${EDITOR_PROPERTIES_DIR})
  file(WRITE "${EDITOR_PROPERTIES_DIR}/launchSettings.json"
[=[{
  "profiles": {
    "Hyperion.Editor": {
      "commandName": "Project",
      "nativeDebugging": true
    }
  }
}
]=])

  set(EDITOR_CSPROJ_PATH "${EDITOR_PROJECT_DIR}/Hyperion.Editor.csproj")

  file(WRITE ${EDITOR_CSPROJ_PATH} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "  <PropertyGroup>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <OutputType>Exe</OutputType>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <TargetFramework>net10.0</TargetFramework>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <ImplicitUsings>enable</ImplicitUsings>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <LangVersion>13.0</LangVersion>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <BuiltInComInteropSupport>true</BuiltInComInteropSupport>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <ApplicationManifest>${CMAKE_CURRENT_SOURCE_DIR}/Managed/app.manifest</ApplicationManifest>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <ApplicationIcon>${CMAKE_CURRENT_SOURCE_DIR}/Managed/Assets/Branding/hyperion-icon.ico</ApplicationIcon>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaUseCompiledBindingsByDefault>true</AvaloniaUseCompiledBindingsByDefault>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <OutputPath>${CMAKE_RUNTIME_OUTPUT_DIRECTORY}</OutputPath>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <RootNamespace>Hyperion.Editor</RootNamespace>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AssemblyName>Hyperion.Editor</AssemblyName>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Product>Hyperion Engine</Product>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}</Version>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <IncludeSourceRevisionInInformationalVersion>false</IncludeSourceRevisionInInformationalVersion>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AssemblyTitle>Hyperion Editor</AssemblyTitle>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Platforms>x64;ARM64</Platforms>\n")
  # PlatformTarget must match the native hyperion.dll architecture,
  # otherwise the managed apphost launches as x64 (emulated on ARM64)
  # and fails to P/Invoke into the native engine library.
  if(HYP_ARCH STREQUAL "ARM64")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PlatformTarget>ARM64</PlatformTarget>")
  else()
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PlatformTarget Condition=\"$([MSBuild]::IsOSPlatform('Windows'))\">x64</PlatformTarget>")
  endif()
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <EnableUnmanagedDebugging>true</EnableUnmanagedDebugging>\n")
  if(CSHARP_CONSTANTS)
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
  endif()
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </PropertyGroup>\n")

  file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")

  if(USE_MSBUILD_FOR_CSHARP_BUILD)
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj\" />\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj\" />\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj\" />\n")
  else()
    file(APPEND ${EDITOR_CSPROJ_PATH} "     <Reference Include=\"Hyperion.NET.Interop\" >\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "         <HintPath>${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.dll</HintPath>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "     </Reference>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "     <Reference Include=\"Hyperion.NET.Shared\" >\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "         <HintPath>${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.dll</HintPath>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "     </Reference>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "     <Reference Include=\"Hyperion.NET.Runtime\" >\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "         <HintPath>${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.dll</HintPath>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "     </Reference>\n")
  endif()

  # need exact versions here!
  # or else it may fail to load if we build a plugin against a different version than the editor
  set(HYP_AVALONIA_VERSION "12.1.4")
  set(HYP_DOCK_VERSION "12.1.0.12")

  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia\" Version=\"${HYP_AVALONIA_VERSION}\" />\n")
  hyp_append_type_registry_generator(${EDITOR_CSPROJ_PATH})
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Desktop\" Version=\"${HYP_AVALONIA_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Themes.Simple\" Version=\"${HYP_AVALONIA_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Fonts.Inter\" Version=\"${HYP_AVALONIA_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"DynamicData\" Version=\"9.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Dock.Avalonia\" Version=\"${HYP_DOCK_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Dock.Avalonia.Themes.Simple\" Version=\"${HYP_DOCK_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Dock.Model.Avalonia\" Version=\"${HYP_DOCK_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Dock.Serializer.Newtonsoft\" Version=\"${HYP_DOCK_VERSION}\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </ItemGroup>\n")

  file(GLOB_RECURSE EDITOR_CS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Managed/*.cs")
  file(GLOB_RECURSE EDITOR_AXAML_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Managed/*.axaml")
  file(GLOB_RECURSE EDITOR_ICON_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Managed/Assets/Icons/*.svg")
  file(GLOB EDITOR_BRANDING_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Managed/Assets/Branding/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/Managed/Assets/Branding/*.ico")
  list(APPEND EDITOR_ICON_ASSETS ${EDITOR_BRANDING_ASSETS})

  # Filter out obj and bin directories to avoid duplicate attributes from previous builds
  list(FILTER EDITOR_CS_SOURCES EXCLUDE REGEX ".*/(obj|bin)/.*")
  list(FILTER EDITOR_AXAML_SOURCES EXCLUDE REGEX ".*/(obj|bin)/.*")

  file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")
  foreach(SOURCE_FILE ${EDITOR_CS_SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Managed" "${SOURCE_FILE}")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <Compile Include=\"${SOURCE_FILE}\" Link=\"${REL_PATH}\" />\n")
  endforeach()
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </ItemGroup>\n")

  # the editor UI of built-in plugins: <Plugin>/Managed/<Module> of each editor module (see HYP_PLUGIN_MODULES in Source/CMakeLists.txt)
  file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")
  foreach(plugin_module ${HYP_PLUGIN_MODULES})
    if(NOT plugin_module MATCHES "Editor$")
      continue()
    endif()

    get_filename_component(plugin_managed_dir "${HYP_PLUGIN_SOURCE_ROOT_${plugin_module}}/../Managed/${plugin_module}" ABSOLUTE)

    file(GLOB_RECURSE plugin_cs_sources "${plugin_managed_dir}/*.cs")
    file(GLOB_RECURSE plugin_axaml_sources "${plugin_managed_dir}/*.axaml")
    list(FILTER plugin_cs_sources EXCLUDE REGEX ".*/(obj|bin)/.*")
    list(FILTER plugin_axaml_sources EXCLUDE REGEX ".*/(obj|bin)/.*")

    # class bindings compile into Hyperion.NET.Runtime (see Engine/DotNET/Runtime/CMakeLists.txt)
    list(FILTER plugin_cs_sources EXCLUDE REGEX "^${plugin_managed_dir}/Bindings/.*")

    foreach(plugin_cs_source ${plugin_cs_sources})
      file(RELATIVE_PATH REL_PATH "${plugin_managed_dir}" "${plugin_cs_source}")
      file(APPEND ${EDITOR_CSPROJ_PATH} "    <Compile Include=\"${plugin_cs_source}\" Link=\"Plugins/${plugin_module}/${REL_PATH}\" />\n")
    endforeach()

    foreach(plugin_axaml_source ${plugin_axaml_sources})
      file(RELATIVE_PATH REL_PATH "${plugin_managed_dir}" "${plugin_axaml_source}")
      file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaXaml Include=\"${plugin_axaml_source}\" Link=\"Plugins/${plugin_module}/${REL_PATH}\" />\n")
    endforeach()
  endforeach()
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </ItemGroup>\n")

  file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")
  foreach(AXAML_FILE ${EDITOR_AXAML_SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Managed" "${AXAML_FILE}")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaXaml Include=\"${AXAML_FILE}\" Link=\"${REL_PATH}\" />\n")
  endforeach()
  foreach(ICON_FILE ${EDITOR_ICON_ASSETS})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Managed" "${ICON_FILE}")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaResource Include=\"${ICON_FILE}\" Link=\"${REL_PATH}\" />\n")
  endforeach()
  # written by BuildEditorIconsCommandlet, so glob at build time rather than configure time
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaResource Include=\"${CMAKE_CURRENT_SOURCE_DIR}/Managed/Assets/Icons/Generated/*.png\" Link=\"Assets/Icons/Generated/%(Filename)%(Extension)\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </ItemGroup>\n")

  file(APPEND ${EDITOR_CSPROJ_PATH} "</Project>\n")

  if(NOT DEFINED HYP_BUILD_TYPE)
    set(HYP_BUILD_TYPE "Release")
  endif()

  # Clear MSBuildSdksPath environment variable if set to prevent SDK resolution issues
  set(MSBuildSdksPath_BACKUP $ENV{MSBuildSdksPath})
  unset(ENV{MSBuildSdksPath})

  execute_process(
        COMMAND ${DOTNET_EXECUTABLE} restore
        WORKING_DIRECTORY ${EDITOR_PROJECT_DIR}
    )

  # Restore environment variable
  if(MSBuildSdksPath_BACKUP)
    set(ENV{MSBuildSdksPath} ${MSBuildSdksPath_BACKUP})
  endif()

  if(USE_MSBUILD_FOR_CSHARP_BUILD)
    enable_language(CSharp)
    include_external_msproject(
            Hyperion.Editor
            "${EDITOR_CSPROJ_PATH}"
            TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
        )

    add_dependencies(Hyperion.Editor hyperion Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop)
  else()
    message(STATUS "Using dotnet CLI to build Hyperion.Editor")

    # Create a wrapper script to clear MSBuildSdksPath before running dotnet build
    # Use CMAKE_HOST_WIN32 to check the build machine (not target platform)
    # This ensures we use .bat on Windows even when cross-compiling for Android
    if(CMAKE_HOST_WIN32)
      set(DOTNET_BUILD_WRAPPER "${EDITOR_PROJECT_DIR}/dotnet-build.bat")
      file(WRITE ${DOTNET_BUILD_WRAPPER} "@echo off\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "set MSBuildSdksPath=\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXECUTABLE}\" build \"${EDITOR_CSPROJ_PATH}\" --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
      add_custom_target(Hyperion.Editor ALL
                  COMMAND ${DOTNET_BUILD_WRAPPER}
                  COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/../Config/DeviceTiers.hmf"
                    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config/DeviceTiers.hmf"
                  COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/../Config/Shaders.hmf"
                    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config/Shaders.hmf"
                  DEPENDS hyperion Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop
                  WORKING_DIRECTORY ${EDITOR_PROJECT_DIR}
              )
    else()
      set(DOTNET_BUILD_WRAPPER "${EDITOR_PROJECT_DIR}/dotnet-build.sh")
      file(WRITE ${DOTNET_BUILD_WRAPPER} "#!/bin/bash\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "unset MSBuildSdksPath\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXECUTABLE}\" build \"${EDITOR_CSPROJ_PATH}\" --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
      execute_process(COMMAND chmod +x ${DOTNET_BUILD_WRAPPER})
      add_custom_target(Hyperion.Editor ALL
                  COMMAND ${DOTNET_BUILD_WRAPPER}
                  COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/../Config/DeviceTiers.hmf"
                    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config/DeviceTiers.hmf"
                  COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/../Config/Shaders.hmf"
                    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config/Shaders.hmf"
                  DEPENDS hyperion Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop
                  WORKING_DIRECTORY ${EDITOR_PROJECT_DIR}
              )
    endif()

    if(TARGET Hyperion.NET.Generators)
      add_dependencies(Hyperion.Editor Hyperion.NET.Generators)
    endif()

    # for Xcode, we want to allow running the editor from within Xcode
    if(CMAKE_GENERATOR MATCHES "Xcode")
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_GENERATE_SCHEME ON)
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_SCHEME_ENABLE_GPU_API_VALIDATION OFF)
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_SCHEME_EXECUTABLE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Hyperion.Editor")
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_SCHEME_LAUNCH_CONFIGURATION_NAME "${HYP_BUILD_TYPE}")
      set_target_properties(Hyperion.Editor PROPERTIES XCODE_SCHEME_HIDE_BUILDABLE_NAME OFF)
    endif()
  endif()
endif()
