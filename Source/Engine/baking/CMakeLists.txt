cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-baking)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY} /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/Generated
    ${THIRD_PARTY_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Engine/baking
)

# Normalize sources/headers to absolute paths under the root
set(CURRENT_SOURCES "")
foreach(filepath ${hyperion-baking_SOURCES})
    list(APPEND CURRENT_SOURCES "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

set(CURRENT_HEADERS "")
foreach(filepath ${hyperion-baking_HEADERS})
    list(APPEND CURRENT_HEADERS "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Code" FILES ${CURRENT_SOURCES} ${CURRENT_HEADERS})

# if submodules/xatlas exists, add xatlas.cpp to sources and xatlas.h to headers
if(EXISTS "${SUBMODULES_DIR}/xatlas/source/xatlas/xatlas.cpp" AND EXISTS "${SUBMODULES_DIR}/xatlas/source/xatlas/xatlas.h")
    add_definitions(-DHYP_XATLAS=1)

    file(GLOB XATLAS_SOURCES "${SUBMODULES_DIR}/xatlas/source/xatlas/*.cpp")
    file(GLOB XATLAS_HEADERS "${SUBMODULES_DIR}/xatlas/source/xatlas/*.h")

    if(XATLAS_SOURCES)
        message(STATUS "Xatlas found, adding to sources")
    endif()

    if(XATLAS_HEADERS)
        message(STATUS "Xatlas found, adding to headers")
    endif()

    set(CURRENT_SOURCES ${CURRENT_SOURCES} ${XATLAS_SOURCES})
    set(CURRENT_HEADERS ${CURRENT_HEADERS} ${XATLAS_HEADERS})

    include_directories("${SUBMODULES_DIR}/xatlas/source/xatlas")
endif()

add_library(${BUILD_NAME} STATIC ${CURRENT_SOURCES} ${CURRENT_HEADERS})

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
    target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

add_dependencies(${BUILD_NAME} hyperion-core hyperion-scene hyperion-rendering)
target_link_libraries(${BUILD_NAME} PRIVATE hyperion-core)
target_link_libraries(${BUILD_NAME} PUBLIC hyperion-scene hyperion-rendering)
