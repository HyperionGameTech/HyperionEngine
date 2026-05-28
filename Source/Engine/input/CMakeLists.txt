cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-input)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(SYSTEM ${THIRD_PARTY_INCLUDE_DIR})
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Generated
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Engine/input
)

# Normalize sources/headers to absolute paths under the root
set(CURRENT_SOURCES "")
foreach(filepath ${hyperion-input_SOURCES})
    list(APPEND CURRENT_SOURCES "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

set(CURRENT_HEADERS "")
foreach(filepath ${hyperion-input_HEADERS})
    list(APPEND CURRENT_HEADERS "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Code" FILES ${CURRENT_SOURCES} ${CURRENT_HEADERS})

add_library(${BUILD_NAME} STATIC ${CURRENT_SOURCES} ${CURRENT_HEADERS})

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
    target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

add_dependencies(${BUILD_NAME} hyperion-core)

target_link_libraries(${BUILD_NAME} PRIVATE hyperion-core)
