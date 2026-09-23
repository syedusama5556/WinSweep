<#
.SYNOPSIS
    Verifies tcscan.dll against the same measurements done in managed code.

.DESCRIPTION
    Builds a temporary tree with known file sizes, then checks that the native
    walker returns byte-identical totals for folder sizes, masked globs and
    project folder discovery. Exits non-zero on the first failure.

.PARAMETER Dll
    Path to tcscan.dll. Defaults to the location build\build-native.bat writes.
#>
[CmdletBinding()]
param(
    [string]$Dll
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Dll) { $Dll = Join-Path $here "..\src\winui\native\tcscan.dll" }
$script:Failures = 0

function Assert-Equal {
    param($Expected, $Actual, [string]$What)
    if ($Expected -eq $Actual) {
        Write-Host ("  PASS  {0}: {1}" -f $What, $Actual)
    } else {
        Write-Host ("  FAIL  {0}: expected {1}, got {2}" -f $What, $Expected, $Actual) -ForegroundColor Red
        $script:Failures++
    }
}

$Dll = (Resolve-Path $Dll).Path
Write-Host "Testing $Dll"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class TcScan {
    [DllImport(@"$Dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    public static extern long tc_folder_size(string path, IntPtr cancel);
    [DllImport(@"$Dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    public static extern long tc_glob_size(string dir, string mask, IntPtr cancel);
    [DllImport(@"$Dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    public static extern int tc_find_dirs(string root, string names, [Out] char[] buffer, int capacity, IntPtr cancel);
}
"@

# ---------------------------------------------------------------- fixture
$root = Join-Path ([IO.Path]::GetTempPath()) ("tcscan-test-" + [Guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory $root -Force | Out-Null

function New-File {
    param([string]$Path, [int]$Bytes)
    New-Item -ItemType Directory (Split-Path $Path) -Force | Out-Null
    [IO.File]::WriteAllBytes($Path, (New-Object byte[] $Bytes))
}

try {
    New-File "$root\a.bin"                    1000
    New-File "$root\nested\b.bin"             2000
    New-File "$root\nested\deep\c.bin"        4000
    New-File "$root\nested\deep\note.log"      500
    New-File "$root\other.log"                 250

    # A real project: build/ next to a marker file, plus a decoy with no marker.
    New-File "$root\project\CMakeLists.txt"     10
    New-File "$root\project\build\out.o"      8000
    New-File "$root\decoy\build\holiday.jpg"  3000
    New-File "$root\project\__pycache__\m.pyc" 600

    Write-Host "`nFolder size"
    $expected = (Get-ChildItem $root -Recurse -File | Measure-Object Length -Sum).Sum
    Assert-Equal $expected ([TcScan]::tc_folder_size($root, [IntPtr]::Zero)) "total bytes under the fixture"

    Write-Host "`nGlob size"
    $expectedLogs = (Get-ChildItem $root -Recurse -File -Filter *.log | Measure-Object Length -Sum).Sum
    Assert-Equal $expectedLogs ([TcScan]::tc_glob_size($root, "*.log", [IntPtr]::Zero)) "*.log bytes, recursive"

    Write-Host "`nMissing paths return zero rather than throwing"
    Assert-Equal 0 ([TcScan]::tc_folder_size("$root\does-not-exist", [IntPtr]::Zero)) "missing folder"
    Assert-Equal 0 ([TcScan]::tc_glob_size("$root\does-not-exist", "*.log", [IntPtr]::Zero)) "missing glob root"

    Write-Host "`nProject folder discovery"
    $buffer = New-Object char[] 65536
    $written = [TcScan]::tc_find_dirs($root, "build`n__pycache__", $buffer, 65536, [IntPtr]::Zero)
    $found = @()
    if ($written -gt 0) {
        $found = (New-Object string($buffer, 0, $written)).Split("`n") | Where-Object { $_ }
    }
    Assert-Equal 3 $found.Count "candidate folders found (both build folders plus __pycache__)"
    Assert-Equal $true ([bool]($found -contains "$root\project\build")) "real build folder reported"
    Assert-Equal $true ([bool]($found -contains "$root\decoy\build")) "decoy build folder reported (marker check is the caller's job)"
    Assert-Equal $true ([bool]($found -contains "$root\project\__pycache__")) "__pycache__ reported"

    Write-Host "`nToo-small buffer reports the required size as a negative count"
    $tiny = New-Object char[] 4
    $needed = [TcScan]::tc_find_dirs($root, "build", $tiny, 4, [IntPtr]::Zero)
    Assert-Equal $true ($needed -lt 0) "negative return when the buffer is too small"
}
finally {
    Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
if ($script:Failures -gt 0) {
    Write-Host "$($script:Failures) check(s) failed." -ForegroundColor Red
    exit 1
}
Write-Host "All native scanner checks passed." -ForegroundColor Green
exit 0
