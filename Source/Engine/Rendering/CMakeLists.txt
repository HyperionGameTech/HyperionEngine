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

# meshoptimizer powers mesh LOD generation (MeshLodGenerator)
set(MESHOPTIMIZER_DIR "${SUBMODULES_DIR}/meshoptimizer/src")

if(EXISTS "${MESHOPTIMIZER_DIR}/meshoptimizer.h")
  message(STATUS "meshoptimizer found, enabling mesh LOD generation")

  add_definitions(-DHYP_MESHOPTIMIZER=1)

  file(GLOB MESHOPTIMIZER_SOURCES "${MESHOPTIMIZER_DIR}/*.cpp")
  file(GLOB MESHOPTIMIZER_HEADERS "${MESHOPTIMIZER_DIR}/*.h")

  set_source_files_properties(${MESHOPTIMIZER_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

  set(CURRENT_SOURCES ${CURRENT_SOURCES} ${MESHOPTIMIZER_SOURCES})
  set(CURRENT_HEADERS ${CURRENT_HEADERS} ${MESHOPTIMIZER_HEADERS})

  include_directories(SYSTEM "${MESHOPTIMIZER_DIR}")
endif()

# manifold powers mesh booleans (MeshBoolean)
set(MANIFOLD_DIR "${SUBMODULES_DIR}/manifold")

if(EXISTS "${MANIFOLD_DIR}/include/manifold/manifold.h")
  message(STATUS "manifold found, enabling mesh booleans")

  add_definitions(-DHYP_MANIFOLD=1 -DMANIFOLD_PAR=-1)

  set(MANIFOLD_SOURCES
    "${MANIFOLD_DIR}/src/boolean3.cpp"
    "${MANIFOLD_DIR}/src/boolean_result.cpp"
    "${MANIFOLD_DIR}/src/constructors.cpp"
    "${MANIFOLD_DIR}/src/csg_tree.cpp"
    "${MANIFOLD_DIR}/src/edge_op.cpp"
    "${MANIFOLD_DIR}/src/execution_impl.cpp"
    "${MANIFOLD_DIR}/src/face_op.cpp"
    "${MANIFOLD_DIR}/src/impl.cpp"
    "${MANIFOLD_DIR}/src/manifold.cpp"
    "${MANIFOLD_DIR}/src/minkowski.cpp"
    "${MANIFOLD_DIR}/src/polygon.cpp"
    "${MANIFOLD_DIR}/src/properties.cpp"
    "${MANIFOLD_DIR}/src/quickhull.cpp"
    "${MANIFOLD_DIR}/src/sdf.cpp"
    "${MANIFOLD_DIR}/src/smoothing.cpp"
    "${MANIFOLD_DIR}/src/sort.cpp"
    "${MANIFOLD_DIR}/src/subdivision.cpp"
    "${MANIFOLD_DIR}/src/tree2d.cpp"
  )

  # manifold's robust predicates rely on strict IEEE semantics
  if(MSVC)
    set(MANIFOLD_COMPILE_OPTIONS "/fp:precise")
  else()
    set(MANIFOLD_COMPILE_OPTIONS "-fno-fast-math")
  endif()

  # manifold's CsgNode trips the engine's -Werror=non-virtual-dtor
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND MANIFOLD_COMPILE_OPTIONS "-Wno-non-virtual-dtor")
  endif()

  set_source_files_properties(${MANIFOLD_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_OPTIONS "${MANIFOLD_COMPILE_OPTIONS}")

  set(CURRENT_SOURCES ${CURRENT_SOURCES} ${MANIFOLD_SOURCES})

  include_directories(SYSTEM "${MANIFOLD_DIR}/include")
endif()

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


hyp_link_core_library(${BUILD_NAME})
target_link_libraries(${BUILD_NAME} PRIVATE hyperion-asset ${RENDERER_LIBRARIES})

if(HYP_GLSLANG)
  message(STATUS "Enabling GLSLANG support in rendering module")
  target_compile_definitions(${BUILD_NAME} PUBLIC HYP_GLSLANG=1)
endif()

if(HYP_DXC)
  message(STATUS "Linking to DXC")
  target_compile_definitions(${BUILD_NAME} PUBLIC HYP_DXC=1)
endif()
