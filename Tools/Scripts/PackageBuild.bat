@echo off
setlocal EnableDelayedExpansion

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR=%%~fi\"

set "BIN_DIR=%HYP_ROOT_DIR%Binaries\Windows\Release"

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"`) do set "TIMESTAMP=%%i"

set "OUT_DIR=%HYP_ROOT_DIR%PackagedBuilds\Windows\Build_%TIMESTAMP%"

if not "%~1"=="" (
    set "GAME_PACKAGE=%~1"
) else (
    set /p "GAME_PACKAGE=Enter game package name (e.g. DefaultGame): "
)

if "%GAME_PACKAGE%"=="" (
    echo Error: No game package specified.
    exit /b 1
)

echo Creating packaged build at: %OUT_DIR%

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OUT_DIR%\Source\Shaders" mkdir "%OUT_DIR%\Source\Shaders"

echo Copying executables...
copy "%BIN_DIR%\*.exe" "%OUT_DIR%\" >nul

echo Copying DLLs...
copy "%BIN_DIR%\*.dll" "%OUT_DIR%\" >nul

echo Copying config files...
copy "%HYP_ROOT_DIR%Config\*Config.json" "%OUT_DIR%\" >nul 2>nul
copy "%HYP_ROOT_DIR%Config\*Config.Windows.json" "%OUT_DIR%\" >nul 2>nul

echo Copying packages...
xcopy "%HYP_ROOT_DIR%Packages\Engine" "%OUT_DIR%\Packages\Engine" /E /I /Y /Q >nul
xcopy "%HYP_ROOT_DIR%Packages\%GAME_PACKAGE%" "%OUT_DIR%\Packages\%GAME_PACKAGE%" /E /I /Y /Q >nul

echo Copying Shaders.ini...
copy "%HYP_ROOT_DIR%Source\Shaders\Shaders.ini" "%OUT_DIR%\Source\Shaders\" >nul

echo Done! Packaged build created at: %OUT_DIR%
endlocal
