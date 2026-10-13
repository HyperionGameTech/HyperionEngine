param(
    [string]$World = "",
    [string]$Package = "",
    [int]$Seconds = 60,
    [string]$Project = "",
    [int]$FrameRate = 0
)

$ErrorActionPreference = "Stop"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$binDir = Join-Path $rootDir "Binaries\Windows\Release"
$configDir = Join-Path $binDir "Config"
$webDir = Join-Path $rootDir "PackagedBuilds\Web"
$recordedDir = Join-Path $webDir "Recorded"
$workDir = Join-Path $recordedDir "Work"
$sampleExe = Join-Path $binDir "hyperion-sample.exe"

function Fail([string]$message)
{
    Write-Host "ERROR: $message" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $sampleExe))
{
    Fail "$sampleExe not found. Build first: Tools\Scripts\BuildHyperion.bat webgpu"
}

$cmakeCache = Join-Path $rootDir "Build\Windows\Release\CMakeCache.txt"

if (-not (Test-Path $cmakeCache) -or -not (Select-String -Path $cmakeCache -Pattern "^HYP_RENDERING_BACKEND:.*=WebGPU\s*$" -Quiet))
{
    Fail "The Windows build is not on the WebGPU backend. Build it first: Tools\Scripts\BuildHyperion.bat webgpu"
}

if ($Package -eq "")
{
    $pointer = Join-Path $webDir ".hyperion-package"

    if (Test-Path $pointer)
    {
        $Package = (Get-Content $pointer -TotalCount 1).Trim()
    }
}

if ($Package -eq "" -or -not (Test-Path (Join-Path $Package "Cache\toc.bin")))
{
    Fail "No cooked package to run the level from. Make one first: Tools\Scripts\PackageBuildWeb.bat --project <project>"
}

Write-Host "Running the level from $Package"

if (Test-Path $workDir)
{
    Remove-Item $workDir -Recurse -Force
}

New-Item -ItemType Directory -Force $workDir | Out-Null
Copy-Item (Join-Path $Package "Cache") (Join-Path $workDir "Cache") -Recurse
Copy-Item (Join-Path $Package "Content") (Join-Path $workDir "Content") -Recurse

$listPath = Join-Path $workDir "engine-assets.txt"

# The run has to see what the browser sees, and must not go looking for a cache server.
$swappedConfigs = @("EngineConfig.json", "GlobalConfig.json")

foreach ($name in $swappedConfigs)
{
    $backup = Join-Path $configDir "$name.recordbackup"

    if (-not (Test-Path $backup))
    {
        Copy-Item (Join-Path $configDir $name) $backup
    }
}

function RunLevel([string]$label, [string[]]$extraArguments)
{
    $arguments = @("--singleplayer=true", "--cachedir=$workDir\Cache", "--contentdir=$workDir\Content") + $extraArguments

    if ($World -ne "")
    {
        $arguments += "--startupworld=$World"
    }

    Write-Host "$label, $Seconds seconds (closing the window ends it sooner)..."

    $process = Start-Process -FilePath $sampleExe -ArgumentList $arguments -WorkingDirectory $binDir -PassThru `
        -RedirectStandardOutput (Join-Path $workDir "$label.log") -RedirectStandardError (Join-Path $workDir "$label.err.log")

    if (-not $process.WaitForExit($Seconds * 1000))
    {
        $process.CloseMainWindow() | Out-Null

        if (-not $process.WaitForExit(30000))
        {
            Stop-Process -Id $process.Id -Force
        }
    }
}

$startTime = Get-Date

try
{
    $engineConfig = Get-Content (Join-Path $rootDir "Config\EngineConfig.Web.json") -Raw

    if ($FrameRate -gt 0)
    {
        $engineConfig = $engineConfig -replace '"TargetFrameRate":\s*\d+', "`"TargetFrameRate`": $FrameRate"
    }

    Set-Content (Join-Path $configDir "EngineConfig.json") $engineConfig -Encoding ascii

    $globalConfig = Get-Content (Join-Path $rootDir "Config\GlobalConfig.Web.json") -Raw | ConvertFrom-Json
    $globalConfig.App.Args = "$($globalConfig.App.Args) --basedir=../../../"
    $globalConfig | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $configDir "GlobalConfig.json") -Encoding ascii

    # A shader variant compiled during a run is not in that run's list, so the first run only compiles.
    RunLevel "Run1-Compile" @()
    RunLevel "Run2-Record" @("--record-engine-assets=$listPath")
}
finally
{
    foreach ($name in $swappedConfigs)
    {
        $backup = Join-Path $configDir "$name.recordbackup"

        if (Test-Path $backup)
        {
            Move-Item $backup (Join-Path $configDir $name) -Force
        }
    }
}

$recordedFiles = @($listPath, (Join-Path $workDir "Cache\shaderprops.bin"), (Join-Path $workDir "Cache\shaderpreload.bin"))

foreach ($file in $recordedFiles)
{
    if (-not (Test-Path $file) -or (Get-Item $file).LastWriteTime -lt $startTime)
    {
        Fail "The run did not write $file. See the logs in $workDir"
    }
}

$recordedFiles += Join-Path $workDir "Cache\pipelinevariants.bin"

foreach ($file in $recordedFiles)
{
    if (Test-Path $file)
    {
        Copy-Item $file $recordedDir -Force
    }
}

$numAssets = (Get-Content (Join-Path $recordedDir "engine-assets.txt") | Measure-Object -Line).Lines
$numMissing = (Select-String -Path (Join-Path $workDir "Run2-Record*.log") -Pattern "Blob data not found").Count

Write-Host "Recorded $numAssets engine assets to $recordedDir"

if ($numMissing -ne 0)
{
    Write-Host "The package the level ran from was missing $numMissing of them; they are in the list now."
}

Remove-Item (Join-Path $workDir "Cache"), (Join-Path $workDir "Content") -Recurse -Force

$packageArguments = "--engine-assets `"$recordedDir\engine-assets.txt`" --shader-cache `"$recordedDir`" --skip-precompile"

if ($World -ne "")
{
    $packageArguments = "--worlds $World $packageArguments"
}

if ($Project -eq "")
{
    Write-Host "Package with:"
    Write-Host "  Tools\Scripts\PackageBuildWeb.bat --project <project> $packageArguments"

    exit 0
}

Write-Host "Packaging $Project..."

& cmd /c "`"$PSScriptRoot\PackageBuildWeb.bat`" --project `"$Project`" $packageArguments < NUL"

exit $LASTEXITCODE
