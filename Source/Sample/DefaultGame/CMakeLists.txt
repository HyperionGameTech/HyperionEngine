set(BUILD_NAME hyperion-sample)

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Zi /MP")
  set(CMAKE_TOOLCHAIN_FILE, "$ENV{VCPKG_ROOT}/../scripts/buildsystems/vcpkg.cmake")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fpermissive")
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)

# Collect all source/header files in this sample directory
file(GLOB_RECURSE SOURCES "*.cpp" "*.c" "*.mm" "*.m")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp" "*.hh" "*.inl")

# remove Platform/**/* files from the sources/headers before we add specific platforms
list(FILTER SOURCES EXCLUDE REGEX ".*/Platform/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/Platform/.*")

set(HYP_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Platform/${HYP_PLATFORM_NAME}")

if(NOT EXISTS "${HYP_PLATFORM_DIR}")
  set(HYP_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Platform/Default")
endif()

message(STATUS "Using platform dir: ${HYP_PLATFORM_DIR}")

if(EXISTS "${HYP_PLATFORM_DIR}")
  file(GLOB PLATFORM_SOURCES "${HYP_PLATFORM_DIR}/*.cpp" "${HYP_PLATFORM_DIR}/*.c" "${HYP_PLATFORM_DIR}/*.mm" "${HYP_PLATFORM_DIR}/*.m")
  list(APPEND SOURCES ${PLATFORM_SOURCES})

  file(GLOB PLATFORM_HEADERS "${HYP_PLATFORM_DIR}/*.hpp" "${HYP_PLATFORM_DIR}/*.h" "${HYP_PLATFORM_DIR}/*.hh" "${HYP_PLATFORM_DIR}/*.inl")
  list(APPEND HEADERS ${PLATFORM_HEADERS})
endif()

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
  set_target_properties(${BUILD_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks")
endif()

if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
  add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${BUILD_NAME}>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:hyperion>"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:hyperion-core>"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:hyperion-script>"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/../Packages"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Packages"
    COMMAND ${CMAKE_COMMAND} -D CONFIG_SRC_DIR="${CMAKE_SOURCE_DIR}/../Config"
      -D CONFIG_DST_DIR="$<TARGET_FILE_DIR:${BUILD_NAME}>/Config"
      -D PLATFORM="${HYP_PLATFORM_NAME}"
      -P "${CMAKE_SOURCE_DIR}/cmake/CopyPlatformConfigs.cmake"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Cache"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/../Data"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Data"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Source/Shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/Shaders/Shaders.ini"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Source/Shaders/Shaders.ini"
  )
endif()

# link with shared library hyperion
add_dependencies(${BUILD_NAME} hyperion hyperion-core)
target_link_libraries(${BUILD_NAME} hyperion hyperion-core)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
