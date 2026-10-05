@echo off
REM Builds the static third-party libraries that "BuildHyperion.bat distribution" links into hyperion.dll.

IF NOT DEFINED VCPKG_ROOT (
    echo VCPKG_ROOT environment variable is not set. Please set it to the path of your vcpkg installation.
    exit /b 1
)

"%VCPKG_ROOT%\vcpkg.exe" install zlib:x64-windows-hyp-static openal-soft:x64-windows-hyp-static freetype:x64-windows-hyp-static "curl[ssl,sspi,non-http]:x64-windows-hyp-static" --overlay-triplets="%~dp0..\vcpkg\triplets"
