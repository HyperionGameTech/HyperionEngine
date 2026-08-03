@echo off
setlocal EnableDelayedExpansion

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR=%%~fi\"

echo Running shipping build (BuildHyperion.bat shipping clang ninja)...

call "%~dp0BuildHyperion.bat" shipping clang ninja regenerate

if errorlevel 1 (
    echo Build failed, aborting packaged build.
    exit /b 1
)

set "BIN_DIR=%HYP_ROOT_DIR%Binaries\Windows\Shipping"

echo Running PrecompileShaders commandlet...

REM Compile shaders for only Windows (DX12 + Vulkan)
"%BIN_DIR%\PrecompileShaders.exe --platform=windows"

echo Running Cook commandlet...
"%BIN_DIR%\BlobStorageCookCommandlet.exe"

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"`) do set "TIMESTAMP=%%i"

set "OUT_DIR=%HYP_ROOT_DIR%PackagedBuilds\Windows\Build_%TIMESTAMP%"

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

echo Updating GlobalConfig.json for packaged build...
powershell -NoProfile -Command "(Get-Content '%OUT_DIR%\GlobalConfig.json') -replace '-BaseDir=[\w/.-]+', '-BaseDir=./' | Set-Content '%OUT_DIR%\GlobalConfig.json'" >nul

echo Copying content and cache...

echo Copying Content...
xcopy "%BIN_DIR%\Content" "%OUT_DIR%\Content" /e /i /y >nul

echo Copying Cache...
xcopy "%BIN_DIR%\Cache" "%OUT_DIR%\Cache" /e /i /y >nul

echo Copying Shaders.ini...
copy "%HYP_ROOT_DIR%Source\Shaders\Shaders.ini" "%OUT_DIR%\Source\Shaders\" >nul

REM For development build, copy the game actions vdf and steam app id
echo Copying development steam files...
REM This will need to not be hardcoded obviously!
copy "%HYP_ROOT_DIR%Source\Sample\DefaultGame\game_actions_480.vdf" "%OUT_DIR%\game_actions_480.vdf" >nul
copy "%HYP_ROOT_DIR%Source\Sample\DefaultGame\steam_appid.txt" "%OUT_DIR%\steam_appid.txt" >nul

echo Done! Packaged build created at: %OUT_DIR%
endlocal
