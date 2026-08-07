@echo off
setlocal EnableDelayedExpansion

set "DEBUG_MODE=0"
if /I "%HYP_DEPLOY_DEBUG%"=="1" set "DEBUG_MODE=1"

:PARSE_ARGS
IF "%~1"=="" GOTO END_PARSE_ARGS
IF /I "%~1"=="--debug" set "DEBUG_MODE=1"
IF /I "%~1"=="-d"     set "DEBUG_MODE=1"
SHIFT
GOTO PARSE_ARGS
:END_PARSE_ARGS

REM Resolve repo root
pushd "%~dp0..\.." >nul 2>&1
set "HYP_ROOT_DIR=%CD%\"
popd >nul 2>&1

set "PACKAGE_NAME=com.hyperion.engine"
set "ACTIVITY_NAME=com.hyperion.engine.MainActivity"

set "ANDROID_PROJECT=%HYP_ROOT_DIR%Source\PlatformSpecific\Android"
set "BUILDS_DIR=%HYP_ROOT_DIR%PackagedBuilds\Android"
set "GRADLE_APK=%ANDROID_PROJECT%\app\build\outputs\apk\debug\app-debug.apk"
set "BIN_DIR=%HYP_ROOT_DIR%Binaries\Android\Release"

REM ---- Find latest packaged build that still has Content ----
set "PACKAGE_DIR="

REM Read from the persistent file written by PackageBuildAndroid.bat
set "PKG_FILE=%ANDROID_PROJECT%\.hyperion-package"
if exist "%PKG_FILE%" (
    for /f "usebackq delims=" %%D in ("%PKG_FILE%") do (
        if exist "%%D\Content" (
            set "PACKAGE_DIR=%%D"
        )
    )
)

REM Fallback: scan PackagedBuilds for latest with Content
if not defined PACKAGE_DIR if exist "%BUILDS_DIR%" (
    for /f "delims=" %%D in ('dir "%BUILDS_DIR%" /ad /b /o-n 2^>nul') do (
        if exist "%BUILDS_DIR%\%%D\Content" (
            set "PACKAGE_DIR=%BUILDS_DIR%\%%D"
            goto PACKAGE_FOUND
        )
    )
)

:PACKAGE_FOUND
if not defined PACKAGE_DIR (
    echo ERROR: No packaged build with Content found.
    echo        Run PackageBuildAndroid.bat first to build and cook content.
    exit /b 1
)

echo Package dir: %PACKAGE_DIR%

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

echo Checking for device...
"%ADB_EXE%" devices | findstr /R /C:"device$" >nul 2>&1
if errorlevel 1 (
    echo ERROR: No device found. Ensure a device is connected and USB debugging is enabled.
    exit /b 1
)

REM ---- Stage fresh engine .so files & clean Gradle caches ----
REM Always use the latest engine build, even if the packaged content is from an
REM older build. This is what lets you iterate on engine code without re-cooking.
echo Staging engine .so files from %BIN_DIR% ...

set "JNILIB_DIR=%ANDROID_PROJECT%\app\src\main\jniLibs\arm64-v8a"
if not exist "%JNILIB_DIR%" mkdir "%JNILIB_DIR%"
del /q "%JNILIB_DIR%\*.so" >nul 2>nul
for %%F in ("%BIN_DIR%\*.so") do (
    copy /Y "%%F" "%JNILIB_DIR%\%%~nxF" >nul
)

REM Nuke stale Debug dir (would be preferred by stageNativeLibs over Release)
if exist "%HYP_ROOT_DIR%Binaries\Android\Debug" rd /s /q "%HYP_ROOT_DIR%Binaries\Android\Debug"

REM ---- Rebuild Gradle APK with packaged content ----
echo Building APK with content from %PACKAGE_DIR% ...

pushd "%ANDROID_PROJECT%"

REM Clean stale Gradle caches to ensure a fresh APK
if exist "app\.cxx" rd /s /q "app\.cxx"
if exist "app\build" rd /s /q "app\build"
if exist ".gradle\configuration-cache" rd /s /q ".gradle\configuration-cache"

set "PACKAGE_DIR_UNIX=%PACKAGE_DIR:\=/%"

if not exist "gradle\wrapper\gradle-wrapper.jar" (
    where gradle >nul 2>&1
    if errorlevel 1 (
        echo ERROR: Neither Gradle wrapper nor system Gradle found.
        popd
        exit /b 1
    )
    set "GRADLE_CMD=gradle"
) else (
    set "GRADLE_CMD=call gradlew.bat"
)

%GRADLE_CMD% assembleDebug "-PhypPackageDir=%PACKAGE_DIR_UNIX%" --no-configuration-cache
if errorlevel 1 (
    echo ERROR: Gradle build failed.
    popd
    exit /b 1
)
popd

if not exist "%GRADLE_APK%" (
    echo ERROR: Gradle APK not found after build at "%GRADLE_APK%".
    exit /b 1
)

REM ---- Deploy ----
echo Installing APK...
"%ADB_EXE%" install -r "%GRADLE_APK%"
if errorlevel 1 (
    echo ERROR: adb install failed.
    exit /b 1
)

if "%DEBUG_MODE%"=="1" (
    call :SETUP_LLDB
)

echo Launching %PACKAGE_NAME%...
if "%DEBUG_MODE%"=="1" (
    "%ADB_EXE%" shell am start -D -n "%PACKAGE_NAME%/%ACTIVITY_NAME%"
) else (
    "%ADB_EXE%" shell am start -n "%PACKAGE_NAME%/%ACTIVITY_NAME%"
)

echo.
echo Tailing logcat (Ctrl+C to stop)...

:LOG_LOOP
timeout /t 1 /nobreak >nul
for /f "tokens=2 delims= " %%i in ('"%ADB_EXE%" shell pidof %PACKAGE_NAME% 2^>nul') do set "APP_PID=%%i"
if defined APP_PID (
    echo [app running, pid %APP_PID%]
    "%ADB_EXE%" logcat --pid=%APP_PID%
) else (
    echo [app not running, watching for crashes]
    "%ADB_EXE%" logcat -s "hyperion:*" "HyLog:*" "DEBUG:*" "AndroidRuntime:*" "*:F"
)
goto LOG_LOOP

endlocal
goto :EOF

REM ============================================================
REM  LLDB Debugger Setup
REM ============================================================
:SETUP_LLDB
echo.
echo ============================================================
echo  Setting up LLDB debugger
echo ============================================================

set "NDK_DIR="
if defined ANDROID_NDK_HOME (
    if exist "%ANDROID_NDK_HOME%\toolchains\llvm\prebuilt\windows-x86_64\bin\lldb.exe" (
        set "NDK_DIR=%ANDROID_NDK_HOME%"
    )
)
if not defined NDK_DIR if defined ANDROID_HOME (
    pushd "%ANDROID_HOME%\ndk" 2>nul && (
        for /f "delims=" %%D in ('dir /ad /b /o-n 2^>nul') do (
            if exist "%%D\toolchains\llvm\prebuilt\windows-x86_64\bin\lldb.exe" (
                set "NDK_DIR=%ANDROID_HOME%\ndk\%%D"
                goto NDK_FOUND
            )
        )
        popd
    )
)
if not defined NDK_DIR (
    pushd "%LOCALAPPDATA%\Android\Sdk\ndk" 2>nul && (
        for /f "delims=" %%D in ('dir /ad /b /o-n 2^>nul') do (
            if exist "%%D\toolchains\llvm\prebuilt\windows-x86_64\bin\lldb.exe" (
                set "NDK_DIR=%LOCALAPPDATA%\Android\Sdk\ndk\%%D"
                goto NDK_FOUND
            )
        )
        popd
    )
)

:NDK_FOUND
if not defined NDK_DIR (
    echo WARNING: NDK not found. Skipping LLDB setup.
    echo   Set ANDROID_NDK_HOME to point at your NDK installation.
    set "DEBUG_MODE=0"
    goto :EOF
)

echo   NDK: %NDK_DIR%
set "LLDB_HOST=%NDK_DIR%\toolchains\llvm\prebuilt\windows-x86_64\bin\lldb.exe"

set "LLDB_SERVER="
for /d %%C in ("%NDK_DIR%\toolchains\llvm\prebuilt\windows-x86_64\lib\clang\*") do (
    if exist "%%C\lib\linux\aarch64\lldb-server" (
        set "LLDB_SERVER=%%C\lib\linux\aarch64\lldb-server"
    )
)
if not defined LLDB_SERVER (
    echo ERROR: lldb-server for aarch64 not found in NDK.
    set "DEBUG_MODE=0"
    goto :EOF
)

echo   lldb-server: %LLDB_SERVER%
echo   Pushing lldb-server to device...
"%ADB_EXE%" push "%LLDB_SERVER%" /data/local/tmp/lldb-server >nul
if errorlevel 1 (
    echo   WARNING: Failed to push lldb-server, skipping debugger.
    set "DEBUG_MODE=0"
    goto :EOF
)
"%ADB_EXE%" shell chmod 755 /data/local/tmp/lldb-server >nul

echo   Starting lldb-server on device...
"%ADB_EXE%" shell /data/local/tmp/lldb-server platform --server --listen "*:5039" >nul 2>&1 &
timeout /t 1 /nobreak >nul

echo   Forwarding port 5039...
"%ADB_EXE%" forward tcp:5039 tcp:5039

echo   Once lldb is connected:
echo     - thread list              - list all threads
echo     - bt                       - show call stack
echo     - thread backtrace all     - show all thread stacks
echo     - continue                 - resume execution
echo.
echo   Launching lldb...

set "LLDB_INIT=%TEMP%\hyperion_lldb_init.txt"
(
    echo platform select remote-android
    echo platform connect connect://localhost:5039
    echo settings set target.inherit-tcc true
    echo.
    echo echo LLDB connected. Run:
    echo echo   thread backtrace all   - all thread stacks
    echo echo   bt                     - current thread stack
    echo echo   continue               - resume execution
    echo echo.
    echo echo App is waiting for debugger. Run:
    echo echo   process attach --name %PACKAGE_NAME%
    echo echo   continue
    echo echo.
) > "%LLDB_INIT%"

start "Hyperion LLDB" "%LLDB_HOST%" -s "%LLDB_INIT%"
goto :EOF
