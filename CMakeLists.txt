cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(vcpkg_path "$ENV{VCPKG_INSTALLED_DIR}/../scripts/buildsystems/vcpkg.cmake")
project(${BUILD_NAME} CXX C)

set(RENDERING_BACKEND "Vulkan" CACHE STRING "Rendering backend to use")

# set HYP_PLATFORM_NAME
if(WIN32)
    set(HYP_PLATFORM_NAME "Win32")
elseif(APPLE)
    set(HYP_PLATFORM_NAME "Mac")
elseif(UNIX)
    set(HYP_PLATFORM_NAME "Linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

string(TOLOWER ${HYP_PLATFORM_NAME} HYP_PLATFORM_NAME_LOWER)

function(RunHypBuildTool)
    if(WIN32)
        set(HYP_BUILD_TOOL_RUN_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/tools/scripts/RunHypBuildTool.bat")
        set(HYP_BUILD_TOOL_BUILD_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/tools/scripts/BuildHypBuildTool.bat")
    else()
        set(HYP_BUILD_TOOL_RUN_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/tools/scripts/RunHypBuildTool.sh")
        set(HYP_BUILD_TOOL_BUILD_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/tools/scripts/BuildHypBuildTool.sh")
    endif()

    message(STATUS "Running HypBuildTool: ${HYP_BUILD_TOOL_RUN_SCRIPT_PATH}")

    if(NOT EXISTS ${HYP_BUILD_TOOL_RUN_SCRIPT_PATH})
        message(FATAL_ERROR "RunHypBuildTool script not found in tools/scripts! Expected path: ${HYP_BUILD_TOOL_RUN_SCRIPT_PATH}")
    endif()
    
    execute_process(
        COMMAND ${HYP_BUILD_TOOL_RUN_SCRIPT_PATH}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE HYP_BUILD_TOOL_RESULT
    )

    if(NOT HYP_BUILD_TOOL_RESULT EQUAL 0)
        message(FATAL_ERROR "HypBuildTool failed with exit code: ${HYP_BUILD_TOOL_RESULT}")
    endif()
endfunction()

# run HypBuildTool before configuring
RunHypBuildTool()

# set HYP_BUILD_TYPE
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(HYP_BUILD_TYPE "Release")
else()
    set(HYP_BUILD_TYPE "Debug")
endif()

set(HYP_DEBUG_MODE 1)

if(HYP_DEBUG_MODE)
    add_definitions(-DHYP_DEBUG_MODE=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(NATIVE false CACHE BOOL "Is the build occuring on a native platform? (Access to native libraries, filesystem etc.)")
else()
    set(NATIVE true CACHE BOOL "Is the build occuring on a native platform? (Access to native libraries, filesystem etc.)")
endif()

if(MSVC)
    message(STATUS "Building on MSVC, checking toolchain is set (should be set to `<vcpkg root path>/scripts/buildsystems/vcpkg.cmake`)")

    if (DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using CMake toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(STATUS "CMake toolchain file not specified -- should be set to vcpkg cmake file! \
            If you don't have vcpkg set up, download vcpkg from Microsoft and set the `-DCMAKE_TOOLCHAIN_FILE` CMake argument to \
            `<vcpkg root path>/scripts/buildsystems/vcpkg.cmake`.")
    endif()

    # disable specific warnings
    add_definitions("/wd4251")
    add_definitions("/wd4275")

    # add _CRT_SECURE_NO_WARNINGS
    add_definitions("/D_CRT_SECURE_NO_WARNINGS")

    # no exceptions
    add_definitions("/D_HAS_EXCEPTIONS=0")

    # add_definitions("/constexpr:depth1024")

    if(HYP_DEBUG_MODE)
        message(STATUS "Generating PDB for Hyperion library")

        set(CMAKE_CXX_FLAGS_RELEASE "/Zc:preprocessor /O2 /MD /Zi /constexpr:depth1024")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/Zc:preprocessor /O2 /MD /constexpr:depth1024")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

    # no exceptions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

    # for full template backtrace
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")

    # # address sanitizer and other debugging utilities
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

set(HYP_VERSION_MAJOR 1)
set(HYP_VERSION_MINOR 2)
set(HYP_VERSION_PATCH 15)

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE} /usr/local/lib ${VCPKG_INSTALLED_DIR}/x64-windows/lib "Z:/gstreamer/1.0/msvc_x86_64/lib")

find_package(PkgConfig)
pkg_check_modules(GST gstreamer-1.0=1.24.5
                      gstreamer-video-1.0=1.24.5
                      gstreamer-app-1.0=1.24.5)

# # TODO: Add windows, linux mono support
# set(ENV{PKG_CONFIG_PATH} "/Library/Frameworks/Mono.framework/Versions/Current/lib/pkgconfig")
# pkg_check_modules(MONO mono-2)

find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenAL)
find_package(Vulkan)
find_package(Bullet)
find_package(Freetype)
# find_package(CURL)

if (NOT DEFINED RENDERING_BACKEND)
    if (EXISTS "${CMAKE_SOURCE_DIR}/submodules/webgpu/CMakeLists.txt")
        set(RENDERING_BACKEND "WebGPU")
    elseif (Vulkan_FOUND)
        set(RENDERING_BACKEND "Vulkan")
    endif()
endif()

if (RENDERING_BACKEND STREQUAL "WebGPU")
    add_definitions(-DHYP_WEBGPU=1)

    include_directories(submodules/webgpu PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/webgpu/include")
    set(COMMON_LIBRARIES ${WGPU_RUNTIME_LIB})

    add_subdirectory(submodules/webgpu)

    message(STATUS "Using experimental WebGPU rendering backend")
elseif (RENDERING_BACKEND STREQUAL "Vulkan")
    add_definitions(-DHYP_VULKAN=1)

    message(STATUS "Using Vulkan rendering backend")
else()
    message(FATAL_ERROR "No rendering backend found! Please install Vulkan or WebGPU.")
endif()

if(${OpenAL_FOUND})
    message(STATUS "OpenAL found")

    add_definitions(-DHYP_OPENAL=1)
endif()

# default definitions

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DHYP_VERSION_MAJOR=${HYP_VERSION_MAJOR})
add_definitions(-DHYP_VERSION_MINOR=${HYP_VERSION_MINOR})
add_definitions(-DHYP_VERSION_PATCH=${HYP_VERSION_PATCH})

set(HYP_SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/res/scripts")
file(MAKE_DIRECTORY ${HYP_SCRIPTS_DIR})

set(CSHARP_PROJECTS_DIR "${CMAKE_SOURCE_DIR}/res/scripts/projects")
file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR})

set(HYP_SCRIPTS_BIN_DIR "${CMAKE_SOURCE_DIR}/res/scripts/bin")
file(MAKE_DIRECTORY ${HYP_SCRIPTS_BIN_DIR})

set(HYP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/build/generated")

add_definitions(-DHYP_EDITOR=1)

add_definitions(-DHYP_SDL=1)

add_definitions(-DSDL_MAIN_HANDLED=1)

add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

# function(collect_sources BASE_DIR SRC_FILES SUB_LIBRARIES)
#     file(GLOB_RECURSE ALL_FILES
#         "${BASE_DIR}/*.cpp"
#         "${BASE_DIR}/*.cxx"
#         "${BASE_DIR}/*.cc"
#         "${BASE_DIR}/*.c"
#         "${BASE_DIR}/*.mm"
#         "${BASE_DIR}/*.m"
#         "${BASE_DIR}/*.h"
#         "${BASE_DIR}/*.hpp"
#         "${BASE_DIR}/*.hxx"
#     )
# 
#     set(FINAL_FILES "")
# 
#     foreach(FILE ${ALL_FILES})
#         get_filename_component(DIR ${FILE} DIRECTORY)
#         
#         # Check if CMakeLists.txt exists in this directory
#         if(EXISTS "${DIR}/CMakeLists.txt")
#             # Skip adding files and instead add subdirectory
#             string(FIND "${SUBDIRS}" "${DIR}" FOUND)
#             if(${FOUND} EQUAL -1)
#                 list(APPEND SUBDIRS "${DIR}")
#             endif()
#         else()
#             message(STATUS "Adding source file to main library: ${FILE}")
#             list(APPEND FINAL_FILES "${FILE}")
#         endif()
#     endforeach()
# 
#     set(${SRC_FILES} ${FINAL_FILES} PARENT_SCOPE)
# 
#     set(LOCAL_SUB_LIBRARIES "")
# 
#     # Add subdirectories
#     foreach(SUBDIR ${SUBDIRS})
#         message(STATUS "Adding subdirectory: ${SUBDIR} to sources")
# 
#         # the subfolder must contain a library with the name hyperion-<subfolder>
# 
#         set(SUBDIR_BASENAME "")
#         string(REGEX MATCH "([^/]+)$" SUBDIR_BASENAME ${SUBDIR})
# 
#         add_subdirectory(${SUBDIR} ${CMAKE_BINARY_DIR}/${SUBDIR_BASENAME})
# 
#         list(APPEND LOCAL_SUB_LIBRARIES "hyperion-${SUBDIR_BASENAME}")
#     endforeach()
# 
#     set(${SUB_LIBRARIES} ${LOCAL_SUB_LIBRARIES} PARENT_SCOPE)
# endfunction()


function(collect_sources_and_modules dir out_sources out_modules)
    set(local_sources "")
    set(local_modules "")

    # if the directory is system/platform/<platform not equal to HYP_PLATFORM_NAME>, skip it
    if(${dir} MATCHES "src/system/platform/.*" AND NOT ${dir} MATCHES "src/system/platform/${HYP_PLATFORM_NAME_LOWER}")
        message(STATUS "Skipping platform directory: ${dir}")
        return()
    endif()

    # If this directory has a CMakeLists.txt, consider it a module and skip recursion.
    if(EXISTS "${dir}/CMakeLists.txt")
        # the subfolder must contain a library with the name hyperion-<subfolder>
 
        set(subdir_basename "")
        string(REGEX MATCH "([^/]+)$" subdir_basename ${dir})

        message(STATUS "Adding subdirectory as module: ${subdir_basename}")

        add_subdirectory(${dir} ${CMAKE_BINARY_DIR}/${subdir_basename})

        list(APPEND local_modules "hyperion-${subdir_basename}")

        set(${out_sources} "${local_sources}" PARENT_SCOPE)
        set(${out_modules} "${local_modules}" PARENT_SCOPE)

        return()
    endif()

    file(GLOB current_sources RELATIVE ${CMAKE_SOURCE_DIR} 
         "${dir}/*.cpp" "${dir}/*.cxx" "${dir}/*.cc" "${dir}/*.c" "${dir}/*.mm" "${dir}/*.m")
    list(APPEND local_sources ${current_sources})

    message(STATUS "Adding sources from ${dir}: ${current_sources}")

    file(GLOB children LIST_DIRECTORIES true "${dir}/*")
    foreach(child ${children})
        if(IS_DIRECTORY "${child}")
            collect_sources_and_modules("${child}" child_sources child_modules)
            list(APPEND local_sources ${child_sources})
            list(APPEND local_modules ${child_modules})
        endif()
    endforeach()

    set(${out_sources} "${local_sources}" PARENT_SCOPE)
    set(${out_modules} "${local_modules}" PARENT_SCOPE)
endfunction()

# if (${HYP_PLATFORM_NAME} STREQUAL "Mac")
#     # Mac can compile objective-c and objective-c++ files
#     file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c" "src/*.m" "src/*.mm")
# else()
#     file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
# endif()

set(SUB_LIBRARIES "")
collect_sources_and_modules(${CMAKE_SOURCE_DIR}/src SOURCES SUB_LIBRARIES)
#collect_sources(${CMAKE_SOURCE_DIR}/src SOURCES SUB_LIBRARIES)


file(GLOB_RECURSE PLATFORM_FILES "src/system/platform/**/*")
foreach(PLATFORM_FILE ${PLATFORM_FILES})
    if(NOT ${PLATFORM_FILE} MATCHES "src/system/platform/${HYP_PLATFORM_NAME_LOWER}")
        list(REMOVE_ITEM SOURCES ${PLATFORM_FILE})
        message(STATUS "Removed source file ${PLATFORM_FILE} for non-matching platform")
    endif()
endforeach()

file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp" "src/*.inl")

file(GLOB_RECURSE CSHARP_CORE_SOURCES "src/dotnet/core/**.cs")
file(GLOB_RECURSE CSHARP_RUNTIME_SOURCES "src/dotnet/runtime/**.cs")
file(GLOB_RECURSE CSHARP_INTEROP_SOURCES "src/dotnet/interop/**.cs")
file(GLOB_RECURSE CSHARP_SCRIPTING_SOURCES "src/dotnet/scripting/**.cs")

# # store sources that are in runtime folder in a separate list
# file(GLOB_RECURSE RUNTIME_SOURCES "src/runtime/*.cpp" "src/runtime/*.c")
# # remove sources that are in runtime folder
# list(FILTER SOURCES EXCLUDE REGEX ".*runtime/.*")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (${HYP_PLATFORM_NAME} STREQUAL "Mac")
    # link with libraries to allow NSAlert and NSString
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        "-framework Cocoa"
        "-framework Foundation"
    )
endif()

if(FREETYPE_FOUND)
    message(STATUS "Freetype found")

    add_definitions(-DHYP_FREETYPE=1)

    include_directories(${FREETYPE_INCLUDE_DIRS})
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${FREETYPE_LIBRARIES}
    )
endif()

if(Bullet_FOUND)
    message(STATUS "Bullet found")

    add_definitions(-DHYP_BULLET=1)

    include_directories(${BULLET_INCLUDE_DIRS})
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${BULLET_LIBRARIES}
    )
endif()

if (${NATIVE})

    # if glslang folder exists, link with it.
    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/glslang/glslang/CMakeLists.txt")
        add_definitions(-DHYP_GLSLANG=1)
        add_definitions(-DHYP_GLSLANG_MODIFICATIONS=1)

        include_directories(BEFORE submodules/glslang)
        add_subdirectory(submodules/glslang)

        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            glslang
            SPIRV
        )
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/libdatachannel/CMakeLists.txt")
        add_definitions(-DHYP_LIBDATACHANNEL=1)

        include_directories(submodules/libdatachannel PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libdatachannel/include")
        add_subdirectory(submodules/libdatachannel)

        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            datachannel-static
        )
    endif()
endif()

if(GST_FOUND)
    add_definitions(-DHYP_GSTREAMER=1)
    add_definitions(-DHYP_GSTREAMER_BIN_DIR="Z:/gstreamer/1.0/msvc_x86_64/bin")

    include_directories(${GST_INCLUDE_DIRS})
    link_directories(${GST_LIBRARY_DIRS})

    # target_link_libraries(${BUILD_NAME} PRIVATE ${GST_LIBRARIES})

    message(STATUS "GStreamer found, linking to it: ${GST_LIBRARIES}\nInclude directories: ${GST_INCLUDE_DIRS}")


    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${GST_LIBRARIES}
    )
endif()

# # if submodule 'oidn' exists , link with it
# if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/oidn/CMakeLists.txt")
#     add_definitions(-DHYP_OIDN=1)
#
#     include_directories(submodules/oidn/include)
#     add_subdirectory(submodules/oidn)
#
#     set(COMMON_LIBRARIES
#         ${COMMON_LIBRARIES}
#         OpenImageDenoise
#     )
# endif()

# if dotnet headers exist, set HYP_DOTNET to 1
if(EXISTS "${CMAKE_SOURCE_DIR}/include/dotnetcore/hostfxr.h")
    # set CLI_CMAKE_PKG_RID

    # if(WIN32)
    #     set(CLI_CMAKE_PKG_RID "win-x64")
    # elseif(APPLE)
    #     set(CLI_CMAKE_PKG_RID "osx-x64")
    # elseif(UNIX)
    #     set(CLI_CMAKE_PKG_RID "linux-x64")
    # else()
    #     message(FATAL_ERROR "Unsupported platform for .NET core")
    # endif()

    # # set CLI_CMAKE_COMMIT_HASH
    # execute_process(
    #     COMMAND git rev-parse HEAD
    #     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/submodules/dotnet
    #     OUTPUT_VARIABLE CLI_CMAKE_COMMIT_HASH
    #     OUTPUT_STRIP_TRAILING_WHITESPACE
    # )

    # # set CLI_CMAKE_FALLBACK_OS
    # if(WIN32)
    #     set(CLI_CMAKE_FALLBACK_OS "win")
    # elseif(APPLE)
    #     set(CLI_CMAKE_FALLBACK_OS "osx")
    # elseif(UNIX)
    #     set(CLI_CMAKE_FALLBACK_OS "linux")
    # else()
    #     message(FATAL_ERROR "Unsupported platform for .NET core")
    # endif()

    # Run dotnet.sh to build the dotnet runtime

    message(STATUS ".NET headers found, building .NET core runtime")
    
    file(GLOB DOTNET_LIBS "${CMAKE_SOURCE_DIR}/submodules/runtime/artifacts/obj/dotnet-nethost/${HYP_BUILD_TYPE}/net8.0/**/output/*.dylib"
        "${CMAKE_SOURCE_DIR}/submodules/runtime/artifacts/obj/dotnet-nethost/${HYP_BUILD_TYPE}/net8.0/**/output/*.so"
        "${CMAKE_SOURCE_DIR}/submodules/runtime/artifacts/obj/dotnet-nethost/${HYP_BUILD_TYPE}/net8.0/**/output/*.a"
        "${CMAKE_SOURCE_DIR}/submodules/runtime/artifacts/obj/dotnet-nethost/${HYP_BUILD_TYPE}/net8.0/**/output/*.lib"
        "${CMAKE_SOURCE_DIR}/submodules/runtime/artifacts/obj/dotnet-nethost/${HYP_BUILD_TYPE}/net8.0/**/output/*.dll")

    foreach(DOTNET_LIB ${DOTNET_LIBS})
        message(STATUS "Copying ${DOTNET_LIB} to ${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}")

        file(COPY ${DOTNET_LIB} DESTINATION "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}")
    endforeach()

    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/libnethost.a")
        message(STATUS ".NET core found, setting HYP_DOTNET to 1")

        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/libnethost.a")
        add_definitions(-DHYP_DOTNET=1)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.lib")
        message(STATUS ".NET core found, setting HYP_DOTNET to 1")

        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.lib")

        # copy the dll to the build directory
        file(COPY "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

        add_definitions(-DHYP_DOTNET=1)
    else()
        message(WARNING "Dotnet headers found, but libnethost not found.")
    endif()
endif()

# if Win32, link with DbgHelp
if(WIN32)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} DbgHelp)
endif()

# if aftermath lib exists on windows, link to it
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/${CMAKE_PLATFORM_NAME}/${CMAKE_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.lib")
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_PLATFORM_NAME}/${CMAKE_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64")
        add_definitions(-DHYP_AFTERMATH=1)

        # copy the dll to the build directory
        file(COPY "${CMAKE_SOURCE_DIR}/lib/${CMAKE_PLATFORM_NAME}/${CMAKE_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endif()

set(HYPERION_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# set to empty list
set(COMMON_INCLUDE_DIRS "")

# if submodules/xatlas exists, add xatlas.cpp to sources and xatlas.h to headers
if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/xatlas/source/xatlas/xatlas.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/submodules/xatlas/source/xatlas/xatlas.h")
    add_definitions(-DHYP_XATLAS=1)

    file(GLOB XATLAS_SOURCES "${CMAKE_SOURCE_DIR}/submodules/xatlas/source/xatlas/*.cpp")
    file(GLOB XATLAS_HEADERS "${CMAKE_SOURCE_DIR}/submodules/xatlas/source/xatlas/*.h")

    if(XATLAS_SOURCES)
        message(STATUS "Xatlas found, adding to sources")
    endif()

    if(XATLAS_HEADERS)
        message(STATUS "Xatlas found, adding to headers")
    endif()

    set(SOURCES ${SOURCES} ${XATLAS_SOURCES})
    set(HEADERS ${HEADERS} ${XATLAS_HEADERS})

    # add headers to COMMON_INCLUDE_DIRS
    set(COMMON_INCLUDE_DIRS ${COMMON_INCLUDE_DIRS} "${CMAKE_SOURCE_DIR}/submodules/xatlas/source/xatlas")
endif()

include_directories(
    ${HYPERION_INCLUDE_DIR}
    ${COMMON_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

# set HYP_BUILD_LIBRARY just for this library (so DLL exports are correct)
add_definitions(-DHYP_BUILD_LIBRARY=1)

file(GLOB_RECURSE REFLECTION_GENERATED_SOURCES "${HYP_GENERATED_DIR}/*.cpp")
message(STATUS "Reflection generated sources directory: ${HYP_GENERATED_DIR}")

foreach(GENERATED_SOURCE ${REFLECTION_GENERATED_SOURCES})
    message(STATUS "Adding generated source file: ${GENERATED_SOURCE}")

    set(SOURCES ${SOURCES} ${GENERATED_SOURCE})
endforeach()

# if(WIN32)
#     # DLL is currently not working on Win32 as it is not exporting the symbols.
#     # When this is fixed, make it also shared.
#     add_library(${BUILD_NAME} ${SOURCES} ${HEADERS})
# else()
    add_library(${BUILD_NAME} SHARED ${SOURCES} ${HEADERS})
# endif()

if(MSVC)
    # Allow multiprocess building with MSVC
    target_compile_options(${BUILD_NAME} PRIVATE "/MP")
endif()

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${CMAKE_THREADS_LIBS_INIT}
    ${OPENAL_LIBRARY}
    ${Vulkan_LIBRARIES}
    ${SDL2_LIBRARIES}
)

message(STATUS "Sub-libraries: ${SUB_LIBRARIES}")

target_link_libraries(${BUILD_NAME} PUBLIC ${COMMON_LIBRARIES})
target_link_libraries(${BUILD_NAME} PRIVATE ${SUB_LIBRARIES})

function(create_csharp_library output_name csharp_sources)
    # make a directory for the .csproj file
    set(CSHARP_PROJECT_DIR ${CSHARP_PROJECTS_DIR}/${output_name})
    file(MAKE_DIRECTORY ${CSHARP_PROJECT_DIR})

    # generate .csproj file
    file(WRITE ${CSHARP_PROJECT_DIR}/${output_name}.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  <PropertyGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <OutputType>Library</OutputType>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <Nullable>enable</Nullable>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <AssemblyName>${output_name}</AssemblyName>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <CopyLocalLockFileAssemblies>true</CopyLocalLockFileAssemblies>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  </PropertyGroup>\n")

    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  <ItemGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
    foreach(CSHARP_SOURCE ${csharp_sources})
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <Compile Include=\"${CSHARP_SOURCE}\" />\n")
    endforeach()

    # if name is not equal to HyperionCore then add a reference to HyperionCore
    if(NOT ${output_name} STREQUAL "HyperionCore")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <ProjectReference Include=\"../HyperionCore/HyperionCore.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    </ProjectReference>\n")
    endif()

    if(${output_name} STREQUAL "HyperionScripting")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp.Scripting\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.Scripting\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.Workspaces.MSBuild\" Version=\"4.9.2\" />\n")
    endif()

    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  </ItemGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  <ItemGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <None Include=\"${CSHARP_PROJECT_DIR}/${output_name}.xml\">\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    </None>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  </ItemGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "</Project>\n")


    # generate .xml file for DLL map

    file(WRITE ${CSHARP_PROJECT_DIR}/${output_name}.xml "<configuration>\n")

    if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
    elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
    elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
    endif()

    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.xml "</configuration>\n")

    # run dotnet restore now
    execute_process(
        COMMAND dotnet restore
        WORKING_DIRECTORY ${CSHARP_PROJECT_DIR}
    )

    add_custom_target(${output_name} ALL
        COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
        DEPENDS HyperionCore #reflection-codegen
        COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECT_DIR}/bin/${HYP_BUILD_TYPE}/net8.0/${output_name}.dll ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECT_DIR}/bin/${HYP_BUILD_TYPE}/net8.0/${output_name}.xml ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.xml
        WORKING_DIRECTORY ${CSHARP_PROJECT_DIR}
    )
endfunction()

# for all csharp sources, compile them into a Class library
# use `dotnet` to compile them

if(CSHARP_CORE_SOURCES)
    # pass the sources to the function as a list
    create_csharp_library(HyperionCore "${CSHARP_CORE_SOURCES}")
endif()

if(CSHARP_RUNTIME_SOURCES)
    create_csharp_library(HyperionRuntime "${CSHARP_RUNTIME_SOURCES}")
endif()

if(CSHARP_INTEROP_SOURCES)
    create_csharp_library(HyperionInterop "${CSHARP_INTEROP_SOURCES}")
endif()

if(CSHARP_SCRIPTING_SOURCES)
    create_csharp_library(HyperionScripting "${CSHARP_SCRIPTING_SOURCES}")
endif()

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

remove_definitions(-DHYP_BUILD_LIBRARY)

add_subdirectory(samples)