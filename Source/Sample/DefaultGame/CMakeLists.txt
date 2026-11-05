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

# the web launch layer is shared with projects' pages
if(HYP_WEB)
  set(HYP_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Templates/Platforms/Web")
endif()

if(NOT EXISTS "${HYP_PLATFORM_DIR}")
  set(HYP_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Platform/Default")
endif()

message(STATUS "Using platform dir: ${HYP_PLATFORM_DIR}")

if(EXISTS "${HYP_PLATFORM_DIR}" AND NOT HYP_ANDROID)
  file(GLOB PLATFORM_SOURCES "${HYP_PLATFORM_DIR}/*.cpp" "${HYP_PLATFORM_DIR}/*.c" "${HYP_PLATFORM_DIR}/*.mm" "${HYP_PLATFORM_DIR}/*.m")
  list(APPEND SOURCES ${PLATFORM_SOURCES})

  file(GLOB PLATFORM_HEADERS "${HYP_PLATFORM_DIR}/*.hpp" "${HYP_PLATFORM_DIR}/*.h" "${HYP_PLATFORM_DIR}/*.hh" "${HYP_PLATFORM_DIR}/*.inl")
  list(APPEND HEADERS ${PLATFORM_HEADERS})
endif()

set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ standard to be used")
include_directories(
    ${HYP_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/../
    ${CMAKE_SOURCE_DIR}/../Engine
)

if(HYP_USE_OBJECT_LIBRARIES)
  set(OBJECTS_TARGET_NAME ${BUILD_NAME}-objects)

  add_library(${OBJECTS_TARGET_NAME} OBJECT ${SOURCES} ${HEADERS})
  target_link_libraries(${OBJECTS_TARGET_NAME} PRIVATE $<COMPILE_ONLY:hyperion> $<COMPILE_ONLY:hyperion-core>)
  target_precompile_headers(${OBJECTS_TARGET_NAME} PRIVATE ${HYP_PCH})

  if(HYP_ANDROID)
    add_library(${BUILD_NAME} SHARED $<TARGET_OBJECTS:${OBJECTS_TARGET_NAME}>)
  else()
    add_executable(${BUILD_NAME} $<TARGET_OBJECTS:${OBJECTS_TARGET_NAME}>)
  endif()
elseif(HYP_ANDROID)
  add_library(${BUILD_NAME} SHARED ${SOURCES} ${HEADERS})
else()
  add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})
endif()

if(HYP_WEB)
  # for test runs under node: a host folder holding Config/, Cache/ and Content/, mounted as /hyperion
  set(HYP_WEB_NODE_PACKAGE_DIR "" CACHE PATH "Host folder to mount as the package when running under node")

  set(HYP_WEB_PAGE_NAME ${BUILD_NAME})
  set(HYP_WEB_PAGE_TITLE "Hyperion")
  configure_file("${HYP_PLATFORM_DIR}/index.html.in" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html" @ONLY)
  configure_file("${HYP_PLATFORM_DIR}/isolation-worker.js" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/isolation-worker.js" COPYONLY)

  target_compile_definitions(${BUILD_NAME} PRIVATE [==[HYP_WEB_GAME_CLASS="DefaultGame"]==])

  # DWARF triples the download (60 MB against 21 MB) and holds the link back to limited optimisation.
  # Function names are kept either way, so a stack trace in the browser's console stays readable.
  option(HYP_WEB_DEBUG_INFO "Keep DWARF debug information in the wasm" OFF)

  if(NOT HYP_WEB_DEBUG_INFO)
    target_link_options(${BUILD_NAME} PRIVATE -g0 --profiling-funcs)
  endif()

  if(HYP_WEB_NODE_PACKAGE_DIR)
    target_compile_definitions(${BUILD_NAME} PRIVATE HYP_WEB_NODE_PACKAGE_DIR="${HYP_WEB_NODE_PACKAGE_DIR}")
  else()
    # JSPI lets the thread that renders wait on the browser (device requests, readbacks, the next animation frame)
    target_link_options(${BUILD_NAME} PRIVATE -sJSPI=1 -sOFFSCREENCANVAS_SUPPORT=1)
  endif()

  target_link_options(${BUILD_NAME} PRIVATE ${HYP_WEB_APP_LINK_OPTIONS})
endif()

if(MSVC)
  set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)
  set_property(TARGET ${BUILD_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
  set_target_properties(${BUILD_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--cooked=false")
endif()

if(CMAKE_GENERATOR MATCHES "Xcode")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_GENERATE_SCHEME ON)
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_ENABLE_GPU_API_VALIDATION OFF)
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_LAUNCH_CONFIGURATION_NAME "${HYP_BUILD_TYPE}")
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_SCHEME_HIDE_BUILDABLE_NAME OFF)
  set_target_properties(${BUILD_NAME} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.hyperion.${BUILD_NAME}")
  set_target_properties(${BUILD_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
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
  )

  if(HYP_DXC)
    add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_LIB_PATH}"
        "$<TARGET_FILE_DIR:${BUILD_NAME}>/Frameworks"
    )
  endif()
endif()

message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")

if(HYP_PLATFORM_NAME STREQUAL "IOS")
  add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -D CONFIG_SRC_DIR="${CMAKE_SOURCE_DIR}/../Config"
      -D CONFIG_DST_DIR="$<TARGET_FILE_DIR:${BUILD_NAME}>/Config"
      -D PLATFORM="${HYP_PLATFORM_NAME}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/../CopyPlatformConfigs.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/../Config/Shaders.hmf"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Config/Shaders.hmf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/../Config/DeviceTiers.hmf"
      "$<TARGET_FILE_DIR:${BUILD_NAME}>/Config/DeviceTiers.hmf"
  )

  add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      -D SRC_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
      -D DST_DIR="$<TARGET_FILE_DIR:${BUILD_NAME}>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/../CopyOutputDirToBundle.cmake"
  )
else()
  add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -D CONFIG_SRC_DIR="${CMAKE_SOURCE_DIR}/../Config"
      -D CONFIG_DST_DIR="$<TARGET_FILE_DIR:${BUILD_NAME}>/Config"
      -D PLATFORM="${HYP_PLATFORM_NAME}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/../CopyPlatformConfigs.cmake"
  )
endif()

if(HYP_PLATFORM_NAME STREQUAL "Windows")
  add_custom_command(TARGET ${BUILD_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_newer
        "${CMAKE_CURRENT_SOURCE_DIR}/steam_appid.txt"
        "$<TARGET_FILE_DIR:${BUILD_NAME}>")
endif()

# link with shared library hyperion
add_dependencies(${BUILD_NAME} hyperion hyperion-core)
target_link_libraries(${BUILD_NAME} hyperion hyperion-core)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
