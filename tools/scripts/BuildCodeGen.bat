@echo off

mkdir build\codegen

pushd build
pushd codegen

choice /C YN /T 3 /D N /M "Regenerate CMake? (will continue without regenerating in 3s)"
if %errorlevel%==1 (
    cmake ..\..\codegen
)

cmake --build . --target hyperion-codegen --parallel 4
if errorlevel 1 (
    exit /b 1
)

if exist hyperion-codegen.exe (
    echo Found hyperion-codegen.exe in current directory
) else (
    if exist Debug\hyperion-codegen.exe (
        echo Found hyperion-codegen.exe in Debug directory
        move Debug\hyperion-codegen.exe ..
    ) else (
        if exist Release\hyperion-codegen.exe (
            echo Found hyperion-codegen.exe in Release directory
            move Release\hyperion-codegen.exe ..
        ) else (
            echo Could not find hyperion-codegen.exe executable!
            exit /b 1
        )
    )
)

popd
popd
