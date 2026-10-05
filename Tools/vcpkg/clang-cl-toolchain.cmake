# vcpkg otherwise picks clang.exe (GNU-style driver) when cl.exe is not on PATH, which rejects the MSVC flags it passes.
# A chainloaded toolchain runs without the vcvars environment, so clang-cl locates MSVC and the Windows SDK itself.
file(GLOB HYP_VCPKG_LLVM_HINTS
    "$ENV{ProgramFiles}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin"
    "$ENV{ProgramFiles}/LLVM/bin"
)
find_program(HYP_VCPKG_CLANG_CL clang-cl HINTS ${HYP_VCPKG_LLVM_HINTS} REQUIRED)
get_filename_component(HYP_VCPKG_LLVM_BIN "${HYP_VCPKG_CLANG_CL}" DIRECTORY)

set(CMAKE_C_COMPILER "${HYP_VCPKG_CLANG_CL}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${HYP_VCPKG_CLANG_CL}" CACHE FILEPATH "" FORCE)
set(CMAKE_LINKER "${HYP_VCPKG_LLVM_BIN}/lld-link.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_AR "${HYP_VCPKG_LLVM_BIN}/llvm-lib.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_RC_COMPILER "${HYP_VCPKG_LLVM_BIN}/llvm-rc.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_MT "${HYP_VCPKG_LLVM_BIN}/llvm-mt.exe" CACHE FILEPATH "" FORCE)

set(CMAKE_RC_FLAGS "/DWIN32" CACHE STRING "")

# included from <vcpkg>/scripts/buildsystems/vcpkg.cmake
get_filename_component(HYP_VCPKG_SCRIPTS_DIR "${CMAKE_PARENT_LIST_FILE}/../.." ABSOLUTE)
include("${HYP_VCPKG_SCRIPTS_DIR}/toolchains/windows.cmake")
