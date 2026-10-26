# Linux half of HyperionSdk.cmake, copied beside it as HyperionSdkPlatform.cmake

set(HYPERION_SDK_RUNTIME_LIBRARY_PATTERN "*.so*")

# the engine libraries are copied beside the game, so it finds them there wherever it's moved to
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

function(hyperion_sdk_packaged_libraries sdk_root runtime_dir out_var)
  file(GLOB libraries "${runtime_dir}/libhyperion.so" "${runtime_dir}/libhyperion-core.so")
  set(${out_var} ${libraries} PARENT_SCOPE)
endfunction()

function(hyperion_sdk_dependency_tool out_platform out_tool out_command)
  set(${out_platform} "linux+elf" PARENT_SCOPE)

  if(CMAKE_OBJDUMP)
    set(${out_tool} "objdump" PARENT_SCOPE)
    set(${out_command} "${CMAKE_OBJDUMP}" PARENT_SCOPE)
  else()
    set(${out_tool} "" PARENT_SCOPE)
  endif()
endfunction()

# NativeAOT's runtime archives reference each other, so they're resolved as one group
function(hyperion_platform_link_scripts target link_inputs)
  find_package(Threads REQUIRED)

  target_link_libraries(${target} PRIVATE -Wl,--start-group ${link_inputs} -Wl,--end-group Threads::Threads)
endfunction()
