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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -fpermissive")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp")

set(CMAKE_CXX_STANDARD 17)
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${BULLET_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
add_executable(${BUILD_NAME} ${SOURCES} ${HEADERS})

# if(APPLE)
  # target_link_libraries(${BUILD_NAME} ${CMAKE_THREAD_LIBS_INIT} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} glfw m)
# else()
  target_link_libraries(${BUILD_NAME} ${CMAKE_THREAD_LIBS_INIT} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} ${BULLET_LIBRARIES} GLEW glfw)
# endif()
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
