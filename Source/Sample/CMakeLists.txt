cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

project(hyperion-sample CXX C)

# Each sample subdirectory with a CMakeLists.txt defines its own target.

# this is temporarily disabled as it has rotted a bit; we should change this to
# not rely on the CacheServerCommandlet and instead just require a command to be run
# to build out the cache data for the game ahead of time.
# add_subdirectory(DefaultGame)

if(HYP_DOTNET AND NOT HYP_ANDROID AND NOT HYP_IOS AND NOT HYP_WEB)
  add_subdirectory(CSharpGame)
endif()

# a C++ game project's Source folder, built into libhyperion-game.so that the Android app loads beside the engine
set(HYP_ANDROID_GAME_DIR "" CACHE PATH "Source folder of a C++ game project to build for Android")

if(HYP_ANDROID AND HYP_ANDROID_GAME_DIR)
  file(GLOB_RECURSE HYP_ANDROID_GAME_SOURCES CONFIGURE_DEPENDS "${HYP_ANDROID_GAME_DIR}/Game/*.cpp")

  add_library(hyperion-game SHARED ${HYP_ANDROID_GAME_SOURCES})
  target_include_directories(hyperion-game PRIVATE
      "${HYP_ANDROID_GAME_DIR}"
      ${HYP_GENERATED_DIR}
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/Engine)
  target_include_directories(hyperion-game SYSTEM PRIVATE ${THIRD_PARTY_INCLUDE_DIR})
  target_link_libraries(hyperion-game PRIVATE hyperion hyperion-core)

  # games are written against the SDK, which always gives them the engine's precompiled header
  target_precompile_headers(hyperion-game PRIVATE "${CMAKE_SOURCE_DIR}/Engine/HyperionPch.hpp")
endif()
