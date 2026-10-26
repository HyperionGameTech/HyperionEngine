param(
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$ContentDir = "",
    [switch]$Run,
    [switch]$ServeOnly,
    [switch]$FastLink,
    [int]$MaxTextureSize = 0,
    [string]$Worlds = "",
    [string]$Name = "",
    [string]$Title = "",
    [string]$PluginModules = "",
    [string]$Plugins = "",
    [int]$Port = 8137
)

# Builds a project for the browser into <project>/Binaries/Web, a folder any static host can serve as it is.
# Intermediates go to <project>/Build/Web. The texture limit comes from the project's packaging settings for web, the worlds from PlatformSettings.Web; the arguments override both.
#
#   -ContentDir the project's asset root when it isn't the project folder (eg <project>/Content); the editor passes it
#   -Run        serve Binaries/Web on localhost afterwards and open it in the browser; keeps function names in the wasm for readable stacks
#   -ServeOnly  skip the build: serve the last one and open it in the browser
#   -FastLink   link at -O1: minutes faster, for testing locally; a build to host links at -Oz
#   -Name       the name of the game's code and page files, -Title the page title; the editor passes both from the project settings
#   -PluginModules  the enabled built-in plugins' runtime modules, linked into the page; -Plugins those plugins, whose content the cook mounts
#
# Build/Web/Built.stamp is written by a finished build, dated when that build started.

$ErrorActionPreference = "Stop"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$webSdk = Join-Path $rootDir "Binaries\Web\Release\Sdk\CMake\HyperionWebSdk.cmake"
$templateDir = Join-Path $rootDir "Source\Templates\Platforms\Web"
$webConfig = Join-Path $rootDir "Config\EngineConfig.Web.json"
$pinnedEmscriptenVersion = "6.0.11"

function Step([int]$percent, [string]$text)
{
    Write-Host "==> [$percent%] $text"
}

function Fail([string]$message)
{
    Write-Host "ERROR: $message"
    exit 1
}

function Invoke-Cmd([string]$commandLine, [string]$failure)
{
    # through a batch file: quotes inside the command survive it, not PowerShell's handing of them to cmd
    $stepScript = Join-Path $buildDir "Step.bat"
    Set-Content $stepScript "@echo off`r`n$commandLine`r`nexit /b %errorlevel%" -Encoding ascii

    & cmd /c "`"$stepScript`""

    if ($LASTEXITCODE -ne 0)
    {
        Fail $failure
    }
}

# Start-Process joins its arguments with spaces, so one holding a space needs its own quotes
function Format-Arguments([string[]]$arguments)
{
    return $arguments | ForEach-Object { if ($_ -match "\s" -and -not $_.StartsWith('"')) { "`"$_`"" } else { $_ } }
}

# the project

if (-not (Test-Path $Project))
{
    Fail "Project folder $Project not found"
}

$Project = (Resolve-Path $Project).Path
$projectFile = Get-ChildItem $Project -Filter *.hypproject | Select-Object -First 1

if (-not $projectFile)
{
    Fail "No .hypproject in $Project"
}

# without -Name, the name the editor gives the game code of a project with no settings for it
if ($name -eq "")
{
    $name = ($projectFile.BaseName -replace '[^A-Za-z0-9_]', '') -replace '^[0-9]+', ''
}

if ($name -eq "")
{
    $name = "MyGame"
}

if ($Title -eq "")
{
    $Title = $name
}

# cmd expands %VAR% even inside quotes
$pageTitle = [System.Net.WebUtility]::HtmlEncode($Title) -replace '%', '&#37;'

$managedProject = Join-Path $Project "Source\$name.csproj"
$isManaged = Test-Path $managedProject

$contentDir = if ($ContentDir -ne "") { (Resolve-Path $ContentDir).Path } else { $Project }

# a project with C# scripts but no C# game of its own still needs them compiled in, around the engine's Game class
$scriptsDir = Join-Path $contentDir "Scripts"
$hasScripts = (Test-Path $scriptsDir) -and (Get-ChildItem $scriptsDir -Recurse -Filter *.cs -ErrorAction SilentlyContinue | Select-Object -First 1)
$needsManaged = $isManaged -or $hasScripts

$projectText = Get-Content $projectFile.FullName -Raw

function Get-ProjectSetting([string]$key, [string]$fallback)
{
    $match = [regex]::Match($projectText, "(?s)PlatformSettings.*?\b$key\s*=\s*""?([^""\r\n}]*)""?")

    if ($match.Success -and $match.Groups[1].Value.Trim() -ne "")
    {
        return $match.Groups[1].Value.Trim()
    }

    return $fallback
}

$editWorld = [regex]::Match($projectText, "EditWorldName\s*=\s*(\S+)").Groups[1].Value

if ($Worlds -eq "") { $Worlds = Get-ProjectSetting "Worlds" $editWorld }

$worldList = @($Worlds -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
$gameSourceDir = Join-Path $Project "Source"
$hasGameCode = Test-Path (Join-Path $gameSourceDir "Game")

$buildDir = Join-Path $Project "Build\Web"
$packageDir = Join-Path $buildDir "Package"
$cmakeDir = Join-Path $buildDir "CMake"
$pageDir = Join-Path $buildDir "Page"
$outputDir = Join-Path $Project "Binaries\Web"
$buildingStamp = Join-Path $buildDir "Building.stamp"
$builtStamp = Join-Path $buildDir "Built.stamp"

function Start-WebServer
{
    if (-not (Get-Command python -ErrorAction SilentlyContinue))
    {
        Fail "python not found on PATH, which serving the page needs"
    }

    $serverPidFile = Join-Path $buildDir "server.pid"

    if (Test-Path $serverPidFile)
    {
        $previousServer = Get-Process -Id ([int](Get-Content $serverPidFile -TotalCount 1)) -ErrorAction SilentlyContinue

        if ($previousServer -and $previousServer.ProcessName -match "python")
        {
            Stop-Process -Id $previousServer.Id -Force
        }
    }

    $server = Start-Process -FilePath python -ArgumentList @("`"$(Join-Path $PSScriptRoot 'ServeWeb.py')`"", "`"$outputDir`"", "`"$outputDir`"", $Port, "`"$(Join-Path $buildDir 'MissingShaders.txt')`"", "`"$pipelineManifest`"") -WindowStyle Hidden -PassThru
    Set-Content $serverPidFile $server.Id -Encoding ascii

    Start-Sleep -Milliseconds 800
    Start-Process "http://localhost:$Port/"

    Write-Host "Serving $outputDir at http://localhost:$Port/ (server pid $($server.Id))"
}

# the pipelines the game compiled when it last ran from the local server, compiled ahead of use by the next build
$pipelineManifest = Join-Path $buildDir "pipelinevariants.bin"

New-Item -ItemType Directory -Force $buildDir | Out-Null

if ($ServeOnly)
{
    if (-not (Test-Path (Join-Path $outputDir "index.html")) -or -not (Test-Path (Join-Path $outputDir "$name.wasm")))
    {
        Fail "$name hasn't been built for the web yet"
    }

    Start-WebServer
    exit 0
}

Set-Content $buildingStamp "" -Encoding ascii

$textureLimit = if ($MaxTextureSize -gt 0) { "textures up to $MaxTextureSize" } else { "texture limit from the project's packaging settings" }
Write-Host "Building $name for the web: worlds $($worldList -join ', '), $textureLimit"

# 1. the toolchain

Step 0 "Checking the toolchain"

if (-not $env:EMSDK -or -not (Test-Path (Join-Path $env:EMSDK "emsdk_env.bat")))
{
    Fail "EMSDK is not set to an emsdk folder. Install emsdk $pinnedEmscriptenVersion and set EMSDK to it"
}

$emscriptenVersionFile = Join-Path $env:EMSDK "upstream\emscripten\emscripten-version.txt"

if ((Test-Path $emscriptenVersionFile) -and -not ((Get-Content $emscriptenVersionFile -Raw) -match [regex]::Escape($pinnedEmscriptenVersion)))
{
    Write-Host "WARNING: emsdk is not $pinnedEmscriptenVersion, the version the engine's web build is made with"
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue))
{
    Fail "ninja not found on PATH"
}

if ($Run -and -not (Get-Command python -ErrorAction SilentlyContinue))
{
    Fail "python not found on PATH, which serving the page needs"
}

if (-not (Test-Path $webSdk))
{
    Fail "The engine's web build is missing ($webSdk). Build it: Tools\Scripts\BuildHyperion.bat web"
}

# the cook compiles shaders from today's source while the page links the prebuilt engine, so engine source changed
# since that build can leave the two disagreeing on buffer layouts, which renders wrong rather than failing
$sdkLibraries = [regex]::Match((Get-Content $webSdk -Raw), "HYPERION_WEB_LIBRARIES \[==\[(.*?)\]==\]").Groups[1].Value -split ";" |
    Where-Object { $_ -like "*.a" -and (Test-Path $_) }
$engineSourceDirs = @("Core", "Engine") | ForEach-Object { Join-Path $rootDir "Source\$_" } | Where-Object { Test-Path $_ }

if ($sdkLibraries -and $engineSourceDirs)
{
    $sdkBuiltAt = ($sdkLibraries | ForEach-Object { (Get-Item $_).LastWriteTime } | Measure-Object -Maximum).Maximum
    $sourceExtensions = @(".cpp", ".hpp", ".h", ".inl", ".c")

    $newerSource = Get-ChildItem $engineSourceDirs -Recurse -File |
        Where-Object { $sourceExtensions -contains $_.Extension -and $_.LastWriteTime -gt $sdkBuiltAt } |
        Select-Object -First 1

    if ($newerSource)
    {
        Fail "The engine's web build is older than its source ($($newerSource.FullName) changed since $sdkBuiltAt). Rebuild it first: Tools\Scripts\BuildHyperion.bat web"
    }
}

# a C# game is compiled with NativeAOT-LLVM, whose bitcode needs the LLVM of emsdk 3.1.56 to become wasm objects
$managedEmsdk = if ($env:HYP_WEB_DOTNET_EMSDK) { $env:HYP_WEB_DOTNET_EMSDK } else { Join-Path (Split-Path $env:EMSDK -Parent) "emsdk-3.1.56" }

if ($needsManaged)
{
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue))
    {
        Fail "dotnet not found on PATH, which compiling the C# game needs"
    }

    if (-not (Test-Path (Join-Path $managedEmsdk "upstream\bin\clang.exe")))
    {
        Fail "C# games need emsdk 3.1.56 for NativeAOT-LLVM at $managedEmsdk (or set HYP_WEB_DOTNET_EMSDK): git clone https://github.com/emscripten-core/emsdk `"$managedEmsdk`" && `"$managedEmsdk\emsdk.bat`" install 3.1.56"
    }

    if (-not (Test-Path (Join-Path $rootDir "Binaries\Web\Release\Hyperion.NET.Runtime.dll")))
    {
        Fail "The engine's web build has no C# assemblies. Rebuild it: Tools\Scripts\BuildHyperion.bat web"
    }
}

$cookDir = Join-Path $rootDir "Binaries\Windows\Release"
$cookExe = Join-Path $cookDir "BlobStorageCookCommandlet.exe"

if (-not (Test-Path $cookExe))
{
    Fail "The editor build's cook commandlet is missing ($cookExe). Build the engine: Tools\Scripts\BuildHyperion.bat"
}

# 2. the cook: what the worlds reach, the engine defaults, and the shaders they draw with on the web

Step 4 "Cooking and compiling shaders"

if (Test-Path $packageDir)
{
    Remove-Item $packageDir -Recurse -Force
}

New-Item -ItemType Directory -Force $packageDir | Out-Null

# without one, the cook would take the shader lists the host tools' own cache holds
$emptyShaderCache = Join-Path $buildDir "NoShaderLists"
New-Item -ItemType Directory -Force $emptyShaderCache | Out-Null

$cookArguments = @("--project=$Project", "--out-cache=$packageDir\Cache", "--out-content=$packageDir\Content", "--target-platform=web",
    "--shader-target=web", "--target-config=$webConfig", "--cachedir=$emptyShaderCache")

if ($worldList.Count -gt 0)
{
    $cookArguments += "--worlds=$($worldList -join ',')"
}

if ($MaxTextureSize -gt 0)
{
    $cookArguments += "--max-texture-size=$MaxTextureSize"
}

if ($Plugins -ne "")
{
    $cookArguments += "--plugins=$Plugins"
}

$cookLog = Join-Path $buildDir "Cook.log"
$cookErrorLog = Join-Path $buildDir "Cook.err.log"

Start-Process -FilePath $cookExe -ArgumentList (Format-Arguments $cookArguments) -WorkingDirectory $cookDir -Wait -NoNewWindow `
    -RedirectStandardOutput $cookLog -RedirectStandardError $cookErrorLog | Out-Null

# the commandlet can crash shutting down after a finished cook, so the log decides
if (-not (Select-String -Path $cookLog, $cookErrorLog -Pattern "Blob storage cook complete." -Quiet))
{
    Fail "The cook failed, see $cookLog"
}

$configDir = Join-Path $packageDir "Config"
New-Item -ItemType Directory -Force $configDir | Out-Null
Copy-Item $webConfig (Join-Path $configDir "EngineConfig.json")
Copy-Item (Join-Path $rootDir "Config\GlobalConfig.Web.json") (Join-Path $configDir "GlobalConfig.json")
Copy-Item (Join-Path $rootDir "Config\Shaders.hmf"), (Join-Path $rootDir "Config\DeviceTiers.hmf") $configDir

if (Test-Path $pipelineManifest)
{
    Copy-Item $pipelineManifest (Join-Path $packageDir "Cache\pipelinevariants.bin") -Force
}

# a C++ game's reflection and the C# of its classes, from its headers, as its desktop build makes them

$gameGeneratedDir = Join-Path $buildDir "Generated"

if ($hasGameCode)
{
    Step 28 "Running CodeGen over the game"

    $codeGenExe = Join-Path $rootDir "Build\hyperion-codegen.exe"
    $engineTypesFile = Join-Path $rootDir "Source\Generated\EngineTypes.json"

    if (-not (Test-Path $codeGenExe) -or -not (Test-Path $engineTypesFile))
    {
        Fail "CodeGen is missing ($codeGenExe, $engineTypesFile). Build the engine: Tools\Scripts\BuildHyperion.bat"
    }

    Invoke-Cmd ("`"$codeGenExe`" --Mode=Game --WorkingDirectory=`"$gameSourceDir`" --SourceDirectory=`"$gameSourceDir`" " + `
        "--CXXOutputDirectory=`"$gameGeneratedDir`" --CSharpOutputDirectory=`"$gameGeneratedDir\CSharp`" --EngineTypes=`"$engineTypesFile`" > `"$buildDir\CodeGen.log`" 2>&1") `
        "CodeGen failed, see $buildDir\CodeGen.log"
}

# 3. a C# game: compiled ahead of time into a static library, linked into the page with the engine

$managedLinkInputs = ""
$gameClassArgument = ""

if ($needsManaged)
{
    Step 30 "Compiling the C# game"

    $managedDir = Join-Path $buildDir "Managed"
    $managedLinkInputs = Join-Path $managedDir "LinkInputs.txt"
    Remove-Item $managedLinkInputs -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $managedDir | Out-Null

    $engineWebDir = (Join-Path $rootDir "Binaries\Web\Release\") -replace '\\', '/'
    $publishProject = $managedProject
    $webGameClass = ""

    if ($isManaged)
    {
        # a C# project's game class is <Name>Game
        $webGameClass = "${name}Game"
    }
    else
    {
        # only scripts: a project to compile them, with no game class of its own
        $scriptsProjectDir = Join-Path $managedDir "ScriptsProject"
        New-Item -ItemType Directory -Force $scriptsProjectDir | Out-Null

        $publishProject = Join-Path $scriptsProjectDir "$name.csproj"
        $sdkDir = (Join-Path $rootDir "Source\Engine\DotNET\Sdk\") -replace '\\', '/'

        Set-Content $publishProject -Encoding utf8 @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <AssemblyName>$name</AssemblyName>
    <HyperionProjectDir>$(($Project -replace '\\', '/') + '/')</HyperionProjectDir>
  </PropertyGroup>
  <Import Project="${sdkDir}HyperionScripts.props" />
</Project>
"@

        # a C++ game module creates the game itself; otherwise the engine's Game
        if (-not $hasGameCode)
        {
            $webGameClass = "Hyperion.Game"
        }
    }

    $gameClassProperty = if ($webGameClass -ne "") { "-p:HyperionHostedGameClass=$webGameClass " } else { "" }
    $gameBindingsProperty = if ($hasGameCode) { "-p:HyperionGameBindingsDir=`"$gameGeneratedDir\CSharp\\`" " } else { "" }

    Invoke-Cmd ("set `"MSBuildSdksPath=`" && dotnet publish `"$publishProject`" -c Release -r browser-wasm --disable-build-servers -nologo " + `
        "-p:HyperionWebBuild=true -p:HyperionBinDir=`"$engineWebDir`" -p:HyperionWebEmsdk=`"$managedEmsdk`" $gameClassProperty$gameBindingsProperty-p:HyperionWriteGlobalConfig=false " + `
        "-p:HyperionProjectContentDir=`"$(($contentDir -replace '\\', '/') + '/')`" " + `
        "-p:OutputPath=`"$managedDir\bin\\`" -p:IntermediateOutputPath=`"$managedDir\obj\\`" -p:MSBuildProjectExtensionsPath=`"$managedDir\ext\\`" " + `
        "-p:HyperionWebLinkInputsFile=`"$managedLinkInputs`" > `"$managedDir\Publish.log`" 2>&1") `
        "Compiling the C# game failed, see $managedDir\Publish.log"

    if (-not (Test-Path $managedLinkInputs))
    {
        Fail "Compiling the C# game wrote no link inputs, see $managedDir\Publish.log"
    }

    $gameClassArgument = $webGameClass
}

# 4. the page: the launch layer and the project's game code, linked against the engine's web build

Step 40 "Building the page"

if (Test-Path $pageDir)
{
    # the package is preloaded at link time, which only runs again when its output is gone
    Remove-Item (Join-Path $pageDir "$name.js") -ErrorAction SilentlyContinue
}

$gameDirArgument = if ($hasGameCode) { $gameSourceDir -replace '\\', '/' } else { "" }
$gameGeneratedDirArgument = if ($hasGameCode) { $gameGeneratedDir -replace '\\', '/' } else { "" }
$linkOptimization = if ($FastLink) { "-O1" } else { "-Oz" }
$keepsFunctionNames = if ($Run) { "ON" } else { "OFF" }

Invoke-Cmd ("set `"EMSDK_QUIET=1`" && call `"$env:EMSDK\emsdk_env.bat`" >nul && emcmake cmake -S `"$templateDir`" -B `"$cmakeDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release " + `
    "-DHYPERION_WEB_SDK=`"$($webSdk -replace '\\', '/')`" -DHYP_WEB_PAGE_NAME=$name -DHYP_WEB_PAGE_TITLE=`"$pageTitle`" -DHYP_WEB_PACKAGE=`"$($packageDir -replace '\\', '/')`" " + `
    "-DHYP_WEB_GAME_DIR=`"$gameDirArgument`" -DHYP_WEB_GAME_GENERATED_DIR=`"$gameGeneratedDirArgument`" -DHYP_WEB_OUTPUT=`"$($pageDir -replace '\\', '/')`" -DHYP_WEB_LINK_OPTIMIZATION=$linkOptimization -DHYP_WEB_FUNCTION_NAMES=$keepsFunctionNames " + `
    "-DHYP_WEB_MANAGED_LINK_INPUTS=`"$($managedLinkInputs -replace '\\', '/')`" -DHYP_WEB_GAME_CLASS=`"$gameClassArgument`" -DHYP_WEB_PLUGIN_MODULES=`"$($PluginModules -replace ',', ';')`" && cmake --build `"$cmakeDir`"") `
    "Building the page failed"

# 5. stage everything a host needs into Binaries/Web, leaving anything else the folder holds (a CNAME, say) alone

Step 97 "Staging into $outputDir"

New-Item -ItemType Directory -Force $outputDir | Out-Null

$pageFiles = @("index.html", "isolation-worker.js", "brotli_dec_wasm.js", "brotli_dec_wasm_bg.wasm", "$name.js", "$name.wasm", "$name.data")

foreach ($fileName in $pageFiles)
{
    $source = Join-Path $pageDir $fileName

    if (-not (Test-Path $source))
    {
        Fail "$source not found after building the page"
    }

    Copy-Item $source $outputDir -Force
}

# a file named after the game that this build didn't write is left from an older build, and the page could load it
foreach ($staleFile in Get-ChildItem $outputDir -File -Filter "$name.*" | Where-Object { $pageFiles -notcontains $_.Name })
{
    Write-Host "Removing $($staleFile.Name), left from an earlier build"
    Remove-Item $staleFile.FullName -Force
}

$outputCacheDir = Join-Path $outputDir "Cache"

if (Test-Path $outputCacheDir)
{
    Remove-Item $outputCacheDir -Recurse -Force
}

Copy-Item (Join-Path $packageDir "Cache") $outputCacheDir -Recurse

# a static host has no directory listing, so the page reads the names from here
(Get-ChildItem $outputCacheDir -File | Where-Object { $_.Name -ne "index.txt" } | ForEach-Object { $_.Name }) | Set-Content (Join-Path $outputCacheDir "index.txt") -Encoding ascii

# hosts gzip the cache at best; Brotli downloads about a third smaller, so the large files only go up that way
$pwsh = Get-Command pwsh -ErrorAction SilentlyContinue

if (-not $pwsh)
{
    Fail "PowerShell 7 (pwsh) is needed to compress the cache with Brotli. Install it from https://aka.ms/powershell"
}

Step 98 "Compressing the cache with Brotli"

& $pwsh.Source -NoProfile -File (Join-Path $PSScriptRoot "CompressBrotli.ps1") -Directory $outputCacheDir -Quality $(if ($FastLink) { 5 } else { 11 })

if ($LASTEXITCODE -ne 0)
{
    Fail "Compressing the cache with Brotli failed"
}

# GitHub Pages runs a site through Jekyll unless this is there
Set-Content (Join-Path $outputDir ".nojekyll") "" -Encoding ascii

foreach ($file in Get-ChildItem $outputDir -Recurse -File)
{
    if ($file.Length -gt 100MB)
    {
        Write-Host "WARNING: $($file.Name) is over 100 MiB, more than GitHub allows in one file"
    }
}

$totalBytes = (Get-ChildItem $outputDir -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ("Built {0} for the web: {1} ({2:N0} MB)" -f $name, $outputDir, ($totalBytes / 1MB))

# moving keeps the time the build started, so anything saved during the build still counts as newer
Move-Item $buildingStamp $builtStamp -Force

# 6. serve it, when asked

if ($Run)
{
    Start-WebServer
}

exit 0
