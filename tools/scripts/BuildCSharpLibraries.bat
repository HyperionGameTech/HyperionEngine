@echo off
SETLOCAL EnableDelayedExpansion
SET "CONFIG=Release"
IF NOT "%~1"=="" SET "CONFIG=%~1"

SET "projects=HyperionCore HyperionRuntime HyperionInterop HyperionScripting"

pushd build
SET "buildDir=%CD%"
pushd CSharpProjects

FOR %%p IN (%projects%) DO (
    echo Building %%p...
    pushd "%%p"
    dotnet build --disable-build-servers --no-restore --configuration %CONFIG%
    IF !ERRORLEVEL! NEQ 0 (
        echo Failed to build %%p
        exit /b 1
    )

    IF NOT EXIST "%buildDir%\bin" mkdir "%buildDir%\bin"

    SET "DSTPATH=%buildDir%\bin\%%p.dll"
    SET "SRCPATH=bin\%CONFIG%\net8.0\%%p.dll"

    echo Copying %%p.dll to "!DSTPATH!"
    copy "!SRCPATH!" "!DSTPATH!"
    popd
)

popd
popd
ENDLOCAL