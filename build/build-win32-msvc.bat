@echo off
rem Builds the single-file Win32 app with MSVC.
rem Run this from a "Developer Command Prompt for Visual Studio".
rem Developer: infusiblecoder
setlocal
set "ROOT=%~dp0.."
where cl.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: MSVC compiler not found.
  echo Open "Developer Command Prompt for Visual Studio" and run this file again.
  exit /b 1
)
if not exist "%ROOT%\dist" md "%ROOT%\dist"
pushd "%ROOT%\dist"

rem Icon and version info.
rc.exe /nologo /i "%ROOT%\src\win32" /fo app.res "%ROOT%\src\win32\app.rc"
if errorlevel 1 (
  popd
  echo ERROR: resource compilation failed.
  exit /b 1
)

cl.exe /nologo /std:c++17 /EHsc /W4 /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  "%ROOT%\src\win32\TempCleanerPro.cpp" /link /SUBSYSTEM:WINDOWS /OUT:TempCleanerPro.exe ^
  app.res user32.lib gdi32.lib kernel32.lib comctl32.lib shell32.lib ole32.lib advapi32.lib
if errorlevel 1 (
  popd
  echo ERROR: Build failed. No usable executable was produced.
  exit /b 1
)
del app.res TempCleanerPro.obj 2>nul
popd
echo SUCCESS: dist\TempCleanerPro.exe
exit /b 0
