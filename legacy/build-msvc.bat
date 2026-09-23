@echo off
setlocal
where cl.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: MSVC compiler not found.
  echo Open "Developer Command Prompt for Visual Studio" and run this file again.
  exit /b 1
)
cl.exe /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  UniversalCacheTempCleaner.cpp /link /SUBSYSTEM:WINDOWS /OUT:UniversalCacheTempCleaner.exe ^
  user32.lib gdi32.lib kernel32.lib comctl32.lib shell32.lib ole32.lib
if errorlevel 1 (
  echo ERROR: Build failed. No usable executable was produced.
  exit /b 1
)
echo SUCCESS: UniversalCacheTempCleaner.exe
exit /b 0
