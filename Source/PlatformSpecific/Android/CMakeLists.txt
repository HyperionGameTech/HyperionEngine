cmake_minimum_required(VERSION 3.22)
project(hyperion-android)

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

add_library(hyperion SHARED IMPORTED)
set_target_properties(hyperion PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/jniLibs/${CMAKE_ANDROID_ARCH_ABI}/libhyperion.so"
)

target_link_libraries(hyperion-android
    PRIVATE
    hyperion
    log
    android
)
