cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(vcpkg_path "$ENV{VCPKG_INSTALLED_DIR}/../scripts/buildsystems/vcpkg.cmake")
project(${BUILD_NAME} CXX C)

# set HYP_PLATFORM_NAME
if(NOT DEFINED HYP_PLATFORM_NAME)
    if(WIN32)
        set(HYP_PLATFORM_NAME "Win32")
    elseif(APPLE)
        set(HYP_PLATFORM_NAME "Mac")
    elseif(UNIX)
        set(HYP_PLATFORM_NAME "Linux")
    elseif(IOS)
        set(HYP_PLATFORM_NAME "iOS")
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

string(TOLOWER ${HYP_PLATFORM_NAME} HYP_PLATFORM_NAME_LOWER)

# set HYP_BUILD_TYPE
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(HYP_BUILD_TYPE "Release")
else()
    set(HYP_BUILD_TYPE "Debug")
endif()

set(THIRD_PARTY_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE})
set(THIRD_PARTY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

enable_language(CXX)
enable_language(C)

if(APPLE)
    enable_language(OBJCXX)
    enable_language(OBJC)
endif()

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

    # # if the generated directory does not exist, create it
    # file(MAKE_DIRECTORY ${HYP_GENERATED_DIR})
    
    # # if it does exist.. remove all source files in it
    # file(GLOB_RECURSE GENERATED_FILES "${HYP_GENERATED_DIR}/**/*.generated.cpp" "${HYP_GENERATED_DIR}/*.cs")
    # foreach(GENERATED_FILE ${GENERATED_FILES})
    #     message(STATUS "Removing generated file: ${GENERATED_FILE}")
    #     file(REMOVE ${GENERATED_FILE})
    # endforeach()
    
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

        set(CMAKE_CXX_FLAGS_RELEASE "/Zc:preprocessor /O2 /MD /Zi /constexpr:depth1024 /bigobj")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/Zc:preprocessor /O2 /MD /constexpr:depth1024 /bigobj")
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
set(HYP_VERSION_MINOR 3)
set(HYP_VERSION_PATCH 0)

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${THIRD_PARTY_LIBRARY_DIR})

if(IOS)
    if (${CMAKE_OSX_SYSROOT} STREQUAL "iphonesimulator")
        set(IOS_SIMULATOR true)
        set(MVK_PACKAGE_NAME "ios-arm64_x86_64-simulator")
    else()
        set(MVK_PACKAGE_NAME "ios-arm64")
    endif()
    
    # ensure VULKAN_SDK is set
    if(NOT DEFINED ENV{VULKAN_SDK})
        message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please set it to the path of your Vulkan SDK.")
    else()
        message(STATUS "VULKAN_SDK environment variable is set to: $ENV{VULKAN_SDK}")
    endif()

    set(CMAKE_SYSTEM_NAME iOS)
    set(NO_CMAKE_SYSTEM_PATH TRUE)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
    set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    
    set(VULKAN_SDK $ENV{VULKAN_SDK})
    set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/include")
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK}/include")
    set(Vulkan_LIBRARY "${VULKAN_SDK}/../MoltenVK/static/MoltenVK.xcframework/${MVK_PACKAGE_NAME}/libMoltenVK.a")
    set(Vulkan_LIBRARIES "${VULKAN_SDK}/../MoltenVK/static/MoltenVK.xcframework/${MVK_PACKAGE_NAME}/libMoltenVK.a")
    set(Vulkan_FOUND TRUE)

    set(SDL2_INCLUDE_DIR "${THIRD_PARTY_INCLUDE_DIR}/SDL")
    set(SDL2_INCLUDE_DIRS "${THIRD_PARTY_INCLUDE_DIR}/SDL")
    set(SDL2_LIBRARIES "${THIRD_PARTY_LIBRARY_DIR}/libSDL2.a")
    set(SDL2_LIBRARY "${THIRD_PARTY_LIBRARY_DIR}/libSDL2.a")
    set(SDL2_FOUND TRUE)
    
    find_library(UIKIT_LIB UIKit)
    find_library(COREMEDIA_LIB CoreMedia)
    find_library(METAL_LIB Metal)

    # These are all SDL requirements - we should remove the SDL dependency for iOS.
    find_library(IOSURFACE_LIB IOSurface)
    find_library(AVFOUNDATION_LIB AVFoundation)
    find_library(GAMECONTROLLER_LIB GameController)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    find_library(QUARTZCORE_LIB QuartzCore)
    find_library(COREHAPTICS_LIB CoreHaptics)
    find_library(COREGRAPHICS_LIB CoreGraphics)
    find_library(COREMOTION_LIB CoreMotion)
    find_library(OPENGLES_LIB OpenGLES)

    set(COMMON_LIBRARIES
        ${UIKIT_LIB}
        ${COREMEDIA_LIB}
        ${METAL_LIB}

        # Begin SDL stuff
        ${IOSURFACE_LIB}
        ${AVFOUNDATION_LIB}
        ${GAMECONTROLLER_LIB}
        ${AUDIOTOOLBOX_LIB}
        ${QUARTZCORE_LIB}
        ${COREHAPTICS_LIB}
        ${COREGRAPHICS_LIB}
        ${COREMOTION_LIB}
        ${OPENGLES_LIB}
    )
endif()

if(CMAKE_GENERATOR STREQUAL "Xcode")
    set(CMAKE_XCODE_SCHEME_ENABLE_GPU_API_VALIDATION FALSE)
    set(CMAKE_XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE DISABLED)
endif()

if(NOT IOS)
    link_directories(/usr/local/lib ${VCPKG_INSTALLED_DIR}/x64-windows/lib "Z:/gstreamer/1.0/msvc_x86_64/lib")

    find_package(PkgConfig)
    pkg_check_modules(GST gstreamer-1.0
                        gstreamer-video-1.0
                        gstreamer-app-1.0)
endif()

find_package(Threads REQUIRED)
find_package(OpenAL)
find_package(Bullet)

if(NOT IOS)
    set(RENDERING_BACKEND "Vulkan")

    find_package(SDL2 REQUIRED)
    find_package(Vulkan)
    find_package(Freetype)
endif()

if (NOT DEFINED RENDERING_BACKEND)
    if (EXISTS "${CMAKE_SOURCE_DIR}/submodules/webgpu/CMakeLists.txt")
        set(RENDERING_BACKEND "WebGPU")
    elseif (Vulkan_FOUND)
        set(RENDERING_BACKEND "Vulkan")
    else()
        message(FATAL_ERROR "No rendering backend found")
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

    message(STATUS "Found Vulkan at: ${Vulkan_INCLUDE_DIRS} ${Vulkan_LIBRARIES}")
else()
    message(FATAL_ERROR "No rendering backend found! Please install Vulkan or WebGPU.")
endif()

if(${OpenAL_FOUND})
    message(STATUS "OpenAL found")

    add_definitions(-DHYP_OPENAL=1)
endif()

if(SDL2_FOUND)
    message(STATUS "SDL2 found")

    add_definitions(-DHYP_SDL=1)
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

add_definitions(-DSDL_MAIN_HANDLED=1)

add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

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

set(SUB_LIBRARIES "")
collect_sources_and_modules(${CMAKE_SOURCE_DIR}/src SOURCES SUB_LIBRARIES)

file(GLOB_RECURSE PLATFORM_FILES "src/system/platform/**/*")
foreach(PLATFORM_FILE ${PLATFORM_FILES})
    if(NOT ${PLATFORM_FILE} MATCHES "src/system/platform/${HYP_PLATFORM_NAME_LOWER}")
        list(REMOVE_ITEM SOURCES ${PLATFORM_FILE})
        message(STATUS "Removed source file ${PLATFORM_FILE} for non-matching platform")
    endif()
endforeach()

file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp" "src/*.inl")

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

if(Bullet_FOUND)
    message(STATUS "Bullet found")

    add_definitions(-DHYP_BULLET=1)

    include_directories(${BULLET_INCLUDE_DIRS})
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${BULLET_LIBRARIES}
    )
endif()

if (${NATIVE} AND NOT ${HYP_PLATFORM_NAME} STREQUAL "iOS")

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

        set(HYP_DOTNET 1)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.lib")
        message(STATUS ".NET core found, setting HYP_DOTNET to 1")

        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.lib")

        # copy the dll to the build directory
        file(COPY "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

        set(HYP_DOTNET 1)
    else()
        message(WARNING "Dotnet headers found, but libnethost not found.")
    endif()

    if(HYP_DOTNET)
        # Temp: disable C# build when generating xcode files due to a bug
        if(CMAKE_GENERATOR STREQUAL "Xcode")
            message(WARNING "C# building disabled temporarily for Xcode generator")
            set(HYP_BUILD_CSHARP_LIBRARIES false)
        else()
            add_definitions(-DHYP_DOTNET=1)
            set(HYP_BUILD_CSHARP_LIBRARIES true)
        endif()
    else()
        set(HYP_BUILD_CSHARP_LIBRARIES false)
    endif()
endif()

# if Win32, link with DbgHelp
if(WIN32)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} DbgHelp)
endif()

# if aftermath lib exists on windows, link to it
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.lib")
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.lib")
        add_definitions(-DHYP_AFTERMATH=1)

        # copy the dll to the build directory
        file(COPY "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

        message(STATUS "Aftermath found, linking to it")
    else()
        message(WARNING "Aftermath lib not found at ${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.lib")
    endif()
endif()

if(NOT IOS)
    if(FREETYPE_FOUND)
        message(STATUS "Freetype found")

        add_definitions(-DHYP_FREETYPE=1)

        include_directories(${FREETYPE_INCLUDE_DIRS})
        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            ${FREETYPE_LIBRARIES}
        )
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

# for all csharp sources, compile them into a Class library
# use `dotnet` to compile them

if(HYP_BUILD_CSHARP_LIBRARIES)
    message(STATUS "Building C# libraries")

    file(GLOB_RECURSE CSHARP_CORE_SOURCES "src/dotnet/core/**.cs")
    file(GLOB_RECURSE CSHARP_RUNTIME_SOURCES "src/dotnet/runtime/**.cs")
    file(GLOB_RECURSE CSHARP_RUNTIME_GENERATED_SOURCES "${HYP_GENERATED_DIR}/dotnet/runtime/**.cs")
    file(GLOB_RECURSE CSHARP_INTEROP_SOURCES "src/dotnet/interop/**.cs")
    file(GLOB_RECURSE CSHARP_SCRIPTING_SOURCES "src/dotnet/scripting/**.cs")

    if(CSHARP_CORE_SOURCES)# make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionCore)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <AssemblyName>HyperionCore</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_CORE_SOURCE ${CSHARP_CORE_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <Compile Include=\"${CSHARP_CORE_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionCore
        )

        add_custom_target(HyperionCore ALL
            COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionCore/bin/${HYP_BUILD_TYPE}/net8.0/HyperionCore.dll ${CMAKE_CURRENT_BINARY_DIR}/HyperionCore.dll
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionCore/bin/${HYP_BUILD_TYPE}/net8.0/HyperionCore.xml ${CMAKE_CURRENT_BINARY_DIR}/HyperionCore.xml
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionCore
        )
    endif()

    # check for any runtime sources or generated sources
    if(CSHARP_RUNTIME_SOURCES OR CSHARP_RUNTIME_GENERATED_SOURCES)
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionRuntime)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <AssemblyName>HyperionRuntime</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        
        # add runtime sources
        foreach(CSHARP_RUNTIME_SOURCE ${CSHARP_RUNTIME_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <Compile Include=\"${CSHARP_RUNTIME_SOURCE}\" />\n")
        endforeach()

        # add generated sources
        foreach(CSHARP_RUNTIME_GENERATED_SOURCE ${CSHARP_RUNTIME_GENERATED_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <Compile Include=\"${CSHARP_RUNTIME_GENERATED_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    </ProjectReference>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionRuntime/HyperionRuntime.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionRuntime
        )

        add_custom_target(HyperionRuntime ALL
            COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
            DEPENDS HyperionCore
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionRuntime/bin/${HYP_BUILD_TYPE}/net8.0/HyperionRuntime.dll ${CMAKE_CURRENT_BINARY_DIR}/HyperionRuntime.dll
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionRuntime/bin/${HYP_BUILD_TYPE}/net8.0/HyperionRuntime.xml ${CMAKE_CURRENT_BINARY_DIR}/HyperionRuntime.xml
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionRuntime
        )
    endif()

    if(CSHARP_INTEROP_SOURCES)
        # make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionInterop)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <AssemblyName>HyperionInterop</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_INTEROP_SOURCE ${CSHARP_INTEROP_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <Compile Include=\"${CSHARP_INTEROP_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    </ProjectReference>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionInterop/HyperionInterop.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionInterop
        )

        add_custom_target(HyperionInterop ALL
            COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
            DEPENDS HyperionCore
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionInterop/bin/${HYP_BUILD_TYPE}/net8.0/HyperionInterop.dll ${CMAKE_CURRENT_BINARY_DIR}/HyperionInterop.dll
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionInterop/bin/${HYP_BUILD_TYPE}/net8.0/HyperionInterop.xml ${CMAKE_CURRENT_BINARY_DIR}/HyperionInterop.xml
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionInterop
        )
    endif()

    if(CSHARP_SCRIPTING_SOURCES)
        # make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionScripting)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <AssemblyName>HyperionScripting</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_SCRIPTING_SOURCE ${CSHARP_SCRIPTING_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <Compile Include=\"${CSHARP_SCRIPTING_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/HyperionCore/HyperionCore.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    </ProjectReference>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp.Scripting\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.Scripting\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.Workspaces.MSBuild\" Version=\"4.9.2\" />\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/HyperionScripting/HyperionScripting.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionScripting
        )

        add_custom_target(HyperionScripting ALL
            COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
            DEPENDS HyperionCore
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionScripting/bin/${HYP_BUILD_TYPE}/net8.0/HyperionScripting.dll ${CMAKE_CURRENT_BINARY_DIR}/HyperionScripting.dll
            COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/HyperionScripting/bin/${HYP_BUILD_TYPE}/net8.0/HyperionScripting.xml ${CMAKE_CURRENT_BINARY_DIR}/HyperionScripting.xml
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/HyperionScripting
        )
    endif()
endif()

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

remove_definitions(-DHYP_BUILD_LIBRARY)

add_subdirectory(samples)