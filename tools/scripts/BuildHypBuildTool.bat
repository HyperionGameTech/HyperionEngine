@echo off

mkdir build\buildtool

pushd build
pushd buildtool

choice /C YN /T 3 /D N /M "Regenerate CMake? (will continue without regenerating in 3s)"
if %errorlevel%==1 (
    cmake ..\..\buildtool
)

cmake --build . --target hyperion-buildtool
if errorlevel 1 (
    exit /b 1
)

if exist hyperion-buildtool (
    echo Found hyperion-buildtool in current directory
) else (
    if exist Debug\hyperion-buildtool (
        echo Found hyperion-buildtool in Debug directory
        move Debug\hyperion-buildtool ..
    ) else (
        if exist Release\hyperion-buildtool (
            echo Found hyperion-buildtool in Release directory
            move Release\hyperion-buildtool ..
        ) else (
            echo Could not find hyperion-buildtool executable
            exit /b 1
        )
    )
)

popd
popd
