# Copied into Sdk/CMake as HyperionCopyRuntimeDependencies.cmake, run with -P after a game links against a shipping SDK.
# The engine is linked into the executable, so only the libraries it imports from the SDK's runtime folder are copied beside it
# (eg. a vcpkg library built as a DLL); system libraries and anything outside that folder are left alone.
#   EXECUTABLE - the linked game
#   RUNTIME_DIR - where the SDK keeps those libraries
#   PLATFORM, TOOL, TOOL_COMMAND - for file(GET_RUNTIME_DEPENDENCIES)

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "${PLATFORM}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "${TOOL}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${TOOL_COMMAND}")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXECUTABLE}"
    DIRECTORIES "${RUNTIME_DIR}"
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
    CONFLICTING_DEPENDENCIES_PREFIX conflicting_dependencies)

get_filename_component(output_dir "${EXECUTABLE}" DIRECTORY)

set(dependency_names "")

foreach(dependency ${resolved_dependencies})
  get_filename_component(dependency_name "${dependency}" NAME)
  list(APPEND dependency_names "${dependency_name}")
endforeach()

list(APPEND dependency_names ${conflicting_dependencies_FILENAMES})

list(REMOVE_DUPLICATES dependency_names)

foreach(dependency_name ${dependency_names})
  # a dependency ships with the game when the SDK has its own copy, wherever it was resolved from
  if(EXISTS "${RUNTIME_DIR}/${dependency_name}")
    file(COPY_FILE "${RUNTIME_DIR}/${dependency_name}" "${output_dir}/${dependency_name}" ONLY_IF_DIFFERENT)
  endif()
endforeach()

# macOS has no system Vulkan loader 
# take it from the Vulkan install the engine was built against
if(PLATFORM MATCHES "^macos")
  set(vulkan_loader_directories "${RUNTIME_DIR}")

  if(DEFINED ENV{VULKAN_SDK})
    list(APPEND vulkan_loader_directories "$ENV{VULKAN_SDK}/lib")
  endif()

  list(APPEND vulkan_loader_directories /usr/local/lib /opt/homebrew/lib)

  foreach(dependency ${resolved_dependencies} ${unresolved_dependencies})
    get_filename_component(dependency_name "${dependency}" NAME)

    if(NOT dependency_name MATCHES "^libvulkan.*\\.dylib$" OR EXISTS "${output_dir}/${dependency_name}")
      continue()
    endif()

    foreach(directory ${vulkan_loader_directories})
      if(EXISTS "${directory}/${dependency_name}")
        file(REAL_PATH "${directory}/${dependency_name}" vulkan_loader)
        file(COPY_FILE "${vulkan_loader}" "${output_dir}/${dependency_name}" ONLY_IF_DIFFERENT)
        break()
      endif()
    endforeach()
  endforeach()
endif()
