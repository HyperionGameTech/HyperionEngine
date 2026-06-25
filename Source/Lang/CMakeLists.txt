cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-script)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} library, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(COMMON_LIBRARIES
    hyperion-core
)

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "HypScript Code" FILES ${SOURCES} ${HEADERS})

set(HYP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/Generated/Lang")
file(GLOB REFLECTION_GENERATED_SOURCES "${HYP_GENERATED_DIR}/*.cpp")
source_group(TREE ${HYP_GENERATED_DIR} PREFIX "Generated Code" FILES ${REFLECTION_GENERATED_SOURCES})

set(SOURCES "${SOURCES}" ${REFLECTION_GENERATED_SOURCES})
set(HEADERS "${HEADERS}" ${REFLECTION_GENERATED_HEADERS})

add_definitions(-DHYP_SCRIPT=1)

include_directories(SYSTEM
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIR}
)
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${HYP_GENERATED_DIR}
)

add_library(${BUILD_NAME} SHARED ${SOURCES} ${HEADERS})
target_compile_definitions(${BUILD_NAME} PRIVATE HYP_BUILD_SCRIPT=1)

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
  target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})
