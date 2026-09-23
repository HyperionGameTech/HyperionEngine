@echo off
setlocal EnableDelayedExpansion

set "HYP_ANDROID=0"
set "HYP_CLANG=0"
set "HYP_NINJA=0"
set "HYP_MINGW=0"
set "HYP_REGENERATE=0"
set "HYP_NOWAIT=0"
set "HYP_SHIPPING=0"
set "HYP_BUILD_TYPE=Release"
set "HYP_ARM64=0"
set "HYP_BACKEND="

:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
IF /I "%~1"=="debug" set "HYP_BUILD_TYPE=Debug"
IF /I "%~1"=="release" set "HYP_BUILD_TYPE=Release"
IF /I "%~1"=="shipping" set "HYP_SHIPPING=1"
IF /I "%~1"=="shipping" set "HYP_BUILD_TYPE=Release"
IF /I "%~1"=="android" set "HYP_ANDROID=1"
IF /I "%~1"=="clang" set "HYP_CLANG=1"
IF /I "%~1"=="ninja" set "HYP_NINJA=1"
IF /I "%~1"=="mingw" set "HYP_MINGW=1"
IF /I "%~1"=="arm64" set "HYP_ARM64=1"
IF /I "%~1"=="dx12" set "HYP_BACKEND=DX12"
IF /I "%~1"=="vulkan" set "HYP_BACKEND=VULKAN"
IF /I "%~1"=="regenerate" set "HYP_REGENERATE=1"
IF /I "%~1"=="nowait" set "HYP_NOWAIT=1"
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

REM Exported so BuildCodeGen.bat (invoked indirectly via RunCodeGen during CMake
REM configuration) knows to build the CodeGen tool with the same MinGW toolchain
REM instead of assuming a Visual Studio generator is available.
set "HYP_MINGW=%HYP_MINGW%"

set "HYP_BUILD_DIR=%HYP_BUILD_TYPE%"
if "%HYP_SHIPPING%"=="1" set "HYP_BUILD_DIR=Shipping"

set "HYP_SHIPPING_CMAKE="
if "%HYP_SHIPPING%"=="1" set "HYP_SHIPPING_CMAKE=-DHYP_SHIPPING=1"

REM Shipping builds output to Binaries/Windows/Shipping instead of Binaries/Windows/Release,
REM but keep the Release build type and third-party libs.
set "HYP_OUTPUT_SUFFIX_ARG="
if "%HYP_SHIPPING%"=="1" set "HYP_OUTPUT_SUFFIX_ARG=-DHYP_OUTPUT_DIRECTORY_SUFFIX=Shipping"

if "%HYP_ANDROID%"=="1" (
    if not exist Build\Android\%HYP_BUILD_DIR% mkdir Build\Android\%HYP_BUILD_DIR%
    pushd Build\Android\%HYP_BUILD_DIR%
) else if "%HYP_ARM64%"=="1" (
    if not exist Build\Windows-ARM64\%HYP_BUILD_DIR% mkdir Build\Windows-ARM64\%HYP_BUILD_DIR%
    pushd Build\Windows-ARM64\%HYP_BUILD_DIR%
) else if "%HYP_MINGW%"=="1" (
    if not exist Build\Windows-MinGW\%HYP_BUILD_DIR% mkdir Build\Windows-MinGW\%HYP_BUILD_DIR%
    pushd Build\Windows-MinGW\%HYP_BUILD_DIR%
) else if "%HYP_NINJA%"=="1" (
    if not exist Build\Windows\%HYP_BUILD_DIR% mkdir Build\Windows\%HYP_BUILD_DIR%
    pushd Build\Windows\%HYP_BUILD_DIR%
) else if "%HYP_CLANG%"=="1" (
    if not exist Build\Windows-Clang\%HYP_BUILD_DIR% mkdir Build\Windows-Clang\%HYP_BUILD_DIR%
    pushd Build\Windows-Clang\%HYP_BUILD_DIR%
) else (
    if not exist Build\Windows\%HYP_BUILD_DIR% mkdir Build\Windows\%HYP_BUILD_DIR%
    pushd Build\Windows\%HYP_BUILD_DIR%
)

REM CHOICE returns ERRORLEVEL 1 for Y, 2 for N
IF "%HYP_REGENERATE%"=="1" GOTO DO_CMAKE_GENERATION
IF "%HYP_NOWAIT%"=="1" GOTO SKIP_CMAKE_GENERATION

ECHO Regenerate CMake? (will continue without regenerating in 3s)
CHOICE /C YN /T 3 /D N /M "Press Y for Yes or N for No"

IF ERRORLEVEL 2 GOTO SKIP_CMAKE_GENERATION

:DO_CMAKE_GENERATION

for %%i in ("%~dp0..\..\..") do set "HYP_ROOT_DIR=%%~fi"
set "HYP_ROOT_DIR=%HYP_ROOT_DIR:\=/%"


if "%HYP_ANDROID%"=="1" GOTO CMAKE_ANDROID
if "%HYP_ARM64%"=="1" GOTO CMAKE_WINDOWS_ARM64
if "%HYP_MINGW%"=="1" GOTO CMAKE_WINDOWS_MINGW
if "%HYP_NINJA%"=="1" GOTO CMAKE_WINDOWS_NINJA
if "%HYP_CLANG%"=="1" GOTO CMAKE_WINDOWS_CLANG
GOTO CMAKE_WINDOWS

:CMAKE_ANDROID
REM Android ARM64
REM TODO Make this not hardcoded, this is just for testing for now
if defined ANDROID_NDK_HOME set "ANDROID_NDK=%ANDROID_NDK_HOME%"
if not defined ANDROID_NDK set "ANDROID_NDK=%LOCALAPPDATA%\Android\Sdk\ndk\29.0.14206865"
set "ANDROID_NDK=%ANDROID_NDK:\=/%"

@REM ECHO Generating CMake for Android ARM64 using NDK at "%ANDROID_NDK%"
@REM cmake ../../../Source ^
@REM     -G "Visual Studio 18 2026" ^
@REM     -A ARM64 ^
@REM     -DCMAKE_SYSTEM_NAME=Android ^
@REM     -DCMAKE_SYSTEM_VERSION=28 ^
@REM     -DCMAKE_ANDROID_STL_TYPE=c++_static ^
@REM     -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" ^
@REM     -DCMAKE_CXX_STANDARD=20 ^
@REM     -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
@REM     -DHYP_PLATFORM_NAME=Android ^
@REM     -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" ^
@REM     -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" ^
@REM     -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" ^
@REM     -DHYP_ROOT_DIR="%HYP_ROOT_DIR%"



REM Locate Ninja
set "NINJA_EXE="
where ninja >nul 2>&1 && set "NINJA_EXE=ninja"

if not defined NINJA_EXE (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do (
            set "VS_NINJA=%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
        )
        if exist "!VS_NINJA!" set "NINJA_EXE=!VS_NINJA!"
    )
)

if not defined NINJA_EXE (
    for /d %%D in ("%LOCALAPPDATA%\Android\Sdk\cmake\*") do (
        if exist "%%D\bin\ninja.exe" set "NINJA_EXE=%%D\bin\ninja.exe"
    )
)

if not defined NINJA_EXE (
    echo ERROR: Ninja not found. Make sure it is installed and in your PATH!
    exit /b 1
)

set "ANDROID_NDK_SYSROOT=%ANDROID_NDK%/toolchains/llvm/prebuilt/windows-x86_64/sysroot"

echo Using Ninja: %NINJA_EXE%
cmake ../../../Source -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK%/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -DHYP_PLATFORM_NAME=Android -DANDROID_NDK_SYSROOT="%ANDROID_NDK_SYSROOT%" -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_ROOT_DIR="%HYP_ROOT_DIR%" %HYP_SHIPPING_CMAKE%
if errorlevel 1 (
    echo CMake generation failed. Aborting build.
    popd
    exit /b 1
)


GOTO SKIP_CMAKE_GENERATION
:CMAKE_WINDOWS_ARM64
IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

REM Locate the Visual Studio installation (needs the VC toolchain with the ClangCL toolset)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_INSTALL_DIR="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL_DIR=%%I"
)

if not defined VS_INSTALL_DIR (
    echo ERROR: Could not locate a Visual Studio installation via vswhere. Install the "Desktop development with C++" workload.
    exit /b 1
)

REM Pick the generator name from the VS version folder (2022 => VS 17, 2026 => VS 18)
for %%V in ("%VS_INSTALL_DIR%") do set "HYP_VS_VERSION=%%~nxV"
set "HYP_VS_GENERATOR=Visual Studio 17 2022"
if "%HYP_VS_VERSION%"=="2026" set "HYP_VS_GENERATOR=Visual Studio 18 2026"

REM Ensure the ClangCL toolset is available for the ARM64 host
set "HYP_CLANGCL_HOST=%VS_INSTALL_DIR%\VC\Tools\Llvm\ARM64\bin\clang-cl.exe"
set "HYP_CLANGCL_CROSS=%VS_INSTALL_DIR%\VC\Tools\Llvm\x64\bin\clang-cl.exe"
if not exist "%HYP_CLANGCL_HOST%" if not exist "%HYP_CLANGCL_CROSS%" (
    echo ERROR: ClangCL toolset not found in Visual Studio. Install the "C++ Clang tools for Windows" component.
    exit /b 1
)

echo Using generator: %HYP_VS_GENERATOR% -A ARM64 -T ClangCL

REM ARM64 defaults to the DX12 rendering backend unless "vulkan" is passed
set "HYP_BACKEND_CMAKE=-DENABLE_EXPERIMENTAL_DX12_RENDERING_BACKEND=1"
if "%HYP_BACKEND%"=="VULKAN" set "HYP_BACKEND_CMAKE="

cmake ../../../Source -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=arm64-windows -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -G "%HYP_VS_GENERATOR%" -A ARM64 -T ClangCL -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_ROOT_DIR="%HYP_ROOT_DIR%" %HYP_SHIPPING_CMAKE% %HYP_OUTPUT_SUFFIX_ARG% %HYP_BACKEND_CMAKE%
if errorlevel 1 (
    echo CMake generation failed. Aborting build.
    popd
    exit /b 1
)

GOTO SKIP_CMAKE_GENERATION
:CMAKE_WINDOWS_MINGW
IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

for %%i in ("%~dp0..\..\..") do set "HYP_ROOT_DIR=%%~fi"
set "HYP_ROOT_DIR=%HYP_ROOT_DIR:\=/%"

REM Locate a MinGW-w64 GCC toolchain (e.g. WinLibs, or MSYS2's mingw64) on PATH.
REM No Visual Studio / MSVC environment is set up or required for this path.
where gcc >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc not found on PATH. Install a MinGW-w64 toolchain ^(e.g. WinLibs or MSYS2^) and ensure it is on PATH.
    exit /b 1
)

REM Locate ninja
set "NINJA_EXE="
where ninja >nul 2>&1 && set "NINJA_EXE=ninja"

if not defined NINJA_EXE (
    echo ERROR: Ninja not found. Make sure it is installed and in your PATH!
    exit /b 1
)

echo Using Ninja: %NINJA_EXE%
echo Using MinGW GCC/G++ from PATH

REM Strata's LLVM JIT/AOT backend needs an LLVM install matching the compiler ABI
REM (LLVMConfig.cmake), which isn't part of this minimal MinGW toolchain. Strata
REM itself still builds as a plain interpreter; only the LLVM-backed JIT is skipped
REM (see the STRATA_ENABLE_LLVM guard around HYP_STRATA_JIT in Source/CMakeLists.txt).
cmake ../../../Source -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic -DVCPKG_HOST_TRIPLET=x64-mingw-dynamic -DSTRATA_ENABLE_LLVM=OFF -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_ROOT_DIR="%HYP_ROOT_DIR%" %HYP_SHIPPING_CMAKE% %HYP_OUTPUT_SUFFIX_ARG%
if errorlevel 1 (
    echo CMake generation failed. Aborting build.
    popd
    exit /b 1
)

GOTO SKIP_CMAKE_GENERATION
:CMAKE_WINDOWS_NINJA
IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

REM Save VCPKG_ROOT before calling vcvars64.bat, as the MSVC environment overrides it with Visual Studio's bundled vcpkg.
set "HYP_VCPKG_ROOT=%VCPKG_ROOT%"

REM Locate the Visual Studio installation (for the MSVC toolchain environment and clang-cl)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_INSTALL_DIR="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL_DIR=%%I"
)

if not defined VS_INSTALL_DIR (
    echo ERROR: Could not locate a Visual Studio installation via vswhere. Install the "Desktop development with C++" workload.
    exit /b 1
)

REM Set up the MSVC environment so clang-cl can find the standard library headers and linkers
call "%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo ERROR: Failed to initialize the MSVC environment ^(vcvars64.bat^).
    exit /b 1
)

REM Locate ninja (bundled with Visual Studio or on PATH)
set "NINJA_EXE="
where ninja >nul 2>&1 && set "NINJA_EXE=ninja"

if not defined NINJA_EXE (
    if exist "%VS_INSTALL_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" set "NINJA_EXE=%VS_INSTALL_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
)

if not defined NINJA_EXE (
    echo ERROR: Ninja not found. Make sure it is installed and in your PATH!
    exit /b 1
)

REM Locate clang-cl (C++ Clang tools for Windows component), fall back to PATH
set "CLANG_CL=%VS_INSTALL_DIR%\VC\Tools\Llvm\x64\bin\clang-cl.exe"
if not exist "%CLANG_CL%" set "CLANG_CL=clang-cl"

echo Using Ninja: %NINJA_EXE%
echo Using ClangCL: %CLANG_CL%

cmake ../../../Source -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_CXX_COMPILER="%CLANG_CL%" -DCMAKE_C_COMPILER="%CLANG_CL%" -DCMAKE_TOOLCHAIN_FILE="%HYP_VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_ROOT_DIR="%HYP_ROOT_DIR%" %HYP_SHIPPING_CMAKE% %HYP_OUTPUT_SUFFIX_ARG%
if errorlevel 1 (
    echo CMake generation failed. Aborting build.
    popd
    exit /b 1
)

GOTO SKIP_CMAKE_GENERATION
:CMAKE_WINDOWS_CLANG
IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

cmake ../../../Source -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -G "Visual Studio 18 2026" -A x64 -T ClangCL -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_ROOT_DIR="%HYP_ROOT_DIR%" %HYP_SHIPPING_CMAKE% %HYP_OUTPUT_SUFFIX_ARG%
if errorlevel 1 (
    echo CMake generation failed. Aborting build.
    popd
    exit /b 1
)

GOTO SKIP_CMAKE_GENERATION
:CMAKE_WINDOWS
IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

cmake ../../../Source -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -G "Visual Studio 18 2026" -A x64 -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\..\Binaries" -DHYP_ROOT_DIR="%HYP_ROOT_DIR%" %HYP_SHIPPING_CMAKE% %HYP_OUTPUT_SUFFIX_ARG%
if errorlevel 1 (
    echo CMake generation failed. Aborting build.
    popd
    exit /b 1
)

:SKIP_CMAKE_GENERATION

cmake --build . --parallel %NUMBER_OF_PROCESSORS% --config %HYP_BUILD_TYPE%
if errorlevel 1 (
    popd
    exit /b 1
)

if "%HYP_MINGW%"=="1" (
    REM Binaries built against a dynamically-linked MinGW runtime need these DLLs at
    REM runtime. They live in the toolchain's own bin directory, which is on PATH in this
    REM dev shell but not necessarily for anyone else (or the app run outside it), producing
    REM a "DLL could not be found" error otherwise. Copy them next to the built executables.
    for /f "delims=" %%G in ('where gcc') do set "HYP_MINGW_BIN=%%~dpG"
    for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
        if exist "!HYP_MINGW_BIN!%%D" copy /Y "!HYP_MINGW_BIN!%%D" "%~dp0..\..\..\Binaries\Windows\%HYP_BUILD_TYPE%\" >nul
    )
)

popd