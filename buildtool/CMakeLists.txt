cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-buildtool)

set(HYP_BUILDTOOL_VERSION_MAJOR 0)
set(HYP_BUILDTOOL_VERSION_MINOR 4)
set(HYP_BUILDTOOL_VERSION_PATCH 1)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

project(${BUILD_NAME} CXX C)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi")

    # set to path of vcpkg
    set(CMAKE_TOOLCHAIN_FILE, "$ENV{VCPKG_ROOT}/../scripts/buildsystems/vcpkg.cmake")
else()
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
    # for debugging
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fpermissive -fsanitize=address,undefined -fno-omit-frame-pointer")
endif()

# if Win32, link with DbgHelp
if(WIN32)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} DbgHelp)
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/../lib /usr/local/lib)

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DHYP_DEBUG_MODE=1)
add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

add_definitions(-DHYP_VULKAN=1)
add_definitions(-DHYP_BUILDTOOL=1)
add_definitions(-DHYP_STATICALLY_LINKED=1)

add_definitions(-DHYP_BUILD_TOOL_VERSION_MAJOR=${HYP_BUILDTOOL_VERSION_MAJOR})
add_definitions(-DHYP_BUILD_TOOL_VERSION_MINOR=${HYP_BUILDTOOL_VERSION_MINOR})
add_definitions(-DHYP_BUILD_TOOL_VERSION_PATCH=${HYP_BUILDTOOL_VERSION_PATCH})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/core
    ${CMAKE_SOURCE_DIR}/../include
)
add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})

if(MSVC)
    set_property(TARGET ${BUILD_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
endif()


# link with shared library hyperion
add_subdirectory(${CMAKE_SOURCE_DIR}/../src/core ${CMAKE_BINARY_DIR}/../core)
add_dependencies(${BUILD_NAME} hyperion-core)
target_link_libraries(${BUILD_NAME} hyperion-core)

# copy the hyperion dll to the executable directory
# add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:hyperion>
#     $<TARGET_FILE_DIR:${BUILD_NAME}>
# )

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})


