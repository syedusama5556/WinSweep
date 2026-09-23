<#
.SYNOPSIS
    Checks that every build reports the same version.

.DESCRIPTION
    The WinUI project file is the single source of truth. Release automation reads
    the version from there, so if the Win32 app, its resource script or the console
    build disagree, a release would ship binaries labelled three different ways.
    This fails the build instead.

    Run it on its own to print the current version:  .\tests\Test-Version.ps1
#>
[CmdletBinding()]
param(
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path (Join-Path $here "..")).Path }
$script:Failures = 0

function Assert-Version {
    param([string]$Expected, [string]$Actual, [string]$Where)
    if ($Expected -eq $Actual) {
        Write-Host "  PASS  $Where reports $Actual"
    } else {
        Write-Host "  FAIL  $Where reports '$Actual', expected '$Expected'" -ForegroundColor Red
        $script:Failures++
    }
}

# ------------------------------------------------- source of truth: the csproj
$csproj = Get-Content (Join-Path $RepoRoot "src\winui\TempCleanerWinUI.csproj") -Raw
if ($csproj -notmatch '<Version>([^<]+)</Version>') {
    Write-Host "  FAIL  no <Version> element in TempCleanerWinUI.csproj" -ForegroundColor Red
    exit 1
}
$version = $Matches[1].Trim()
Write-Host "Version from src/winui/TempCleanerWinUI.csproj: $version`n"

if ($version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Host "  FAIL  version '$version' is not major.minor.patch" -ForegroundColor Red
    $script:Failures++
}

# ------------------------------------------------------------------ Win32 app
$cpp = Get-Content (Join-Path $RepoRoot "src\win32\TempCleanerPro.cpp") -Raw
if ($cpp -match 'APP_VERSION\[\]\s*=\s*L"([^"]+)"') {
    Assert-Version $version $Matches[1] "src/win32/TempCleanerPro.cpp APP_VERSION"
} else {
    Write-Host "  FAIL  APP_VERSION not found in TempCleanerPro.cpp" -ForegroundColor Red
    $script:Failures++
}

# --------------------------------------------------------- Win32 resource file
$rc = Get-Content (Join-Path $RepoRoot "src\win32\app.rc") -Raw
$quad = "$version.0"
foreach ($field in @("FileVersion", "ProductVersion")) {
    if ($rc -match "VALUE\s+`"$field`",\s+`"([^`"]+)`"") {
        Assert-Version $quad $Matches[1] "app.rc $field"
    }
}
foreach ($field in @("FILEVERSION", "PRODUCTVERSION")) {
    if ($rc -match "$field\s+([\d,\s]+)") {
        Assert-Version ($quad -replace '\.', ',') ($Matches[1].Trim() -replace '\s', '') "app.rc $field"
    }
}

# --------------------------------------------------------------- console build
$bat = Get-Content (Join-Path $RepoRoot "src\batch\TempCleaner.bat")
$titleLine = $bat | Where-Object { $_ -match '^title\s' } | Select-Object -First 1
if ($titleLine -match 'v(\d+\.\d+\.\d+)') {
    Assert-Version $version $Matches[1] "TempCleaner.bat title"
}
$banner = $bat | Where-Object { $_ -match 'TempCleaner Pro v\d+\.\d+\.\d+' }
$bannerVersions = $banner | ForEach-Object {
    if ($_ -match 'v(\d+\.\d+\.\d+)') { $Matches[1] }
} | Sort-Object -Unique
foreach ($found in $bannerVersions) {
    Assert-Version $version $found "TempCleaner.bat banner"
}

Write-Host ""
if ($script:Failures -gt 0) {
    Write-Host "$($script:Failures) version mismatch(es). Update them to $version." -ForegroundColor Red
    exit 1
}
Write-Host "Every build reports $version." -ForegroundColor Green

# Release automation consumes this line.
if ($env:GITHUB_OUTPUT) { "version=$version" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8 }
exit 0
