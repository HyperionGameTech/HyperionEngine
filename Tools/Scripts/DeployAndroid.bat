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

REM Resolve repo root -- pushd/popd works reliably across invocation methods
pushd "%~dp0..\.." >nul 2>&1
set "HYP_ROOT_DIR=%CD%\"
popd >nul 2>&1

set "PACKAGE_NAME=com.hyperion.engine"
set "ACTIVITY_NAME=com.hyperion.engine.MainActivity"

set "ANDROID_PROJECT=%HYP_ROOT_DIR%Source\PlatformSpecific\Android"
set "GRADLE_APK=%ANDROID_PROJECT%\app\build\outputs\apk\debug\app-debug.apk"
set "BUILDS_DIR=%HYP_ROOT_DIR%PackagedBuilds\Android"

set "APK_PATH="

if exist "%BUILDS_DIR%" (
    for /f "delims=" %%D in ('dir "%BUILDS_DIR%" /ad /b /o-n 2^>nul') do (
        for %%F in ("%BUILDS_DIR%\%%D\*.apk") do (
            set "APK_PATH=%%F"
            goto APK_FOUND
        )
    )
)

if exist "%GRADLE_APK%" set "APK_PATH=%GRADLE_APK%"

:APK_FOUND
if not defined APK_PATH (
    echo ERROR: No APK found. Build one with PackageBuildAndroid.bat first, or
    echo        build the Gradle project with assembleDebug.
    exit /b 1
)

echo APK: %APK_PATH%

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

echo Installing APK...
"%ADB_EXE%" install -r "%APK_PATH%"
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

REM ---- Locate NDK ----
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

REM Find lldb-server for aarch64 inside whatever clang version dir exists
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

echo   LLDB will attach once the app starts.
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
    echo echo LLDB connected. Run these commands when you hit a breakpoint or crash:
    echo echo   thread list     - list all threads
    echo echo   bt              - show call stack
    echo echo   thread backtrace all - show all thread stacks
    echo echo   continue        - resume
    echo echo   quit            - exit
    echo echo.
    echo echo App is waiting for debugger. Run:
    echo echo   process attach --name %PACKAGE_NAME%
    echo echo   continue
    echo echo.
) > "%LLDB_INIT%"

start "Hyperion LLDB" "%LLDB_HOST%" -s "%LLDB_INIT%"

goto :EOF
