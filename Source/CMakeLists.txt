cmake_minimum_required(VERSION 3.10)

set(BUILD_NAME hyperion)

if(POLICY CMP0114)
  cmake_policy(SET CMP0114 NEW)
endif()

if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type to use" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# set HYP_PLATFORM_NAME
if(NOT DEFINED HYP_PLATFORM_NAME)
  if(ANDROID)
    set(HYP_PLATFORM_NAME "Android")
  elseif(WIN32)
    set(HYP_PLATFORM_NAME "Windows")
  elseif(IOS)
    set(HYP_PLATFORM_NAME "IOS")
  elseif(APPLE)
    set(HYP_PLATFORM_NAME "Mac")
  elseif(UNIX)
    set(HYP_PLATFORM_NAME "Linux")
  else()
    message(FATAL_ERROR "Unsupported platform")
  endif()
endif()

message(STATUS "Platform name: ${HYP_PLATFORM_NAME}")

# use params passed in
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HYP_LIBRARY_OUTPUT_DIRECTORY}/${HYP_PLATFORM_NAME}/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HYP_LIBRARY_OUTPUT_DIRECTORY}/${HYP_PLATFORM_NAME}/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY ${HYP_THIRD_PARTY_LIBRARY_DIRECTORY}/${HYP_PLATFORM_NAME}/${CMAKE_BUILD_TYPE})
set(CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY_RELEASE ${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})
set(CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY_DEBUG ${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations to build" FORCE)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(${BUILD_NAME} CXX)

# platform and compiler definitions
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_definitions(-DHYP_MSVC=1)
  set(HYP_MSVC 1)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_definitions(-DHYP_CLANG=1)
  add_definitions(-DHYP_CLANG_OR_GCC=1)
  set(HYP_CLANG 1)
  set(HYP_CLANG_OR_GCC 1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_definitions(-DHYP_GCC=1)
  add_definitions(-DHYP_CLANG_OR_GCC=1)
  set(HYP_GCC 1)
  set(HYP_CLANG_OR_GCC 1)
endif()

if(HYP_PLATFORM_NAME STREQUAL "Windows")
  set(CSHARP_CONSTANTS "HYP_WINDOWS")
  add_definitions(-DHYP_WINDOWS=1)
  add_definitions(-DHYP_PLATFORM_NAME_STR="${HYP_PLATFORM_NAME}")

  set(HYP_WINDOWS 1)
elseif(HYP_PLATFORM_NAME STREQUAL "Mac")
  set(CSHARP_CONSTANTS "HYP_MACOS;HYP_APPLE;HYP_UNIX")
  add_definitions(-DHYP_APPLE=1)
  add_definitions(-DHYP_UNIX=1)
  add_definitions(-DHYP_MACOS=1)
  add_definitions(-DHYP_PLATFORM_NAME_STR="${HYP_PLATFORM_NAME}")

  set(HYP_APPLE 1)
  set(HYP_UNIX 1)
  set(HYP_MACOS 1)
elseif(HYP_PLATFORM_NAME STREQUAL "IOS")
  set(CSHARP_CONSTANTS "HYP_IOS;HYP_APPLE;HYP_UNIX")
  add_definitions(-DHYP_APPLE=1)
  add_definitions(-DHYP_UNIX=1)
  add_definitions(-DHYP_IOS=1)
  add_definitions(-DHYP_PLATFORM_NAME_STR="${HYP_PLATFORM_NAME}")

  set(HYP_APPLE 1)
  set(HYP_UNIX 1)
  set(HYP_IOS 1)
elseif(HYP_PLATFORM_NAME STREQUAL "Android")
  set(CSHARP_CONSTANTS "HYP_ANDROID;HYP_UNIX")
  add_definitions(-DHYP_ANDROID=1)
  add_definitions(-DHYP_UNIX=1)
  add_definitions(-DHYP_PLATFORM_NAME_STR="${HYP_PLATFORM_NAME}")

  set(HYP_ANDROID 1)
  set(HYP_UNIX 1)
elseif(HYP_PLATFORM_NAME STREQUAL "Linux")
  set(CSHARP_CONSTANTS "HYP_LINUX;HYP_UNIX")
  add_definitions(-DHYP_LINUX=1)
  add_definitions(-DHYP_UNIX=1)
  add_definitions(-DHYP_PLATFORM_NAME_STR="${HYP_PLATFORM_NAME}")

  set(HYP_LINUX 1)
  set(HYP_UNIX 1)
endif()

if (HYP_CLANG AND HYP_WINDOWS)
  set(HYP_CLANG_WINDOWS true)
endif()

message(STATUS "Building for platform: ${HYP_PLATFORM_NAME} with compiler: ${CMAKE_CXX_COMPILER_ID}")

set(USE_MSBUILD_FOR_CSHARP_BUILD false)

# If generating for Visual Studio projects, we use MSBuild for C# projects
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
  set(USE_MSBUILD_FOR_CSHARP_BUILD true)
  message(STATUS "Using MSBuild for C# projects")
endif()

set(HYP_BUILD_TYPE ${CMAKE_BUILD_TYPE})

set(THIRD_PARTY_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE})
set(THIRD_PARTY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../External/ThirdParty/Source)
set(SUBMODULES_DIR ${CMAKE_SOURCE_DIR}/../External/Submodules)

enable_language(CXX)
enable_language(C)

if(APPLE)
  enable_language(OBJCXX)
  enable_language(OBJC)
endif()

# Include ExternalProject for building submodules separately
include(ExternalProject)

function(RunCodeGen version_major version_minor)

  if(CMAKE_HOST_WIN32)
    set(HYP_CODEGEN_RUN_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/../Tools/Scripts/RunCodeGen.bat")
    set(HYP_CODEGEN_BUILD_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/../Tools/Scripts/BuildCodeGen.bat")
  else()
    set(HYP_CODEGEN_RUN_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/../Tools/Scripts/RunCodeGen.sh")
    set(HYP_CODEGEN_BUILD_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/../Tools/Scripts/BuildCodeGen.sh")
  endif()

  message(STATUS "Running CodeGen: ${HYP_CODEGEN_RUN_SCRIPT_PATH} with version ${version_major}.${version_minor}")

  if(NOT EXISTS ${HYP_CODEGEN_RUN_SCRIPT_PATH})
    message(FATAL_ERROR "RunCodeGen script not found in ${CMAKE_SOURCE_DIR}/../Tools/Scripts! Expected path: ${HYP_CODEGEN_RUN_SCRIPT_PATH}")
  endif()

  execute_process(
        COMMAND ${HYP_CODEGEN_RUN_SCRIPT_PATH} ${version_major} ${version_minor}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../
        RESULT_VARIABLE HYP_CODEGEN_RESULT
    )

  if(NOT HYP_CODEGEN_RESULT EQUAL 0)
    message(FATAL_ERROR "CodeGen failed with exit code: ${HYP_CODEGEN_RESULT}")
  endif()
endfunction()

# Run CodeGen with version 0.9
RunCodeGen(0 9)

# set HYP_DEBUG_MODE to 1 for debug builds, 0 for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Configuring for a Debug build, setting HYP_DEBUG_MODE=1")

  add_definitions(-DHYP_DEBUG_MODE=1)
  set(HYP_DEBUG_MODE 1)
endif()

# Uncomment to enable release final build optimizations
# set(HYPERION_BUILD_RELEASE_FINAL 1)

if(NOT HYPERION_BUILD_RELEASE_FINAL)
  add_definitions(-DHYP_ENABLE_CORE_ASSERTIONS=1)
endif()

# set HYPERION_ENGINE to 1 (used to differentiate between engine and build tool or other executables using core lib)
add_definitions(-DHYPERION_ENGINE=1)
set(HYPERION_ENGINE 1)

if (NOT HYP_ANDROID AND NOT HYP_IOS AND NOT HYPERION_BUILD_RELEASE_FINAL)
  set(HYP_TESTS 1)
  add_definitions(-DHYP_TESTS=1)
endif()

# enable TypeId generatation at compile time
# will increase compile times slightly but improve runtime performance
add_definitions(-DHYP_TYPE_ID_COMPILE_TIME=1)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(NATIVE false CACHE BOOL "Is the build occuring on a native platform? (Access to native libraries, filesystem etc.)")
else()
  set(NATIVE true CACHE BOOL "Is the build occuring on a native platform? (Access to native libraries, filesystem etc.)")
endif()

if(HYP_MSVC)
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
  add_definitions("/DNOMINMAX")

  if (HYPERION_BUILD_RELEASE_FINAL)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /Zc:__cplusplus /O2 /Oi /MD /Zi /GL /GF /Zc:inline /constexpr:depth1024 /bigobj /MP4 /arch:AVX2 /fp:fast")
  else()
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /Zc:__cplusplus /O2 /Oi /MD /Z7 /MP /constexpr:depth1024 /bigobj /arch:AVX2 /fp:fast")
    
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /INCREMENTAL /OPT:REF /OPT:ICF")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /NATVIS:${CMAKE_SOURCE_DIR}/Engine/Hyperion.natvis")
  endif()

  # if(HYP_DEBUG_MODE)
  # else()
  #     set(CMAKE_CXX_FLAGS "/Zc:preprocessor /Zc:__cplusplus /O2 /Oy /MD /constexpr:depth1024 /bigobj /MP4")
  # endif()

  # address sanitizer: Enable if you 
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
else()
  # for clang on windows - no min/max macros
  if(${HYP_PLATFORM_NAME} STREQUAL "Windows")
    add_definitions(-DNOMINMAX)
    message(STATUS "Building with Clang on Windows, adding NOMINMAX definition to avoid min/max macro conflicts")
  endif()

  if(HYPERION_BUILD_RELEASE_FINAL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -flto -Werror")
  elseif(NOT HYP_DEBUG_MODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -Werror=return-type -Werror=address -Werror=non-virtual-dtor -Werror=unused-result")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g -fno-omit-frame-pointer -Werror=return-type -Werror=address -Werror=non-virtual-dtor -Werror=unused-result")
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

  # no exceptions
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

  # for full template backtrace
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # no ast recovery or spelling correction/suggestion for clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fno-recovery-ast -fno-spell-checking")
  endif()

  # # address sanitizer -- uncomment to enable
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

  # ClangCL on Windows uses link.exe, so it supports /DEBUG and /NATVIS
  if(HYP_CLANG_WINDOWS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /NATVIS:${CMAKE_SOURCE_DIR}/Engine/Hyperion.natvis /errorlimit:0")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG /NATVIS:${CMAKE_SOURCE_DIR}/Engine/Hyperion.natvis /errorlimit:0")
  endif()
endif()

set(HYP_VERSION_MAJOR 1)
set(HYP_VERSION_MINOR 4)
set(HYP_VERSION_PATCH 0)

link_directories(${THIRD_PARTY_LIBRARY_DIR})

if(CMAKE_GENERATOR STREQUAL "Xcode")
  set(CMAKE_XCODE_SCHEME_GENERATE ON)
  set(CMAKE_XCODE_SCHEME_ENABLE_GPU_API_VALIDATION FALSE)

  # disable xcode code signing
  set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES "CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED")
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
  set(CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.hyperion.\${PRODUCT_NAME:rfc1034identifier}")
  set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "")

  # for VS solutions, we want to add some files as a custom solution item
elseif(HYP_MSVC OR HYP_CLANG_WINDOWS)
  set(SOLUTION_FILES "${CMAKE_SOURCE_DIR}/Engine/Hyperion.natvis" "${CMAKE_SOURCE_DIR}/../.clang-format" "${CMAKE_SOURCE_DIR}/../.clang-tidy")
  add_custom_target(VSSolutionFiles SOURCES ${SOLUTION_FILES})
endif()

# if(NOT IOS)
#     link_directories(/usr/local/lib ${VCPKG_INSTALLED_DIR}/x64-windows/lib "Z:/gstreamer/1.0/msvc_x86_64/lib")

#     find_package(PkgConfig)
#     pkg_check_modules(GST gstreamer-1.0
#                         gstreamer-video-1.0
#                         gstreamer-app-1.0)
# endif()

find_package(Threads REQUIRED)
find_package(Bullet)

if(Bullet_FOUND)
  message(STATUS "Bullet found")

  add_definitions(-DHYP_BULLET=1)

  include_directories(SYSTEM ${BULLET_INCLUDE_DIRS})

  set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${BULLET_LIBRARIES}
    )
else()
  message(WARNING "Bullet not found, physics will be disabled!")
endif()

# PCHs
set(HYP_PCH
    "${CMAKE_SOURCE_DIR}/Engine/HyperionPch.hpp"
)

# /PCHs

# default definitions

add_definitions(-DHYP_VERSION_MAJOR=${HYP_VERSION_MAJOR})
add_definitions(-DHYP_VERSION_MINOR=${HYP_VERSION_MINOR})
add_definitions(-DHYP_VERSION_PATCH=${HYP_VERSION_PATCH})

set(HYP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/Generated")

set(CSHARP_PROJECTS_DIR "${CMAKE_SOURCE_DIR}/../Build/CSharpProjects")
file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR})

if(NOT HYP_ANDROID AND NOT HYP_IOS)
  # TODO if not final release build check.
  # for final release follow the android technique

  # we need to escape if on windows:
  if(WIN32)
    string(REPLACE "\\" "\\\\" HYP_ROOT_DIR_ESCAPED "${HYP_ROOT_DIR}")
  else()
    set(HYP_ROOT_DIR_ESCAPED "${HYP_ROOT_DIR}")
  endif()

  add_definitions(-DHYP_ROOT_DIR="${HYP_ROOT_DIR_ESCAPED}")

  set(HYP_EDITOR 1)
  add_definitions(-DHYP_EDITOR=1)

  add_definitions(-DHYP_ENABLE_SHADER_RELOAD=1)
endif()

# build libraries as static for iOS or in final build
# android needs shared lib
if((HYP_IOS OR HYPERION_BUILD_RELEASE_FINAL) AND NOT HYP_ANDROID)
  set(HYP_BUILD_STATIC 1)

  add_definitions(-DHYP_BUILD_STATIC=1)
endif()

# enable HypScript
add_definitions(-DHYP_SCRIPT=1)

set(HYP_GENERATE_CSHARP_LIBRARIES true)

# Set to true if C# is not planned on being used for scripting
# .NET is necessary for the editor, but for a leaner build this is recommended to disable if not using C# for scripts.

set(HYP_DOTNET_ONLY_FOR_EDITOR true)
if(NOT HYP_DOTNET_ONLY_FOR_EDITOR AND NOT DEFINED HYP_DOTNET_HOST)
  set(HYP_DOTNET_HOST 1)
endif()

if(HYP_DOTNET_HOST)
  # if dotnet headers exist, set HYP_DOTNET to 1
  # TODO: Get working on Android, iOS, Steam Deck...
  if(EXISTS "${THIRD_PARTY_INCLUDE_DIR}/dotnetcore/hostfxr.h")
    message(STATUS ".NET headers found")
    set(HYP_DOTNET 1)

    if(HYP_DOTNET)
      if(EXISTS "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/libnethost.a")
        message(STATUS ".NET core found, setting HYP_DOTNET to 1")

        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/libnethost.a")

        set(HYP_DOTNET 1)

        add_definitions(-DHYP_DOTNET=1)
      elseif(EXISTS "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/nethost.lib")
        message(STATUS ".NET core found, setting HYP_DOTNET to 1")

        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/nethost.lib")

        # copy the dll to the build directory
        file(COPY "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/nethost.dll" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

        set(HYP_DOTNET 1)

        add_definitions(-DHYP_DOTNET=1)
      else()
        message(STATUS ".NET core not found, expected to find libnethost.a or nethost.lib in ${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}")
      endif()
    endif()
  else()
    message(STATUS ".NET headers not found, skipping .NET core runtime")
  endif()
endif()

if(HYP_EDITOR)
  # for editor build, we don't actually need .NET host as the managed application (Hyperion.Editor.exe)
  # will call into the engine via InitFromManaged.
  if(HYP_DOTNET_HOST)
    add_definitions(-DHYP_DOTNET_HOST=1)
  else()
    add_definitions(-DHYP_DOTNET_ONLY_FOR_EDITOR=1)
  endif()

  set(HYP_DOTNET 1)
  add_definitions(-DHYP_DOTNET=1)
endif()


set(RENDERING_BACKEND "Vulkan")

if(WIN32)
  # set(ENABLE_EXPERIMENTAL_DX12_RENDERING_BACKEND true)

  if(ENABLE_EXPERIMENTAL_DX12_RENDERING_BACKEND)
    set(RENDERING_BACKEND "DX12")
  endif()
endif()

if (NOT DEFINED RENDERING_BACKEND)
  if (WIN32)
    set(RENDERING_BACKEND "DX12")
  elseif((APPLE) OR (UNIX))
    set(RENDERING_BACKEND "Vulkan")
  else()
    message(FATAL_ERROR "No rendering backend defined and unable to select default for platform!")
  endif()
endif()

if (RENDERING_BACKEND STREQUAL "Vulkan")
  if(HYP_ANDROID OR HYP_IOS)
    # iOS stuff - set up based on VULKAN_SDK
    if(HYP_IOS)
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

      find_library(UIKIT_LIB UIKit)
      find_library(COREMEDIA_LIB CoreMedia)
      find_library(METAL_LIB Metal)
      find_library(IOSURFACE_LIB IOSurface)
      find_library(QUARTZCORE_LIB QuartzCore)
      find_library(COREGRAPHICS_LIB CoreGraphics)
      find_library(FOUNDATION_LIB Foundation)

      set(RENDERER_LIBRARIES
            ${UIKIT_LIB}
            ${COREMEDIA_LIB}
            ${METAL_LIB}
            ${IOSURFACE_LIB}
            ${QUARTZCORE_LIB}
            ${COREGRAPHICS_LIB}
            ${FOUNDATION_LIB}
        )
    endif()


    add_definitions(-DHYP_VULKAN=1)
    set(HYP_VULKAN 1)
    if(NOT IOS)
      set(RENDERER_LIBRARIES ${RENDERER_LIBRARIES} vulkan)
    endif()
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
      include_directories(BEFORE SYSTEM ${Vulkan_INCLUDE_DIRS})
    endif()
    message(STATUS "Using platform-provided Vulkan")
  else()
    find_package(Vulkan REQUIRED)
    add_definitions(-DHYP_VULKAN=1)
    set(HYP_VULKAN 1)
    message(STATUS "Found Vulkan at: ${Vulkan_INCLUDE_DIRS} ${Vulkan_LIBRARIES}")
    include_directories(BEFORE SYSTEM ${Vulkan_INCLUDE_DIRS})
  endif()
elseif(RENDERING_BACKEND STREQUAL "DX12")
  add_definitions(-DHYP_DX12=1)
  set(HYP_DX12 1)

  message(STATUS "Using DirectX 12 as rendering backend")

  # check for D3D12MemoryAllocator ThirdParty module
  if(EXISTS "${THIRD_PARTY_INCLUDE_DIR}/D3D12MemoryAllocator/CMakeLists.txt")
    add_subdirectory(${THIRD_PARTY_INCLUDE_DIR}/D3D12MemoryAllocator ${CMAKE_BINARY_DIR}/ThirdParty/D3D12MemoryAllocator_build)

    include_directories(BEFORE SYSTEM ${THIRD_PARTY_INCLUDE_DIR}/D3D12MemoryAllocator/include)

    set(RENDERER_LIBRARIES
            ${RENDERER_LIBRARIES}
            D3D12MemoryAllocator)
  endif()
else()
  message(FATAL_ERROR "No rendering backend found!")
endif()

if (RENDERING_BACKEND STREQUAL "Vulkan")
  if (HYP_CLANG_WINDOWS)
    message(STATUS "Building with Clang on Windows - not including glslang due to build issues with Clang on Windows. If you want to use glslang, build with MSVC or on another platform.")
  endif()

  # if (${NATIVE} AND NOT HYP_IOS AND NOT HYP_ANDROID AND NOT HYP_CLANG_WINDOWS)
  #     # if glslang folder exists, link with it.
  #     if(EXISTS "${SUBMODULES_DIR}/glslang/glslang/CMakeLists.txt")
  #         set(HYP_GLSLANG 1)

  #         add_definitions(-DHYP_GLSLANG=1)
  #         add_definitions(-DHYP_GLSLANG_MODIFICATIONS=1)

  #         # Build glslang during CMake configuration time (not as part of the main build)
  #         # This keeps glslang completely separate from the VS solution / main project
  #         set(GLSLANG_BUILD_DIR "${CMAKE_BINARY_DIR}/glslang_build")
  #         set(GLSLANG_SOURCE_DIR "${SUBMODULES_DIR}/glslang")

  #         # Check if glslang needs to be built (check for a marker file or key library)
  #         if(WIN32)
  #             set(GLSLANG_MARKER_LIB "${GLSLANG_BUILD_DIR}/glslang/Release/glslang.lib")
  #         else()
  #             set(GLSLANG_MARKER_LIB "${GLSLANG_BUILD_DIR}/glslang/libglslang.a")
  #         endif()

  #         if(NOT EXISTS "${GLSLANG_MARKER_LIB}")
  #             message(STATUS "Building glslang...")

  #             # Create the build directory if it doesn't exist
  #             file(MAKE_DIRECTORY ${GLSLANG_BUILD_DIR})

  #             # Configure glslang
  #             execute_process(
  #                 COMMAND ${CMAKE_COMMAND}
  #                     -S ${GLSLANG_SOURCE_DIR}
  #                     -B ${GLSLANG_BUILD_DIR}
  #                     -DCMAKE_BUILD_TYPE=Release
  #                     -DGLSLANG_ENABLE_INSTALL=OFF
  #                     -DGLSLANG_ENABLE_INSTALL_DEFAULT=OFF
  #                     -DGLSLANG_TESTS=OFF
  #                     -DENABLE_GLSLANG_BINARIES=OFF
  #                     -DENABLE_OPT=OFF
  #                     -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  #                     "-DCMAKE_CXX_FLAGS=-DHYP_GLSLANG_MODIFICATIONS=1"
  #                 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  #                 RESULT_VARIABLE GLSLANG_CONFIGURE_RESULT
  #             )

  #             if(NOT GLSLANG_CONFIGURE_RESULT EQUAL 0)
  #                 message(FATAL_ERROR "Failed to configure glslang (exit code: ${GLSLANG_CONFIGURE_RESULT})")
  #             endif()

  #             # Build glslang
  #             execute_process(
  #                 COMMAND ${CMAKE_COMMAND} --build ${GLSLANG_BUILD_DIR} --config Release --parallel
  #                 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  #                 RESULT_VARIABLE GLSLANG_BUILD_RESULT
  #             )

  #             if(NOT GLSLANG_BUILD_RESULT EQUAL 0)
  #                 message(FATAL_ERROR "Failed to build glslang (exit code: ${GLSLANG_BUILD_RESULT})")
  #             endif()

  #             message(STATUS "glslang build completed successfully")
  #         else()
  #             message(STATUS "glslang already built, skipping rebuild")
  #         endif()

  #         # Create imported targets for all the libraries we need
  #         add_library(glslang STATIC IMPORTED)
  #         add_library(SPIRV STATIC IMPORTED)
  #         add_library(glslang-default-resource-limits STATIC IMPORTED)
  #         # add_library(SPIRV-Tools STATIC IMPORTED)
  #         # add_library(SPIRV-Tools-opt STATIC IMPORTED)
  #         add_library(MachineIndependent STATIC IMPORTED)
  #         add_library(GenericCodeGen STATIC IMPORTED)
  #         add_library(OSDependent STATIC IMPORTED)

  #         # Set the imported library locations
  #         if(WIN32)
  #             set_target_properties(glslang PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/Release/glslang.lib
  #             )
  #             set_target_properties(SPIRV PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/SPIRV/Release/SPIRV.lib
  #             )
  #             set_target_properties(glslang-default-resource-limits PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/Release/glslang-default-resource-limits.lib
  #             )
  #             # set_target_properties(SPIRV-Tools PROPERTIES
  #             #     IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/External/spirv-tools/source/Release/SPIRV-Tools.lib
  #             # )
  #             # set_target_properties(SPIRV-Tools-opt PROPERTIES
  #             #     IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/External/spirv-tools/source/opt/Release/SPIRV-Tools-opt.lib
  #             # )
  #             set_target_properties(MachineIndependent PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/Release/MachineIndependent.lib
  #             )
  #             set_target_properties(GenericCodeGen PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/Release/GenericCodeGen.lib
  #             )
  #             set_target_properties(OSDependent PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/OSDependent/Windows/Release/OSDependent.lib
  #             )
  #         else()
  #             set_target_properties(glslang PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/libglslang.a
  #             )
  #             set_target_properties(SPIRV PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/SPIRV/libSPIRV.a
  #             )
  #             set_target_properties(glslang-default-resource-limits PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/libglslang-default-resource-limits.a
  #             )
  #             # set_target_properties(SPIRV-Tools PROPERTIES
  #             #     IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/External/spirv-tools/source/libSPIRV-Tools.a
  #             # )
  #             # set_target_properties(SPIRV-Tools-opt PROPERTIES
  #             #     IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/External/spirv-tools/source/opt/libSPIRV-Tools-opt.a
  #             # )
  #             set_target_properties(MachineIndependent PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/libMachineIndependent.a
  #             )
  #             set_target_properties(GenericCodeGen PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/libGenericCodeGen.a
  #             )
  #             set_target_properties(OSDependent PROPERTIES
  #                 IMPORTED_LOCATION ${GLSLANG_BUILD_DIR}/glslang/OSDependent/Unix/libOSDependent.a
  #             )
  #         endif()

  #         include_directories(BEFORE ${SUBMODULES_DIR}/glslang)

  #         set(RENDERER_LIBRARIES
  #             ${RENDERER_LIBRARIES}
  #             glslang
  #             SPIRV
  #             glslang-default-resource-limits
  #             MachineIndependent
  #             GenericCodeGen
  #             OSDependent
  #         )
  #     endif()
  # endif()
elseif(RENDERING_BACKEND STREQUAL "DX12")
  # add DirectX-Headers include dir if DX12 rendering backend
  include_directories(BEFORE SYSTEM ${THIRD_PARTY_INCLUDE_DIR}/DirectX-Headers)
endif()

# Check if Steam sdk exists (Windows only for now).
if(HYP_WINDOWS)
  if(EXISTS "${THIRD_PARTY_INCLUDE_DIR}/steam/steam_api.h")
    message(STATUS "Steam SDK header found")

    # ensure the lib file exists
    # (needs to be copied there by the user)

    if(EXISTS "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/steam_api64.lib")
      set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/steam_api64.lib")
    else()
      message(WARNING "steam_api64.lib not found in third party libs when building with Steam SDK support")
    endif()
    if(EXISTS "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/steam_api64.dll")
      # needs to be copied to output dir
      message(STATUS "Copying steam_api64.dll to ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
      file(COPY "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/steam_api64.dll" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    else()
      message(WARNING "steam_api64.dll not found in third party libs when building with Steam SDK support")
    endif()

    set(HYP_STEAM_SDK 1)
    add_definitions(-DHYP_STEAM_SDK=1)
  endif()
endif()

# setup DXC if found
if(EXISTS "${THIRD_PARTY_INCLUDE_DIR}/dxc/dxcapi.h")
  message(STATUS "DXC headers found, setting up DXC")

  include_directories(BEFORE SYSTEM ${THIRD_PARTY_INCLUDE_DIR}/dxc)

  if(WIN32)
    set(DXC_LIB_PATH "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/dxcompiler.lib")
    set(DXC_DLL_PATH "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/dxcompiler.dll")
  elseif(APPLE)
    set(DXC_LIB_PATH "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/libdxcompiler.dylib")
  else()
    set(DXC_LIB_PATH "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/libdxcompiler.so")
  endif()

  if(EXISTS "${DXC_LIB_PATH}")
    message(STATUS "DXC library found at ${DXC_LIB_PATH}, linking to it")

    set(RENDERER_LIBRARIES
            ${RENDERER_LIBRARIES}
            "${DXC_LIB_PATH}"
        )

    set(HYP_DXC 1)
    add_definitions(-DHYP_DXC=1)

    # copy the dll to the build directory if on Windows
    if(WIN32)
      file(COPY "${DXC_DLL_PATH}" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    endif()
  else()
    message(WARNING "DXC library not found at ${DXC_LIB_PATH}, skipping DXC setup")
  endif()
else()
  message(STATUS "DXC headers not found, skipping DXC setup")
endif()

# if aftermath lib exists on windows, link to it
if(WIN32)
  if(EXISTS "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/GFSDK_Aftermath_Lib.x64.lib")
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/GFSDK_Aftermath_Lib.x64.lib")
    set(HYP_AFTERMATH 1)

    # copy the dll to the build directory
    file(COPY "${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/GFSDK_Aftermath_Lib.x64.dll" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

    message(STATUS "Aftermath found, linking to it")
  else()
    message(WARNING "Aftermath lib not found at ${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY}/GFSDK_Aftermath_Lib.x64.lib")
  endif()
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/Core ${CMAKE_BINARY_DIR}/Core)
add_subdirectory(${CMAKE_SOURCE_DIR}/Lang ${CMAKE_BINARY_DIR}/Lang)

function(collect_sources_and_modules dir out_sources out_headers out_modules)

  set(local_sources "")
  set(local_headers "")
  set(local_modules "")
  set(local_pchs "${HYP_PCH}") # start with global PCHs

  # if the directory is system/platform/<platform not equal to HYP_PLATFORM_NAME>, skip it
  if(${dir} MATCHES "${CMAKE_SOURCE_DIR}/Engine/System/Platform/.*" AND NOT ${dir} MATCHES "${CMAKE_SOURCE_DIR}/Engine/System/Platform/${HYP_PLATFORM_NAME}")
    message(STATUS "Skipping platform directory: ${dir}")
    return()
  endif()

  # skip Steam if HYP_STEAM_SDK is not defined
  if(${dir} MATCHES "${CMAKE_SOURCE_DIR}/Engine/Steam" AND NOT HYP_STEAM_SDK)
    message(STATUS "Skipping Steam directory (HYP_STEAM_SDK not defined)")
    return()
  endif()

  # skip Baking if HYP_EDITOR is not defined
  if(${dir} MATCHES "${CMAKE_SOURCE_DIR}/Engine/Baking" AND NOT HYP_EDITOR)
    message(STATUS "Skipping Baking directory (HYP_EDITOR not defined)")
    return()
  endif()

  file(GLOB current_sources RELATIVE ${CMAKE_SOURCE_DIR}
        "${dir}/*.cpp" "${dir}/*.cxx" "${dir}/*.cc" "${dir}/*.c" "${dir}/*.mm" "${dir}/*.m")
  list(APPEND local_sources ${current_sources})
  file(GLOB current_headers RELATIVE ${CMAKE_SOURCE_DIR}
        "${dir}/*.hpp" "${dir}/*.hxx" "${dir}/*.hh" "${dir}/*.h" "${dir}/*.inl" "${dir}/*.inc" "${dir}/*.hlsli")
  list(APPEND local_headers ${current_headers})

  # If this directory has a CMakeLists.txt, consider it a module
  if(EXISTS "${dir}/CMakeLists.txt")
    set(subdir_basename "")
    set(sub_library_name "")
    set(should_skip_module FALSE)
    set(should_recurse TRUE)

    string(REGEX MATCH "([^/]+)$" subdir_basename ${dir})

    if(${dir} MATCHES "CodeGen" OR ${dir} MATCHES "Sample")
      return()
    endif()

    # handle ThirdParty modules separately
    if(dir MATCHES "ThirdParty")
      return()
    endif()

    if(dir MATCHES "DotNET/Runtime")
      set(sub_library_name "hyperion-dotnet-runtime")
    elseif(dir MATCHES "DotNET/Shared")
      set(sub_library_name "hyperion-dotnet-shared")
    elseif(dir MATCHES "DotNET/Interop")
      set(sub_library_name "hyperion-dotnet-interop")
    elseif(dir MATCHES "DotNET/Scripting")
      set(sub_library_name "hyperion-dotnet-scripting")
    elseif(dir MATCHES "Rendering/") # skip rendering subdirectory, allow rendering module to handle that.
      set(should_skip_module TRUE)
    else()
      # the subfolder must contain a library with the name hyperion-<subfolder>
      string(TOLOWER "${subdir_basename}" subdir_basename_lower)
      set(sub_library_name "hyperion-${subdir_basename_lower}")
    endif()

    # if should_skip_module was set, return early
    if(should_skip_module)
      return()
    endif()

    list(APPEND local_modules ${sub_library_name})

    # Find files ending with Pch.hpp to add to PCHs for this module
    file(GLOB pch_files RELATIVE ${CMAKE_SOURCE_DIR} "${dir}/*Pch.hpp")

    if (pch_files)
      message(STATUS "Found PCH files in ${dir}: ${pch_files}")
      foreach(pch_file ${pch_files})
        list(APPEND local_pchs "${CMAKE_SOURCE_DIR}/${pch_file}")
      endforeach()
    endif()

    # Recurse into children so nested modules with their own CMakeLists.txt get added
    file(GLOB children LIST_DIRECTORIES true "${dir}/*")
    foreach(child ${children})
      if(NOT child MATCHES "CodeGen" AND NOT child MATCHES "Sample")
        if(IS_DIRECTORY "${child}")
          collect_sources_and_modules("${child}" child_sources child_headers child_modules)
          list(APPEND local_modules ${child_modules})

          if(EXISTS "${child}/CMakeLists.txt")
            # Treat child as its own module; do not pull its sources into this target
          else()
            list(APPEND local_sources ${child_sources})
            list(APPEND local_headers ${child_headers})
          endif()
        endif()
      endif()
    endforeach()

    # Avoid double-adding targets if a nested CMakeLists already defined them
    if(NOT TARGET ${sub_library_name})
      set("${sub_library_name}_SOURCES" ${local_sources})
      set("${sub_library_name}_HEADERS" ${local_headers})

      # debugging android build intellisense errors:
      if(NOT HYP_ANDROID)
        set("${sub_library_name}_PCH" ${local_pchs})
      endif()

      message(STATUS "Adding subdirectory ${dir} as module ${sub_library_name}")

      add_subdirectory(${dir} ${CMAKE_BINARY_DIR}/${subdir_basename})
    else()
      message(STATUS "Skipping add_subdirectory for ${dir} (target ${sub_library_name} already exists)")
    endif()

    set(${out_sources} "${local_sources}" PARENT_SCOPE)
    set(${out_headers} "${local_headers}" PARENT_SCOPE)
    set(${out_modules} "${local_modules}" PARENT_SCOPE)

    source_group("${sub_library_name} Library Code" FILES ${local_sources} ${local_headers})
  else()
    file(GLOB children LIST_DIRECTORIES true "${dir}/*")
    foreach(child ${children})
      if(NOT child MATCHES "CodeGen" AND NOT child MATCHES "Sample")
        if(IS_DIRECTORY "${child}")
          if(EXISTS "${child}/CMakeLists.txt")
            # Treat child as its own module; do not pull its sources into this target
            collect_sources_and_modules("${child}" child_sources child_headers child_modules)
            list(APPEND local_modules ${child_modules})

            # Remove any files from this module directory that were already added
            file(GLOB_RECURSE child_all_sources RELATIVE ${CMAKE_SOURCE_DIR}
                            "${child}/*.cpp" "${child}/*.cxx" "${child}/*.cc" "${child}/*.c" "${child}/*.mm" "${child}/*.m")
            file(GLOB_RECURSE child_all_headers RELATIVE ${CMAKE_SOURCE_DIR}
                            "${child}/*.hpp" "${child}/*.hxx" "${child}/*.hh" "${child}/*.h" "${child}/*.inl" "${child}/*.inc" "${child}/*.hlsli")
            list(REMOVE_ITEM local_sources ${child_all_sources})
            list(REMOVE_ITEM local_headers ${child_all_headers})
          else()
            collect_sources_and_modules("${child}" child_sources child_headers child_modules)
            list(APPEND local_sources ${child_sources})
            list(APPEND local_headers ${child_headers})
            list(APPEND local_modules ${child_modules})
          endif()
        endif()
      endif()
    endforeach()

    set(${out_sources} "${local_sources}" PARENT_SCOPE)
    set(${out_headers} "${local_headers}" PARENT_SCOPE)
    set(${out_modules} "${local_modules}" PARENT_SCOPE)
  endif()
endfunction()

set(SUB_LIBRARIES "")

collect_sources_and_modules(${CMAKE_SOURCE_DIR}/Engine SOURCES HEADERS SUB_LIBRARIES)
source_group(TREE ${CMAKE_SOURCE_DIR}/Engine PREFIX "Source Code" FILES ${SOURCES} ${HEADERS})

set(THIRD_PARTY_SOURCES "")
set(THIRD_PARTY_HEADERS "")
set(THIRD_PARTY_LIBRARIES "")
collect_sources_and_modules(${THIRD_PARTY_INCLUDE_DIR} THIRD_PARTY_SOURCES THIRD_PARTY_HEADERS THIRD_PARTY_LIBRARIES)
source_group(TREE ${THIRD_PARTY_INCLUDE_DIR} PREFIX "Third-Party Code" FILES ${THIRD_PARTY_SOURCES} ${THIRD_PARTY_HEADERS})

# set(PLATFORM_SPECIFIC_SOURCES "")
# set(PLATFORM_SPECIFIC_HEADERS "")
# set(PLATFORM_SPECIFIC_LIBRARIES "")
# collect_sources_and_modules(${CMAKE_SOURCE_DIR}/PlatformSpecific PLATFORM_SPECIFIC_SOURCES PLATFORM_SPECIFIC_HEADERS PLATFORM_SPECIFIC_LIBRARIES)
# source_group(TREE ${CMAKE_SOURCE_DIR}/PlatformSpecific PREFIX "Platform Specific Code" FILES ${PLATFORM_SPECIFIC_SOURCES} ${PLATFORM_SPECIFIC_HEADERS})

# add platform specific sources to the main sources list, but keep them in their own filter in the solution
# list(APPEND SOURCES ${PLATFORM_SPECIFIC_SOURCES})
# list(APPEND HEADERS ${PLATFORM_SPECIFIC_HEADERS})

# platform specific system stuff
file(GLOB_RECURSE PLATFORM_FILES "${CMAKE_SOURCE_DIR}/Engine/System/Platform/**/*")
foreach(PLATFORM_FILE ${PLATFORM_FILES})
  if(NOT ${PLATFORM_FILE} MATCHES "${CMAKE_SOURCE_DIR}/Engine/System/Platform/${HYP_PLATFORM_NAME}")
    list(REMOVE_ITEM SOURCES ${PLATFORM_FILE})
    list(REMOVE_ITEM HEADERS ${PLATFORM_FILE})
  endif()
endforeach()

# include shader files in the sln
file(GLOB_RECURSE SHADER_FILES "${CMAKE_SOURCE_DIR}/Shaders/*")
# we don't want to build shader files to be compiled by fxc but we want them in the solution
foreach(SHADER_FILE ${SHADER_FILES})
  source_group("Shader Code" FILES ${SHADER_FILE})

  if(HYP_MSVC OR HYP_CLANG_WINDOWS)
    set_property(SOURCE ${SHADER_FILE} PROPERTY VS_TOOL_OVERRIDE "None")
  endif()

  # add to sources so they appear in the solution
  list(APPEND SOURCES ${SHADER_FILE})

  message(STATUS "Adding shader file to solution: ${SHADER_FILE}")
endforeach()
include_directories(${CMAKE_SOURCE_DIR}/Shaders) # so VS can find them for intellisense

if (${HYP_PLATFORM_NAME} STREQUAL "Mac")
  # link with libraries to allow NSAlert and NSString
  set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        "-framework Cocoa"
        "-framework Foundation"
        "-framework AppKit"
        "-framework QuartzCore"
    )
endif()

if (${NATIVE} AND NOT ${HYP_PLATFORM_NAME} STREQUAL "IOS" AND NOT HYP_ANDROID)
  if(EXISTS "${SUBMODULES_DIR}/libdatachannel/CMakeLists.txt")
    add_definitions(-DHYP_LIBDATACHANNEL=1)

    # Build libdatachannel as external project (only for VS and Xcode)
    if(HYP_MSVC) # OR CMAKE_GENERATOR STREQUAL "Xcode"
      ExternalProject_Add(libdatachannel_external
                SOURCE_DIR ${SUBMODULES_DIR}/libdatachannel
                CMAKE_ARGS
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libdatachannel_install
                    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                BUILD_ALWAYS FALSE
                INSTALL_COMMAND ""
                EXCLUDE_FROM_ALL TRUE
            )

      # Create imported target
      ExternalProject_Get_Property(libdatachannel_external BINARY_DIR)

      add_library(datachannel-static STATIC IMPORTED)
      add_dependencies(datachannel-static libdatachannel_external)

      if(WIN32)
        set_target_properties(datachannel-static PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/${CMAKE_BUILD_TYPE}/datachannel-static.lib
                )
      else()
        set_target_properties(datachannel-static PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/libdatachannel-static.a
                )
      endif()

      include_directories(SYSTEM ${SUBMODULES_DIR}/libdatachannel/include)

      set(COMMON_LIBRARIES
                ${COMMON_LIBRARIES}
                datachannel-static
            )
    else()
      # For other generators (Makefiles, Ninja, etc.), use add_subdirectory
      add_subdirectory(${SUBMODULES_DIR}/libdatachannel EXCLUDE_FROM_ALL)

      include_directories(SYSTEM ${SUBMODULES_DIR}/libdatachannel/include)

      set(COMMON_LIBRARIES
                ${COMMON_LIBRARIES}
                datachannel-static
            )
    endif()
  endif()
endif()

# # if submodule 'oidn' exists , link with it
# if(EXISTS "${SUBMODULES_DIR}/oidn/CMakeLists.txt")
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

if(HYP_ANDROID)
  set(COMMON_LIBRARIES ${COMMON_LIBRARIES} android log)
endif()

if(NOT IOS)
  # if(GST_FOUND)
  #     add_definitions(-DHYP_GSTREAMER=1)
  #     add_definitions(-DHYP_GSTREAMER_BIN_DIR="Z:/gstreamer/1.0/msvc_x86_64/bin")

  #     include_directories(${GST_INCLUDE_DIRS})
  #     link_directories(${GST_LIBRARY_DIRS})

  #     # target_link_libraries(${BUILD_NAME} PRIVATE ${GST_LIBRARIES})

  #     message(STATUS "GStreamer found, linking to it: ${GST_LIBRARIES}\nInclude directories: ${GST_INCLUDE_DIRS}")

  #     set(COMMON_LIBRARIES
  #         ${COMMON_LIBRARIES}
  #         ${GST_LIBRARIES}
  #     )
  # endif()
endif()

# set to empty list
set(COMMON_INCLUDE_DIRS "")

include_directories(SYSTEM ${THIRD_PARTY_INCLUDE_DIR})
include_directories(
    ${HYP_GENERATED_DIR}
    ${COMMON_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
)

file(GLOB REFLECTION_GENERATED_SOURCES "${HYP_GENERATED_DIR}/*.cpp")
source_group(TREE ${HYP_GENERATED_DIR} PREFIX "Generated Code" FILES ${REFLECTION_GENERATED_SOURCES})

if(HYP_DEBUG_MODE)
  # make sure generated sources are compiled without debug info - they are large and slow down debug builds
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${REFLECTION_GENERATED_SOURCES} PROPERTIES COMPILE_FLAGS "-g0")
  endif()
endif()

set(SOURCES ${SOURCES} ${REFLECTION_GENERATED_SOURCES})
set(SOURCES ${SOURCES} ${THIRD_PARTY_SOURCES})

# disable precompiled headers for third-party sources
set_source_files_properties(${THIRD_PARTY_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

if(HYP_BUILD_STATIC)
  add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})
else()
  add_library(${BUILD_NAME} SHARED ${SOURCES} ${HEADERS})
endif()

target_compile_definitions(${BUILD_NAME} PRIVATE HYP_BUILD_ENGINE=1)

# if(HYP_ANDROID)
#     # add hyperion-android module
#     add_subdirectory(${CMAKE_SOURCE_DIR}/PlatformSpecific/Android ${CMAKE_BINARY_DIR}/android)
# endif()

# PCHs
# debugging android build intellisense errors:
if(NOT HYP_ANDROID)
  target_precompile_headers(${BUILD_NAME} PRIVATE ${HYP_PCH})
endif()

# if XCode: set the PCH file for the target
if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
  set_target_properties(${BUILD_NAME} PROPERTIES
        XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
        XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${CMAKE_SOURCE_DIR}/Engine/HyperionPch.hpp"
    )
endif()
# /PCHs

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${CMAKE_THREADS_LIBS_INIT}
)

set(SUB_LIBRARIES ${SUB_LIBRARIES} ${THIRD_PARTY_LIBRARIES})

target_link_libraries(${BUILD_NAME} PUBLIC ${COMMON_LIBRARIES})
target_link_libraries(${BUILD_NAME} PUBLIC ${RENDERER_LIBRARIES})
target_link_libraries(${BUILD_NAME} PUBLIC hyperion-script) #Lang

# add sublibraries as dependencies so any changes get picked up.
foreach(SUB_LIBRARY ${SUB_LIBRARIES})
  add_dependencies(${BUILD_NAME} ${SUB_LIBRARY})
endforeach()

if(APPLE)
  # needs to use -force_load on macOS for static libraries to re-export symbols correctly
  # for the Hyperion.NET assemblies, since we define functions for P/Invoke there.
  foreach(SUB_LIBRARY ${SUB_LIBRARIES})
    if (SUB_LIBRARY MATCHES "hyperion-dotnet-.*")
      target_link_libraries(${BUILD_NAME} PRIVATE -force_load $<TARGET_FILE:${SUB_LIBRARY}>)
    else()
      target_link_libraries(${BUILD_NAME} PRIVATE ${SUB_LIBRARY})
    endif()
  endforeach()
elseif(HYP_MSVC OR HYP_CLANG_WINDOWS)
  # needs to use /WHOLEARCHIVE on Windows for emulating the above behavior
  foreach(SUB_LIBRARY ${SUB_LIBRARIES})
    if (SUB_LIBRARY MATCHES "hyperion-dotnet-.*")
      target_link_options(${BUILD_NAME} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:${SUB_LIBRARY}>")
    else()
      target_link_libraries(${BUILD_NAME} PRIVATE ${SUB_LIBRARY})
    endif()
  endforeach()
else()
  target_link_libraries(${BUILD_NAME} PRIVATE ${SUB_LIBRARIES})
endif()


# Add dependencies on external projects to ensure they're built before the main library

if(TARGET libdatachannel_external)
  add_dependencies(${BUILD_NAME} libdatachannel_external)
endif()
if(TARGET webgpu_external)
  add_dependencies(${BUILD_NAME} webgpu_external)
endif()

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

add_subdirectory(Sample)

# If editor build, add the commandlet targets.
# otherwise, we don't want 'em
if(HYP_EDITOR)
  add_subdirectory(Editor)
  add_subdirectory(Commandlets)

  add_dependencies(${BUILD_NAME} hyperion-editor)

  if(APPLE)
    target_link_libraries(${BUILD_NAME} PRIVATE "-force_load $<TARGET_FILE:hyperion-editor>")
  elseif(MSVC)
    target_link_options(${BUILD_NAME} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:hyperion-editor>")
  else()
    target_link_libraries(${BUILD_NAME} PRIVATE -Wl,--whole-archive $<TARGET_FILE:hyperion-editor> -Wl,--no-whole-archive)
  endif()
endif()

if (CMAKE_GENERATOR MATCHES "Ninja")
  # Copy compile_commands.json to workspace root for clangd/LSP support in editors like VSCode, Zed, etc.
  message(STATUS "Syncing compile_commands.json to workspace root")
  # for VS with open cmake project
  # target_link_options(${BUILD_NAME} PRIVATE "/NATVIS:${CMAKE_SOURCE_DIR}/Engine/Hyperion.natvis")
  add_custom_target(sync_compile_commands ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/../compile_commands.json
      COMMENT "Syncing compile_commands.json to workspace root"
      VERBATIM
  )
endif()

# # copy sample scripts to the output directory
# file(GLOB SAMPLE_SCRIPTS "${CMAKE_SOURCE_DIR}/../Data/Scripts/*.hyp")
# foreach(SAMPLE_SCRIPT ${SAMPLE_SCRIPTS})
#     file(COPY ${SAMPLE_SCRIPT} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
# endforeach()

# # if .vscode/launch.json does not exist, create it
# if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.vscode/launch.json")
#     # copy the template files
#     file(COPY "${CMAKE_SOURCE_DIR}/launch-template.json" DESTINATION "${CMAKE_SOURCE_DIR}/.vscode/launch.json")
#     file(COPY "${CMAKE_SOURCE_DIR}/tasks-template.json" DESTINATION "${CMAKE_SOURCE_DIR}/.vscode/tasks.json")
#     message(STATUS "Created launch and task json files in .vscode directory")
# endif()

# # for all <ConfigName>Template.json files in the root directory, copy them to the CMAKE_RUNTIME_OUTPUT_DIRECTORY directory (if they do not exist),
# # and remove the "Template" part from the filename
# # this is used for configuration files for the engine, like GlobalConfig.json, Hyperion
# file(GLOB CONFIG_TEMPLATES "${CMAKE_SOURCE_DIR}/*ConfigTemplate.json")
# foreach(CONFIG_TEMPLATE ${CONFIG_TEMPLATES})
#     get_filename_component(CONFIG_NAME_WITH_TEMPLATE ${CONFIG_TEMPLATE} NAME_WE)
#     string(REPLACE "ConfigTemplate" "Config" CONFIG_NAME ${CONFIG_NAME_WITH_TEMPLATE})
#     set(OUTPUT_CONFIG_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG_NAME}.json")

#     if(NOT EXISTS ${OUTPUT_CONFIG_FILE})
#         file(COPY ${CONFIG_TEMPLATE} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
#         file(RENAME "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG_NAME_WITH_TEMPLATE}.json" "${OUTPUT_CONFIG_FILE}")
#         message(STATUS "Copied configuration template: ${CONFIG_TEMPLATE} to ${OUTPUT_CONFIG_FILE}")
#     else()
#         message(STATUS "Configuration file already exists: ${OUTPUT_CONFIG_FILE}, skipping copy")
#     endif()

#     # If building with Visual Studio, add the generated config file into the
#     # solution so it's visible in the IDE under the VSSolutionFiles target.
#     if(HYP_MSVC)
#         if(TARGET VSSolutionFiles)
#             set_property(TARGET VSSolutionFiles APPEND PROPERTY SOURCES ${OUTPUT_CONFIG_FILE})
#         endif()
#     endif()
# endforeach()
