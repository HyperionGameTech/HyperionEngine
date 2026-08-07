@echo off
setlocal EnableDelayedExpansion

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR=%%~fi\"

echo Building...

call "%~dp0BuildHyperion.bat" Shipping Clang Ninja Regenerate

if errorlevel 1 (
    echo Build failed, aborting packaged build.
    exit /b 1
)

set "BIN_DIR=%HYP_ROOT_DIR%Binaries\Windows\Shipping"

REM Commandlets not included / wont work for shipping so we use release
set "BIN_DIR_RELEASE=%HYP_ROOT_DIR%Binaries\Windows\Release"

echo Running PrecompileShaders commandlet...

REM Compile shaders for only Windows (DX12 + Vulkan)
"%BIN_DIR_RELEASE%\PrecompileShaders.exe" --platform=windows

echo Running Cook commandlet...

REM Get the project name input from user, pass it concat with Projects/ below:
set /p "PROJECT_NAME=Enter the project name (folder under Projects/): "
if "%PROJECT_NAME%"=="" (
    echo No project name entered, aborting packaged build.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"`) do set "TIMESTAMP=%%i"
set "OUT_DIR=%HYP_ROOT_DIR%PackagedBuilds\Windows\Build_%TIMESTAMP%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
echo Creating packaged build at: %OUT_DIR%

"%BIN_DIR_RELEASE%\BlobStorageCookCommandlet.exe" --content=Projects/%PROJECT_NAME% --CacheDir=%OUT_DIR%\Cache
if errorlevel 1 (
    echo Cook commandlet failed, aborting packaged build.
    exit /b 1
)

if not exist "%OUT_DIR%\Source\Shaders" mkdir "%OUT_DIR%\Source\Shaders"

echo Copying executables...
copy "%BIN_DIR%\*.exe" "%OUT_DIR%\" >nul

echo Copying DLLs...
copy "%BIN_DIR%\*.dll" "%OUT_DIR%\" >nul

echo Copying config files...
if not exist "%OUT_DIR%\Config" mkdir "%OUT_DIR%\Config"
copy "%HYP_ROOT_DIR%Config\*Config.json" "%OUT_DIR%\Config\" >nul 2>nul
copy "%HYP_ROOT_DIR%Config\*Config.Windows.json" "%OUT_DIR%\Config\" >nul 2>nul

echo Updating GlobalConfig.json for packaged build...
powershell -NoProfile -Command "(Get-Content '%OUT_DIR%\Config\GlobalConfig.json') -replace '-BaseDir=[\w/.-]+', '-BaseDir=./' | Set-Content '%OUT_DIR%\Config\GlobalConfig.json'" >nul

echo Copying Shaders.hmf...
copy "%HYP_ROOT_DIR%Config\Shaders.hmf" "%OUT_DIR%\Config\" >nul

REM For development build, copy the game actions vdf and steam app id
echo Copying development steam files...
REM This will need to not be hardcoded obviously!
copy "%HYP_ROOT_DIR%Source\Sample\DefaultGame\game_actions_480.vdf" "%OUT_DIR%\game_actions_480.vdf" >nul
copy "%HYP_ROOT_DIR%Source\Sample\DefaultGame\steam_appid.txt" "%OUT_DIR%\steam_appid.txt" >nul

echo Done! Packaged build created at: %OUT_DIR%
endlocal
