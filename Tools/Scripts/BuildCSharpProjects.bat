@echo off
SETLOCAL EnableDelayedExpansion
SET "CONFIG=Release"
IF NOT "%~1"=="" SET "CONFIG=%~1"

REM Windows only because batch
SET "CURR_PLATFORM=Windows"

SET "projects=Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop Hyperion.NET.Scripting Hyperion.NET.Editor"

pushd Build
SET "buildDir=%CD%"
pushd CSharpProjects

FOR %%p IN (%projects%) DO (
    echo Building %%p in %CONFIG% configuration...
    pushd "%%p"
    dotnet build --disable-build-servers --no-restore --configuration %CONFIG%
    IF !ERRORLEVEL! NEQ 0 (
        echo Failed to build %%p
        exit /b 1
    )

    IF NOT EXIST "%buildDir%\..\Binaries\%CURR_PLATFORM%\%CONFIG%" mkdir "%buildDir%\..\Binaries\%CURR_PLATFORM%\%CONFIG%"

    SET "DSTPATH=%buildDir%\..\Binaries\%CURR_PLATFORM%\%CONFIG%\%%p.dll"
    SET "SRCPATH=bin\%CONFIG%\net9.0\%%p.dll"

    echo Copying %%p.dll to "!DSTPATH!"
    copy "!SRCPATH!" "!DSTPATH!"
    popd
)

popd
popd
ENDLOCAL
