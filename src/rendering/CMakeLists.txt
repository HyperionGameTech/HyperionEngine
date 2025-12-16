cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-rendering)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(RENDERER_LIBRARIES
    hyperion-core
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi /MP")
    add_definitions("/DNOMINMAX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fpermissive")
endif()

add_definitions(-DHYP_BUILD_LIBRARY=1)

if(IOS)
    find_package(SDL2 REQUIRED)

    if(SDL2_FOUND)
        set(HYP_SDL 1)
    endif()

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

    set(RENDERER_LIBRARIES
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
elseif(HYP_PLATFORM_NAME STREQUAL "Linux")
    # we still use sdl on Linux (for now)
    find_package(SDL2 REQUIRED)

    if(SDL2_FOUND)
        set(HYP_SDL 1)
    endif()
endif()

add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

include_directories(
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/rendering
)

# Normalize sources/headers to absolute paths under the root
set(CURRENT_SOURCES "")
foreach(filepath ${hyperion-rendering_SOURCES})
    list(APPEND CURRENT_SOURCES "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

set(CURRENT_HEADERS "")
foreach(filepath ${hyperion-rendering_HEADERS})
    list(APPEND CURRENT_HEADERS "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

add_library(${BUILD_NAME} STATIC ${CURRENT_SOURCES} ${CURRENT_HEADERS})

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/submodules/glslang)

if(TARGET glslang_build)
    add_dependencies(${BUILD_NAME} glslang_build)
endif()

target_link_libraries(${BUILD_NAME} PRIVATE ${RENDERER_LIBRARIES})

# add definitions that should be visible to the main project as well
if(HYP_SDL)
    target_compile_definitions(${BUILD_NAME} PUBLIC HYP_SDL=1)
endif()
if(HYP_GLSLANG)
    target_compile_definitions(${BUILD_NAME} PUBLIC HYP_GLSLANG=1)
endif()