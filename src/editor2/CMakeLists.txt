cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-editor2)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} library, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(COMMON_LIBRARIES
    hyperion-core
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi /MP")
    add_definitions("/DNOMINMAX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fpermissive")
endif()

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/native/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/native/*.cc" "${CMAKE_CURRENT_SOURCE_DIR}/native/*.c")
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/native/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/native/*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/native PREFIX "Hyperion Editor Native Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)
add_definitions(-DHYP_EDITOR2=1)

include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/editor2
)

add_library(${BUILD_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})

find_program(DOTNET_EXECUTABLE dotnet)

if(DOTNET_EXECUTABLE)
    # Generate Hyperion.Editor.csproj
    set(EDITOR_PROJECT_DIR "${CMAKE_SOURCE_DIR}/build/CSharpProjects/Hyperion.Editor")
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
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <OutputType>WinExe</OutputType>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <TargetFramework>net8.0</TargetFramework>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <BuiltInComInteropSupport>true</BuiltInComInteropSupport>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <ApplicationManifest>${CMAKE_CURRENT_SOURCE_DIR}/managed/app.manifest</ApplicationManifest>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AvaloniaUseCompiledBindingsByDefault>true</AvaloniaUseCompiledBindingsByDefault>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <OutputPath>${CMAKE_RUNTIME_OUTPUT_DIRECTORY}</OutputPath>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <RootNamespace>Hyperion.Editor</RootNamespace>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AssemblyName>Hyperion.Editor</AssemblyName>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <Product>Hyperion Editor</Product>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <AssemblyTitle>Hyperion Editor</AssemblyTitle>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <Platforms>AnyCPU;x64</Platforms>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PlatformTarget>x64</PlatformTarget>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <EnableUnmanagedDebugging>true</EnableUnmanagedDebugging>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "  </PropertyGroup>\n")

    file(APPEND ${EDITOR_CSPROJ_PATH} "  <ItemGroup>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia\" Version=\"11.*\" />\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Desktop\" Version=\"11.*\" />\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Themes.Fluent\" Version=\"11.*\" />\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Fonts.Inter\" Version=\"11.*\" />\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Avalonia.Diagnostics\" Version=\"11.*\">\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "      <IncludeAssets Condition=\"'\$(Configuration)' != 'Debug'\">None</IncludeAssets>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "      <PrivateAssets Condition=\"'\$(Configuration)' != 'Debug'\">All</PrivateAssets>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    </PackageReference>\n")
    file(APPEND ${EDITOR_CSPROJ_PATH} "    <PackageReference Include=\"Ribbon.Avalonia\" Version=\"*\" />\n")
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

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        enable_language(CSharp)
        include_external_msproject(
            Hyperion.Editor
            "${EDITOR_CSPROJ_PATH}"
            TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
        )
    else()
        add_custom_target(Hyperion.Editor ALL
            COMMAND ${DOTNET_EXECUTABLE} build "${EDITOR_CSPROJ_PATH}" --configuration ${HYP_BUILD_TYPE}
            COMMENT "Building Hyperion.Editor (Avalonia)"
        )
    endif()
endif()
