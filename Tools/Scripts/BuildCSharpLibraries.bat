@echo off
SETLOCAL EnableDelayedExpansion
SET "CONFIG=Release"
IF NOT "%~1"=="" SET "CONFIG=%~1"

SET "projects=Hyperion.NET.Shared Hyperion.NET.Runtime Hyperion.NET.Interop Hyperion.NET.Scripting"

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

    IF NOT EXIST "%buildDir%\..\Binaries\Engine" mkdir "%buildDir%\..\Binaries\Engine"

    SET "DSTPATH=%buildDir%\..\Binaries\Engine\%%p.dll"
    SET "SRCPATH=Binaries\%CONFIG%\net9.0\%%p.dll"

    echo Copying %%p.dll to "!DSTPATH!"
    copy "!SRCPATH!" "!DSTPATH!"
    popd
)

popd
popd
ENDLOCAL