<#
.SYNOPSIS
    Runs a staged editor build (see PackageEditorWindows.ps1) inside Windows Sandbox and collects the results.
#>
param(
    [string]$BuildDir,
    [int]$DurationSeconds = 90,
    [int]$TimeoutMinutes = 10,
    [switch]$NoGpu,
    [switch]$ProbeDlls,
    [switch]$Network
)

$ErrorActionPreference = "Stop"

$RootDir = (Resolve-Path "$PSScriptRoot/../../..").Path
if (-not $BuildDir) { $BuildDir = Join-Path $RootDir "PackagedBuilds/EditorPreview/Hyperion-Editor-0.5.1-win64" }
$BuildDir = (Resolve-Path $BuildDir).Path
$TestDir = Join-Path $RootDir "PackagedBuilds/EditorPreview/SandboxTest"
$ResultsDir = Join-Path $TestDir "Results"
$DotNetDir = Join-Path $env:ProgramFiles "dotnet"

if (-not (Test-Path "$env:SystemRoot\System32\WindowsSandbox.exe"))
{
    Write-Host "Windows Sandbox is not installed. In an elevated PowerShell run:" -ForegroundColor Yellow
    Write-Host "    Enable-WindowsOptionalFeature -Online -FeatureName Containers-DisposableClientVM -All"
    Write-Host "then reboot and run this script again."
    exit 1
}
if (Get-Process -Name "WindowsSandbox*" -ErrorAction SilentlyContinue)
{
    throw "A Windows Sandbox instance is already running; only one can run at a time. Close it first."
}
if (-not (Test-Path (Join-Path $DotNetDir "dotnet.exe"))) { throw "dotnet not found at $DotNetDir" }

if (Test-Path $ResultsDir) { Remove-Item $ResultsDir -Recurse -Force }
New-Item -ItemType Directory -Path $ResultsDir -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot "SandboxGuestTest.ps1") $ResultsDir

$vGpu = if ($NoGpu) { "Disable" } else { "Enable" }
$networking = if ($Network) { "Default" } else { "Disable" }
$guestArguments ="-DurationSeconds $DurationSeconds" + $(if ($ProbeDlls) { " -ProbeDlls" } else { "" })
$config = @"
<Configuration>
  <VGpu>$vGpu</VGpu>
  <Networking>$networking</Networking>
  <MemoryInMB>16384</MemoryInMB>
  <MappedFolders>
    <MappedFolder><HostFolder>$BuildDir</HostFolder><SandboxFolder>C:\Source\Build</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$DotNetDir</HostFolder><SandboxFolder>C:\dotnet</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$ResultsDir</HostFolder><SandboxFolder>C:\Results</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>powershell.exe -ExecutionPolicy Bypass -File C:\Results\SandboxGuestTest.ps1 $guestArguments</Command>
  </LogonCommand>
</Configuration>
"@
$configPath = Join-Path $TestDir "EditorTest.wsb"
Set-Content -Path $configPath -Value $config -Encoding utf8

Write-Host "Launching sandbox (vGPU: $vGpu, networking: $networking), build: $BuildDir"
Start-Process $configPath

$donePath = Join-Path $ResultsDir "done.txt"
$deadline = (Get-Date).AddMinutes($TimeoutMinutes)
while (-not (Test-Path $donePath))
{
    if ((Get-Date) -gt $deadline) { throw "Timed out after $TimeoutMinutes minutes waiting for the sandbox test; see $ResultsDir" }
    Start-Sleep -Seconds 5
}

Write-Host ""
Get-Content (Join-Path $ResultsDir "guest.log")
Write-Host ""
Write-Host "Results in $ResultsDir" -ForegroundColor Green
