cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-sample)

project(${BUILD_NAME} CXX C)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Zi")

    #set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2")
    #set(CMAKE_CXX_FLAGS_DEBUG "/MDd")

    # set to path of vcpkg
    set(CMAKE_TOOLCHAIN_FILE, "${VCPKG_INSTALLED_DIR}/../scripts/buildsystems/vcpkg.cmake")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fpermissive")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fpermissive")
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

file(GLOB_RECURSE SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
include_directories(
    ${HYPERION_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})

if(MSVC)
    set_property(TARGET ${BUILD_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
endif()


# link with shared library hyperion
add_dependencies(${BUILD_NAME} hyperion)
target_link_libraries(${BUILD_NAME} hyperion)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})


