cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-core)

project(${BUILD_NAME} CXX C)

set(OpenGL_GL_PREFERENCE GLVND)

message(STATUS "Building ${BUILD_NAME} library, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(COMMON_LIBRARIES 
    ${COMMON_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

find_package(Threads REQUIRED)
find_package(ZLIB)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi /MP")
    add_definitions("/DNOMINMAX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fpermissive")
endif()

if(NOT IOS)
    if(APPLE)
        set(CURL_LIBRARY /opt/homebrew/opt/curl/lib/libcurl.dylib)
        set(CURL_INCLUDE_DIR /opt/homebrew/opt/curl/include)
    endif()
    
    find_package(CURL)
endif()

if(CURL_FOUND)
    message(STATUS "CURL found at ${CURL_INCLUDE_DIRS}")

    add_definitions(-DHYP_CURL=1)

    include_directories(${CURL_INCLUDE_DIRS})
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} ${CURL_LIBRARIES})
endif()

if(ZLIB_FOUND)
    message(STATUS "Zlib found at ${ZLIB_INCLUDE_DIRS}")

    add_definitions(-DHYP_ZLIB=1)

    include_directories(${ZLIB_INCLUDE_DIRS})
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} ${ZLIB_LIBRARIES})
elseif(EXISTS "${CMAKE_SOURCE_DIR}/submodules/zlib/CMakeLists.txt")
    message(STATUS "Zlib submodule found")

    add_definitions(-DHYP_ZLIB=1)

    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)

    include_directories(${CMAKE_SOURCE_DIR}/submodules/zlib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/zlib")
    add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/zlib ${CMAKE_CURRENT_BINARY_DIR}/zlib)

    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        zlibstatic
    )
endif()

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Core Library Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

add_library(${BUILD_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})