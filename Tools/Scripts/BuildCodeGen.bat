@echo off

echo "Running BuildCodeGen.bat from %CD%"

mkdir .\Build\CodeGen
pushd .\Build\CodeGen

choice /C YN /T 3 /D N /M "Regenerate CMake? (will continue without regenerating in 3s)"
if %errorlevel%==1 (
    cmake ..\..\Tools\CodeGen
)

cmake --build . --target hyperion-codegen --parallel 4
if errorlevel 1 (
    exit /b 1
)

set MOVED_EXE=0
set MOVED_DLL=0

if exist hyperion-codegen.exe (
    echo Found hyperion-codegen.exe in current directory
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

if exist hyperion-core.dll (
    move hyperion-core.dll ..
    set MOVED_DLL=1
) else (
    if exist Debug\hyperion-core.dll (
        move Debug\hyperion-core.dll ..
        set MOVED_DLL=1
    ) else (
        if exist Release\hyperion-core.dll (
            move Release\hyperion-core.dll ..
            set MOVED_DLL=1
        ) else (
            echo Could not find hyperion-core.dll - executable may fail to launch!
        )
    )
)

popd
