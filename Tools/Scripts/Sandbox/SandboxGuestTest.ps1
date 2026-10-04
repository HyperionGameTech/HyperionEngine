param(
    [int]$DurationSeconds = 90
)

$ErrorActionPreference = "Continue"

$ResultsDir = "C:\Results"
$SourceDir = "C:\Source\Build"
$InstallDir = "C:\Users\WDAGUtilityAccount\Hyperion"
$LogPath = Join-Path $ResultsDir "guest.log"

function Log([string]$Message)
{
    $line = "[{0:HH:mm:ss}] {1}" -f (Get-Date), $Message
    Add-Content -Path $LogPath -Value $line
}

function Save-Screenshot([string]$Name)
{
    try
    {
        Add-Type -AssemblyName System.Windows.Forms, System.Drawing
        $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
        $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
        $bitmap.Save((Join-Path $ResultsDir "$Name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        $graphics.Dispose()
        $bitmap.Dispose()
        Log "screenshot $Name.png"
    }
    catch { Log "screenshot failed: $_" }
}

Set-Content -Path $LogPath -Value ""

Log "sandbox OS: $((Get-CimInstance Win32_OperatingSystem).Caption) $((Get-CimInstance Win32_OperatingSystem).Version)"
Get-CimInstance Win32_VideoController | ForEach-Object { Log "adapter: $($_.Name) driver $($_.DriverVersion) ($($_.Status))" }
foreach ($systemDll in "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "d3d12.dll", "dxgi.dll")
{
    Log ("system32 has {0}: {1}" -f $systemDll, (Test-Path "$env:SystemRoot\System32\$systemDll"))
}
Log ("host dotnet mapped: {0}" -f (Test-Path "C:\dotnet\dotnet.exe"))

Log "copying build to $InstallDir"
$copyTimer = [System.Diagnostics.Stopwatch]::StartNew()
robocopy $SourceDir $InstallDir /E /NFL /NDL /NJH /NJS /NP | Out-Null
Log ("copy finished in {0:N1}s, exit {1}" -f $copyTimer.Elapsed.TotalSeconds, $LASTEXITCODE)

$installedFiles = Get-ChildItem $InstallDir -Recurse -File | Select-Object -ExpandProperty FullName
$baselineFiles = [System.Collections.Generic.HashSet[string]]::new($installedFiles, [System.StringComparer]::OrdinalIgnoreCase)

$env:DOTNET_ROOT = "C:\dotnet"
$env:DOTNET_ROOT_X64 = "C:\dotnet"
$env:DOTNET_MULTILEVEL_LOOKUP = "0"

$editorPath = Join-Path $InstallDir "Hyperion.Editor.exe"
Log "launching $editorPath"
$launchTime = Get-Date
$process = Start-Process -FilePath $editorPath -WorkingDirectory $InstallDir -PassThru -RedirectStandardOutput (Join-Path $ResultsDir "editor_stdout.txt") -RedirectStandardError (Join-Path $ResultsDir "editor_stderr.txt")

function Get-RuntimeFileCount { @(Get-ChildItem (Join-Path $InstallDir "Temp") -Recurse -File -ErrorAction SilentlyContinue).Count }

$dumpTaken = $false
$everHung = $false
$elapsed = 0
$lastFileCount = -1
$stalledSamples = 0
while ($elapsed -lt $DurationSeconds)
{
    Start-Sleep -Seconds 5
    $elapsed += 5
    $process.Refresh()
    if ($process.HasExited)
    {
        Log "PROCESS EXITED at ${elapsed}s, exit code $($process.ExitCode)"
        break
    }

    $responding = $process.Responding
    $fileCount = Get-RuntimeFileCount
    if (-not $responding) { $everHung = $true }
    if ($fileCount -eq $lastFileCount) { $stalledSamples++ } else { $stalledSamples = 0 }
    $lastFileCount = $fileCount

    Log ("t={0}s responding={1} ws={2:N0} MB threads={3} cpu={4:N1}s temp files={5}" -f $elapsed, $responding, ($process.WorkingSet64 / 1MB), $process.Threads.Count, $process.TotalProcessorTime.TotalSeconds, $fileCount)
    if ($elapsed -in 15, 45, 75) { Save-Screenshot ("screenshot_{0:D3}s" -f $elapsed) }

    if (-not $dumpTaken -and $elapsed -ge 40 -and ($everHung -or $stalledSamples -ge 4))
    {
        $dumpPath = Join-Path $ResultsDir "editor_hung.dmp"
        Log "no progress, writing full dump to $dumpPath"
        Start-Process -FilePath "rundll32.exe" -ArgumentList "C:\Windows\System32\comsvcs.dll, MiniDump $($process.Id) $dumpPath full" -Wait
        Log ("dump size: {0:N0} MB" -f ((Get-Item $dumpPath -ErrorAction SilentlyContinue).Length / 1MB))
        $dumpTaken = $true
    }
}

$process.Refresh()
$exited = $process.HasExited
if (-not $exited)
{
    Save-Screenshot "screenshot_final"

    $allowedRoots = @($InstallDir, "C:\dotnet", "$env:SystemRoot")
    $modules = $process.Modules | ForEach-Object { $_.FileName }
    $modules | Sort-Object | Set-Content (Join-Path $ResultsDir "loaded_modules.txt")
    $unexpected = $modules | Where-Object { $path = $_; -not ($allowedRoots | Where-Object { $path.StartsWith($_, [System.StringComparison]::OrdinalIgnoreCase) }) }
    Log ("loaded modules: {0}, outside install/dotnet/windows: {1}" -f $modules.Count, @($unexpected).Count)
    $unexpected | ForEach-Object { Log "  unexpected module: $_" }

    $children = Get-CimInstance Win32_Process | Where-Object { $_.ParentProcessId -eq $process.Id } | Select-Object -ExpandProperty Name
    Log "child processes: $($children -join ', ')"

    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
}

$crashEvents = Get-WinEvent -FilterHashtable @{ LogName = "Application"; StartTime = $launchTime; Id = 1000, 1001, 1026 } -ErrorAction SilentlyContinue
$crashEvents | ForEach-Object { Log "event $($_.Id): $(($_.Message -split "`n" | Select-Object -First 4) -join ' | ')" }
$crashEvents | Select-Object TimeCreated, Id, Message | Format-List | Out-File (Join-Path $ResultsDir "crash_events.txt")

$newFiles = Get-ChildItem $InstallDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object { -not $baselineFiles.Contains($_.FullName) }
$newFiles | ForEach-Object { "{0,10:N0} B  {1}" -f $_.Length, $_.FullName.Substring($InstallDir.Length + 1) } | Set-Content (Join-Path $ResultsDir "files_created_at_runtime.txt")
Log "files created at runtime: $(@($newFiles).Count)"

foreach ($logDirectory in "Logs", "Temp", "Cache")
{
    $source = Join-Path $InstallDir $logDirectory
    if (Test-Path $source) { robocopy $source (Join-Path $ResultsDir "runtime\$logDirectory") /E /NFL /NDL /NJH /NJS /NP /MAXAGE:1 | Out-Null }
}
Get-ChildItem $InstallDir -Recurse -Include *.log, *.txt -File -ErrorAction SilentlyContinue |
    Where-Object { -not $baselineFiles.Contains($_.FullName) } |
    ForEach-Object { Copy-Item $_.FullName (Join-Path $ResultsDir ("runtime_" + $_.Name)) -Force }

$hangEvents = @($crashEvents | Where-Object { $_.Message -match "AppHang" }).Count
$verdict = if ($exited) { "FAIL: editor exited early (code $($process.ExitCode))" }
    elseif ($everHung -or $hangEvents -gt 0) { "FAIL: editor hung (not responding or AppHang event)" }
    elseif ($lastFileCount -le 0) { "FAIL: editor stayed up but never got as far as compiling shaders" }
    else { "PASS: editor stayed up and responsive for ${DurationSeconds}s" }
Log $verdict
Set-Content -Path (Join-Path $ResultsDir "done.txt") -Value $verdict
