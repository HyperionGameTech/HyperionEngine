@echo off
setlocal EnableDelayedExpansion

rem -- parse CLI arguments for build tool version --
if "%~1"=="" (
    echo Usage: %~nx0 ^<major^> ^<minor^>
    exit /b 1
)
if "%~2"=="" (
    echo Usage: %~nx0 ^<major^> ^<minor^>
    exit /b 1
)
set "HYP_BUILD_TOOL_VERSION_MAJOR=%~1"
set "HYP_BUILD_TOOL_VERSION_MINOR=%~2"

set "WORKING_DIR=%cd%"

rem -- version‐based rebuild logic start --
set "REBUILD=false"
set "INC_FILE=%WORKING_DIR%\build\generated\BuildToolOutput.inc"

rem -- Check if both the build tool and version file exist --
if not exist "%WORKING_DIR%\build\hyperion-buildtool.exe" (
    echo Hyperion build tool not found. Running BuildHypBuildTool...
    set "REBUILD=true"
    goto do_rebuild
)

if not exist "%INC_FILE%" (
    echo Version file not found. Running BuildHypBuildTool...
    set "REBUILD=true" 
    goto do_rebuild
)

rem -- Read and check version --
for /f "usebackq delims=" %%L in ("%INC_FILE%") do (
    set "line=%%L"
    goto check_version
)

:check_version
if not defined line (
    echo Version not found in file. Running BuildHypBuildTool...
    set "REBUILD=true"
    goto do_rebuild
)

rem expect "//! VERSION major.minor.patch"
for /f "tokens=3,4 delims=. " %%a in ("!line!") do (
    set "fileMajor=%%a"
    set "fileMinor=%%b"
)

set "fileMajorClean=!fileMajor: =!"
set "fileMinorClean=!fileMinor: =!"
set "majorClean=!HYP_BUILD_TOOL_VERSION_MAJOR: =!"
set "minorClean=!HYP_BUILD_TOOL_VERSION_MINOR: =!"

set "REBUILD_REASON="
if !fileMajorClean! NEQ !majorClean! set "REBUILD=true" & set "REBUILD_REASON=Major version mismatch"
if !fileMinorClean! NEQ !minorClean! set "REBUILD=true" & set "REBUILD_REASON=Minor version mismatch"

if "!REBUILD!"=="true" (
    echo Rebuild needed: !REBUILD_REASON!
    goto do_rebuild
)

echo Build tool is up-to-date, skipping rebuild.
goto _skipBuild

:do_rebuild
echo Running BuildHypBuildTool...
echo y| call tools\scripts\BuildHypBuildTool.bat
if errorlevel 1 (
    echo Failed to build Hyperion build tool.
    exit /b 1
) else (
    rem Check if build tool was created
    if not exist "%WORKING_DIR%\build\hyperion-buildtool.exe" (
        echo Build tool returned success, but the executable could not be found!
        exit /b 1
    )
)

:_skipBuild
rem -- version‐based rebuild logic end --

build\hyperion-buildtool.exe --WorkingDirectory=%WORKING_DIR% --SourceDirectory=%WORKING_DIR%\src --CXXOutputDirectory=%WORKING_DIR%\build\generated --CSharpOutputDirectory=%WORKING_DIR%\build\generated\CSharp --HypScriptOutputDirectory=%WORKING_DIR%\build --ExcludeDirectories=%WORKING_DIR%\src\generated --ExcludeFiles=%WORKING_DIR%\src\core\Defines.hpp
