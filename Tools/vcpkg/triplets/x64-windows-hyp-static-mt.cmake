# Static third-party libraries with the static VC runtime, for the shipping SDK (HYP_SHIPPING_SDK) that Package Game links games against.
# The game swaps the static UCRT for the dynamic one at link time, as NativeAOT does.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../clang-cl-toolchain.cmake")
