@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%..\.."

if not exist ".gitmodules" (
    echo [ERROR] .gitmodules not found in "%CD%"
    popd
    exit /b 1
)

echo Updating submodules...

for /f "tokens=2" %%P in ('git config --file .gitmodules --get-regexp "submodule\..*\.path"') do (
    call :process_submodule "%%P"
)

popd
echo.
echo Done.
exit /b 0

:process_submodule
setlocal
set "SM_PATH=%~1"
set "SM_URL="
set "SM_BRANCH="
set "SM_COMMIT="

for /f "tokens=*" %%U in ('git config --file .gitmodules --get "submodule.%SM_PATH%.url" 2^>nul') do set "SM_URL=%%U"
for /f "tokens=*" %%B in ('git config --file .gitmodules --get "submodule.%SM_PATH%.branch" 2^>nul') do set "SM_BRANCH=%%B"
for /f "tokens=*" %%C in ('git config --file .gitmodules --get "submodule.%SM_PATH%.commit" 2^>nul') do set "SM_COMMIT=%%C"

echo.
if not defined SM_URL (
    echo [WARNING] No URL found for submodule: %SM_PATH%
    endlocal
    exit /b 1
)

set "SM_IS_REPO="
if exist "%SM_PATH%\.git" set "SM_IS_REPO=1"

if defined SM_IS_REPO (
    echo [UPDATE] %SM_PATH%
    git -C "%SM_PATH%" fetch --all --tags --prune
    if defined SM_BRANCH (
        git -C "%SM_PATH%" checkout "%SM_BRANCH%"
        git -C "%SM_PATH%" pull origin "%SM_BRANCH%"
    ) else if defined SM_COMMIT (
        git -C "%SM_PATH%" checkout "%SM_COMMIT%"
    ) else (
        git -C "%SM_PATH%" pull
    )
) else (
    if exist "%SM_PATH%\" (
        dir /a /b "%SM_PATH%" | findstr "^" >nul
        if not errorlevel 1 (
            echo [WARNING] %SM_PATH% exists but is not a git checkout and is not empty; skipping. Remove it manually to allow cloning.
            endlocal
            exit /b 1
        )
        rmdir "%SM_PATH%"
    )

    echo [CLONE] %SM_PATH%  ^<--  %SM_URL%
    if defined SM_BRANCH (
        git clone --branch "%SM_BRANCH%" -- "%SM_URL%" "%SM_PATH%"
    ) else (
        git clone -- "%SM_URL%" "%SM_PATH%"
        if defined SM_COMMIT (
            git -C "%SM_PATH%" checkout "%SM_COMMIT%"
        )
    )
)
endlocal
exit /b 0
