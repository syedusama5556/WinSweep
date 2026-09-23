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

rem Compile the icon and version info into a linkable object first.
where windres.exe >nul 2>nul
if errorlevel 1 (
  echo WARNING: windres not found, building without the app icon.
  set "RES="
) else (
  windres.exe -I "%ROOT%\src\win32" "%ROOT%\src\win32\app.rc" -O coff -o "%ROOT%\dist\app.res.o"
  if errorlevel 1 (
    echo ERROR: resource compilation failed.
    exit /b 1
  )
  set "RES=%ROOT%\dist\app.res.o"
)

g++.exe -std=c++17 -O2 -municode -mwindows "%ROOT%\src\win32\TempCleanerPro.cpp" %RES% ^
  -lcomctl32 -lshell32 -lole32 -ladvapi32 -lgdi32 -o "%ROOT%\dist\TempCleanerPro.exe"
if errorlevel 1 (
  echo ERROR: Build failed. No usable executable was produced.
  exit /b 1
)
if exist "%ROOT%\dist\app.res.o" del "%ROOT%\dist\app.res.o"
echo SUCCESS: dist\TempCleanerPro.exe
exit /b 0
