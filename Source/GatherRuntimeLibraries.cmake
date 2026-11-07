if(POLICY CMP0207)
  cmake_policy(SET CMP0207 NEW)
endif()

get_filename_component(runtime_dir "${LIBRARY}" DIRECTORY)
get_filename_component(library_name "${LIBRARY}" NAME)

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "${PLATFORM}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "${TOOL}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${TOOL_COMMAND}")

file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${LIBRARY}"
    DIRECTORIES "${runtime_dir}"
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
    CONFLICTING_DEPENDENCIES_PREFIX conflicting_dependencies)

set(runtime_libraries "${library_name}")

# a dependency ships with the game when the build output has its own copy, wherever the linker resolved it from
foreach(dependency ${resolved_dependencies})
  get_filename_component(dependency_name "${dependency}" NAME)

  if(EXISTS "${runtime_dir}/${dependency_name}")
    list(APPEND runtime_libraries "${dependency_name}")
  endif()
endforeach()

foreach(conflict ${conflicting_dependencies_FILENAMES})
  if(EXISTS "${runtime_dir}/${conflict}")
    list(APPEND runtime_libraries "${conflict}")
  endif()
endforeach()

if(EXCLUDE)
  list(REMOVE_ITEM runtime_libraries ${EXCLUDE})
endif()

list(REMOVE_DUPLICATES runtime_libraries)
list(SORT runtime_libraries CASE INSENSITIVE)
list(JOIN runtime_libraries "\n" runtime_libraries_text)

file(WRITE "${OUTPUT}" "${runtime_libraries_text}\n")
