@echo off
setlocal EnableDelayedExpansion

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

echo Running PrecompileShaders commandlet...

REM Compile shaders for only Android
"%BIN_DIR_RELEASE%\PrecompileShaders.exe" --platform=android

echo Running Cook commandlet...

REM Get the project name input from user, pass it concat with Projects/ below:
set /p "PROJECT_NAME=Enter the project name (folder under Projects/): "
if "%PROJECT_NAME%"=="" (
    echo No project name entered, aborting packaged build.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"`) do set "TIMESTAMP=%%i"
set "OUT_DIR=%HYP_ROOT_DIR%PackagedBuilds\Android\Build_%TIMESTAMP%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Cooking project: %PROJECT_NAME%

REM Content is written next to whatever --CacheDir resolves to (EngineGlobals::GetCacheDirectory()
REM .BasePath() / "Content"), so pointing CacheDir at OUT_DIR\Cache lands both Cache and Content
REM directly in the package dir - no separate Content copy step needed.
"%BIN_DIR_RELEASE%\BlobStorageCookCommandlet.exe" --content=Projects/%PROJECT_NAME% --CacheDir=%OUT_DIR%\Cache
if errorlevel 1 (
    echo Cook commandlet failed, aborting packaged build.
    exit /b 1
)

echo Assembling package directory...

echo Copying config files...
REM Prefer the Android-specific variant of each config (EngineConfig.Android.json etc.),
REM falling back to the platform-agnostic file, and write everything under the base name -
REM this mirrors what CopyPlatformConfigs.cmake does for the desktop targets.
set "KNOWN_PLATFORMS=IOS Android Windows Mac Linux"
for %%F in ("%HYP_ROOT_DIR%Config\*.json") do (
    set "FN=%%~nxF"
    set "SKIP=0"
    for %%P in (%KNOWN_PLATFORMS%) do (
        if /I not "%%P"=="Android" (
            echo !FN!| findstr /I /E /C:".%%P.json" >nul
            if not errorlevel 1 set "SKIP=1"
        )
    )
    if "!SKIP!"=="0" (
        echo !FN!| findstr /I /E /C:".Android.json" >nul
        if not errorlevel 1 (
            set "BASE=!FN:.Android.json=.json!"
            copy "%%F" "%OUT_DIR%\!BASE!" >nul
        ) else (
            set "PLATFORM_FILE=%HYP_ROOT_DIR%Config\!FN:.json=.Android.json!"
            if exist "!PLATFORM_FILE!" (
                copy "!PLATFORM_FILE!" "%OUT_DIR%\!FN!" >nul
            ) else (
                copy "%%F" "%OUT_DIR%\!FN!" >nul
            )
        )
    )
)

echo Copying Shaders.ini...
if not exist "%OUT_DIR%\Source\Shaders" mkdir "%OUT_DIR%\Source\Shaders"
copy "%HYP_ROOT_DIR%Source\Shaders\Shaders.ini" "%OUT_DIR%\Source\Shaders\" >nul

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

echo Building release APK ^(Gradle^)...

REM Gradle just mirrors this whole directory into assets/ - see stagePackageAssets in
REM app/build.gradle. hypPackageDir needs forward slashes even on Windows.
set "OUT_DIR_UNIX=%OUT_DIR:\=/%"

pushd "%ANDROID_PROJECT%"

if not exist "gradle\wrapper\gradle-wrapper.jar" (
    echo WARNING: gradle-wrapper.jar not found, falling back to system Gradle.
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

%GRADLE_CMD% assembleRelease "-PhypPackageDir=%OUT_DIR_UNIX%"
if errorlevel 1 (
    echo Gradle build failed, aborting packaged build.
    popd
    exit /b 1
)
popd

echo Copying APK...
set "APK_SRC_DIR=%ANDROID_PROJECT%\app\build\outputs\apk\release"
set "APK_FOUND=0"
for %%F in ("%APK_SRC_DIR%\*.apk") do (
    copy "%%F" "%OUT_DIR%\" >nul
    set "APK_FOUND=1"
)
if "%APK_FOUND%"=="0" (
    echo ERROR: No APK found in "%APK_SRC_DIR%".
    exit /b 1
)

echo Cleaning up staged package contents ^(already baked into the APK^)...
if exist "%OUT_DIR%\Content" rd /s /q "%OUT_DIR%\Content"
if exist "%OUT_DIR%\Cache" rd /s /q "%OUT_DIR%\Cache"
if exist "%OUT_DIR%\Source" rd /s /q "%OUT_DIR%\Source"
del /q "%OUT_DIR%\*.json" >nul 2>nul

echo Done! Packaged build created at: %OUT_DIR%
endlocal
