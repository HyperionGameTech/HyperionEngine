@echo off
setlocal EnableDelayedExpansion

set "SKIP_PRECOMPILE=0"
set "COOK_ASSETS=0"
set "PROJECT_NAME="

:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
IF /I "%~1"=="--skip-precompile" set "SKIP_PRECOMPILE=1"
IF /I "%~1"=="--cook" set "COOK_ASSETS=1"
IF /I "%~1"=="--project" (
    set "PROJECT_NAME=%~2"
    SHIFT
)
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR=%%~fi\"

echo Building...

call "%~dp0BuildHyperion.bat" Shipping Android Regenerate

if errorlevel 1 (
    echo Build failed, aborting packaged build.
    exit /b 1
)

REM Native .so libs land here regardless of shipping (Android CMake build doesn't use a
REM separate output suffix; the Gradle project only looks for a Debug or Release folder).
set "BIN_DIR=%HYP_ROOT_DIR%Binaries\Android\Release"

if not exist "%BIN_DIR%" (
    echo ERROR: Native engine binaries not found at "%BIN_DIR%".
    exit /b 1
)

REM Commandlets can't run on-device, and aren't built for shipping anyway, so shader
REM precompilation and cooking use the Windows host tools ^(built via a normal Release build^).
set "BIN_DIR_RELEASE=%HYP_ROOT_DIR%Binaries\Windows\Release"

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"`) do set "TIMESTAMP=%%i"
set "OUT_DIR=%HYP_ROOT_DIR%PackagedBuilds\Android\Build_%TIMESTAMP%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

if "%SKIP_PRECOMPILE%"=="1" (
    echo Skipping shader precompilation.
) else (
    echo Running PrecompileShaders commandlet...
    "%BIN_DIR_RELEASE%\PrecompileShaders.exe" --platform=android
)

if "%COOK_ASSETS%"=="0" (
    echo Skipping game registry cook, will cook minimal engine cache only.

    "%BIN_DIR_RELEASE%\BlobStorageCookCommandlet.exe" --engine-only=true --out-cache=%OUT_DIR%\Cache --out-content=%OUT_DIR%\Content
    if errorlevel 1 (
        echo Cook commandlet failed, aborting packaged build.
        exit /b 1
    )
) else (
    echo Running Cook commandlet...

    REM Get the project name input from user, pass it concat with Projects/ below:
    if "%PROJECT_NAME%"=="" set /p "PROJECT_NAME=Enter the project name (folder under Projects/): "
    if "%PROJECT_NAME%"=="" (
        echo No project name entered, aborting packaged build.
        exit /b 1
    )

    echo Cooking project: %PROJECT_NAME%

    "%BIN_DIR_RELEASE%\BlobStorageCookCommandlet.exe" --project=Projects/%PROJECT_NAME% --out-cache=%OUT_DIR%\Cache --out-content=%OUT_DIR%\Content
    if errorlevel 1 (
        echo Cook commandlet failed, aborting packaged build.
        exit /b 1
    )
)

echo Assembling package directory...

echo Copying config files...

if not exist "%OUT_DIR%\Config" mkdir "%OUT_DIR%\Config"

echo Copying config files...
if not exist "%OUT_DIR%\Config" mkdir "%OUT_DIR%\Config"
copy "%HYP_ROOT_DIR%Config\*Config.json" "%OUT_DIR%\Config\" >nul 2>nul
copy "%HYP_ROOT_DIR%Config\*Config.Android.json" "%OUT_DIR%\Config\" >nul 2>nul

echo Copying Shaders.hmf...
copy "%HYP_ROOT_DIR%Config\Shaders.hmf" "%OUT_DIR%\Config\" >nul

set "ANDROID_PROJECT=%HYP_ROOT_DIR%Source\PlatformSpecific\Android"

if not exist "%ANDROID_PROJECT%" (
    echo ERROR: Android project not found at "%ANDROID_PROJECT%".
    exit /b 1
)

if not exist "%ANDROID_PROJECT%\local.properties" (
    if defined ANDROID_HOME (
        echo sdk.dir=%ANDROID_HOME:\=/%> "%ANDROID_PROJECT%\local.properties"
    ) else if defined ANDROID_SDK_ROOT (
        echo sdk.dir=%ANDROID_SDK_ROOT:\=/%> "%ANDROID_PROJECT%\local.properties"
    ) else (
        echo sdk.dir=%LOCALAPPDATA:\=/%/Android/Sdk> "%ANDROID_PROJECT%\local.properties"
    )
    echo Generated local.properties
)

echo Building debug APK ^(Gradle^)...

REM Stage fresh native .so files into jniLibs so Gradle bundles the current build,
REM not stale libs left over from a previous build. This mirrors what the Gradle
REM stageNativeLibs task does, but ensures it always runs with the latest engine output.
REM Also remove stale Debug dir which Gradle's stageNativeLibs would prefer over Release.
if exist "%HYP_ROOT_DIR%Binaries\Android\Debug" rd /s /q "%HYP_ROOT_DIR%Binaries\Android\Debug"
set "JNILIB_DIR=%ANDROID_PROJECT%\app\src\main\jniLibs\arm64-v8a"
if not exist "%JNILIB_DIR%" mkdir "%JNILIB_DIR%"
echo Staging engine .so files from %BIN_DIR% ...
del /q "%JNILIB_DIR%\*.so" >nul 2>nul
for %%F in ("%BIN_DIR%\*.so") do (
    copy /Y "%%F" "%JNILIB_DIR%\%%~nxF" >nul
)

echo Packaged build created at: %OUT_DIR%

REM Gradle just mirrors this whole directory into assets/ - see stagePackageAssets in
REM app/build.gradle. hypPackageDir needs forward slashes even on Windows.
set "OUT_DIR_UNIX=%OUT_DIR:\=/%"

REM Persist the package directory so DeployAndroid.bat and Android Studio can find it.
echo %OUT_DIR%> "%ANDROID_PROJECT%\.hyperion-package"

endlocal
