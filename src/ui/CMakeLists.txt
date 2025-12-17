cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-ui)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

add_definitions(-DHYP_BUILD_LIBRARY=1)

set(UI_LIBRARIES "")

include_directories(
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
)

if(NOT IOS)
    find_package(Freetype)

    if(FREETYPE_FOUND)
        message(STATUS "Freetype found")

        add_definitions(-DHYP_FREETYPE=1)

        include_directories(${FREETYPE_INCLUDE_DIRS})
        set(UI_LIBRARIES
            ${UI_LIBRARIES}
            ${FREETYPE_LIBRARIES}
        )
    endif()
endif()

# Normalize sources/headers to absolute paths under the root
set(CURRENT_SOURCES "")
foreach(filepath ${hyperion-ui_SOURCES})
    list(APPEND CURRENT_SOURCES "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

set(CURRENT_HEADERS "")
foreach(filepath ${hyperion-ui_HEADERS})
    list(APPEND CURRENT_HEADERS "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

add_library(${BUILD_NAME} STATIC ${CURRENT_SOURCES} ${CURRENT_HEADERS})

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
    target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

add_dependencies(${BUILD_NAME} hyperion-core hyperion-scene hyperion-rendering)
target_link_libraries(${BUILD_NAME} PRIVATE ${UI_LIBRARIES} hyperion-core hyperion-scene hyperion-rendering)
