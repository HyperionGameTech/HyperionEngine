@echo off
setlocal EnableDelayedExpansion

set "DEPLOY_ONLY=0"

:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
IF /I "%~1"=="--deploy-only" set "DEPLOY_ONLY=1"
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

REM ---- Resolve repo root ----
set "HYP_ROOT="
if exist "%CD%\Source\CMakeLists.txt" set "HYP_ROOT=%CD%"
if not defined HYP_ROOT (
    pushd "%~dp0..\.." >nul
    set "HYP_ROOT=%CD%"
    popd >nul
)

set "ENGINE_BIN=%HYP_ROOT%\Binaries\Engine"
set "ANDROID_PROJECT=%HYP_ROOT%\Source\PlatformSpecific\Android"
set "JNILIB_DIR=%ANDROID_PROJECT%\app\src\main\jniLibs\arm64-v8a"
set "APK_PATH=%ANDROID_PROJECT%\app\build\outputs\apk\debug\app-debug.apk"
set "PACKAGE_NAME=com.hyperion.engine"
set "ACTIVITY_NAME=com.hyperion.engine.MainActivity"

if not exist "%ANDROID_PROJECT%" (
    echo ERROR: Android project not found at "%ANDROID_PROJECT%".
    exit /b 1
)

REM ---- Locate adb ----
set "ADB_EXE="
where adb >nul 2>&1 && set "ADB_EXE=adb"
if not defined ADB_EXE if defined ANDROID_HOME (
    if exist "%ANDROID_HOME%\platform-tools\adb.exe" set "ADB_EXE=%ANDROID_HOME%\platform-tools\adb.exe"
)
if not defined ADB_EXE if defined ANDROID_SDK_ROOT (
    if exist "%ANDROID_SDK_ROOT%\platform-tools\adb.exe" set "ADB_EXE=%ANDROID_SDK_ROOT%\platform-tools\adb.exe"
)
if not defined ADB_EXE (
    if exist "%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe" set "ADB_EXE=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe"
)
if not defined ADB_EXE (
    echo ERROR: adb not found. Add Android SDK platform-tools to PATH or set ANDROID_HOME.
    exit /b 1
)
echo Using adb: %ADB_EXE%

if "%DEPLOY_ONLY%"=="1" GOTO STEP_INSTALL

echo.
echo ============================================================
echo  Step 1/4: Building native code
echo ============================================================
call "%~dp0BuildHyperion.bat" nowait android
if %ERRORLEVEL% neq 0 (
    echo ERROR: Native build failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Step 2/4: Staging shared libraries
echo ============================================================
if not exist "%JNILIB_DIR%" mkdir "%JNILIB_DIR%"
set "STAGED_COUNT=0"
for %%F in ("%ENGINE_BIN%\*.so") do (
    echo   Copying %%~nxF
    copy /Y "%%F" "%JNILIB_DIR%\%%~nxF" >nul
    set /a STAGED_COUNT+=1
)
if %STAGED_COUNT% equ 0 (
    echo WARNING: No .so files found in %ENGINE_BIN% yet. Continuing without native libs.
)
echo   Staged %STAGED_COUNT% libraries.

echo.
echo ============================================================
echo  Step 3/4: Building APK
echo ============================================================
if not exist "%ANDROID_PROJECT%\local.properties" (
    if defined ANDROID_HOME (
        echo sdk.dir=%ANDROID_HOME:\=/%> "%ANDROID_PROJECT%\local.properties"
    ) else if defined ANDROID_SDK_ROOT (
        echo sdk.dir=%ANDROID_SDK_ROOT:\=/%> "%ANDROID_PROJECT%\local.properties"
    ) else (
        echo sdk.dir=%LOCALAPPDATA:\=/%/Android/Sdk> "%ANDROID_PROJECT%\local.properties"
    )
    echo   Generated local.properties
)
pushd "%ANDROID_PROJECT%"
call gradlew.bat assembleDebug
if %ERRORLEVEL% neq 0 (
    echo ERROR: Gradle build failed.
    popd
    exit /b 1
)
popd

:STEP_INSTALL
echo.
echo ============================================================
echo  Step 4/4: Installing and launching on device
echo ============================================================
"%ADB_EXE%" devices | findstr /R /C:"device$" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: No device found. Ensure a device is connected and USB debugging is enabled
    exit /b 1
)

echo   Installing APK...
"%ADB_EXE%" install -r "%APK_PATH%"
if %ERRORLEVEL% neq 0 (
    echo ERROR: adb install failed.
    exit /b 1
)

echo   Launching %PACKAGE_NAME%...
"%ADB_EXE%" shell am start -n "%PACKAGE_NAME%/%ACTIVITY_NAME%"

echo.
echo ============================================================
echo  App launched on device
echo ============================================================
echo.
echo Tailing logcat ^(Ctrl+C to stop^)...
"%ADB_EXE%" logcat -s "ActivityManager:*" "AndroidRuntime:*" "hyperion:*" "*:F"


REM ---- Resolve paths ----
set "HYP_ROOT="

REM Prefer current directory when script is launched from repo root.
if exist "%CD%\Source\CMakeLists.txt" set "HYP_ROOT=%CD%"

REM Fallback to script-relative resolution.
if not defined HYP_ROOT (
    pushd "%~dp0..\.." >nul
    set "HYP_ROOT=%CD%"
    popd >nul
)

set "ENGINE_BIN=%HYP_ROOT%\Binaries\Engine"
set "ANDROID_PROJECT=%HYP_ROOT%\Source\PlatformSpecific\Android"
set "JNILIB_DIR=%ANDROID_PROJECT%\app\src\main\jniLibs\arm64-v8a"
set "APK_PATH=%ANDROID_PROJECT%\app\build\outputs\apk\debug\app-debug.apk"
set "PACKAGE_NAME=com.hyperion.engine"
set "ACTIVITY_NAME=com.hyperion.engine.MainActivity"

if not exist "%ANDROID_PROJECT%" (
    echo ERROR: Android project not found at "%ANDROID_PROJECT%".
    echo       Current directory was "%CD%" and resolved root was "%HYP_ROOT%".
    exit /b 1
)

REM ---- Locate adb ----
set "ADB_EXE="

where adb >nul 2>&1
if %ERRORLEVEL% equ 0 set "ADB_EXE=adb"

if not defined ADB_EXE if defined ANDROID_HOME (
    if exist "%ANDROID_HOME%\platform-tools\adb.exe" set "ADB_EXE=%ANDROID_HOME%\platform-tools\adb.exe"
)

if not defined ADB_EXE if defined ANDROID_SDK_ROOT (
    if exist "%ANDROID_SDK_ROOT%\platform-tools\adb.exe" set "ADB_EXE=%ANDROID_SDK_ROOT%\platform-tools\adb.exe"
)

if not defined ADB_EXE (
    if exist "%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe" set "ADB_EXE=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe"
)

if not defined ADB_EXE (
    echo ERROR: adb not found.
    echo Looked in PATH, ANDROID_HOME, ANDROID_SDK_ROOT, and %LOCALAPPDATA%\Android\Sdk.
    exit /b 1
)

echo Using adb: %ADB_EXE%

REM ---- Optional native build and staging ----
if "%DEPLOY_ONLY%"=="1" GOTO STEP_INSTALL

if "%WITH_NATIVE%"=="1" (
    if "%SKIP_BUILD%"=="0" (
        echo.
        echo ============================================================
        echo  Step 1/4: Building native code (Ninja)
        echo ============================================================
        call "%~dp0BuildHyperion.bat" nowait android
        if %ERRORLEVEL% neq 0 (
            echo ERROR: Native build failed.
            exit /b 1
        )
    )

    echo.
    echo ============================================================
    echo  Step 2/4: Staging shared libraries
    echo ============================================================
    if not exist "%JNILIB_DIR%" mkdir "%JNILIB_DIR%"

    set "STAGED_COUNT=0"
    for %%F in ("%ENGINE_BIN%\*.so") do (
        echo   Copying %%~nxF
        copy /Y "%%F" "%JNILIB_DIR%\%%~nxF" >nul
        set /a STAGED_COUNT+=1
    )

    if %STAGED_COUNT% equ 0 (
        echo WARNING: No .so files found in %ENGINE_BIN%.
        echo          Continuing without native libs.
    )

    echo   Staged %STAGED_COUNT% libraries into jniLibs\arm64-v8a\
)

REM ---- Step 3: Gradle assemble ----
echo.
echo ============================================================
echo  Step 3/4: Building APK (Gradle)
echo ============================================================

REM Generate local.properties if it doesn't exist
if not exist "%ANDROID_PROJECT%\local.properties" (
    if defined ANDROID_HOME (
        echo sdk.dir=%ANDROID_HOME:\=/%> "%ANDROID_PROJECT%\local.properties"
        echo   Generated local.properties with ANDROID_HOME
    ) else if defined ANDROID_SDK_ROOT (
        echo sdk.dir=%ANDROID_SDK_ROOT:\=/%> "%ANDROID_PROJECT%\local.properties"
        echo   Generated local.properties with ANDROID_SDK_ROOT
    ) else (
        echo sdk.dir=%LOCALAPPDATA:\=/%/Android/Sdk> "%ANDROID_PROJECT%\local.properties"
        echo   Generated local.properties with default SDK path
    )
)

pushd "%ANDROID_PROJECT%"

REM Check for gradle-wrapper.jar; if missing, guide the user
if not exist "gradle\wrapper\gradle-wrapper.jar" (
    echo.
    echo WARNING: gradle-wrapper.jar not found.
    echo   Run the following once to bootstrap it:
    echo     cd "%ANDROID_PROJECT%"
    echo     gradle wrapper
    echo   Or download the wrapper JAR from https://services.gradle.org
    echo.
    echo   Attempting to use system Gradle instead...
    
    where gradle >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Neither Gradle wrapper nor system Gradle found.
        popd
        exit /b 1
    )
    set "GRADLE_CMD=gradle"
) else (
    set "GRADLE_CMD=call gradlew.bat"
)

%GRADLE_CMD% assembleDebug
if %ERRORLEVEL% neq 0 (
    echo ERROR: Gradle build failed.
    popd
    exit /b 1
)
popd

:STEP_INSTALL
REM ---- Step 4: Install and launch ----
echo.
echo ============================================================
echo  Step 4/4: Installing and launching on device
echo ============================================================

REM Check for a connected device
"%ADB_EXE%" devices | findstr /R /C:"device$" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: No Android device detected. Make sure USB debugging is enabled
    echo        and the device is connected.
    exit /b 1
)

echo   Installing APK...
"%ADB_EXE%" install -r "%APK_PATH%"
if %ERRORLEVEL% neq 0 (
    echo ERROR: adb install failed.
    exit /b 1
)

echo   Launching %PACKAGE_NAME%...
"%ADB_EXE%" shell am start -n "%PACKAGE_NAME%/%ACTIVITY_NAME%"

echo.
echo ============================================================
echo  Done. App launched on device.
echo ============================================================

REM Tail logcat for native output (Ctrl+C to exit)
echo.
echo Tailing logcat (Ctrl+C to stop)...
"%ADB_EXE%" logcat -s "ActivityManager:*" "AndroidRuntime:*" "hyperion:*" "*:F"
