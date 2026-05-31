cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-editor)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} library, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

include_directories(SYSTEM
    ${SDL2_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIR}
)
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Generated/editor
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Editor
)

file(GLOB_RECURSE CURRENT_SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE CURRENT_HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Code" FILES ${CURRENT_SOURCES} ${CURRENT_HEADERS})

set(HYP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/Generated/editor")
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

target_link_libraries(${BUILD_NAME} PRIVATE hyperion-core hyperion-scene hyperion-rendering hyperion-asset hyperion-ui hyperion-input hyperion-baking)

# CSHARP_PROJECTS_DIR must be defined (from CMakeLists.txt in src/) as it is used to locate the Hyperion.NET packages
# check to ensure it is defined
if(NOT DEFINED CSHARP_PROJECTS_DIR)
  message(FATAL_ERROR "CSHARP_PROJECTS_DIR is not defined. Please define it in the CMakeLists.txt located in src/")
endif()

find_program(DOTNET_EXECUTABLE dotnet)

if(DOTNET_EXECUTABLE)
  # Generate Hyperion.Editor.csproj
  set(EDITOR_PROJECT_DIR "${CMAKE_SOURCE_DIR}/../Build/CSharpProjects/Hyperion.Editor")
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
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <TargetFramework>net9.0</TargetFramework>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <ImplicitUsings>enable</ImplicitUsings>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <LangVersion>13.0</LangVersion>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <BuiltInComInteropSupport>true</BuiltInComInteropSupport>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <ApplicationManifest>${CMAKE_CURRENT_SOURCE_DIR}/managed/app.manifest</ApplicationManifest>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaUseCompiledBindingsByDefault>true</AvaloniaUseCompiledBindingsByDefault>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <OutputPath>${CMAKE_RUNTIME_OUTPUT_DIRECTORY}</OutputPath>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <RootNamespace>Hyperion.Editor</RootNamespace>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AssemblyName>Hyperion.Editor</AssemblyName>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Product>Hyperion Editor</Product>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <AssemblyTitle>Hyperion Editor</AssemblyTitle>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <Platforms>x64;ARM64</Platforms>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PlatformTarget Condition=\"$([MSBuild]::IsOSPlatform('Windows'))\">x64</PlatformTarget>")
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

  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia\" Version=\"11.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Desktop\" Version=\"11.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Themes.Simple\" Version=\"11.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Fonts.Inter\" Version=\"11.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Diagnostics\" Version=\"11.*\">\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "      <IncludeAssets Condition=\"'\$(Configuration)' != 'Debug'\">None</IncludeAssets>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "      <PrivateAssets Condition=\"'\$(Configuration)' != 'Debug'\">All</PrivateAssets>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    </PackageReference>\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Lucide.Avalonia\" Version=\"0.1.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"DynamicData\" Version=\"9.*\" />\n")
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </ItemGroup>\n")

  file(GLOB_RECURSE EDITOR_CS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/managed/*.cs")
  file(GLOB_RECURSE EDITOR_AXAML_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/managed/*.axaml")

  # Filter out obj and bin directories to avoid duplicate attributes from previous builds
  list(FILTER EDITOR_CS_SOURCES EXCLUDE REGEX ".*/(obj|bin)/.*")
  list(FILTER EDITOR_AXAML_SOURCES EXCLUDE REGEX ".*/(obj|bin)/.*")

  file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")
  foreach(SOURCE_FILE ${EDITOR_CS_SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/managed" "${SOURCE_FILE}")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <Compile Include=\"${SOURCE_FILE}\" Link=\"${REL_PATH}\" />\n")
  endforeach()
  file(APPEND ${EDITOR_CSPROJ_PATH} "  </ItemGroup>\n")

  file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")
  foreach(AXAML_FILE ${EDITOR_AXAML_SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/managed" "${AXAML_FILE}")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaXaml Include=\"${AXAML_FILE}\" Link=\"${REL_PATH}\" />\n")
  endforeach()
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
                  DEPENDS hyperion Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop
                  WORKING_DIRECTORY ${EDITOR_PROJECT_DIR}
              )
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
