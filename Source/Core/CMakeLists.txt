cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-core)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} library with compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations to build" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

if(HYPERION_ENGINE)
    message(STATUS "Building hyperion-core for the engine, setting HYPERION_ENGINE=1 in Core.")
    add_definitions(-DHYPERION_ENGINE=1)
endif()

message(STATUS "HYP_PLATFORM_NAME: ${HYP_PLATFORM_NAME}")

if(HYP_PLATFORM_NAME STREQUAL "Android")
    message(STATUS "Building for Android, setting HYP_ANDROID=1 in Core.")
    add_definitions(-DHYP_ANDROID=1)
endif()

find_package(Threads REQUIRED)
find_package(ZLIB)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi /MP /Zc:__cplusplus")
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
elseif(EXISTS "${SUBMODULES_DIR}/zlib/CMakeLists.txt")
    message(STATUS "Zlib submodule found")

    add_definitions(-DHYP_ZLIB=1)

    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)

    include_directories(${SUBMODULES_DIR}/zlib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/zlib")
    add_subdirectory(${SUBMODULES_DIR}/zlib ${CMAKE_CURRENT_BINARY_DIR}/zlib)

    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        zlibstatic
    )
endif()

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "${BUILD_NAME} Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../Generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../External/ThirdParty/Source
)

message(STATUS "Configuring hyperion-core in ${CMAKE_CURRENT_SOURCE_DIR}")

set(SOURCES ${SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/../../External/ThirdParty/Source/tlsf/tlsf.c)

add_library(${BUILD_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})
