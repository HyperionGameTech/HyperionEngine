@echo off

REM Get the script directory
SET SCRIPT_DIR=%~dp0
REM Remove trailing backslash
SET SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

ECHO Regenerate CMake? (will continue without regenerating in 3s)
CHOICE /C YN /T 3 /D N /M "Press Y for Yes or N for No"

IF ERRORLEVEL 2 (
    echo n| call "%SCRIPT_DIR%\tools\scripts\BuildHyperion.bat" %*
) ELSE (
    echo y| call "%SCRIPT_DIR%\tools\scripts\BuildHyperion.bat" %*
)