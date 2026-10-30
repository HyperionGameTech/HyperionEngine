# manifold powers mesh booleans (MeshBoolean)
set(MANIFOLD_DIR "${plugin_external_dir}/manifold")

if(EXISTS "${MANIFOLD_DIR}/include/manifold/manifold.h")
  message(STATUS "manifold found, enabling mesh booleans")

  set(MANIFOLD_SOURCES
    "${MANIFOLD_DIR}/src/boolean3.cpp"
    "${MANIFOLD_DIR}/src/boolean_result.cpp"
    "${MANIFOLD_DIR}/src/constructors.cpp"
    "${MANIFOLD_DIR}/src/csg_tree.cpp"
    "${MANIFOLD_DIR}/src/edge_op.cpp"
    "${MANIFOLD_DIR}/src/execution_impl.cpp"
    "${MANIFOLD_DIR}/src/face_op.cpp"
    "${MANIFOLD_DIR}/src/impl.cpp"
    "${MANIFOLD_DIR}/src/manifold.cpp"
    "${MANIFOLD_DIR}/src/minkowski.cpp"
    "${MANIFOLD_DIR}/src/polygon.cpp"
    "${MANIFOLD_DIR}/src/properties.cpp"
    "${MANIFOLD_DIR}/src/quickhull.cpp"
    "${MANIFOLD_DIR}/src/sdf.cpp"
    "${MANIFOLD_DIR}/src/smoothing.cpp"
    "${MANIFOLD_DIR}/src/sort.cpp"
    "${MANIFOLD_DIR}/src/subdivision.cpp"
    "${MANIFOLD_DIR}/src/tree2d.cpp"
  )

  # manifold's robust predicates rely on strict IEEE semantics
  if(MSVC)
    set(MANIFOLD_COMPILE_OPTIONS "/fp:precise")
  else()
    set(MANIFOLD_COMPILE_OPTIONS "-fno-fast-math")
  endif()

  # manifold's CsgNode trips the engine's -Werror=non-virtual-dtor
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND MANIFOLD_COMPILE_OPTIONS "-Wno-non-virtual-dtor")
  endif()

  set_source_files_properties(${MANIFOLD_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_OPTIONS "${MANIFOLD_COMPILE_OPTIONS}")

  source_group(TREE "${MANIFOLD_DIR}/src" PREFIX "Third-Party Code" FILES ${MANIFOLD_SOURCES})

  target_sources(${plugin_target} PRIVATE ${MANIFOLD_SOURCES})
  target_compile_definitions(${plugin_target} PRIVATE HYP_MANIFOLD=1 MANIFOLD_PAR=-1)
  target_include_directories(${plugin_target} SYSTEM PRIVATE "${MANIFOLD_DIR}/include")
endif()
