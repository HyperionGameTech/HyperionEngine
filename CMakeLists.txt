cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

set(vcpkg_path "$ENV{VCPKG_INSTALLED_DIR}/../scripts/buildsystems/vcpkg.cmake")
#set(CMAKE_TOOLCHAIN_FILE ${vcpkg_path} CACHE STRING "CMake toolchain file")
#set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
#set(VCPKG_LIBRARY_LINKAGE dynamic)
#set(VCPKG_BUILD_TYPE release)
project(${BUILD_NAME} CXX C)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    message(STATUS "Building on MSVC, checking toolchain is set (should be set to `<vcpkg root path>/scripts/buildsystems/vcpkg.cmake`)")

    if (DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using CMake toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(SEND_ERROR "CMake toolchain file not specified -- should be set to vcpkg cmake file! \
            If you don't have vcpkg set up, download vcpkg from Microsoft and set the `-DCMAKE_TOOLCHAIN_FILE` CMake argument to \
            `<vcpkg root path>/scripts/buildsystems/vcpkg.cmake`.")
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MD")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address,undefined")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib ${VCPKG_INSTALLED_DIR}/x64-windows/lib "Z:/gstreamer/1.0/msvc_x86_64/lib")

find_package(PkgConfig)
pkg_check_modules(GST gstreamer-1.0>=1.4
                      gstreamer-sdp-1.0>=1.4
                      gstreamer-video-1.0>=1.4
                      gstreamer-app-1.0>=1.4)

find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenAL)

if(${OpenAL_FOUND})
    add_definitions(-DHYP_OPENAL=1)
endif()

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(Bullet_FOUND)
    add_definitions(-DHYP_BULLET=1)
endif()

# if glslang folder exists, link with it.
if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/glslang/glslang/CMakeLists.txt")
    add_definitions(-DHYP_GLSLANG=1)

    include_directories(BEFORE submodules/glslang)
    add_subdirectory(submodules/glslang)

    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        glslang
        SPIRV
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/zlib/CMakeLists.txt")
    add_definitions(-DHYP_ZLIB=1)

    include_directories(submodules/zlib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/zlib")
    add_subdirectory(submodules/zlib)
    
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        zlibstatic
    )
endif()


if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/libdatachannel/CMakeLists.txt")
    add_definitions(-DHYP_LIBDATACHANNEL=1)

    include_directories(submodules/libdatachannel PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libdatachannel/include")
    add_subdirectory(submodules/libdatachannel)
    
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        datachannel-static
    )
endif()

if(GST_FOUND)
    add_definitions(-DHYP_GSTREAMER=1)
    add_definitions(-DHYP_GSTREAMER_BIN_DIR="Z:/gstreamer/1.0/msvc_x86_64/bin")

    include_directories(${GST_INCLUDE_DIRS})

    # target_link_libraries(${BUILD_NAME} PRIVATE ${GST_LIBRARIES})

    
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${GST_LIBRARIES}
    )
endif()

# if aftermath lib exists on windows, link to it
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/windows/64/GFSDK_Aftermath_Lib.x64.lib")
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} windows/64/GFSDK_Aftermath_Lib.x64)
        add_definitions(-DHYP_AFTERMATH=1)
    endif()
endif()

set(HYPERION_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

include_directories(
    ${HYPERION_INCLUDE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${CMAKE_THREADS_LIBS_INIT}
    ${OPENAL_LIBRARY}
    ${Vulkan_LIBRARIES}
    ${SDL2_LIBRARIES}
)


target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

add_subdirectory(samples)