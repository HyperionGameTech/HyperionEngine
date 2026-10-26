cmake_minimum_required(VERSION 3.20)

project(HyperionWebPage CXX C)

set(HYP_WEB_LINK_OPTIMIZATION "-Oz" CACHE STRING "Optimization level")
option(HYP_WEB_FUNCTION_NAMES "Keep function names in wasm" OFF)

foreach(requiredVariable HYPERION_WEB_SDK HYP_WEB_PAGE_NAME HYP_WEB_PACKAGE HYP_WEB_OUTPUT)
  if(NOT ${requiredVariable})
    message(FATAL_ERROR "${requiredVariable} is not set")
  endif()
endforeach()

if(NOT HYP_WEB_PAGE_TITLE)
  set(HYP_WEB_PAGE_TITLE "${HYP_WEB_PAGE_NAME}")
endif()

include("${HYPERION_WEB_SDK}")

set(CMAKE_CXX_STANDARD 20)
string(APPEND CMAKE_CXX_FLAGS " ${HYPERION_WEB_CXX_FLAGS}")

set(PAGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

if(HYP_WEB_GAME_DIR)
  file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${HYP_WEB_GAME_DIR}/Game/*.cpp")
  list(APPEND PAGE_SOURCES ${GAME_SOURCES})

  # BuildWebGame.ps1 runs CodeGen over the game's headers first
  if(HYP_WEB_GAME_GENERATED_DIR)
    list(APPEND PAGE_SOURCES "${HYP_WEB_GAME_GENERATED_DIR}/GeneratedSources.cpp")
  endif()
endif()

# the registration object of each of the project's enabled built-in plugin modules (BuildWebGame.ps1 -PluginModules), which pulls the module in
foreach(pluginModule ${HYP_WEB_PLUGIN_MODULES})
  set(pluginModuleObject "")

  foreach(entry ${HYPERION_WEB_PLUGIN_MODULE_OBJECTS})
    if(entry MATCHES "^${pluginModule}=(.+)$")
      set(pluginModuleObject "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  if(pluginModuleObject)
    list(APPEND PAGE_SOURCES "${pluginModuleObject}")
  else()
    message(WARNING "The engine's web build has no plugin module ${pluginModule}, so the page is linked without it")
  endif()
endforeach()

add_executable(${HYP_WEB_PAGE_NAME} ${PAGE_SOURCES})

target_include_directories(${HYP_WEB_PAGE_NAME} PRIVATE ${HYPERION_WEB_INCLUDE_DIRS})
target_compile_definitions(${HYP_WEB_PAGE_NAME} PRIVATE ${HYPERION_WEB_COMPILE_DEFINITIONS})
target_link_directories(${HYP_WEB_PAGE_NAME} PRIVATE ${HYPERION_WEB_LINK_DIRECTORIES})
target_link_libraries(${HYP_WEB_PAGE_NAME} PRIVATE ${HYPERION_WEB_LIBRARIES})

if(HYP_WEB_GAME_DIR)
  if(HYP_WEB_GAME_GENERATED_DIR)
    target_include_directories(${HYP_WEB_PAGE_NAME} BEFORE PRIVATE "${HYP_WEB_GAME_GENERATED_DIR}")
  endif()

  target_include_directories(${HYP_WEB_PAGE_NAME} PRIVATE "${HYP_WEB_GAME_DIR}")
  target_compile_definitions(${HYP_WEB_PAGE_NAME} PRIVATE HYP_WEB_GAME_MODULE=1)
elseif(HYP_WEB_GAME_CLASS)
  target_compile_definitions(${HYP_WEB_PAGE_NAME} PRIVATE "HYP_WEB_GAME_CLASS=\"${HYP_WEB_GAME_CLASS}\"")
else()
  # a project without game code runs the engine's own game class
  target_compile_definitions(${HYP_WEB_PAGE_NAME} PRIVATE [==[HYP_WEB_GAME_CLASS="Game"]==])
endif()

target_link_options(${HYP_WEB_PAGE_NAME} PRIVATE
  ${HYPERION_WEB_LINK_OPTIONS}
  ${HYP_WEB_LINK_OPTIMIZATION}
  -g0
  $<IF:$<BOOL:${HYP_WEB_FUNCTION_NAMES}>,--profiling-funcs,--emit-symbol-map>
  -sJSPI=1
  -sOFFSCREENCANVAS_SUPPORT=1
  "--preload-file=${HYP_WEB_PACKAGE}/Config@/hyperion/Config"
  "--preload-file=${HYP_WEB_PACKAGE}/Content@/hyperion/Content")

if(HYP_WEB_MANAGED_LINK_INPUTS)
  file(STRINGS "${HYP_WEB_MANAGED_LINK_INPUTS}" managedLinkInputs)

  target_compile_definitions(${HYP_WEB_PAGE_NAME} PRIVATE HYP_WEB_MANAGED=1)
  target_link_libraries(${HYP_WEB_PAGE_NAME} PRIVATE ${managedLinkInputs})

  target_link_options(${HYP_WEB_PAGE_NAME} PRIVATE
    -Wl,--no-check-features
    -sERROR_ON_UNDEFINED_SYMBOLS=0
    -sEXPORTED_RUNTIME_METHODS=ccall,HEAP8,HEAP16,HEAP32,HEAPU8,HEAPU16,HEAPU32)

  set_property(TARGET ${HYP_WEB_PAGE_NAME} APPEND PROPERTY LINK_DEPENDS ${managedLinkInputs})
endif()

set_target_properties(${HYP_WEB_PAGE_NAME} PROPERTIES
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${HYP_WEB_OUTPUT}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/index.html.in" "${HYP_WEB_OUTPUT}/index.html" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/isolation-worker.js" "${HYP_WEB_OUTPUT}/isolation-worker.js" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/brotli_dec_wasm.js" "${HYP_WEB_OUTPUT}/brotli_dec_wasm.js" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/brotli_dec_wasm_bg.wasm" "${HYP_WEB_OUTPUT}/brotli_dec_wasm_bg.wasm" COPYONLY)
