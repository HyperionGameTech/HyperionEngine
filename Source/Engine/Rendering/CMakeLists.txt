cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-rendering)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(RENDERER_LIBRARIES "")

add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

if(HYP_AFTERMATH)
  add_definitions(-DHYP_AFTERMATH=1)
endif()

include_directories(SYSTEM ${THIRD_PARTY_INCLUDE_DIR})
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Generated
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Engine/Rendering
)

if(HYP_VULKAN)
  include_directories(SYSTEM ${Vulkan_INCLUDE_DIRS})
  add_definitions(-DHYP_VULKAN=1)
elseif(HYP_DX12)
  add_definitions(-DHYP_DX12=1)
endif()

# Normalize sources/headers to absolute paths under the root
set(CURRENT_SOURCES "")
foreach(filepath ${hyperion-rendering_SOURCES})
  list(APPEND CURRENT_SOURCES "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()

set(CURRENT_HEADERS "")
foreach(filepath ${hyperion-rendering_HEADERS})
  list(APPEND CURRENT_HEADERS "${CMAKE_SOURCE_DIR}/${filepath}")
endforeach()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Code" FILES ${CURRENT_SOURCES} ${CURRENT_HEADERS})

add_library(${BUILD_NAME} STATIC ${CURRENT_SOURCES} ${CURRENT_HEADERS})
target_compile_definitions(${BUILD_NAME} PRIVATE HYP_BUILD_ENGINE=1)

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
  target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

include_directories(BEFORE SYSTEM ${SUBMODULES_DIR}/glslang)

if(TARGET glslang_build)
  add_dependencies(${BUILD_NAME} glslang_build)
endif()

if(RENDERING_BACKEND STREQUAL "Vulkan")
  message(STATUS "Using Vulkan rendering backend")

  # find all Vulkan sources and headers
  file(GLOB_RECURSE VULKAN_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.hpp")
  file(GLOB_RECURSE VULKAN_HEADERS RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.hpp")
  # source_group(TREE ${CMAKE_SOURCE_DIR}/rendering/vulkan PREFIX "Vulkan Backend" FILES ${VULKAN_SOURCES} ${VULKAN_HEADERS})

  set(hyperion-vulkan_SOURCES ${VULKAN_SOURCES})
  set(hyperion-vulkan_HEADERS ${VULKAN_HEADERS})
  set(hyperion-vulkan_PCH ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/VulkanPch.hpp)

  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vulkan)
  set(RENDERER_LIBRARIES hyperion-vulkan)
elseif(RENDERING_BACKEND STREQUAL "DX12")
  message(STATUS "Using DirectX 12 rendering backend")

  file(GLOB_RECURSE DX12_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/dx12/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/dx12/*.hpp")
  file(GLOB_RECURSE DX12_HEADERS RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/dx12/*.hpp")
  # source_group(TREE ${CMAKE_SOURCE_DIR}/rendering/dx12 PREFIX "DX12 Backend" FILES ${DX12_SOURCES} ${DX12_HEADERS})

  set(hyperion-dx12_SOURCES ${DX12_SOURCES})
  set(hyperion-dx12_HEADERS ${DX12_HEADERS})
  set(hyperion-dx12_PCH ${CMAKE_CURRENT_SOURCE_DIR}/DX12/DX12Pch.hpp)

  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dx12)
  set(RENDERER_LIBRARIES hyperion-dx12)
endif()

add_dependencies(${BUILD_NAME} hyperion-core hyperion-asset)

target_link_libraries(${BUILD_NAME} PRIVATE hyperion-core hyperion-asset ${RENDERER_LIBRARIES})

if(HYP_GLSLANG)
  message(STATUS "Enabling GLSLANG support in rendering module")
  target_compile_definitions(${BUILD_NAME} PUBLIC HYP_GLSLANG=1)
endif()

if(HYP_DXC)
  message(STATUS "Linking to DXC")
  target_compile_definitions(${BUILD_NAME} PUBLIC HYP_DXC=1)
endif()
