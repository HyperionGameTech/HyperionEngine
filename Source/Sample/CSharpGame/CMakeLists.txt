cmake_minimum_required(VERSION 3.5)

# A game written in C# against the engine runtime

find_program(DOTNET_EXECUTABLE dotnet)

if(NOT DOTNET_EXECUTABLE)
  message(WARNING "dotnet not found - skipping the C# sample game")
  return()
endif()

set(SAMPLE_NAME Hyperion.Sample.CSharpGame)
set(SAMPLE_PROJECT_DIR "${CSHARP_PROJECTS_DIR}/${SAMPLE_NAME}")
set(SAMPLE_CSPROJ_PATH "${SAMPLE_PROJECT_DIR}/${SAMPLE_NAME}.csproj")

file(MAKE_DIRECTORY ${SAMPLE_PROJECT_DIR})

file(GLOB_RECURSE SAMPLE_CS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cs")
list(FILTER SAMPLE_CS_SOURCES EXCLUDE REGEX ".*/(obj|bin)/.*")

file(WRITE ${SAMPLE_CSPROJ_PATH} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "  <PropertyGroup>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <OutputType>Exe</OutputType>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <TargetFramework>net10.0</TargetFramework>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <LangVersion>13.0</LangVersion>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <ImplicitUsings>enable</ImplicitUsings>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <AssemblyName>CSharpGame</AssemblyName>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <RootNamespace>Hyperion.Samples</RootNamespace>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <HyperionBinDir>${CMAKE_RUNTIME_OUTPUT_DIRECTORY}</HyperionBinDir>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "    <HyperionOutputToBinDir>true</HyperionOutputToBinDir>\n")
if(CSHARP_CONSTANTS)
  file(APPEND ${SAMPLE_CSPROJ_PATH} "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
endif()
file(APPEND ${SAMPLE_CSPROJ_PATH} "  </PropertyGroup>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "  <Import Project=\"${CMAKE_SOURCE_DIR}/Engine/DotNET/Sdk/Hyperion.Game.props\" />\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "  <ItemGroup>\n")
foreach(SAMPLE_CS_SOURCE ${SAMPLE_CS_SOURCES})
  file(APPEND ${SAMPLE_CSPROJ_PATH} "    <Compile Include=\"${SAMPLE_CS_SOURCE}\" />\n")
endforeach()
file(APPEND ${SAMPLE_CSPROJ_PATH} "  </ItemGroup>\n")
file(APPEND ${SAMPLE_CSPROJ_PATH} "</Project>\n")

if(NOT DEFINED HYP_BUILD_TYPE)
  set(HYP_BUILD_TYPE "Release")
endif()

set(MSBuildSdksPath_BACKUP $ENV{MSBuildSdksPath})
unset(ENV{MSBuildSdksPath})

execute_process(
    COMMAND ${DOTNET_EXECUTABLE} restore
    WORKING_DIRECTORY ${SAMPLE_PROJECT_DIR}
)

if(MSBuildSdksPath_BACKUP)
  set(ENV{MSBuildSdksPath} ${MSBuildSdksPath_BACKUP})
endif()

if(USE_MSBUILD_FOR_CSHARP_BUILD)
  enable_language(CSharp)
  include_external_msproject(
      ${SAMPLE_NAME}
      ${SAMPLE_CSPROJ_PATH}
      TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
  )

  add_dependencies(${SAMPLE_NAME} hyperion Hyperion.NET.Shared Hyperion.NET.Interop Hyperion.NET.Runtime)
else()
  if(CMAKE_HOST_WIN32)
    set(DOTNET_BUILD_WRAPPER "${SAMPLE_PROJECT_DIR}/dotnet-build.bat")
    file(WRITE ${DOTNET_BUILD_WRAPPER} "@echo off\n")
    file(APPEND ${DOTNET_BUILD_WRAPPER} "set MSBuildSdksPath=\n")
    file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXECUTABLE}\" build \"${SAMPLE_CSPROJ_PATH}\" --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
  else()
    set(DOTNET_BUILD_WRAPPER "${SAMPLE_PROJECT_DIR}/dotnet-build.sh")
    file(WRITE ${DOTNET_BUILD_WRAPPER} "#!/bin/bash\n")
    file(APPEND ${DOTNET_BUILD_WRAPPER} "unset MSBuildSdksPath\n")
    file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXECUTABLE}\" build \"${SAMPLE_CSPROJ_PATH}\" --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
    execute_process(COMMAND chmod +x ${DOTNET_BUILD_WRAPPER})
  endif()

  add_custom_target(${SAMPLE_NAME} ALL
      COMMAND ${DOTNET_BUILD_WRAPPER}
      COMMAND ${CMAKE_COMMAND} -D CONFIG_SRC_DIR="${CMAKE_SOURCE_DIR}/../Config"
        -D CONFIG_DST_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config"
        -D PLATFORM="${HYP_PLATFORM_NAME}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/../CopyPlatformConfigs.cmake"
      DEPENDS hyperion Hyperion.NET.Shared Hyperion.NET.Interop Hyperion.NET.Runtime
      WORKING_DIRECTORY ${SAMPLE_PROJECT_DIR}
  )
endif()
