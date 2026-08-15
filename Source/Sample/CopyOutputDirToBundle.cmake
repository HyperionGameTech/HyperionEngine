if(NOT DEFINED SRC_DIR OR NOT DEFINED DST_DIR)
    message(FATAL_ERROR "SRC_DIR and DST_DIR must be defined")
endif()

file(MAKE_DIRECTORY "${DST_DIR}")
get_filename_component(DST_DIR_REAL "${DST_DIR}" REALPATH)

file(GLOB ITEMS "${SRC_DIR}/*")
foreach(ITEM ${ITEMS})
    get_filename_component(ITEM_REAL "${ITEM}" REALPATH)
    get_filename_component(FILENAME "${ITEM}" NAME)

    if(ITEM_REAL STREQUAL DST_DIR_REAL OR FILENAME MATCHES "\\.app$")
        continue()
    endif()

    if(FILENAME MATCHES "^\\.")
        continue()
    endif()

    if(IS_DIRECTORY "${ITEM}")
        file(REMOVE_RECURSE "${DST_DIR}/${FILENAME}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${ITEM}" "${DST_DIR}/${FILENAME}")
    else()
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ITEM}" "${DST_DIR}/${FILENAME}")
    endif()
endforeach()
