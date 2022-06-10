cmake_minimum_required(VERSION 3.5)

file (GLOB_RECURSE ace-vm_SOURCES "*.cpp")
file (GLOB_RECURSE ace-vm_HEADERS "../../include/script/vm/*.hpp")

foreach(f IN LISTS ace-vm_SOURCES)
  get_filename_component(b ${f} NAME)
  set_source_files_properties(${f} PROPERTIES
    COMPILE_DEFINITIONS "__FILENAME__=${b}")
endforeach()
foreach(f IN LISTS ace-vm_HEADERS)
  get_filename_component(b ${f} NAME)
  set_source_files_properties(${f} PROPERTIES
    COMPILE_DEFINITIONS "__FILENAME__=${b}")
endforeach()

add_library(ace-vm STATIC ${ace-vm_SOURCES} ${ace-vm_HEADERS})
