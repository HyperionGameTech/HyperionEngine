@echo off

if not exist Build mkdir Build
pushd Build
:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

ECHO Regenerate CMake? (will continue without regenerating in 3s)
CHOICE /C YN /T 3 /D N /M "Press Y for Yes or N for No"

REM CHOICE returns ERRORLEVEL 1 for Y, 2 for N
IF ERRORLEVEL 2 GOTO SKIP_CMAKE_GENERATION

IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR_ABS=%%~fi"
set "HYP_ROOT_DIR_ABS=%HYP_ROOT_DIR_ABS:\=/%"

cmake ../Source -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 18 2026" -A x64 -DHYP_THIRD_PARTY_LIBRARY_DIRECTORY="%~dp0..\..\Binaries\ThirdParty" -DHYP_LIBRARY_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" -DHYP_RUNTIME_OUTPUT_DIRECTORY="%~dp0..\..\Binaries\Engine" -DHYP_ROOT_DIR="%HYP_ROOT_DIR_ABS%"

:SKIP_CMAKE_GENERATION

cmake --build . --parallel 8 --config Release

popd