@echo off

echo "Running BuildCodeGen.bat from %CD%"

mkdir .\Build\CodeGen
pushd .\Build\CodeGen

set "HYP_CODEGEN_CMAKE_GEN_ARGS="
if "%HYP_MINGW%"=="1" (
    set "HYP_CODEGEN_CMAKE_GEN_ARGS=-G Ninja -DCMAKE_MAKE_PROGRAM=ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++"
)

REM On ARM64 devices (Windows on ARM), the Visual Studio generator defaults
REM to the x64 platform which would build the codegen tool for emulated x64.
REM Force the native ARM64 platform when running natively on an ARM64 host.
if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "HYP_CODEGEN_CMAKE_GEN_ARGS=%HYP_CODEGEN_CMAKE_GEN_ARGS% -A ARM64"

choice /C YN /T 3 /D N /M "Regenerate CMake? (will continue without regenerating in 3s)"
if %errorlevel%==1 (
    cmake ..\..\Tools\CodeGen %HYP_CODEGEN_CMAKE_GEN_ARGS%
)

cmake --build . --target hyperion-codegen --parallel 4
if errorlevel 1 (
    exit /b 1
)

set MOVED_EXE=0
set MOVED_DLL=0

if exist hyperion-codegen.exe (
    echo Found hyperion-codegen.exe in current directory
    move /Y hyperion-codegen.exe .. >nul
    set MOVED_EXE=1
) else (
    if exist Debug\hyperion-codegen.exe (
        echo Found hyperion-codegen.exe in Debug directory
        move Debug\hyperion-codegen.exe ..
        set MOVED_EXE=1
    ) else (
        if exist Release\hyperion-codegen.exe (
            echo Found hyperion-codegen.exe in Release directory
            move Release\hyperion-codegen.exe ..
            set MOVED_EXE=1
        ) else (
            echo Could not find hyperion-codegen.exe executable!
            exit /b 1
        )
    )
)

popd
