@echo off
setlocal enabledelayedexpansion

REM Get absolute repo root (two levels up from Tools\Scripts\)
for %%i in ("%~dp0..\..\") do set "REPO_ROOT=%%~fi"
REM Strip trailing backslash
if "%REPO_ROOT:~-1%"=="\" set "REPO_ROOT=%REPO_ROOT:~0,-1%"

echo Repo root: %REPO_ROOT%
echo.
echo This script renames subdirectories of Source/Core and Source/Engine to TitleCase.
echo It does this in two git commits to work around Git's case-insensitive filesystem.
echo.

pushd "%REPO_ROOT%"

echo === Step 1: Rename directories to temporary names ===
echo.

REM Process bottom-up (sort /r = deepest paths first) so children are renamed before parents
for /f "delims=" %%D in ('dir /b /s /ad "Source\Core" "Source\Engine" 2^>nul ^| sort /r') do (
    set "FULLPATH=%%D"
    set "DIRNAME=%%~nxD"
    set "DPPATH=%%~dpD"

    REM Compute TitleCase via PowerShell
    for /f "delims=" %%T in ('powershell -NoProfile -Command "(Get-Culture).TextInfo.ToTitleCase('!DIRNAME!'.ToLower())"') do set "TITLED=%%T"

    if /i not "!DIRNAME!"=="!TITLED!" (
        set "REL_OLD=!FULLPATH:%REPO_ROOT%\=!"
        set "REL_TMP=!DPPATH:%REPO_ROOT%\=!!DIRNAME!_RENAMETMP"
        echo   !REL_OLD! -^> !DIRNAME!_RENAMETMP
        git mv "!REL_OLD!" "!REL_TMP!"
    )
)

echo.
git commit -m "Rename subdirs to temp names (step 1/2)"

echo.
echo === Step 2: Rename temporary names to final TitleCase names ===
echo.

REM _RENAMETMP is 10 chars; check suffix with ~-10
for /f "delims=" %%D in ('dir /b /s /ad "Source\Core" "Source\Engine" 2^>nul ^| sort /r') do (
    set "FULLPATH=%%D"
    set "DIRNAME=%%~nxD"
    set "DPPATH=%%~dpD"

    REM Only process dirs that end in _RENAMETMP
    if "!DIRNAME:~-10!"=="_RENAMETMP" (
        set "ORIGNAME=!DIRNAME:~0,-10!"

        for /f "delims=" %%T in ('powershell -NoProfile -Command "(Get-Culture).TextInfo.ToTitleCase('!ORIGNAME!'.ToLower())"') do set "TITLED=%%T"

        set "REL_TMP=!FULLPATH:%REPO_ROOT%\=!"
        set "REL_FINAL=!DPPATH:%REPO_ROOT%\=!!TITLED!"
        echo   !DIRNAME! -^> !TITLED!
        git mv "!REL_TMP!" "!REL_FINAL!"
    )
)

echo.
git commit -m "Rename subdirs to TitleCase (step 2/2)"

popd

echo.