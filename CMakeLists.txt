cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

project(${BUILD_NAME} CXX C)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
    #set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2")
    #set(CMAKE_CXX_FLAGS_DEBUG "/MDd")

    # set to path of vcpkg
    set(CMAKE_TOOLCHAIN_FILE, "${VCPKG_INSTALLED_DIR}/../scripts/buildsystems/vcpkg.cmake")
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    else()
        if(APPLE)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMVK_DEBUG=1")
        endif()

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    endif()
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib ${VCPKG_INSTALLED_DIR}/)

find_package(Threads REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Bullet REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(SDL2 REQUIRED)

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h"   "src/*.hpp")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OPENAL_INCLUDE_DIR}
    ${BULLET_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

set(COMMON_LIBRARIES
    ${CMAKE_THREADS_LIBS_INIT}
    ${OPENAL_LIBRARY}
    ${Vulkan_LIBRARIES}
    ${BULLET_LIBRARIES}
    glfw
    ${SDL2_LIBRARIES}
)

target_link_libraries(${BUILD_NAME} ${COMMON_LIBRARIES})

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})


# sample subdir
add_subdirectory(samples)

