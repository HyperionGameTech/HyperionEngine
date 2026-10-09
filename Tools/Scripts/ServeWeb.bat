@echo off
setlocal EnableDelayedExpansion

REM Serves a packaged web build for local testing.
REM
REM   ServeWeb.bat [package dir] [port]
REM
REM Without a package dir, serves the one PackageBuildWeb.bat made last. The port defaults to 8080.

for %%i in ("%~dp0..\..") do set "HYP_ROOT_DIR=%%~fi\"

set "PACKAGE_DIR=%~1"
set "PORT=%~2"
if "%PORT%"=="" set "PORT=8080"

if "%PACKAGE_DIR%"=="" (
    if exist "%HYP_ROOT_DIR%PackagedBuilds\Web\.hyperion-package" (
        set /p PACKAGE_DIR=<"%HYP_ROOT_DIR%PackagedBuilds\Web\.hyperion-package"
    )
)

if "%PACKAGE_DIR%"=="" (
    echo ERROR: No package dir given and none recorded. Run PackageBuildWeb.bat first.
    exit /b 1
)

if not exist "%PACKAGE_DIR%\index.html" (
    echo ERROR: "%PACKAGE_DIR%" holds no index.html.
    exit /b 1
)

echo Serving %PACKAGE_DIR%
echo Open http://localhost:%PORT%/ in Chrome or Edge. Ctrl+C stops the server.

python "%~dp0ServeWeb.py" "%PACKAGE_DIR%" "%PACKAGE_DIR%" %PORT%

endlocal
