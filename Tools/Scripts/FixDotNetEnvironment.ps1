# Script to fix broken MSBuildSdksPath environment variable
# Run this script as Administrator to permanently remove the incorrect environment variable

Write-Host "Checking for MSBuildSdksPath environment variable..."

$machineValue = [Environment]::GetEnvironmentVariable("MSBuildSdksPath", "Machine")
$userValue = [Environment]::GetEnvironmentVariable("MSBuildSdksPath", "User")

if ($machineValue) {
    Write-Host "Found MSBuildSdksPath at Machine level: $machineValue" -ForegroundColor Yellow
    Write-Host "Removing..."
    [Environment]::SetEnvironmentVariable("MSBuildSdksPath", $null, "Machine")
    Write-Host "Removed MSBuildSdksPath from Machine environment variables." -ForegroundColor Green
} else {
    Write-Host "No MSBuildSdksPath found at Machine level." -ForegroundColor Green
}

if ($userValue) {
    Write-Host "Found MSBuildSdksPath at User level: $userValue" -ForegroundColor Yellow
    Write-Host "Removing..."
    [Environment]::SetEnvironmentVariable("MSBuildSdksPath", $null, "User")
    Write-Host "Removed MSBuildSdksPath from User environment variables." -ForegroundColor Green
} else {
    Write-Host "No MSBuildSdksPath found at User level." -ForegroundColor Green
}

Write-Host ""
Write-Host "Environment variable fix complete!" -ForegroundColor Green
Write-Host "Please restart your terminal/IDE for changes to take effect."
