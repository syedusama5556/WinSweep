<#
.SYNOPSIS
    Creates a desktop shortcut for the console build, with the WinSweep icon.

.DESCRIPTION
    A .bat file cannot carry its own icon, so the icon lives beside it in
    TempCleaner.ico and gets attached to a shortcut instead. The shortcut also
    runs the script elevated, which is what you want for the system rows.

.PARAMETER Destination
    Where to put the shortcut. Defaults to your Desktop.
#>
[CmdletBinding()]
param(
    [string]$Destination
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Destination) { $Destination = [Environment]::GetFolderPath("Desktop") }

$target = Join-Path $here "TempCleaner.bat"
$icon = Join-Path $here "TempCleaner.ico"
if (-not (Test-Path $target)) { throw "TempCleaner.bat was not found next to this script." }

$link = Join-Path $Destination "WinSweep Console.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($link)
$shortcut.TargetPath = "$env:SystemRoot\System32\cmd.exe"
$shortcut.Arguments = "/c `"$target`""
$shortcut.WorkingDirectory = $here
$shortcut.Description = "WinSweep - console cleaner"
if (Test-Path $icon) { $shortcut.IconLocation = "$icon,0" }
$shortcut.Save()

# Set the "run as administrator" bit, byte 21 of the shortcut's header.
$bytes = [IO.File]::ReadAllBytes($link)
$bytes[21] = $bytes[21] -bor 0x20
[IO.File]::WriteAllBytes($link, $bytes)

Write-Host "Created $link" -ForegroundColor Green
