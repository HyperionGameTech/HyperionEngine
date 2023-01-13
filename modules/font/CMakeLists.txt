cmake_minimum_required(VERSION 3.24)

set(BUILD_NAME hyperion-font)

project(${BUILD_NAME} CXX)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)

include_directories(
        ${CMAKE_SOURCE_DIR}/modules/
        ${CMAKE_SOURCE_DIR}/src
)

file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.hpp")

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

target_link_libraries(${BUILD_NAME} ${FREETYPE_LIBRARIES})
target_include_directories(${BUILD_NAME} PRIVATE ${FREETYPE_INCLUDE_DIRS})
