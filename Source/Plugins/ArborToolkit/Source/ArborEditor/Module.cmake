

set(ARBOR_DIR "${plugin_external_dir}/arbor")

set(ARBOR_SUPPORTED FALSE)

if(NOT HYP_WEB AND NOT HYP_ANDROID AND NOT HYP_IOS)
  set(ARBOR_SUPPORTED TRUE)
endif()

set(ARBOR_PROBLEM "")

if(ARBOR_SUPPORTED AND NOT EXISTS "${ARBOR_DIR}/crates/arbor-ffi/Cargo.toml")
  set(ARBOR_PROBLEM "the arbor submodule is missing. Run: git submodule update --init Source/Plugins/ArborToolkit/External/arbor")
endif()

if(ARBOR_SUPPORTED AND NOT ARBOR_PROBLEM AND NOT COMMAND corrosion_import_crate)
  if(NOT EXISTS "${SUBMODULES_DIR}/corrosion/CMakeLists.txt")
    set(ARBOR_PROBLEM "the corrosion submodule is missing. Run: git submodule update --init External/Submodules/corrosion")
  else()
    # the engine builds against the MSVC ABI on Windows, so a GNU toolchain (rustup's default on some machines, and what
    # Corrosion caches when it's the default) can't build for it: use an MSVC toolchain beside it, or add the MSVC target
    if(WIN32 AND (NOT DEFINED Rust_TOOLCHAIN OR Rust_TOOLCHAIN MATCHES "-windows-gnu$"))
      execute_process(COMMAND rustup toolchain list OUTPUT_VARIABLE ARBOR_RUST_TOOLCHAINS ERROR_QUIET RESULT_VARIABLE ARBOR_RUSTUP_RESULT)

      if(ARBOR_RUSTUP_RESULT EQUAL 0 AND ARBOR_RUST_TOOLCHAINS MATCHES "(^|\n)(stable-x86_64-pc-windows-msvc)")
        set(Rust_TOOLCHAIN "${CMAKE_MATCH_2}" CACHE STRING "The rustup toolchain to use" FORCE)
      else()
        set(Rust_RUSTUP_INSTALL_MISSING_TARGET ON CACHE BOOL "Install the Rust target the build needs when rustup lacks it" FORCE)
      endif()
    endif()

    list(APPEND CMAKE_MODULE_PATH "${SUBMODULES_DIR}/corrosion/cmake")
    find_package(Rust QUIET)

    if(Rust_FOUND)
      add_subdirectory("${SUBMODULES_DIR}/corrosion" "${CMAKE_BINARY_DIR}/corrosion")
    else()
      set(ARBOR_PROBLEM "no Rust toolchain found. Install it from https://rustup.rs and reconfigure")
    endif()
  endif()
endif()

if(ARBOR_SUPPORTED AND NOT ARBOR_PROBLEM)
  if(NOT TARGET arbor_ffi)
    # always optimized: a debug Arbor grows a tree many times slower
    corrosion_import_crate(
      MANIFEST_PATH "${ARBOR_DIR}/Cargo.toml"
      CRATES arbor-ffi
      CRATE_TYPES staticlib
      PROFILE release)
  endif()

  message(STATUS "Building arbor_ffi from ${ARBOR_DIR}, enabling Arbor plant generation")

  target_link_libraries(${plugin_target} PRIVATE arbor_ffi)
  target_include_directories(${plugin_target} PRIVATE "${ARBOR_DIR}/crates/arbor-ffi/include")
  target_compile_definitions(${plugin_target} PRIVATE HYP_ARBOR=1)
elseif(ARBOR_SUPPORTED AND NOT HYP_BAKE_ROOT_DIR)
  # a distribution build: shipping the Arbor menus without the library behind them is a broken editor
  message(FATAL_ERROR "ArborEditor: ${ARBOR_PROBLEM}")
elseif(ARBOR_SUPPORTED)
  message(WARNING "ArborEditor: ${ARBOR_PROBLEM}. Until then Arbor plants cannot be grown.")
endif()
