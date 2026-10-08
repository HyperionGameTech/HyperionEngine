@echo off
setlocal EnableDelayedExpansion

REM Cooks a project for the browser, builds the WebAssembly sample against it and puts everything a web server needs
REM in one folder. Serve the result with ServeWeb.bat.
REM
REM   PackageBuildWeb.bat --project <name or path> [--worlds <names>] [--engine-assets <list>]
REM                       [--skip-precompile] [--skip-build]
REM
REM --engine-assets takes the file a native WebGPU run of the level wrote with --record-engine-assets=<file>. Only the
REM engine assets in it are cooked, and of the shaders only the WebGPU variants. Without it all engine content is cooked.

REM taken before the arguments are parsed: SHIFT moves %0 along with the rest
set "SCRIPT_DIR=%~dp0"

set "SKIP_PRECOMPILE=0"
set "SKIP_BUILD=0"
set "PROJECT_NAME="
set "WORLDS="
set "ENGINE_ASSETS="

:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
IF /I "%~1"=="--skip-precompile" set "SKIP_PRECOMPILE=1"
IF /I "%~1"=="--skip-build" set "SKIP_BUILD=1"
IF /I "%~1"=="--project" (
    set "PROJECT_NAME=%~2"
    SHIFT
)
IF /I "%~1"=="--worlds" (
    set "WORLDS=%~2"
    SHIFT
)
IF /I "%~1"=="--engine-assets" (
    set "ENGINE_ASSETS=%~f2"
    SHIFT
)
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

for %%i in ("%SCRIPT_DIR%..\..") do set "HYP_ROOT_DIR=%%~fi\"

REM Commandlets can't run in the browser, so shader conversion and cooking use the Windows host tools.
set "BIN_DIR_RELEASE=%HYP_ROOT_DIR%Binaries\Windows\Release"

if not exist "%BIN_DIR_RELEASE%\BlobStorageCookCommandlet.exe" (
    echo ERROR: Host tools not found at "%BIN_DIR_RELEASE%". Build the engine for Windows ^(Release^) first.
    exit /b 1
)

if "%PROJECT_NAME%"=="" set /p "PROJECT_NAME=Enter the project name (folder under Projects/) or a path to it: "
if "%PROJECT_NAME%"=="" (
    echo No project entered, aborting packaged build.
    exit /b 1
)

set "PROJECT_PATH=Projects/%PROJECT_NAME%"
if exist "%PROJECT_NAME%\" for %%i in ("%PROJECT_NAME%") do set "PROJECT_PATH=%%~fi"
set "PROJECT_PATH=%PROJECT_PATH:\=/%"

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"`) do set "TIMESTAMP=%%i"
set "OUT_DIR=%HYP_ROOT_DIR%PackagedBuilds\Web\Build_%TIMESTAMP%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

pushd "%BIN_DIR_RELEASE%"

if "%SKIP_PRECOMPILE%"=="1" (
    echo Skipping shader precompilation.
) else (
    echo Running PrecompileShaders commandlet...
    "%BIN_DIR_RELEASE%\PrecompileShaders.exe" --platform=web --api=webgpu
)

set "COOK_ARGS=--project=%PROJECT_PATH% --out-cache=%OUT_DIR%\Cache --out-content=%OUT_DIR%\Content"
if not "%WORLDS%"=="" set "COOK_ARGS=%COOK_ARGS% --worlds=%WORLDS%"

if not "%ENGINE_ASSETS%"=="" (
    if not exist "%ENGINE_ASSETS%" (
        echo ERROR: Engine asset list "%ENGINE_ASSETS%" not found.
        popd
        exit /b 1
    )

    echo Narrowing the engine asset list to WebGPU shader variants...
    python "%SCRIPT_DIR%FilterEngineAssetList.py" "%ENGINE_ASSETS%" "%HYP_ROOT_DIR%Content\Engine" WEBGPU WEB "%OUT_DIR%\EngineAssets.txt"
    if errorlevel 1 (
        echo Filtering the engine asset list failed, aborting packaged build.
        popd
        exit /b 1
    )

    set "COOK_ARGS=!COOK_ARGS! --engine-assets=%OUT_DIR%\EngineAssets.txt"
)

echo Cooking project: %PROJECT_PATH%

"%BIN_DIR_RELEASE%\BlobStorageCookCommandlet.exe" %COOK_ARGS% > "%OUT_DIR%\Cook.log" 2>&1

popd

REM The commandlet can crash while shutting down after a finished cook, so the log decides, not the exit code.
findstr /C:"Blob storage cook complete." "%OUT_DIR%\Cook.log" >nul
if errorlevel 1 (
    echo Cook commandlet failed, see "%OUT_DIR%\Cook.log". Aborting packaged build.
    exit /b 1
)

echo Copying config files...
if not exist "%OUT_DIR%\Config" mkdir "%OUT_DIR%\Config"
copy "%HYP_ROOT_DIR%Config\EngineConfig.Web.json" "%OUT_DIR%\Config\EngineConfig.json" >nul
copy "%HYP_ROOT_DIR%Config\GlobalConfig.Web.json" "%OUT_DIR%\Config\GlobalConfig.json" >nul
copy "%HYP_ROOT_DIR%Config\Shaders.hmf" "%OUT_DIR%\Config\" >nul
copy "%HYP_ROOT_DIR%Config\DeviceTiers.hmf" "%OUT_DIR%\Config\" >nul

REM The page fetches the cache files by name, and a web server has no directory listing to offer it.
dir /b "%OUT_DIR%\Cache" > "%OUT_DIR%\index.tmp"
move /Y "%OUT_DIR%\index.tmp" "%OUT_DIR%\Cache\index.txt" >nul

set "WEB_BIN_DIR=%HYP_ROOT_DIR%Binaries\Web\Release"

if "%SKIP_BUILD%"=="1" (
    echo Skipping the WebAssembly build.
) else (
    echo Building...

    REM Config/ and Content/ are packed into hyperion-sample.data at link time, which only reruns when its output is gone.
    del /q "%WEB_BIN_DIR%\hyperion-sample.js" >nul 2>nul

    set "HYP_WEB_PACKAGE_DIR=%OUT_DIR%"

    pushd "%HYP_ROOT_DIR%"
    call "%SCRIPT_DIR%BuildHyperion.bat" Web Regenerate
    set "BUILD_RESULT=!errorlevel!"
    popd

    if not "!BUILD_RESULT!"=="0" (
        echo Build failed, aborting packaged build.
        exit /b 1
    )
)

echo Copying the page...
for %%F in (index.html hyperion-sample.js hyperion-sample.wasm hyperion-sample.data) do (
    if not exist "%WEB_BIN_DIR%\%%F" (
        echo ERROR: "%WEB_BIN_DIR%\%%F" not found.
        exit /b 1
    )

    copy /Y "%WEB_BIN_DIR%\%%F" "%OUT_DIR%\" >nul
)

REM Persist the package directory so ServeWeb.bat can find it.
if not exist "%HYP_ROOT_DIR%PackagedBuilds\Web" mkdir "%HYP_ROOT_DIR%PackagedBuilds\Web"
echo %OUT_DIR%> "%HYP_ROOT_DIR%PackagedBuilds\Web\.hyperion-package"

echo Packaged build created at: %OUT_DIR%
echo Run Tools\Scripts\ServeWeb.bat to serve it.

endlocal
