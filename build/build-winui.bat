@echo off
rem Builds the WinUI 3 app, unpackaged and self contained.
rem Developer: infusiblecoder
setlocal
set "ROOT=%~dp0.."
where dotnet.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: .NET SDK not found. Install it with:  winget install Microsoft.DotNet.SDK.9
  exit /b 1
)

rem The native scanner is optional, but without it measuring falls back to the
rem slower managed walk, so build it first.
call "%~dp0build-native.bat"
if errorlevel 1 echo WARNING: tcscan.dll was not built. The app will use the managed scanner.

pushd "%ROOT%\src\winui"
rem "build" rather than "publish": an unpackaged publish drops the app's own
rem resources.pri and .xbf files, and the app then crashes on startup.
dotnet build -c Release -r win-x64
if errorlevel 1 (
  popd
  echo ERROR: Build failed. No usable executable was produced.
  exit /b 1
)
popd

echo.
echo SUCCESS: src\winui\bin\Release\net9.0-windows10.0.26100.0\win-x64\TempCleanerWinUI.exe
echo Ship that whole win-x64 folder: it is self contained and needs no .NET install.
exit /b 0
