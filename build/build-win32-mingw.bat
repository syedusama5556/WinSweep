@echo off
rem Builds the single-file Win32 app with MinGW-w64.
rem Developer: infusiblecoder
setlocal
set "ROOT=%~dp0.."
where g++.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: MinGW-w64 g++ not found in PATH.
  exit /b 1
)
if not exist "%ROOT%\dist" md "%ROOT%\dist"

g++.exe -std=c++17 -O2 -municode -mwindows "%ROOT%\src\win32\TempCleanerPro.cpp" ^
  -lcomctl32 -lshell32 -lole32 -ladvapi32 -lgdi32 -o "%ROOT%\dist\TempCleanerPro.exe"
if errorlevel 1 (
  echo ERROR: Build failed. No usable executable was produced.
  exit /b 1
)
echo SUCCESS: dist\TempCleanerPro.exe
exit /b 0
