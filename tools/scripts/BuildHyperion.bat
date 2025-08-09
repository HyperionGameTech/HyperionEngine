@echo off

if not exist build mkdir build
pushd build
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

cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_DEFAULT_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -A x64

:SKIP_CMAKE_GENERATION

cmake --build . --parallel 8 --config Release

popd