@echo off

REM Runs a level on the native WebGPU build and records the engine assets and shaders it uses, which is what
REM PackageBuildWeb.bat needs to cook a small package. Needs the Windows build on the WebGPU backend
REM (BuildHyperion.bat webgpu) and one earlier package to run the level from.
REM
REM   RecordWebAssets.bat [-World <name>] [-Project <name or path>] [-Seconds <n>] [-Package <dir>] [-FrameRate <n>]
REM
REM -World    the world to start in, the project's own startup world unless given.
REM -Project  package for the web afterwards with what was recorded. Without it the command to do so is printed.
REM -Seconds  how long each of the two runs lasts, 60 unless given.
REM -FrameRate caps the frame rate of the runs, the web config's own unless given.
REM -Package  the package to run the level from, the last one PackageBuildWeb.bat made unless given.
REM
REM The lists end up in PackagedBuilds\Web\Recorded.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0RecordWebAssets.ps1" %*
exit /b %ERRORLEVEL%
