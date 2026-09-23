@echo off
rem Builds tcscan.dll, the native scanner used by the WinUI app.
rem Developer: infusiblecoder
setlocal
set "ROOT=%~dp0.."
set "SRC=%ROOT%\src\native\tcscan.cpp"
set "OUTDIR=%ROOT%\src\winui\native"
set "OUT=%OUTDIR%\tcscan.dll"
if not exist "%OUTDIR%" md "%OUTDIR%"

rem cl.exe only works from a Developer Command Prompt, where INCLUDE is set.
where cl.exe >nul 2>nul
if not errorlevel 1 if defined INCLUDE (
  cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /LD /DUNICODE /D_UNICODE ^
    "%SRC%" /Fe:"%OUT%" /link kernel32.lib
  if errorlevel 1 goto fail
  goto done
)

where g++.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: neither MSVC cl.exe nor MinGW g++.exe was found.
  exit /b 1
)
g++.exe -std=c++17 -O2 -Wall -Wextra -shared -static -municode "%SRC%" -o "%OUT%"
if errorlevel 1 goto fail

:done
echo SUCCESS: %OUT%
exit /b 0

:fail
echo ERROR: Build failed. No usable tcscan.dll was produced.
exit /b 1
