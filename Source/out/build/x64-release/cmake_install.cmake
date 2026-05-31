# Install script for directory: C:/Users/andre/hyperion-engine/Source

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/andre/hyperion-engine/Source/out/install/x64-release")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/ThirdParty/D3D12MemoryAllocator_build/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/Core/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/Lang/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/asset/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/audio/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/baking/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/interop/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/runtime/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/scripting/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/shared/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/editor/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/input/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/rendering/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/scene/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/system/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/ui/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/Sample/cmake_install.cmake")
  include("C:/Users/andre/hyperion-engine/Source/out/build/x64-release/Commandlets/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/andre/hyperion-engine/Source/out/build/x64-release/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/andre/hyperion-engine/Source/out/build/x64-release/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
