cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

if(POLICY CMP0114)
  cmake_policy(SET CMP0114 NEW)
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type to use" FORCE)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebugInfo" CACHE STRING "Configurations to build" FORCE)
set(CMAKE_TRY_COMPILE_CONFIGURATION "Release" CACHE STRING "Configuration to use for try_compile" FORCE)

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

set(HYP_BUILD_TYPE "Release")

set(HYP_BUILD_TOOL_VERSION_MAJOR 0)
set(HYP_BUILD_TOOL_VERSION_MINOR 6)
set(HYP_BUILD_TOOL_VERSION_PATCH 0)

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
    
    execute_process(
        COMMAND ${HYP_BUILD_TOOL_RUN_SCRIPT_PATH} ${HYP_BUILD_TOOL_VERSION_MAJOR} ${HYP_BUILD_TOOL_VERSION_MINOR}
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
add_definitions(-DHYP_DEBUG_MODE=1)

# set HYPERION_ENGINE to 1 (used to differentiate between engine and build tool or other executables using core lib)
add_definitions(-DHYPERION_ENGINE=1)
set(HYPERION_ENGINE 1)

# if(NOT HYP_DEBUG_MODE)
    # enable TypeId generatation at compile time
    # will increase compile times slightly but improve runtime performance
    add_definitions(-DHYP_TYPE_ID_COMPILE_TIME=1)
# endif()

# platform and compiler definitions
if(MSVC)
    add_definitions(-DHYP_MSVC=1)
elseif(__clang__)
    add_definitions(-DHYP_CLANG=1)
    add_definitions(-DHYP_CLANG_OR_GCC=1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_definitions(-DHYP_GCC=1)
    add_definitions(-DHYP_CLANG_OR_GCC=1)
endif()

if(APPLE)
    add_definitions(-DHYP_UNIX=1)
    add_definitions(-DHYP_APPLE=1)
    if(IOS)
        add_definitions(-DHYP_IOS=1)
    else()
        add_definitions(-DHYP_MACOS=1)
    endif()
elseif(UNIX)
    add_definitions(-DHYP_UNIX=1)
    add_definitions(-DHYP_LINUX=1)
elseif(WIN32)
    add_definitions(-DHYP_WINDOWS=1)
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
    add_definitions("/DNOMINMAX")

    add_compile_options(/MP6)

    if(HYP_DEBUG_MODE)
        set(CMAKE_CXX_FLAGS "/Zc:preprocessor /O2 /Oy /MD /Zi /constexpr:depth1024 /bigobj /MP")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /NATVIS:${CMAKE_SOURCE_DIR}/src/Hyperion.natvis")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    else()
        set(CMAKE_CXX_FLAGS "/Zc:preprocessor /O2 /Oy /MD /constexpr:depth1024 /bigobj /MP")
    endif()
else()
    if(HYP_DEBUG_MODE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g -Wno-undefined-internal")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    endif()
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

    # no exceptions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

    # for full template backtrace
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")

    # # address sanitizer and other debugging utilities
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")
endif()

set(HYP_VERSION_MAJOR 1)
set(HYP_VERSION_MINOR 4)
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

    # disable xcode code signing
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES "CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")

# for VS solutions, we want to add some files as a custom solution item
elseif(MSVC)
    set(SOLUTION_FILES "${CMAKE_SOURCE_DIR}/src/Hyperion.natvis" "${CMAKE_SOURCE_DIR}/.clang-format" "${CMAKE_SOURCE_DIR}/.clang-tidy")
    add_custom_target(VSSolutionFiles SOURCES ${SOLUTION_FILES})
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

    if(NOT WIN32)
        find_package(SDL2 REQUIRED)
    endif()

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

    # Build WebGPU as external project
    ExternalProject_Add(webgpu_external
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/webgpu
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/webgpu_install
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        BUILD_ALWAYS FALSE
        INSTALL_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
    )

    # Create imported target for WebGPU
    ExternalProject_Get_Property(webgpu_external BINARY_DIR)
    
    add_library(webgpu STATIC IMPORTED)
    add_dependencies(webgpu webgpu_external)
    
    if(WIN32)
        set_target_properties(webgpu PROPERTIES
            IMPORTED_LOCATION ${BINARY_DIR}/${CMAKE_BUILD_TYPE}/webgpu.lib
        )
    else()
        set_target_properties(webgpu PROPERTIES
            IMPORTED_LOCATION ${BINARY_DIR}/libwebgpu.a
        )
    endif()

    include_directories(submodules/webgpu/include)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} webgpu)

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

if(SDL2_FOUND AND NOT WIN32)
    message(STATUS "SDL2 found")

    add_definitions(-DHYP_SDL=1)
endif()

# default definitions

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DHYP_VERSION_MAJOR=${HYP_VERSION_MAJOR})
add_definitions(-DHYP_VERSION_MINOR=${HYP_VERSION_MINOR})
add_definitions(-DHYP_VERSION_PATCH=${HYP_VERSION_PATCH})

set(HYP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated")

set(CSHARP_PROJECTS_DIR "${CMAKE_SOURCE_DIR}/build/CSharpProjects")
file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR})

add_definitions(-DHYP_EDITOR=1)

add_definitions(-DSDL_MAIN_HANDLED=1)

add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

add_definitions(-DHYP_SCRIPT=1)

# uncomment to explicitly disable dotnet
# set(HYP_NO_DOTNET 1)

if(NOT HYP_NO_DOTNET)
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
            file(COPY "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/nethost.dll" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

            set(HYP_DOTNET 1)
        else()
            message(WARNING "Dotnet headers found, but libnethost not found.")
        endif()

        if(HYP_DOTNET)
            add_definitions(-DHYP_DOTNET=1)
            set(HYP_GENERATE_CSHARP_LIBRARIES true)
        else()
            set(HYP_GENERATE_CSHARP_LIBRARIES false)
        endif()
    endif()
endif()

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

        source_group("hyperion-${subdir_basename} Library Code" FILES ${local_sources})

        return()
    endif()

    file(GLOB current_sources RELATIVE ${CMAKE_SOURCE_DIR} 
         "${dir}/*.cpp" "${dir}/*.cxx" "${dir}/*.cc" "${dir}/*.c" "${dir}/*.h" "${dir}/*.hpp" "${dir}/*.hxx" "${dir}/*.hxx" "${dir}/*.inl" "${dir}/*.mm" "${dir}/*.m")
    list(APPEND local_sources ${current_sources})

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
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "Engine Code" FILES ${SOURCES})

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

# Include ExternalProject for building submodules separately
include(ExternalProject)

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

        # Build glslang as external project to avoid cluttering solution
        ExternalProject_Add(glslang_external
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/glslang
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/glslang_install
                -DGLSLANG_ENABLE_INSTALL=OFF
                -DGLSLANG_ENABLE_INSTALL_DEFAULT=OFF
                -DGLSLANG_TESTS=OFF
                -DENABLE_GLSLANG_BINARIES=OFF
                -DENABLE_SPVREMAPPER=OFF
                -DENABLE_SPIRV=ON
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -DCMAKE_CXX_FLAGS=-DHYP_GLSLANG_MODIFICATIONS=1
            BUILD_ALWAYS FALSE
            INSTALL_COMMAND ""
            EXCLUDE_FROM_ALL TRUE
        )

        # Create imported targets for all the libraries we need
        ExternalProject_Get_Property(glslang_external BINARY_DIR)
        
        add_library(glslang STATIC IMPORTED)
        add_library(SPIRV STATIC IMPORTED)
        add_library(glslang-default-resource-limits STATIC IMPORTED)
        add_library(SPIRV-Tools STATIC IMPORTED)
        add_library(SPIRV-Tools-opt STATIC IMPORTED)
        add_library(MachineIndependent STATIC IMPORTED)
        add_library(GenericCodeGen STATIC IMPORTED)
        add_library(OSDependent STATIC IMPORTED)
        
        add_dependencies(glslang glslang_external)
        add_dependencies(SPIRV glslang_external)
        add_dependencies(glslang-default-resource-limits glslang_external)
        add_dependencies(SPIRV-Tools glslang_external)
        add_dependencies(SPIRV-Tools-opt glslang_external)
        add_dependencies(MachineIndependent glslang_external)
        add_dependencies(GenericCodeGen glslang_external)
        add_dependencies(OSDependent glslang_external)
        
        # Set the imported library locations
        if(WIN32)
            set_target_properties(glslang PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/glslang.lib
            )
            set_target_properties(SPIRV PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/SPIRV/${CMAKE_BUILD_TYPE}/SPIRV.lib
            )
            set_target_properties(glslang-default-resource-limits PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/glslang-default-resource-limits.lib
            )
            set_target_properties(SPIRV-Tools PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/${CMAKE_BUILD_TYPE}/SPIRV-Tools.lib
            )
            set_target_properties(SPIRV-Tools-opt PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/opt/${CMAKE_BUILD_TYPE}/SPIRV-Tools-opt.lib
            )
            set_target_properties(MachineIndependent PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/MachineIndependent.lib
            )
            set_target_properties(GenericCodeGen PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/GenericCodeGen.lib
            )
            set_target_properties(OSDependent PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/OSDependent/Windows/${CMAKE_BUILD_TYPE}/OSDependent.lib
            )
        else()
            set_target_properties(glslang PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libglslang.a
            )
            set_target_properties(SPIRV PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/SPIRV/${CMAKE_BUILD_TYPE}/libSPIRV.a
            )
            set_target_properties(glslang-default-resource-limits PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libglslang-default-resource-limits.a
            )
            set_target_properties(SPIRV-Tools PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/${CMAKE_BUILD_TYPE}/libSPIRV-Tools.a
            )
            set_target_properties(SPIRV-Tools-opt PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/opt/${CMAKE_BUILD_TYPE}/libSPIRV-Tools-opt.a
            )
            set_target_properties(MachineIndependent PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libMachineIndependent.a
            )
            set_target_properties(GenericCodeGen PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libGenericCodeGen.a
            )
            set_target_properties(OSDependent PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/glslang/OSDependent/Unix/${CMAKE_BUILD_TYPE}/libOSDependent.a
            )
        endif()

        include_directories(BEFORE submodules/glslang)

        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            glslang
            SPIRV
            glslang-default-resource-limits
            SPIRV-Tools
            SPIRV-Tools-opt
            MachineIndependent
            GenericCodeGen
            OSDependent
        )
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/libdatachannel/CMakeLists.txt")
        add_definitions(-DHYP_LIBDATACHANNEL=1)

        # Build libdatachannel as external project
        ExternalProject_Add(libdatachannel_external
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/libdatachannel
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

        include_directories(submodules/libdatachannel/include)

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
        file(COPY "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/${HYP_BUILD_TYPE}/GFSDK_Aftermath_Lib.x64.dll" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

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

set(HYPERION_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include ${HYP_GENERATED_DIR})

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
    ${CMAKE_SOURCE_DIR}/src
)

if(NOT WIN32)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

# set HYP_BUILD_LIBRARY just for this library (so DLL exports are correct)
add_definitions(-DHYP_BUILD_LIBRARY=1)

file(GLOB_RECURSE REFLECTION_GENERATED_SOURCES "${HYP_GENERATED_DIR}/*.cpp")
message(STATUS "Reflection generated sources directory: ${HYP_GENERATED_DIR}")
source_group(TREE ${HYP_GENERATED_DIR} PREFIX "Generated Code" FILES ${REFLECTION_GENERATED_SOURCES})

foreach(GENERATED_SOURCE ${REFLECTION_GENERATED_SOURCES})
    message(STATUS "Adding generated source file: ${GENERATED_SOURCE}")

    set(SOURCES ${SOURCES} ${GENERATED_SOURCE})
endforeach()

add_library(${BUILD_NAME} SHARED ${SOURCES} ${HEADERS})

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${CMAKE_THREADS_LIBS_INIT}
    ${OPENAL_LIBRARY}
    ${Vulkan_LIBRARIES}
)

if(NOT DEFINED WIN32)
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${SDL2_LIBRARIES}
    )
endif()

message(STATUS "Sub-libraries: ${SUB_LIBRARIES}")

target_link_libraries(${BUILD_NAME} PUBLIC ${COMMON_LIBRARIES})
target_link_libraries(${BUILD_NAME} PRIVATE ${SUB_LIBRARIES})

# Add dependencies on external projects to ensure they're built before the main library
if(TARGET glslang_external)
    add_dependencies(${BUILD_NAME} glslang_external)
endif()
if(TARGET libdatachannel_external)
    add_dependencies(${BUILD_NAME} libdatachannel_external)
endif()
if(TARGET webgpu_external)
    add_dependencies(${BUILD_NAME} webgpu_external)
endif()

# for all C# sources, compile them into a Class library
# use `dotnet` to compile them

if(HYP_GENERATE_CSHARP_LIBRARIES)
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        message(WARNING "C# scripts will not be built in Xcode generator mode. \
        To manually build C# libraries, run the `BuildCSharpLibraries` script:\n./tools/scripts/BuildCSharpLibraries.sh")

        set(HYP_BUILD_CSHARP_SOURCES false)
    else()
        set(HYP_BUILD_CSHARP_SOURCES true)
    endif()

    file(GLOB_RECURSE CSHARP_SHARED_SOURCES "src/dotnet/shared/**.cs")
    file(GLOB_RECURSE CSHARP_RUNTIME_SOURCES "src/dotnet/runtime/**.cs" "${HYP_GENERATED_DIR}/csharp/**.cs")
    file(GLOB_RECURSE CSHARP_INTEROP_SOURCES "src/dotnet/interop/**.cs")
    file(GLOB_RECURSE CSHARP_SCRIPTING_SOURCES "src/dotnet/scripting/**.cs")

    # build C# constant properties shared across all C# projects
    if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
        set(CSHARP_CONSTANTS "HYP_WINDOWS")
    elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
        set(CSHARP_CONSTANTS "HYP_MACOS;HYP_UNIX")
    elseif(${HYP_PLATFORM_NAME} STREQUAL "iOS")
        set(CSHARP_CONSTANTS "HYP_IOS;HYP_UNIX")
    elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
        set(CSHARP_CONSTANTS "HYP_LINUX;HYP_UNIX")
    endif()

    if(CSHARP_SHARED_SOURCES)# make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <AssemblyName>Hyperion.NET.Shared</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        if(CSHARP_CONSTANTS)
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
        endif()
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_SHARED_SOURCE ${CSHARP_SHARED_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <Compile Include=\"${CSHARP_SHARED_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared
        )

        if(NOT DEFINED MSVC)
            add_custom_target(Hyperion.NET.Shared ALL
                COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Shared.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.dll
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Shared.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.xml
                WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared
            )
        endif()
    endif()

    if(CSHARP_RUNTIME_SOURCES)# make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <AssemblyName>Hyperion.NET.Runtime</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        if(CSHARP_CONSTANTS)
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
        endif()
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_RUNTIME_SOURCE ${CSHARP_RUNTIME_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <Compile Include=\"${CSHARP_RUNTIME_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    </ProjectReference>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime
        )

        if(NOT DEFINED MSVC)
            add_custom_target(Hyperion.NET.Runtime ALL
                COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
                DEPENDS Hyperion.NET.Shared
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Runtime.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.dll
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Runtime.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.xml
                WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime
            )
        endif()
    endif()

    if(CSHARP_INTEROP_SOURCES)
        # make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <AssemblyName>Hyperion.NET.Interop</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        if(CSHARP_CONSTANTS)
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
        endif()
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_INTEROP_SOURCE ${CSHARP_INTEROP_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <Compile Include=\"${CSHARP_INTEROP_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    </ProjectReference>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop
        )

        if(NOT DEFINED MSVC)
            add_custom_target(Hyperion.NET.Interop ALL
                COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
                DEPENDS Hyperion.NET.Shared
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Interop.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.dll
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Interop.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.xml
                WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop
            )
        endif()
    endif()

    if(CSHARP_SCRIPTING_SOURCES)
        # make a directory for the .csproj file
        file(MAKE_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting)

        # generate .csproj file
        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "  <PropertyGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <OutputType>Library</OutputType>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <Nullable>enable</Nullable>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <AssemblyName>Hyperion.NET.Scripting</AssemblyName>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
        if(CSHARP_CONSTANTS)
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
        endif()
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "  </PropertyGroup>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
        foreach(CSHARP_SCRIPTING_SOURCE ${CSHARP_SCRIPTING_SOURCES})
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <Compile Include=\"${CSHARP_SCRIPTING_SOURCE}\" />\n")
        endforeach()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj\" >\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "      <Private>true</Private>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    </ProjectReference>\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp.Scripting\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.Scripting\" Version=\"4.9.2\" />\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <PackageReference Include=\"Microsoft.CodeAnalysis.Workspaces.MSBuild\" Version=\"4.9.2\" />\n")

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "  <ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    <None Include=\"${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.xml\">\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "    </None>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "  </ItemGroup>\n")
        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj "</Project>\n")


        # generate .xml file for DLL map

        file(WRITE ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.xml "<configuration>\n")

        if(${HYP_PLATFORM_NAME} STREQUAL "Win32")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.xml "  <dllmap dll=\"libhyperion\" target=\"hyperion.dll\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Mac")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.dylib\" />\n")
        elseif(${HYP_PLATFORM_NAME} STREQUAL "Linux")
            file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.xml "  <dllmap dll=\"libhyperion\" target=\"libhyperion.so\" />\n")
        endif()

        file(APPEND ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.xml "</configuration>\n")

        # run dotnet restore now
        execute_process(
            COMMAND dotnet restore
            WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting
        )

        if(NOT DEFINED MSVC)
            add_custom_target(Hyperion.NET.Scripting ALL
                COMMAND dotnet build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}
                DEPENDS Hyperion.NET.Shared
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Scripting.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Scripting.dll
                COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Scripting.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Scripting.xml
                WORKING_DIRECTORY ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting
            )
        endif()
    endif()

    if(HYP_BUILD_CSHARP_SOURCES)
        # Add C# projects to Visual Studio solution if building with MSVC
        if(MSVC)
            # Enable C# language support for proper project inclusion
            enable_language(CSharp)
            
            # Include existing .csproj files in the solution
            include_external_msproject(Hyperion.NET.Shared ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/Hyperion.NET.Shared.csproj)
            include_external_msproject(Hyperion.NET.Runtime ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/Hyperion.NET.Runtime.csproj)
            include_external_msproject(Hyperion.NET.Interop ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/Hyperion.NET.Interop.csproj)
            
            # Check if Scripting project exists and include it
            if(EXISTS ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj)
                include_external_msproject(Hyperion.NET.Scripting ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/Hyperion.NET.Scripting.csproj)

                # copy assemblies after build (depends on all C# projects)
                add_custom_target(CopyCSharpAssemblies ALL
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Shared.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Shared.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.xml
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Runtime.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Runtime.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.xml
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Interop.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Interop.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.xml
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Scripting.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Scripting.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Scripting/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Scripting.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Scripting.xml
                    DEPENDS Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop Hyperion.NET.Scripting
                )
            else()
                # copy assemblies after build (depends on all C# projects)
                add_custom_target(CopyCSharpAssemblies ALL
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Shared.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Shared/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Shared.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Shared.xml
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Runtime.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Runtime/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Runtime.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Runtime.xml
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Interop.dll ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${CSHARP_PROJECTS_DIR}/Hyperion.NET.Interop/bin/${HYP_BUILD_TYPE}/net8.0/Hyperion.NET.Interop.xml ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Interop.xml
                    DEPENDS Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop
                )
            endif()
        endif()
    endif()
        
else()
    message(STATUS "Not building C# libraries, HYP_GENERATE_CSHARP_LIBRARIES is false")
endif()

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

remove_definitions(-DHYP_BUILD_LIBRARY)

add_subdirectory(samples)

# if .vscode/launch.json does not exist, create it
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.vscode/launch.json")
    # copy the template files
    file(COPY "${CMAKE_SOURCE_DIR}/launch-template.json" DESTINATION "${CMAKE_SOURCE_DIR}/.vscode/launch.json")
    file(COPY "${CMAKE_SOURCE_DIR}/tasks-template.json" DESTINATION "${CMAKE_SOURCE_DIR}/.vscode/tasks.json")
    message(STATUS "Created launch and task json files in .vscode directory")
endif()

# for all <ConfigName>Template.json files in the root directory, copy them to the CMAKE_RUNTIME_OUTPUT_DIRECTORY directory (if they do not exist),
# and remove the "Template" part from the filename
# this is used for configuration files for the engine, like GlobalConfig.json, Hyperion
file(GLOB CONFIG_TEMPLATES "${CMAKE_SOURCE_DIR}/*ConfigTemplate.json")
foreach(CONFIG_TEMPLATE ${CONFIG_TEMPLATES})
    get_filename_component(CONFIG_NAME_WITH_TEMPLATE ${CONFIG_TEMPLATE} NAME_WE)
    string(REPLACE "ConfigTemplate" "Config" CONFIG_NAME ${CONFIG_NAME_WITH_TEMPLATE})
    set(OUTPUT_CONFIG_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG_NAME}.json")

    if(NOT EXISTS ${OUTPUT_CONFIG_FILE})
        file(COPY ${CONFIG_TEMPLATE} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        file(RENAME "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG_NAME_WITH_TEMPLATE}.json" "${OUTPUT_CONFIG_FILE}")
        message(STATUS "Copied configuration template: ${CONFIG_TEMPLATE} to ${OUTPUT_CONFIG_FILE}")
    else()
        message(STATUS "Configuration file already exists: ${OUTPUT_CONFIG_FILE}, skipping copy")
    endif()
endforeach()