cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-dotnet-runtime)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(COMMON_LIBRARIES
    hyperion-core
)

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "${BUILD_NAME} Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(SYSTEM
    ${Vulkan_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIR}
)
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Engine/DotNET
)

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
  target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})

if(HYP_GENERATE_CSHARP_LIBRARIES)
  file(GLOB_RECURSE CSHARP_RUNTIME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/**.cs" "${HYP_GENERATED_DIR}/CSharp/**.cs")

  if(CSHARP_RUNTIME_SOURCES)
    file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime)

    set(CSPROJ_PATH "${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj")

    file(WRITE ${CSPROJ_PATH} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
    file(APPEND ${CSPROJ_PATH} "  <PropertyGroup>\n")
    file(APPEND ${CSPROJ_PATH} "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
    file(APPEND ${CSPROJ_PATH} "    <OutputType>Library</OutputType>\n")
    file(APPEND ${CSPROJ_PATH} "    <TargetFramework>net9.0</TargetFramework>\n")
    file(APPEND ${CSPROJ_PATH} "    <LangVersion>13.0</LangVersion>\n")
    file(APPEND ${CSPROJ_PATH} "    <ImplicitUsings>enable</ImplicitUsings>\n")
    file(APPEND ${CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
    file(APPEND ${CSPROJ_PATH} "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
    file(APPEND ${CSPROJ_PATH} "    <IsAotCompatible>true</IsAotCompatible>\n")
    file(APPEND ${CSPROJ_PATH} "    <AssemblyName>Hyperion.NET.Runtime</AssemblyName>\n")
    file(APPEND ${CSPROJ_PATH} "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
    file(APPEND ${CSPROJ_PATH} "    <Platforms>x64;ARM64</Platforms>\n")
    if(CSHARP_CONSTANTS)
      file(APPEND ${CSPROJ_PATH} "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
    endif()
    file(APPEND ${CSPROJ_PATH} "  </PropertyGroup>\n")

    file(APPEND ${CSPROJ_PATH} "  <ItemGroup>\n")
    file(APPEND ${CSPROJ_PATH} "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
    foreach(CSHARP_RUNTIME_SOURCE ${CSHARP_RUNTIME_SOURCES})
      file(APPEND ${CSPROJ_PATH} "    <Compile Include=\"${CSHARP_RUNTIME_SOURCE}\" />\n")
    endforeach()

    if(USE_MSBUILD_FOR_CSHARP_BUILD)
      file(APPEND ${CSPROJ_PATH} "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj\" />\n")
    else()
      file(APPEND ${CSPROJ_PATH} "    <Reference Include=\"Hyperion.NET.Shared\" >\n")
      file(APPEND ${CSPROJ_PATH} "        <HintPath>${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.dll</HintPath>\n")
      file(APPEND ${CSPROJ_PATH} "    </Reference>\n")
    endif()

    file(APPEND ${CSPROJ_PATH} "  </ItemGroup>\n")
    # copy dll
    file(APPEND ${CSPROJ_PATH} "  <Target Name=\"CopyDLL\" AfterTargets=\"AfterBuild\">\n")
    file(APPEND ${CSPROJ_PATH} "    <Copy SourceFiles=\"$(TargetPath)\" DestinationFolder=\"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/\" />\n")
    file(APPEND ${CSPROJ_PATH} "  </Target>\n")
    file(APPEND ${CSPROJ_PATH} "</Project>\n")

    # find dotnet executable
    find_program(DOTNET_EXE dotnet)
    if(NOT DOTNET_EXE)
      message(FATAL_ERROR "dotnet executable not found. Please install the .NET SDK to build C# projects.")
    endif()

    # Clear MSBuildSdksPath environment variable if set to prevent SDK resolution issues
    set(MSBuildSdksPath_BACKUP $ENV{MSBuildSdksPath})
    unset(ENV{MSBuildSdksPath})

    execute_process(
            COMMAND ${DOTNET_EXE} restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime
        )

    # Restore environment variable
    if(MSBuildSdksPath_BACKUP)
      set(ENV{MSBuildSdksPath} ${MSBuildSdksPath_BACKUP})
    endif()

    if(NOT USE_MSBUILD_FOR_CSHARP_BUILD)
      message(STATUS "Using dotnet CLI to build Hyperion.NET.Runtime")

      # Create a wrapper script to clear MSBuildSdksPath before running dotnet build
      # Use CMAKE_HOST_WIN32 to check the build machine (not target platform)
      # This ensures we use .bat on Windows even when cross-compiling for Android
      if(CMAKE_HOST_WIN32)
        set(DOTNET_BUILD_WRAPPER "${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/dotnet-build.bat")
        file(WRITE ${DOTNET_BUILD_WRAPPER} "@echo off\n")
        file(APPEND ${DOTNET_BUILD_WRAPPER} "set MSBuildSdksPath=\n")
        file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXE}\" build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
        add_custom_target(Hyperion.NET.Runtime ALL
                  COMMAND ${DOTNET_BUILD_WRAPPER}
                  DEPENDS hyperion-dotnet-shared
                  WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime
              )
      else()
        set(DOTNET_BUILD_WRAPPER "${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/dotnet-build.sh")
        file(WRITE ${DOTNET_BUILD_WRAPPER} "#!/bin/bash\n")
        file(APPEND ${DOTNET_BUILD_WRAPPER} "unset MSBuildSdksPath\n")
        file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXE}\" build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
        execute_process(COMMAND chmod +x ${DOTNET_BUILD_WRAPPER})
        add_custom_target(Hyperion.NET.Runtime ALL
                  COMMAND ${DOTNET_BUILD_WRAPPER}
                  DEPENDS hyperion-dotnet-shared
                  WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime
              )
      endif()
    else()
      enable_language(CSharp)
      include_external_msproject(
                Hyperion.NET.Runtime
                ${CSPROJ_PATH}
                TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
            )

      add_dependencies(Hyperion.NET.Runtime hyperion-dotnet-shared)
    endif()

    add_dependencies(${BUILD_NAME} Hyperion.NET.Runtime hyperion-core hyperion-rendering)
  endif()
endif()
