cmake_minimum_required(VERSION 3.10)

function(add_commandlet_target COMMANDLET_NAME)
  set(TARGET_NAME "hyperion-commandlet-${COMMANDLET_NAME}")

  set(COMMANDLET_MAIN_CPP "${CMAKE_SOURCE_DIR}/Engine/Framework/Commandlet/CommandletMain.cpp")

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${COMMANDLET_NAME}.cpp")
    set(COMMANDLET_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${COMMANDLET_NAME}.cpp")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/Engine/Framework/Commandlet/Commandlets/${COMMANDLET_NAME}.cpp")
    set(COMMANDLET_SOURCE "${CMAKE_SOURCE_DIR}/Engine/Framework/Commandlet/Commandlets/${COMMANDLET_NAME}.cpp")
  else()
    set(COMMANDLET_SOURCE "")
  endif()

  if(HYP_USE_OBJECT_LIBRARIES)
    set(OBJECTS_TARGET_NAME "${TARGET_NAME}-objects")

    add_library(${OBJECTS_TARGET_NAME} OBJECT ${COMMANDLET_MAIN_CPP} ${COMMANDLET_SOURCE})
    target_link_libraries(${OBJECTS_TARGET_NAME} PRIVATE $<COMPILE_ONLY:hyperion> $<COMPILE_ONLY:hyperion-core>)

    add_executable(${TARGET_NAME} $<TARGET_OBJECTS:${OBJECTS_TARGET_NAME}>)
  else()
    set(OBJECTS_TARGET_NAME "${TARGET_NAME}")

    add_executable(${TARGET_NAME} ${COMMANDLET_MAIN_CPP} ${COMMANDLET_SOURCE})
  endif()

  target_compile_definitions(${OBJECTS_TARGET_NAME} PRIVATE
        HYP_COMMANDLET_NAME=${COMMANDLET_NAME}
        HYP_COMMANDLET=1
    )

  target_include_directories(${OBJECTS_TARGET_NAME} PRIVATE
        ${HYP_GENERATED_DIR}
        ${CMAKE_SOURCE_DIR}/../
        ${CMAKE_SOURCE_DIR}/../Engine
    )

  if(NOT HYP_ANDROID AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
    target_precompile_headers(${OBJECTS_TARGET_NAME} PRIVATE ${HYP_PCH})
  endif()

  set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${COMMANDLET_NAME}")

  add_dependencies(${TARGET_NAME} hyperion hyperion-core)
  target_link_libraries(${TARGET_NAME} hyperion hyperion-core)

  if(MSVC)
    set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_FLAGS /DEBUG)
  endif()

  if(CMAKE_GENERATOR MATCHES "Xcode")
    set_target_properties(${TARGET_NAME} PROPERTIES
            XCODE_GENERATE_SCHEME ON
            XCODE_SCHEME_ENABLE_GPU_API_VALIDATION OFF
            XCODE_SCHEME_LAUNCH_CONFIGURATION_NAME "${HYP_BUILD_TYPE}"
            XCODE_SCHEME_HIDE_BUILDABLE_NAME OFF
        )
  endif()

  message(STATUS "Registered commandlet target ${COMMANDLET_NAME}")
endfunction()

# ----- Commandlets to build as separate EXEs -----
add_commandlet_target(PrecompileShaders)
# add_commandlet_target(BuildGizmosCommandlet)
add_commandlet_target(GenerateBlueNoiseBlob)
add_commandlet_target(BuildShapesCommandlet)
add_commandlet_target(BlobStorageCookCommandlet)
add_commandlet_target(CacheServerCommandlet)
add_commandlet_target(CookTexturesCommandlet)
add_commandlet_target(BuildEditorIconsCommandlet)
# add_commandlet_target(LogMemoryPools)
