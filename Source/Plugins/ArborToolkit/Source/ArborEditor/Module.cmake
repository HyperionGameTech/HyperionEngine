# arbor_ffi comes from the arbor repo.

set(ARBOR_DROP_DIR "${plugin_external_dir}/arbor")
set(ARBOR_DROP_TAG "hyperion-drop-1")
set(ARBOR_DROP_SHA256 "4498d8cdcffba71241d6504eae3e1c2088c3e10a")

if(WIN32 AND NOT HYP_WEB AND NOT HYP_ANDROID AND NOT EXISTS "${ARBOR_DROP_DIR}/lib/arbor_ffi.lib" AND ARBOR_DROP_TAG)
  set(ARBOR_DROP_ZIP "${CMAKE_BINARY_DIR}/arbor-hyperion-${ARBOR_DROP_TAG}.zip")

  message(STATUS "Downloading arbor_ffi ${ARBOR_DROP_TAG}")

  file(DOWNLOAD
    "https://github.com/ajmd17/arbor/releases/download/${ARBOR_DROP_TAG}/arbor-hyperion-${ARBOR_DROP_TAG}.zip"
    "${ARBOR_DROP_ZIP}"
    EXPECTED_HASH SHA256=${ARBOR_DROP_SHA256}
    STATUS ARBOR_DROP_STATUS
    TLS_VERIFY ON)

  list(GET ARBOR_DROP_STATUS 0 ARBOR_DROP_STATUS_CODE)

  if(ARBOR_DROP_STATUS_CODE EQUAL 0)
    file(ARCHIVE_EXTRACT INPUT "${ARBOR_DROP_ZIP}" DESTINATION "${ARBOR_DROP_DIR}")
  else()
    message(WARNING "ArborEditor: could not download arbor_ffi ${ARBOR_DROP_TAG}: ${ARBOR_DROP_STATUS}")
  endif()
endif()

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
  target_compile_definitions(${plugin_target} PRIVATE HYP_ARBOR=1)
elseif(WIN32 AND NOT HYP_WEB AND NOT HYP_ANDROID AND NOT HYP_BAKE_ROOT_DIR)
  message(FATAL_ERROR "ArborEditor: no arbor_ffi in ${ARBOR_DROP_DIR}, and none downloaded. Set ARBOR_DROP_TAG and ARBOR_DROP_SHA256, or run tools/hyperion-drop.sh from the arbor repo.")
else()
  message(WARNING "ArborEditor: no arbor_ffi in ${ARBOR_DROP_DIR}. Run tools/hyperion-drop.sh <HyperionEngine dir> from the arbor repo; until then Arbor plants cannot be grown.")
endif()
