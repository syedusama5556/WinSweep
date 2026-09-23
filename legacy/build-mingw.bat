@echo off
setlocal
where g++.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: MinGW-w64 g++ not found in PATH.
  exit /b 1
)
g++.exe -std=c++17 -O2 -Wall -Wextra -municode -mwindows UniversalCacheTempCleaner.cpp ^
  -lcomctl32 -lshell32 -lole32 -o UniversalCacheTempCleaner.exe
if errorlevel 1 (
  echo ERROR: Build failed. No usable executable was produced.
  exit /b 1
)
echo SUCCESS: UniversalCacheTempCleaner.exe
exit /b 0
