<#
.SYNOPSIS
    Sanity checks the cleanup catalogue in all three implementations.

.DESCRIPTION
    The catalogue is the part of this project that can quietly rot: a typo in a
    path, a duplicated row, a risk label that no longer matches the others. This
    parses each implementation and checks the shape of what it declares, then
    runs the console build's own --validate mode to confirm it still enumerates.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path (Join-Path $here "..")).Path }
$script:Failures = 0

function Assert-True {
    param([bool]$Condition, [string]$What, [string]$Detail = "")
    if ($Condition) {
        Write-Host "  PASS  $What"
    } else {
        Write-Host "  FAIL  $What$(if ($Detail) { " - $Detail" })" -ForegroundColor Red
        $script:Failures++
    }
}

$validRisks = @("Safe", "Care", "Risk", "SAFE", "CARE", "RISK",
                "RiskLevel.Safe", "RiskLevel.Care", "RiskLevel.Risk")

# ------------------------------------------------------------------ WinUI (C#)
Write-Host "`nWinUI catalogue - src/winui/Services/Catalog.cs"
$csharp = Get-Content (Join-Path $RepoRoot "src\winui\Services\Catalog.cs") -Raw
$csRows = [regex]::Matches($csharp, 'Add\(\s*(\d+)\s*,\s*(RiskLevel\.\w+)\s*,\s*ActionKind\.(\w+)\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"')
$csNames = $csRows | ForEach-Object { $_.Groups[4].Value }

Assert-True ($csRows.Count -ge 60) "declares at least 60 rows" "found $($csRows.Count)"
Assert-True (($csNames | Sort-Object -Unique).Count -eq $csNames.Count) "no duplicate row names" `
    (($csNames | Group-Object | Where-Object Count -gt 1 | ForEach-Object Name) -join ", ")
Assert-True (($csRows | Where-Object { $validRisks -notcontains $_.Groups[2].Value }).Count -eq 0) "every risk label is known"
Assert-True (($csRows | Where-Object { $_.Groups[1].Value -ge 6 }).Count -eq 0) "every row lands in one of the six categories"
Assert-True (($csRows | Where-Object { $_.Groups[5].Value.Trim().Length -lt 10 }).Count -eq 0) "every row explains what happens"

$categories = $csRows | ForEach-Object { [int]$_.Groups[1].Value } | Sort-Object -Unique
Assert-True ($categories.Count -eq 6) "all six categories are populated" "populated: $($categories -join ', ')"

# ------------------------------------------------------------------ Win32 (C++)
Write-Host "`nWin32 catalogue - src/win32/TempCleanerPro.cpp"
$cpp = Get-Content (Join-Path $RepoRoot "src\win32\TempCleanerPro.cpp") -Raw
$cppRows = [regex]::Matches($cpp, 'add\(\s*(\d+)\s*,\s*Risk::(\w+)\s*,\s*Act::(\w+)\s*,\s*L"([^"]+)"')
$cppNames = $cppRows | ForEach-Object { $_.Groups[4].Value }

Assert-True ($cppRows.Count -ge 60) "declares at least 60 rows" "found $($cppRows.Count)"
Assert-True (($cppNames | Sort-Object -Unique).Count -eq $cppNames.Count) "no duplicate row names" `
    (($cppNames | Group-Object | Where-Object Count -gt 1 | ForEach-Object Name) -join ", ")
Assert-True (($cppRows | Where-Object { @("Safe", "Care", "Danger") -notcontains $_.Groups[2].Value }).Count -eq 0) "every risk label is known"

# ------------------------------------------------------------- Console (batch)
Write-Host "`nConsole catalogue - src/batch/TempCleaner.bat --validate"
$bat = Join-Path $RepoRoot "src\batch\TempCleaner.bat"
$output = & cmd.exe /c "`"$bat`" --validate" 2>&1
$rows = $output | Where-Object { $_ -match '^\d+\|' }
$declared = ($output | Where-Object { $_ -match '^Items defined: (\d+)' } | Select-Object -First 1)

Assert-True ($rows.Count -ge 60) "enumerates at least 60 rows" "found $($rows.Count)"
if ($declared -match 'Items defined: (\d+)') {
    Assert-True ([int]$Matches[1] -eq $rows.Count) "reported count matches the rows printed"
}

$batNames = $rows | ForEach-Object { ($_ -split '\|')[4] }
Assert-True (($batNames | Sort-Object -Unique).Count -eq $batNames.Count) "no duplicate row names" `
    (($batNames | Group-Object | Where-Object Count -gt 1 | ForEach-Object Name) -join ", ")

$badRisk = $rows | Where-Object { @("SAFE", "CARE", "RISK") -notcontains ($_ -split '\|')[3] }
Assert-True ($badRisk.Count -eq 0) "every risk label is known"

$emptyTargets = $rows | Where-Object {
    $parts = $_ -split '\|'
    $type = $parts[2]
    # Tool rows drive a command instead of a path, so an empty target is expected there.
    ($type -in @("FILES", "DIR", "SWEEP", "WU")) -and [string]::IsNullOrWhiteSpace($parts[5])
}
Assert-True ($emptyTargets.Count -eq 0) "every path-based row has a target" `
    (($emptyTargets | Select-Object -First 3) -join " / ")

# --------------------------------------------------------- cross-implementation
Write-Host "`nCross-implementation"
Assert-True ([math]::Abs($csRows.Count - $cppRows.Count) -le 5) `
    "WinUI and Win32 catalogues are the same size, give or take" `
    "WinUI $($csRows.Count), Win32 $($cppRows.Count)"

$shared = $csNames | Where-Object { $cppNames -contains $_ }
Assert-True ($shared.Count -ge 40) "WinUI and Win32 share most row names" "$($shared.Count) shared"

Write-Host ""
if ($script:Failures -gt 0) {
    Write-Host "$($script:Failures) check(s) failed." -ForegroundColor Red
    exit 1
}
Write-Host "All catalogue checks passed." -ForegroundColor Green
exit 0
