# Windows half of HyperionSdk.cmake, copied beside it as HyperionSdkPlatform.cmake

set(HYPERION_SDK_RUNTIME_LIBRARY_PATTERN "*.dll")

if(HYPERION_SDK_SHIPPING)
  # the static VC runtime with the dynamic UCRT (part of Windows 10 and later), which the shipping engine is built on.
  # NativeAOT links its runtime the same way, so a game's scripts can go into the same executable.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

  if(MSVC)
    list(APPEND HYPERION_SDK_LINK_OPTIONS /NODEFAULTLIB:libucrt.lib /DEFAULTLIB:ucrt.lib)
  endif()
else()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

function(hyperion_sdk_packaged_libraries sdk_root runtime_dir out_var)
  set(${out_var} "${sdk_root}/Lib/hyperion.lib" PARENT_SCOPE)
endfunction()

function(hyperion_sdk_dependency_tool out_platform out_tool out_command)
  get_filename_component(linker_dir "${CMAKE_LINKER}" DIRECTORY)
  find_program(HYPERION_SDK_DUMPBIN dumpbin HINTS "${linker_dir}")

  set(${out_platform} "windows+pe" PARENT_SCOPE)

  if(HYPERION_SDK_DUMPBIN)
    set(${out_tool} "dumpbin" PARENT_SCOPE)
    set(${out_command} "${HYPERION_SDK_DUMPBIN}" PARENT_SCOPE)
  elseif(CMAKE_OBJDUMP)
    set(${out_tool} "objdump" PARENT_SCOPE)
    set(${out_command} "${CMAKE_OBJDUMP}" PARENT_SCOPE)
  else()
    set(${out_tool} "" PARENT_SCOPE)
  endif()
endfunction()

# the NativeAOT runtime is built against the static VC runtime with the dynamic UCRT, so the executable linking it is as well.
function(hyperion_platform_link_scripts target link_inputs)
  file(TO_CMAKE_PATH "${link_inputs}" link_inputs)

  set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
  target_link_libraries(${target} PRIVATE ${link_inputs})
  target_link_options(${target} PRIVATE /NODEFAULTLIB:libucrt.lib /DEFAULTLIB:ucrt.lib)
endfunction()
