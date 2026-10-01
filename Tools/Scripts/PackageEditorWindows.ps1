<#
.SYNOPSIS
    Stages a relocatable Windows editor preview build from Binaries/Windows/<Configuration> and zips it.

.DESCRIPTION
    Only files on an explicit allowlist are staged: the editor and its managed deps (from Hyperion.Editor.deps.json),
    a handful of runtime executables, and the native DLLs they import (resolved with dumpbin). Local leftovers such as
    Projects/, Cache/, debug vcpkg DLLs and PDBs are never picked up.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools/Scripts/PackageEditorWindows.ps1 -Symbols
#>
param(
    [string]$Configuration = "Release",
    [string]$OutputDir,
    [switch]$Build,
    [switch]$IncludeShaderCache,
    [switch]$Symbols,
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RootDir = (Resolve-Path "$PSScriptRoot/../..").Path
$BinDir = Join-Path $RootDir "Binaries/Windows/$Configuration"
if (-not $OutputDir) { $OutputDir = Join-Path $RootDir "PackagedBuilds/EditorPreview" }

# Executables shipped alongside the editor. CacheServerCommandlet is launched by play-in-editor.
$RuntimeExecutables = @(
    "Hyperion.Editor.exe",
    "hyperion-sample.exe",
    "CacheServerCommandlet.exe",
    "PrecompileShaders.exe",
    "BlobStorageCookCommandlet.exe",
    "CookTexturesCommandlet.exe",
    "stratac.exe"
)

# Loaded through DllImport / LoadLibrary rather than the import table, so dumpbin won't find them from the executables.
$DynamicallyLoadedDlls = @(
    "hyperion.dll",
    "Hyperion.NET.Scripting.dll"
)

$ManagedEditorFiles = @(
    "Hyperion.Editor.dll",
    "Hyperion.Editor.deps.json",
    "Hyperion.Editor.runtimeconfig.json"
)

$Warnings = [System.Collections.Generic.List[string]]::new()

function Write-Step([string]$Message) { Write-Host "==> $Message" -ForegroundColor Cyan }
function Add-Warning([string]$Message) { $Warnings.Add($Message); Write-Host "WARNING: $Message" -ForegroundColor Yellow }

function Get-EngineVersion
{
    $cmakeText = Get-Content (Join-Path $RootDir "Source/CMakeLists.txt") -Raw
    $parts = foreach ($component in "MAJOR", "MINOR", "PATCH")
    {
        if ($cmakeText -notmatch "set\(HYP_VERSION_$component\s+(\d+)\)") { throw "HYP_VERSION_$component not found in Source/CMakeLists.txt" }
        $Matches[1]
    }
    return $parts -join "."
}

function Get-VisualStudioPath
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; Visual Studio with the C++ workload is required" }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw "No Visual Studio installation with the C++ toolset found" }
    return $vsPath
}

function Get-DumpbinPath([string]$VsPath)
{
    $dumpbin = Get-ChildItem (Join-Path $VsPath "VC/Tools/MSVC") -Recurse -Filter dumpbin.exe |
        Where-Object { $_.FullName -match "Hostx64\\x64" } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $dumpbin) { throw "dumpbin.exe not found under $VsPath" }
    return $dumpbin.FullName
}

function Get-CrtRedistDir([string]$VsPath)
{
    $crtDir = Get-ChildItem (Join-Path $VsPath "VC/Redist/MSVC") -Directory |
        Where-Object { $_.Name -match "^\d+\.\d+\.\d+$" } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } |
        Select-Object -First 1
    if (-not $crtDir) { throw "No x64 VC++ CRT redist found under $VsPath/VC/Redist/MSVC" }
    return $crtDir.FullName
}

function Get-NativeImports([string]$Dumpbin, [string]$FilePath)
{
    & $Dumpbin /nologo /dependents $FilePath |
        Where-Object { $_ -match "^\s+(\S+\.dll)\s*$" } |
        ForEach-Object { $Matches[1] }
}

function Copy-StagedFile([string]$Source, [string]$Destination)
{
    $destinationDir = Split-Path $Destination -Parent
    if (-not (Test-Path $destinationDir)) { New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null }
    [System.IO.File]::Copy($Source, $Destination, $true)
}

function Copy-TrackedFiles([string[]]$RepoPaths, [string]$StageDir)
{
    Push-Location $RootDir
    try
    {
        $trackedFiles = git -c core.quotepath=off ls-files -- $RepoPaths
        if ($LASTEXITCODE -ne 0) { throw "git ls-files failed" }
    }
    finally { Pop-Location }

    $copied = 0
    foreach ($relativePath in $trackedFiles)
    {
        $source = Join-Path $RootDir $relativePath
        # tracked but deleted in the working tree
        if (-not (Test-Path -LiteralPath $source)) { continue }
        Copy-StagedFile $source (Join-Path $StageDir $relativePath)
        $copied++
    }
    return $copied
}

function Copy-Directory([string]$Source, [string]$Destination)
{
    robocopy $Source $Destination /E /NFL /NDL /NJH /NJS /NP | Out-Null
    # robocopy exit codes below 8 are success
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed copying $Source (exit $LASTEXITCODE)" }
    $global:LASTEXITCODE = 0
}

$Version = Get-EngineVersion
$StageName = "Hyperion-Editor-$Version-win64"
$StageDir = Join-Path $OutputDir $StageName

Write-Step "Packaging Hyperion editor preview $Version ($Configuration)"

if ($Build)
{
    Write-Step "Building $Configuration"
    Push-Location $RootDir
    try
    {
        # regenerate so the distribution options reach the CMake cache
        & cmd /c "`"$RootDir\Tools\Scripts\BuildHyperion.bat`" $Configuration distribution regenerate"
        if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    }
    finally { Pop-Location }
}

if (-not (Test-Path (Join-Path $BinDir "Hyperion.Editor.exe"))) { throw "Hyperion.Editor.exe not found in $BinDir. Build the $Configuration configuration first (or pass -Build)." }

$VsPath = Get-VisualStudioPath
$Dumpbin = Get-DumpbinPath $VsPath
$CrtRedistDir = Get-CrtRedistDir $VsPath

if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

# --- Managed editor ---------------------------------------------------------------------------------------------------

Write-Step "Staging managed editor assemblies"

$StagedBinaries = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

foreach ($file in $ManagedEditorFiles)
{
    $source = Join-Path $BinDir $file
    if (-not (Test-Path $source)) { throw "Missing $file in $BinDir" }
    Copy-StagedFile $source (Join-Path $StageDir $file)
    [void]$StagedBinaries.Add($file)
}

$depsJson = Get-Content (Join-Path $BinDir "Hyperion.Editor.deps.json") -Raw | ConvertFrom-Json
$depsTarget = $depsJson.targets.PSObject.Properties[$depsJson.runtimeTarget.name].Value

foreach ($library in $depsTarget.PSObject.Properties)
{
    $runtimeAssets = $library.Value.PSObject.Properties["runtime"]
    if ($runtimeAssets)
    {
        foreach ($asset in $runtimeAssets.Value.PSObject.Properties.Name)
        {
            # app-local layout flattens lib/<tfm>/X.dll to X.dll
            $fileName = Split-Path $asset -Leaf
            $source = Join-Path $BinDir $fileName
            if (-not (Test-Path $source)) { throw "deps.json lists $fileName ($($library.Name)) but it is missing from $BinDir" }
            Copy-StagedFile $source (Join-Path $StageDir $fileName)
            [void]$StagedBinaries.Add($fileName)
        }
    }

    $runtimeTargets = $library.Value.PSObject.Properties["runtimeTargets"]
    if ($runtimeTargets)
    {
        foreach ($asset in $runtimeTargets.Value.PSObject.Properties)
        {
            if ($asset.Value.rid -ne "win-x64" -or $asset.Name -like "*.pdb") { continue }
            $source = Join-Path $BinDir $asset.Name
            if (-not (Test-Path $source)) { throw "deps.json lists $($asset.Name) but it is missing from $BinDir" }
            Copy-StagedFile $source (Join-Path $StageDir $asset.Name)
        }
    }
}

# --- Native binaries --------------------------------------------------------------------------------------------------

Write-Step "Resolving native dependencies"

$pending = [System.Collections.Generic.Queue[string]]::new()
foreach ($file in $RuntimeExecutables + $DynamicallyLoadedDlls)
{
    if (-not (Test-Path (Join-Path $BinDir $file))) { throw "Missing $file in $BinDir" }
    $pending.Enqueue($file)
}

$visited = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$crtImports = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

while ($pending.Count -gt 0)
{
    $file = $pending.Dequeue()
    if (-not $visited.Add($file)) { continue }

    $source = Join-Path $BinDir $file
    Copy-StagedFile $source (Join-Path $StageDir $file)
    [void]$StagedBinaries.Add($file)

    foreach ($import in Get-NativeImports $Dumpbin $source)
    {
        if (Test-Path (Join-Path $BinDir $import)) { $pending.Enqueue($import) }
        elseif ($import -match "^(msvcp|vcruntime|concrt|vccorlib)140") { [void]$crtImports.Add($import) }
    }
}

if ($crtImports | Where-Object { $_ -match "d\.dll$" }) { Add-Warning "Debug CRT imports found ($($crtImports -join ', ')); a debug-built DLL slipped into the dependency graph" }

if ($StagedBinaries.Contains("GFSDK_Aftermath_Lib.x64.dll"))
{
    Add-Warning "hyperion.dll links NVIDIA Aftermath, so GFSDK_Aftermath_Lib.x64.dll was staged. Rebuild with 'BuildHyperion.bat $Configuration distribution regenerate' (or pass -Build) to leave it out."
}
if ($StagedBinaries.Contains("steam_api64.dll"))
{
    Add-Warning "steam_api64.dll was staged. Steamworks redistribution is limited to Steam partners; build without the Steam SDK for public releases."
}

Write-Step "Staging VC++ runtime from $CrtRedistDir"
Get-ChildItem $CrtRedistDir -Filter *.dll | ForEach-Object { Copy-StagedFile $_.FullName (Join-Path $StageDir $_.Name) }
$missingCrt = $crtImports | Where-Object { -not (Test-Path (Join-Path $CrtRedistDir $_)) }
if ($missingCrt) { Add-Warning "CRT imports not present in the redist folder: $($missingCrt -join ', ')" }

# --- Config -----------------------------------------------------------------------------------------------------------

Write-Step "Staging config"

$ConfigSourceDir = Join-Path $RootDir "Config"
$ConfigStageDir = Join-Path $StageDir "Config"
New-Item -ItemType Directory -Path $ConfigStageDir -Force | Out-Null

# Mirrors Source/Sample/CopyPlatformConfigs.cmake for PLATFORM=Windows
$KnownPlatforms = "IOS", "Android", "Windows", "Mac", "Linux"
foreach ($configFile in Get-ChildItem $ConfigSourceDir -File)
{
    if ($configFile.Extension -ne ".json")
    {
        Copy-StagedFile $configFile.FullName (Join-Path $ConfigStageDir $configFile.Name)
        continue
    }

    $platformSuffix = $KnownPlatforms | Where-Object { $configFile.Name -match "\.$_\.json$" } | Select-Object -First 1
    if ($platformSuffix) { continue }

    $windowsVariant = Join-Path $ConfigSourceDir ($configFile.BaseName + ".Windows.json")
    $source = if (Test-Path $windowsVariant) { $windowsVariant } else { $configFile.FullName }
    Copy-StagedFile $source (Join-Path $ConfigStageDir $configFile.Name)
}

$GlobalConfigPath = Join-Path $ConfigStageDir "GlobalConfig.json"
$globalConfig = Get-Content $GlobalConfigPath -Raw | ConvertFrom-Json
# basedir points at the source tree, trace/cache servers are dev-only localhost endpoints
$releaseArgs = ($globalConfig.App.Args -split "\s+") |
    Where-Object { $_ -and $_ -notmatch "^--(basedir|traceserver|cacheserver)=" }
$globalConfig.App.Args = (@($releaseArgs) + "--basedir=./") -join " "
[System.IO.File]::WriteAllText($GlobalConfigPath, ($globalConfig | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))

# --- Content, shaders, scripts ----------------------------------------------------------------------------------------

Write-Step "Staging engine and editor content"
$contentCount = Copy-TrackedFiles @("Content/Engine", "Content/Editor") $StageDir
Write-Host "    $contentCount files"

if ($IncludeShaderCache)
{
    Write-Step "Staging compiled shader cache"
    foreach ($shaderDir in "Content/Engine/Shaders", "Content/Engine/ShaderBundles")
    {
        $source = Join-Path $RootDir $shaderDir
        if (Test-Path $source) { Copy-Directory $source (Join-Path $StageDir $shaderDir) }
        else { Add-Warning "$shaderDir not found; run PrecompileShaders first" }
    }
}

Write-Step "Staging shader sources"
$shaderCount = Copy-TrackedFiles @("Source/Shaders") $StageDir
Write-Host "    $shaderCount files"

$StrataScriptsDir = Join-Path $RootDir "Data/Scripts/Strata"
if (Test-Path $StrataScriptsDir)
{
    Write-Step "Staging generated Strata bindings"
    Copy-Directory $StrataScriptsDir (Join-Path $StageDir "Data/Scripts/Strata")
}
else
{
    Add-Warning "Data/Scripts/Strata not found; CodeGen output is missing and Strata scripts won't resolve engine modules"
}

# --- Legal ------------------------------------------------------------------------------------------------------------

Copy-StagedFile (Join-Path $RootDir "LICENSE") (Join-Path $StageDir "LICENSE")
$NoticesPath = Join-Path $RootDir "THIRD_PARTY_NOTICES.md"
if (Test-Path $NoticesPath)
{
    Copy-StagedFile $NoticesPath (Join-Path $StageDir "THIRD_PARTY_NOTICES.md")
    $openNoticeItems = @(Select-String -Path $NoticesPath -Pattern "\bTODO\b").Count
    if ($openNoticeItems -gt 0) { Add-Warning "THIRD_PARTY_NOTICES.md has $openNoticeItems line(s) with TODO; resolve them before publishing" }
}
else { Add-Warning "THIRD_PARTY_NOTICES.md missing at repo root; required before publishing (OpenAL Soft is LGPL)" }

$LicensesDir = Join-Path $RootDir "Documentation/ThirdPartyLicenses"
if (Test-Path $LicensesDir) { Copy-Directory $LicensesDir (Join-Path $StageDir "Licenses") }
else { Add-Warning "Documentation/ThirdPartyLicenses missing; third-party license texts won't ship" }

# --- Sanity checks ----------------------------------------------------------------------------------------------------

$hyperionDllBytes = [System.IO.File]::ReadAllBytes((Join-Path $StageDir "hyperion.dll"))
$hyperionDllText = [System.Text.Encoding]::GetEncoding(28591).GetString($hyperionDllBytes)
if ($hyperionDllText.Contains($RootDir.Replace("\", "/"))) { Add-Warning "hyperion.dll has the source tree path ($RootDir) baked in via HYP_ROOT_DIR. Rebuild with 'BuildHyperion.bat $Configuration distribution regenerate' (or pass -Build)." }

$droppedBinaries = Get-ChildItem $BinDir -File |
    Where-Object { $_.Extension -in ".dll", ".exe" -and -not $StagedBinaries.Contains($_.Name) -and -not (Test-Path (Join-Path $StageDir $_.Name)) } |
    ForEach-Object Name
if ($droppedBinaries) { Write-Host "    Not staged from ${BinDir}: $($droppedBinaries -join ', ')" -ForegroundColor DarkGray }

# --- Archives ---------------------------------------------------------------------------------------------------------

if ($Symbols)
{
    Write-Step "Collecting symbols"
    $SymbolsDir = Join-Path $OutputDir "$StageName-symbols"
    if (Test-Path $SymbolsDir) { Remove-Item $SymbolsDir -Recurse -Force }
    New-Item -ItemType Directory -Path $SymbolsDir -Force | Out-Null
    foreach ($binary in $StagedBinaries)
    {
        $pdb = Join-Path $BinDir ([System.IO.Path]::ChangeExtension($binary, ".pdb"))
        if (Test-Path $pdb) { Copy-StagedFile $pdb (Join-Path $SymbolsDir (Split-Path $pdb -Leaf)) }
    }
}

if (-not $NoZip)
{
    $zipTargets = @($StageName)
    if ($Symbols) { $zipTargets += "$StageName-symbols" }

    foreach ($target in $zipTargets)
    {
        $zipPath = Join-Path $OutputDir "$target.zip"
        Write-Step "Creating $zipPath"
        if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
        # bsdtar zips far faster than Compress-Archive and has no 2 GB limit
        tar.exe -a -c -f $zipPath -C $OutputDir $target
        if ($LASTEXITCODE -ne 0) { throw "tar failed creating $zipPath" }
    }
}

$stageSize = (Get-ChildItem $StageDir -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ""
Write-Host ("Staged {0} ({1:N0} MB) at {2}" -f $StageName, ($stageSize / 1MB), $StageDir) -ForegroundColor Green
if ($Warnings.Count -gt 0)
{
    Write-Host "$($Warnings.Count) warning(s):" -ForegroundColor Yellow
    $Warnings | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
}
