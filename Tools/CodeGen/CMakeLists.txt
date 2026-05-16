cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-codegen)

set(HYP_CODEGEN_VERSION_MAJOR 0)
set(HYP_CODEGEN_VERSION_MINOR 9)
set(HYP_CODEGEN_VERSION_PATCH 0)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/../../Build/CodeGen)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

project(${BUILD_NAME} CXX C)

message(STATUS "Building CodeGen Tool with compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if(NOT DEFINED HYP_PLATFORM_NAME)
    if(WIN32)
        set(HYP_PLATFORM_NAME "Windows")
    elseif(APPLE)
        set(HYP_PLATFORM_NAME "Mac")
    elseif(UNIX)
        set(HYP_PLATFORM_NAME "Linux")
    else()
        message(FATAL_ERROR "Unsupported platform for CodeGen tool: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

# only build codegen for desktop platforms
if(HYP_PLATFORM_NAME STREQUAL "Windows")
    add_definitions(-DHYP_WINDOWS=1)

    set(HYP_WINDOWS 1)
elseif(HYP_PLATFORM_NAME STREQUAL "Mac")
    add_definitions(-DHYP_APPLE=1)
    add_definitions(-DHYP_UNIX=1)
    add_definitions(-DHYP_MACOS=1)

    set(HYP_APPLE 1)
    set(HYP_UNIX 1)
    set(HYP_MACOS 1)
elseif(HYP_PLATFORM_NAME STREQUAL "Linux")
    add_definitions(-DHYP_LINUX=1)
    add_definitions(-DHYP_UNIX=1)

    set(HYP_LINUX 1)
    set(HYP_UNIX 1)
endif()

if(HYP_MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi /Zc:__cplusplus")

    # set to path of vcpkg
    set(CMAKE_TOOLCHAIN_FILE, "$ENV{VCPKG_ROOT}/../scripts/buildsystems/vcpkg.cmake")
else()
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
    # for debugging
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fpermissive -fsanitize=undefined -fno-omit-frame-pointer")
endif()

# if Win32, link with DbgHelp
if(WIN32)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} DbgHelp)
endif()

set(OpenGL_GL_PREFERENCE GLVND)

if(NOT SUBMODULES_DIR)
    set(SUBMODULES_DIR ${CMAKE_SOURCE_DIR}/../../External/ThirdParty/Source)
endif()

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DHYP_DEBUG_MODE=1)
add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

add_definitions(-DHYP_VULKAN=1)
add_definitions(-DHYP_TOOL=1)
add_definitions(-DHYP_STATICALLY_LINKED=1)

add_definitions(-DHYP_CODEGEN_VERSION_MAJOR=${HYP_CODEGEN_VERSION_MAJOR})
add_definitions(-DHYP_CODEGEN_VERSION_MINOR=${HYP_CODEGEN_VERSION_MINOR})
add_definitions(-DHYP_CODEGEN_VERSION_PATCH=${HYP_CODEGEN_VERSION_PATCH})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

file(GLOB_RECURSE CODEGEN_SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE CODEGEN_HEADERS "*.h" "*.hpp")

include_directories(
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/../../Source
    ${CMAKE_SOURCE_DIR}/../../External/ThirdParty/Source
)


add_executable(${BUILD_NAME} ${CODEGEN_SOURCES} ${CODEGEN_HEADERS})

if(HYP_MSVC)
    set_property(TARGET ${BUILD_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
endif()


# link with shared library hyperion
add_subdirectory(${CMAKE_SOURCE_DIR}/../../Source/Core ${CMAKE_BINARY_DIR}/../Core)
add_dependencies(${BUILD_NAME} hyperion-core)
target_link_libraries(${BUILD_NAME} hyperion-core)

# copy the hyperion dll to the executable directory
# add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:hyperion>
#     $<TARGET_FILE_DIR:${BUILD_NAME}>
# )

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})


