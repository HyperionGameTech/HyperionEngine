cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-sample)

project(${BUILD_NAME} CXX C)

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Zi /MP")

  #set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2")
  #set(CMAKE_CXX_FLAGS_DEBUG "/MDd")

  # set to path of vcpkg
  set(CMAKE_TOOLCHAIN_FILE, "$ENV{VCPKG_ROOT}/../scripts/buildsystems/vcpkg.cmake")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fpermissive")
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fpermissive")
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)

file(GLOB_RECURSE SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
include_directories(
    ${HYP_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/../
    ${CMAKE_SOURCE_DIR}/../Engine
)
add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})

if(MSVC)
  set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)
  set_property(TARGET ${BUILD_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
endif()

if(CMAKE_GENERATOR MATCHES "Xcode")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_GENERATE_SCHEME ON)
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_ENABLE_GPU_API_VALIDATION OFF)
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_LAUNCH_CONFIGURATION_NAME "${HYP_BUILD_TYPE}")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_HIDE_BUILDABLE_NAME OFF)
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.hyperion.${BUILD_NAME}")
endif()

# link with shared library hyperion
add_dependencies(${BUILD_NAME} hyperion hyperion-core)
target_link_libraries(${BUILD_NAME} hyperion hyperion-core)

# copy the hyperion dll to the executable directory
# add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:hyperion>
#     $<TARGET_FILE_DIR:${BUILD_NAME}>
# )

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
