cmake_minimum_required(VERSION 3.22)
project(hyperion-android)

# TODO Check this over, for mobile we want static
add_library(hyperion-android SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/cpp/hyperion_jni.cpp
)

message(STATUS "ANDROID_NDK_SYSROOT = ${ANDROID_NDK_SYSROOT}")
if(ANDROID_NDK_SYSROOT AND EXISTS "${ANDROID_NDK_SYSROOT}/usr/include/jni.h")
    message(STATUS "Android JNI include dir: ${ANDROID_NDK_SYSROOT}/usr/include")
    target_include_directories(hyperion-android PRIVATE "${ANDROID_NDK_SYSROOT}/usr/include")
elseif(CMAKE_SYSROOT AND EXISTS "${CMAKE_SYSROOT}/usr/include/jni.h")
    message(STATUS "Android JNI include dir (sysroot): ${CMAKE_SYSROOT}/usr/include")
    target_include_directories(hyperion-android PRIVATE "${CMAKE_SYSROOT}/usr/include")
else()
    message(WARNING "Could not locate jni.h -- pass -DANDROID_NDK_SYSROOT=<ndk>/toolchains/llvm/prebuilt/<host>/sysroot "
                    "or ensure CMAKE_SYSROOT is set. ANDROID_NDK_SYSROOT=${ANDROID_NDK_SYSROOT}  CMAKE_SYSROOT=${CMAKE_SYSROOT}")
endif()

# TODO Check this over, for mobile we want static
add_library(hyperion SHARED IMPORTED)

set_target_properties(hyperion PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/jniLibs/${CMAKE_ANDROID_ARCH_ABI}/libhyperion.so"
)

add_dependencies(hyperion-android hyperion)
target_link_libraries(hyperion-android
    PRIVATE
    hyperion
    log
    android
)

# ---------------------------------------------------------------------------
# IDE-only target: makes Android Studio show and index the engine source tree.
# Nothing here is compiled – libhyperion.so is pre-built separately.
# ---------------------------------------------------------------------------
set(HYP_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

file(GLOB_RECURSE HYP_ENGINE_IDE_SOURCES
    "${HYP_ENGINE_ROOT}/Source/Engine/*.cpp"
    "${HYP_ENGINE_ROOT}/Source/Engine/*.hpp"
    "${HYP_ENGINE_ROOT}/Source/Core/*.cpp"
    "${HYP_ENGINE_ROOT}/Source/Core/*.hpp"
)

add_custom_target(hyperion-sources SOURCES ${HYP_ENGINE_IDE_SOURCES})

# Teach clangd / the IDE about include paths and compile flags for those files.
# We attach them to the JNI target (which IS compiled) so Android Studio's
# CMake model inherits the same flags when you open an engine header.
target_include_directories(hyperion-android PRIVATE
    "${HYP_ENGINE_ROOT}/Source/Engine"
    "${HYP_ENGINE_ROOT}/Source/Core"
    "${HYP_ENGINE_ROOT}/Source"
)

target_compile_definitions(hyperion-android PRIVATE
    HYP_ANDROID=1
    HYP_UNIX=1
    HYP_VULKAN=1
    HYPERION_ENGINE=1
)
