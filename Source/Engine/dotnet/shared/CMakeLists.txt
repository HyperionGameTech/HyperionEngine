cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-dotnet-shared)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(COMMON_LIBRARIES
    hyperion-core
)

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "${BUILD_NAME} Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/dotnet
)

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
    target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})

if(HYP_GENERATE_CSHARP_LIBRARIES)
    file(GLOB_RECURSE CSHARP_SHARED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/**.cs")

    if(CSHARP_SHARED_SOURCES)
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared)

        set(CSPROJ_PATH "${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj")

        file(WRITE ${CSPROJ_PATH} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSPROJ_PATH} "  <PropertyGroup>\n")
        file(APPEND ${CSPROJ_PATH} "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSPROJ_PATH} "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSPROJ_PATH} "    <TargetFramework>net9.0</TargetFramework>\n")
        file(APPEND ${CSPROJ_PATH} "    <LangVersion>13.0</LangVersion>\n")
        file(APPEND ${CSPROJ_PATH} "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSPROJ_PATH} "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSPROJ_PATH} "    <AssemblyName>Hyperion.NET.Shared</AssemblyName>\n")
        file(APPEND ${CSPROJ_PATH} "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        if(CSHARP_CONSTANTS)
            file(APPEND ${CSPROJ_PATH} "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
        endif()
        file(APPEND ${CSPROJ_PATH} "  </PropertyGroup>\n")

        file(APPEND ${CSPROJ_PATH} "  <ItemGroup>\n")
        file(APPEND ${CSPROJ_PATH} "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_SHARED_SOURCE ${CSHARP_SHARED_SOURCES})
            file(APPEND ${CSPROJ_PATH} "    <Compile Include=\"${CSHARP_SHARED_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSPROJ_PATH} "  </ItemGroup>\n")
        file(APPEND ${CSPROJ_PATH} "</Project>\n")

        # find dotnet executable
        find_program(DOTNET_EXE dotnet)
        if(NOT DOTNET_EXE)
            message(FATAL_ERROR "dotnet executable not found. Please install the .NET SDK to build C# projects.")
        endif()

        execute_process(
            COMMAND ${DOTNET_EXE} restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared
        )

        if(NOT DEFINED MSVC)
            add_custom_target(Hyperion.NET.Shared ALL
                COMMAND ${DOTNET_EXE} build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
                DEPENDS hyperion-core
                WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared
            )
        else()
            enable_language(CSharp)
            include_external_msproject(
                Hyperion.NET.Shared
                ${CSPROJ_PATH}
                TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
            )
        endif()

        add_dependencies(${BUILD_NAME} Hyperion.NET.Shared hyperion-core)
    endif()
endif()
