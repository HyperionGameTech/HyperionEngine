# arbor_ffi is built by the arbor repo's tools/hyperion-drop.sh, which drops the static library,
# its header and the texture sets the built-in species use into External/arbor
set(ARBOR_DROP_DIR "${plugin_external_dir}/arbor")

if(WIN32 AND NOT HYP_WEB AND NOT HYP_ANDROID AND EXISTS "${ARBOR_DROP_DIR}/lib/arbor_ffi.lib")
  message(STATUS "arbor_ffi found, enabling Arbor plant generation")

  if(NOT TARGET arbor_ffi)
    file(STRINGS "${ARBOR_DROP_DIR}/native-libs.txt" ARBOR_NATIVE_LIBS)

    add_library(arbor_ffi STATIC IMPORTED GLOBAL)
    set_target_properties(arbor_ffi PROPERTIES
      IMPORTED_LOCATION "${ARBOR_DROP_DIR}/lib/arbor_ffi.lib"
      INTERFACE_INCLUDE_DIRECTORIES "${ARBOR_DROP_DIR}/include"
      INTERFACE_LINK_LIBRARIES "${ARBOR_NATIVE_LIBS}")
  endif()

  target_link_libraries(${plugin_target} PRIVATE arbor_ffi)
  target_compile_definitions(${plugin_target} PRIVATE
    HYP_ARBOR=1
    ARBOR_TEXTURE_DIR="${ARBOR_DROP_DIR}/textures")
else()
  message(WARNING "ArborEditor: no arbor_ffi in ${ARBOR_DROP_DIR}. Run tools/hyperion-drop.sh <HyperionEngine dir> from the arbor repo; until then Arbor plants cannot be grown.")
endif()
