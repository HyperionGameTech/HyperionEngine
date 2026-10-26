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
