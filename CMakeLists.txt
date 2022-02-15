cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

project(${BUILD_NAME} CXX C)

if(MSVC)
# set to path of vcpkg
set(CMAKE_TOOLCHAIN_FILE, "C:\\Users\\andre\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake")
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Bullet REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\" -g -O2 -fpermissive")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp")

set(CMAKE_CXX_STANDARD 17)
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${BULLET_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)
add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS} src/system/debug.cpp src/system/debug.h)


set(COMMON_LIBRARIES
    ${CMAKE_THREADS_LIBS_INIT}
    ${OPENAL_LIBRARY}
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${Vulkan_LIBRARIES}
    ${BULLET_LIBRARIES}
    glfw
    SDL2
)

if(APPLE)
  target_link_libraries(${BUILD_NAME} ${COMMON_LIBRARIES} glfw)
elseif(MSVC)
  target_link_libraries(${BUILD_NAME} ${COMMON_LIBRARIES} glfw)
else()
  target_link_libraries(${BUILD_NAME} ${COMMON_LIBRARIES} glfw)
endif()
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
