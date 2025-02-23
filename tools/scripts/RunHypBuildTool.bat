@echo off

rem Check if build\hyperion-buildtool.exe exists; adjust if your file isn’t an .exe
if not exist "build\hyperion-buildtool.exe" (
    echo Hyperion build tool not found. Running BuildHypBuildTool...
    rem Simulate input by echoing "y" into the build tool builder script.
    echo y| call tools\scripts\BuildHypBuildTool.bat
    if errorlevel 1 (
        echo Failed to build Hyperion build tool.
        exit /b 1
    ) else (
        echo Hyperion build tool built successfully.
    )
)

rem Set the working directory variable (current directory)
set "WORKING_DIR=%cd%"

rem Run the build tool with the specified parameters.
rem Note: Adjust executable name if necessary.
build\hyperion-buildtool.exe --WorkingDirectory=%WORKING_DIR% --SourceDirectory=%WORKING_DIR%\src --CXXOutputDirectory=%WORKING_DIR%\build\generated --CSharpOutputDirectory=%WORKING_DIR%\src\dotnet\runtime\gen --ExcludeDirectories=%WORKING_DIR%\src\generated --ExcludeFiles=%WORKING_DIR%\src\core\Defines.hpp
