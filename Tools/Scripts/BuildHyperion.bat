@echo off
setlocal EnableDelayedExpansion

set "HYP_ANDROID=0"
set "HYP_REGENERATE=0"
set "HYP_NOWAIT=0"
set "HYP_BUILD_TYPE=Release"

:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
IF /I "%~1"=="debug" set "HYP_BUILD_TYPE=Debug"
IF /I "%~1"=="release" set "HYP_BUILD_TYPE=Release"
IF /I "%~1"=="android" set "HYP_ANDROID=1"
IF /I "%~1"=="regenerate" set "HYP_REGENERATE=1"
IF /I "%~1"=="nowait" set "HYP_NOWAIT=1"
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

if "%HYP_ANDROID%"=="1" (
    if not exist Build\Android-arm64-v8a mkdir Build\Android-arm64-v8a
    pushd Build\Android-arm64-v8a
) else (
    if not exist Build mkdir Build
    pushd Build
)

REM CHOICE returns ERRORLEVEL 1 for Y, 2 for N
IF "%HYP_REGENERATE%"=="1" GOTO DO_CMAKE_GENERATION
IF "%HYP_NOWAIT%"=="1" GOTO SKIP_CMAKE_GENERATION

ECHO Regenerate CMake? (will continue without regenerating in 3s)
CHOICE /C YN /T 3 /D N /M "Press Y for Yes or N for No"

IF ERRORLEVEL 2 GOTO SKIP_CMAKE_GENERATION

:DO_CMAKE_GENERATION

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR_ABS=%%~fi"
set "HYP_ROOT_DIR_ABS=%HYP_ROOT_DIR_ABS:\=/%"

if "%HYP_ANDROID%"=="1" GOTO CMAKE_ANDROID
GOTO CMAKE_WINDOWS

:CMAKE_ANDROID
REM Android ARM64
REM TODO Make this not hardcoded, this is just for testing for now
if defined ANDROID_NDK_HOME set "ANDROID_NDK=%ANDROID_NDK_HOME%"
if not defined ANDROID_NDK set "ANDROID_NDK=%LOCALAPPDATA%\Android\Sdk\ndk\29.0.14206865"
set "ANDROID_NDK=%ANDROID_NDK:\=/%"

@REM ECHO Generating CMake for Android ARM64 using NDK at "%ANDROID_NDK%"
@REM cmake ../../Source ^
@REM     -G "Visual Studio 18 2026" ^
@REM     -A ARM64 ^
@REM     -DCMAKE_SYSTEM_NAME=Android ^
@REM     -DCMAKE_SYSTEM_VERSION=28 ^
@REM     -DCMAKE_ANDROID_STL_TYPE=c++_static ^
@REM     -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" ^
@REM     -DCMAKE_CXX_STANDARD=20 ^
@REM     -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
@REM     -DHYP_PLATFORM_NAME=Android ^
@REM     -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\External\ThirdParty\Binaries" ^
@REM     -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" ^
@REM     -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" ^
@REM     -DHYP_ROOT_DIR="%HYP_ROOT_DIR_ABS%"



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
cmake ../../Source -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK%/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -DHYP_PLATFORM_NAME=Android -DANDROID_NDK_SYSROOT="%ANDROID_NDK_SYSROOT%" -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" -DHYP_ROOT_DIR="%HYP_ROOT_DIR_ABS%"


GOTO SKIP_CMAKE_GENERATION

:CMAKE_WINDOWS
IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

cmake ../Source -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE="%HYP_BUILD_TYPE%" -G "Visual Studio 18 2026" -A x64 -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\External\ThirdParty\Binaries" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" -DHYP_ROOT_DIR="%HYP_ROOT_DIR_ABS%"

:SKIP_CMAKE_GENERATION

cmake --build . --parallel 8 --config Release

popd