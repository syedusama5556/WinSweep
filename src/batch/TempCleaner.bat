@echo off
setlocal EnableExtensions EnableDelayedExpansion
title TempCleaner Pro v2.0.0

rem ============================================================
rem  TempCleaner Pro v2.0.0
rem  Tabbed, selectable, size-aware cleaner with live progress.
rem  Developer: infusiblecoder
rem  Catalogue based on TempCleaner v1.5.0 by Prashant Thakur.
rem ============================================================

rem ---- enable ANSI colours in the console ----
reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f >nul 2>&1

rem ---- request admin rights ----
if /i "%~1"=="--validate" goto skip_elevate
if /i "%~1"=="--noelevate" goto skip_elevate
fltmc >nul 2>&1
if %errorlevel% NEQ 0 (
    echo Requesting admin rights...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs" >nul 2>&1
    exit /B
)
:skip_elevate

rem ---- escape char for ANSI sequences ----
for /f %%E in ('echo prompt $E^| cmd') do set "ESC=%%E"
set "C0=%ESC%[0m"
set "CB=%ESC%[1;36m"
set "CY=%ESC%[1;33m"
set "CG=%ESC%[1;32m"
set "CR=%ESC%[1;31m"
set "CW=%ESC%[1;37m"
set "CD=%ESC%[90m"
set "INV=%ESC%[7;36m"
set "SPACES=                                                            "
set "DOTS=............................................................"

rem ---- working files ----
set "CFG=%~dp0TempCleaner.cfg"
set "WORK=%TEMP%\TempCleanerPro"
if not exist "%WORK%" md "%WORK%" >nul 2>&1
set "SCANPS=%WORK%\scan.ps1"
set "SCANIN=%WORK%\scan_in.txt"
set "SCANOUT=%WORK%\scan_out.txt"

rem ---- defaults ----
set "DEVROOT=%USERPROFILE%"
set "TAB=1"
set "IC=0"
set "MSG="
set "EMPTYN=0"

call :extract_ps
call :define_items
call :load_config

if /i "%~1"=="--validate" (
    echo Items defined: %IC%
    for /L %%i in (1,1,%IC%) do echo %%i^|!I%%i_TAB!^|!I%%i_TYPE!^|!I%%i_RISK!^|!I%%i_NAME!^|!I%%i_TGT!
    exit /b 0
)

goto main

rem ============================================================
rem  ITEM TABLE
rem ============================================================
:def
rem  %1 tab  %2 risk  %3 type  %4 name  %5 targets  %6 default-selected
set /a IC+=1
set "I%IC%_TAB=%~1"
set "I%IC%_RISK=%~2"
set "I%IC%_TYPE=%~3"
set "I%IC%_NAME=%~4"
set "I%IC%_TGT=%~5"
set "I%IC%_SEL=%~6"
set "SZ_%IC%=-1"
goto :eof

:define_items
set "TABNAME1=Windows"
set "TABNAME2=Browsers"
set "TABNAME3=Developer"
set "TABNAME4=Apps and Games"
set "TABNAME5=Logs and Network"
set "TABNAME6=Maintenance"
set "TABNAME7=Help"

rem ---------------- TAB 1 : Windows ----------------
call :def 1 SAFE FILES "User TEMP files" "%TEMP%\*.*|%LOCALAPPDATA%\LocalLow\Temp\*.*" 1
call :def 1 SAFE FILES "Windows TEMP files" "%WINDIR%\Temp\*.*" 1
call :def 1 SAFE FILES "Crash and memory dumps" "%WINDIR%\MEMORY.DMP|%WINDIR%\Minidump\*.*|%LOCALAPPDATA%\CrashDumps\*.*" 1
call :def 1 SAFE DIR "Windows Error Reporting queue" "%ProgramData%\Microsoft\Windows\WER\ReportQueue|%ProgramData%\Microsoft\Windows\WER\ReportArchive|%LOCALAPPDATA%\Microsoft\Windows\WER" 1
call :def 1 SAFE FILES "Explorer thumbnail and icon cache" "%LOCALAPPDATA%\Microsoft\Windows\Explorer\thumbcache_*.db|%LOCALAPPDATA%\Microsoft\Windows\Explorer\iconcache_*.db" 1
call :def 1 SAFE DIR "Windows internet cache INetCache" "%LOCALAPPDATA%\Microsoft\Windows\INetCache" 1
call :def 1 SAFE DIR "Shader caches D3D NVIDIA AMD Intel" "%LOCALAPPDATA%\D3DSCache|%LOCALAPPDATA%\NVIDIA\DXCache|%LOCALAPPDATA%\NVIDIA\GLCache|%LOCALAPPDATA%\AMD\DxCache|%LOCALAPPDATA%\Intel\ShaderCache" 1
call :def 1 SAFE DIR "System icon and font caches" "%LOCALAPPDATA%\Microsoft\Windows\Caches|%WINDIR%\ServiceProfiles\LocalService\AppData\Local\FontCache" 0
call :def 1 SAFE CMD "Empty Recycle Bin" "powershell -NoProfile -Command Clear-RecycleBin -Force -ErrorAction SilentlyContinue" 1
call :def 1 CARE WU "Windows Update download cache" "%WINDIR%\SoftwareDistribution\Download" 1
call :def 1 CARE DIR "Delivery Optimization cache" "%WINDIR%\SoftwareDistribution\DeliveryOptimization|%LOCALAPPDATA%\Microsoft\Windows\DeliveryOptimization\Cache" 1
call :def 1 CARE FILES "Prefetch data" "%WINDIR%\Prefetch\*.*" 0
call :def 1 CARE FILES "Recent items and jump lists" "%APPDATA%\Microsoft\Windows\Recent\*.*" 0
call :def 1 RISK DIR "Windows.old previous install" "%SystemDrive%\Windows.old" 0

rem ---------------- TAB 2 : Browsers ----------------
call :def 2 SAFE DIR "Edge cache" "%LOCALAPPDATA%\Microsoft\Edge\User Data\Default\Cache|%LOCALAPPDATA%\Microsoft\Edge\User Data\Default\Code Cache|%LOCALAPPDATA%\Microsoft\Edge\User Data\Default\GPUCache" 1
call :def 2 SAFE DIR "Edge service worker and media cache" "%LOCALAPPDATA%\Microsoft\Edge\User Data\Default\Service Worker\CacheStorage|%LOCALAPPDATA%\Microsoft\Edge\User Data\Default\Media Cache" 1
call :def 2 SAFE DIR "Chrome cache" "%LOCALAPPDATA%\Google\Chrome\User Data\Default\Cache|%LOCALAPPDATA%\Google\Chrome\User Data\Default\Code Cache|%LOCALAPPDATA%\Google\Chrome\User Data\Default\GPUCache" 1
call :def 2 SAFE DIR "Chrome service worker and media cache" "%LOCALAPPDATA%\Google\Chrome\User Data\Default\Service Worker\CacheStorage|%LOCALAPPDATA%\Google\Chrome\User Data\Default\Media Cache" 1
call :def 2 SAFE DIR "Brave cache" "%LOCALAPPDATA%\BraveSoftware\Brave-Browser\User Data\Default\Cache|%LOCALAPPDATA%\BraveSoftware\Brave-Browser\User Data\Default\Code Cache|%LOCALAPPDATA%\BraveSoftware\Brave-Browser\User Data\Default\GPUCache" 1
call :def 2 SAFE DIR "Opera and Opera GX cache" "%LOCALAPPDATA%\Opera Software\Opera Stable\Cache|%LOCALAPPDATA%\Opera Software\Opera Stable\Code Cache|%LOCALAPPDATA%\Opera Software\Opera GX Stable\Cache" 1
call :def 2 SAFE DIR "Vivaldi cache" "%LOCALAPPDATA%\Vivaldi\User Data\Default\Cache|%LOCALAPPDATA%\Vivaldi\User Data\Default\Code Cache" 1
call :def 2 SAFE FILES "Firefox profile caches" "%LOCALAPPDATA%\Mozilla\Firefox\Profiles\*.*" 1
call :def 2 CARE FILES "Internet Explorer legacy WebCache" "%LOCALAPPDATA%\Microsoft\Windows\WebCache\*.log" 0

rem ---------------- TAB 3 : Developer ----------------
call :def 3 SAFE DIR "Gradle build cache and transforms" "%USERPROFILE%\.gradle\caches\build-cache-1|%USERPROFILE%\.gradle\caches\transforms-3|%USERPROFILE%\.gradle\caches\transforms-4" 1
call :def 3 CARE DIR "Gradle daemon and native cache" "%USERPROFILE%\.gradle\daemon|%USERPROFILE%\.gradle\native" 1
call :def 3 RISK DIR "Gradle whole caches folder" "%USERPROFILE%\.gradle\caches" 0
call :def 3 SAFE DIR "Android SDK and build cache" "%USERPROFILE%\.android\cache|%USERPROFILE%\.android\build-cache|%LOCALAPPDATA%\Android\Sdk\.temp" 1
call :def 3 SAFE DIR "Android Studio and JetBrains caches" "%LOCALAPPDATA%\Google\AndroidStudio2024.1\caches|%LOCALAPPDATA%\Google\AndroidStudio2023.3\caches|%LOCALAPPDATA%\JetBrains" 0
call :def 3 SAFE DIR "VS Code caches and workspace storage" "%APPDATA%\Code\Cache|%APPDATA%\Code\CachedData|%APPDATA%\Code\Code Cache|%APPDATA%\Code\logs|%APPDATA%\Code\User\workspaceStorage" 1
call :def 3 SAFE DIR "Visual Studio local cache" "%LOCALAPPDATA%\Microsoft\VSApplicationInsights|%LOCALAPPDATA%\Microsoft\VisualStudio\Packages\_Instances" 0
call :def 3 SAFE DIR "npm cache" "%APPDATA%\npm-cache\_cacache|%LOCALAPPDATA%\npm-cache\_cacache" 1
call :def 3 SAFE DIR "Yarn cache" "%LOCALAPPDATA%\Yarn\Cache" 1
call :def 3 SAFE DIR "pnpm store" "%LOCALAPPDATA%\pnpm-cache|%LOCALAPPDATA%\pnpm\store" 1
call :def 3 SAFE DIR "Python pip cache" "%LOCALAPPDATA%\pip\Cache" 1
call :def 3 SAFE DIR "NuGet http cache" "%LOCALAPPDATA%\NuGet\v3-cache|%LOCALAPPDATA%\Temp\NuGetScratch" 1
call :def 3 SAFE DIR "Go build cache" "%LOCALAPPDATA%\go-build" 1
call :def 3 SAFE DIR "Cargo registry cache and sources" "%USERPROFILE%\.cargo\registry\cache|%USERPROFILE%\.cargo\registry\src" 1
call :def 3 CARE DIR "Dart and Flutter pub cache" "%LOCALAPPDATA%\Pub\Cache\hosted" 0
call :def 3 RISK DIR "Maven local repository" "%USERPROFILE%\.m2\repository" 0
call :def 3 SAFE DIR "Unity and Unreal derived caches" "%LOCALAPPDATA%\Unity\cache|%APPDATA%\Unity\Asset Store-5.x|%LOCALAPPDATA%\UnrealEngine\Common\DerivedDataCache" 1
call :def 3 CARE CMD "Docker dangling build cache prune" "docker builder prune -f" 0
call :def 3 SAFE SWEEP "Project __pycache__ folders" "SWEEP:%DEVROOT%::__pycache__" 1
call :def 3 SAFE SWEEP "Project pytest mypy ruff caches" "SWEEP:%DEVROOT%::.pytest_cache|SWEEP:%DEVROOT%::.mypy_cache|SWEEP:%DEVROOT%::.ruff_cache" 1
call :def 3 CARE SWEEP "Project build output folders" "SWEEP:%DEVROOT%::build" 0
call :def 3 CARE SWEEP "Project .NET obj folders" "SWEEP:%DEVROOT%::obj" 0
call :def 3 CARE SWEEP "Project gradle and dart_tool folders" "SWEEP:%DEVROOT%::.gradle|SWEEP:%DEVROOT%::.dart_tool" 0
call :def 3 RISK SWEEP "Project node_modules folders" "SWEEP:%DEVROOT%::node_modules" 0
call :def 3 RISK SWEEP "Project Rust target folders" "SWEEP:%DEVROOT%::target" 0

rem ---------------- TAB 4 : Apps and Games ----------------
call :def 4 SAFE DIR "Discord cache" "%APPDATA%\discord\Cache|%APPDATA%\discord\Code Cache|%APPDATA%\discord\GPUCache" 1
call :def 4 SAFE DIR "Spotify cache" "%LOCALAPPDATA%\Spotify\Data|%LOCALAPPDATA%\Spotify\Storage" 1
call :def 4 SAFE DIR "Teams cache" "%APPDATA%\Microsoft\Teams\Cache|%APPDATA%\Microsoft\Teams\Code Cache|%APPDATA%\Microsoft\Teams\GPUCache|%LOCALAPPDATA%\Packages\MSTeams_8wekyb3d8bbwe\LocalCache" 1
call :def 4 SAFE DIR "Slack cache" "%APPDATA%\Slack\Cache|%APPDATA%\Slack\Code Cache|%APPDATA%\Slack\GPUCache" 1
call :def 4 SAFE DIR "Zoom cache" "%APPDATA%\Zoom\data\Cache" 1
call :def 4 SAFE DIR "Steam shader cache and downloads" "%ProgramFiles(x86)%\Steam\steamapps\shadercache|%ProgramFiles(x86)%\Steam\steamapps\downloading|%ProgramFiles(x86)%\Steam\appcache\httpcache" 1
call :def 4 SAFE DIR "Epic Games web cache and logs" "%LOCALAPPDATA%\EpicGamesLauncher\Saved\webcache|%LOCALAPPDATA%\EpicGamesLauncher\Saved\Logs" 1
call :def 4 SAFE DIR "Battle.net and EA cache" "%LOCALAPPDATA%\Battle.net\Cache|%LOCALAPPDATA%\Electronic Arts\EA Desktop\Logs" 1
call :def 4 SAFE DIR "Adobe media cache" "%APPDATA%\Adobe\Common\Media Cache Files|%APPDATA%\Adobe\Common\Media Cache" 1
call :def 4 SAFE DIR "Office document cache" "%LOCALAPPDATA%\Microsoft\Office\16.0\OfficeFileCache" 0
call :def 4 CARE DIR "Telegram and WhatsApp media cache" "%APPDATA%\Telegram Desktop\tdata\user_data\cache|%LOCALAPPDATA%\WhatsApp\Cache" 0

rem ---------------- TAB 5 : Logs and Network ----------------
call :def 5 SAFE FILES "Windows setup and CBS logs" "%WINDIR%\Logs\CBS\CbsPersist*.log|%WINDIR%\Logs\MoSetup\*.log|%WINDIR%\Panther\*.log|%WINDIR%\Logs\*.log" 1
call :def 5 SAFE FILES "Windows Defender logs" "%ProgramData%\Microsoft\Windows Defender\*.log|%ProgramData%\Microsoft\Windows Defender\Scans\History\Service\*.log" 1
call :def 5 SAFE FILES "WebCache and INetCache logs" "%LOCALAPPDATA%\Microsoft\Windows\WebCache\*.log|%LOCALAPPDATA%\Microsoft\Windows\INetCache\*.log" 1
call :def 5 SAFE FILES "DISM logs" "%WINDIR%\Logs\DISM\*.log" 0
call :def 5 SAFE CMD "Flush DNS resolver cache" "ipconfig /flushdns" 1
call :def 5 CARE CMD "Flush ARP cache" "netsh interface ip delete arpcache" 0
call :def 5 CARE NET "Release and renew IP address" "ipconfig" 0
call :def 5 RISK CMD "Reset Winsock catalog - needs reboot" "netsh winsock reset" 0
call :def 5 RISK EVT "Clear all Windows event logs" "wevtutil" 0

rem ---------------- TAB 6 : Maintenance ----------------
call :def 6 SAFE CMD "Create system restore point" "powershell -NoProfile -Command Checkpoint-Computer -Description TempCleanerPro -RestorePointType MODIFY_SETTINGS -ErrorAction SilentlyContinue" 0
call :def 6 SAFE CMD "Run Disk Cleanup preset 50" "cleanmgr /sagerun:50" 0
call :def 6 CARE CMD "DISM component store cleanup - slow" "dism /online /cleanup-image /startcomponentcleanup" 0
call :def 6 CARE CMD "Optimize and TRIM system drive" "defrag %SystemDrive% /O" 0
call :def 6 SAFE EXPL "Restart Explorer and rebuild icon cache" "explorer" 0
goto :eof

rem ============================================================
rem  CONFIG
rem ============================================================
:save_config
> "%CFG%" echo # TempCleaner Pro selection config
>> "%CFG%" echo DEVROOT=%DEVROOT%
for /L %%i in (1,1,%IC%) do >> "%CFG%" echo SEL%%i=!I%%i_SEL!
set "MSG=%CG%Selection saved to TempCleaner.cfg%C0%"
goto :eof

:load_config
if not exist "%CFG%" goto :eof
for /f "usebackq tokens=1,* delims==" %%a in ("%CFG%") do (
    set "k=%%a"
    if /i "!k!"=="DEVROOT" (
        set "DEVROOT=%%b"
    ) else (
        if "!k:~0,3!"=="SEL" (
            set "n=!k:~3!"
            if defined I!n!_SEL set "I!n!_SEL=%%b"
        )
    )
)
call :retarget_all
goto :eof

rem ============================================================
rem  UI
rem ============================================================
:main
cls
call :draw_header
call :draw_tabs
echo(
if "%TAB%"=="7" (call :draw_help) else (call :draw_items)
echo(
call :draw_footer
set "in="
set /p "in=  %CW%Command: %C0%"
if not defined in (
    set /a EMPTYN+=1
    if !EMPTYN! GEQ 25 goto bye
    goto main
)
set "EMPTYN=0"
set "ACT="
call :handle "!in!"
if "!ACT!"=="BYE" goto bye
if "!ACT!"=="RUN" goto run
if "!ACT!"=="SCAN" goto scan_all
if "!ACT!"=="ROOT" call :setroot
goto main

:draw_header
set /a selcnt=0
set /a seltotal=0
set "anyunknown=0"
for /L %%i in (1,1,%IC%) do (
    if "!I%%i_SEL!"=="1" (
        set /a selcnt+=1
        if !SZ_%%i! GEQ 0 (set /a seltotal+=!SZ_%%i!) else (set "anyunknown=1")
    )
)
call :fmtkb %seltotal%
set "totaltxt=%FMT%"
if "%anyunknown%"=="1" (
    if %seltotal%==0 (set "totaltxt=not scanned yet - press s") else (set "totaltxt=%FMT% so far - press s")
)
echo(
echo  %CB%===============================================================================%C0%
echo   %CY%TempCleaner Pro v2.0.0%C0%   %CD%pick what goes, watch it go%C0%
echo  %CB%===============================================================================%C0%
echo   %CW%Selected:%C0% %CG%%selcnt%%C0% of %IC%   %CW%Reclaimable:%C0% %CG%%totaltxt%%C0%   %CW%Dev root:%C0% %CD%%DEVROOT%%C0%
goto :eof

:draw_tabs
set "tbar= "
for /L %%t in (1,1,7) do (
    set "nm=!TABNAME%%t!"
    if "%TAB%"=="%%t" (set "tbar=!tbar! %INV% %%t !nm! %C0%") else (set "tbar=!tbar! %CD% %%t !nm! %C0%")
)
echo  %CB%-------------------------------------------------------------------------------%C0%
echo !tbar!
echo  %CB%-------------------------------------------------------------------------------%C0%
goto :eof

:draw_items
echo   %CD%      ID  ITEM                                            RISK      SIZE%C0%
for /L %%i in (1,1,%IC%) do (
    if "!I%%i_TAB!"=="%TAB%" (
        set "mark= "
        set "mc=%CD%"
        if "!I%%i_SEL!"=="1" (set "mark=X") & (set "mc=%CG%")
        set "nm=!I%%i_NAME!%SPACES%"
        set "nm=!nm:~0,46!"
        set "id=  %%i"
        set "id=!id:~-3!"
        set "rk=!I%%i_RISK!"
        set "rc=%CG%"
        if "!rk!"=="CARE" set "rc=%CY%"
        if "!rk!"=="RISK" set "rc=%CR%"
        call :fmtkb !SZ_%%i!
        set "sz=%SPACES%!FMT!"
        set "sz=!sz:~-10!"
        echo    !mc![!mark!]%C0% !id!  %CW%!nm!%C0% !rc!!rk!%C0%  %CD%!sz!%C0%
    )
)
goto :eof

:draw_footer
echo  %CB%-------------------------------------------------------------------------------%C0%
echo   %CW%1-%IC%%C0% toggle item   %CW%t1-t7%C0% switch tab   %CW%a%C0% all in tab   %CW%n%C0% none in tab
echo   %CW%s%C0% scan sizes      %CW%p1%C0% safe preset  %CW%p2%C0% standard     %CW%p3%C0% deep   %CW%p0%C0% clear
echo   %CW%d%C0% set dev root    %CW%c%C0% save config  %CG%r%C0% RUN CLEANUP  %CR%q%C0% quit
if defined MSG echo   !MSG!
set "MSG="
goto :eof

:draw_help
echo   %CY%How this works%C0%
echo(
echo   %CW%Tabs%C0%      Type t1 to t7 to move between categories. Tab 7 is this page.
echo   %CW%Select%C0%    Type an item ID to tick or untick it. Only ticked items are touched.
echo   %CW%Scan%C0%      Type s to measure every path first. Sizes appear in the SIZE column.
echo   %CW%Run%C0%       Type r. Every item shows a live bar and the space it actually freed.
echo   %CW%Config%C0%    Type c to save your ticks to TempCleaner.cfg beside this script.
echo   %CW%Dev root%C0%  Type d to change the folder that project sweeps search.
echo(
echo   %CY%Risk labels%C0%
echo     %CG%SAFE%C0%  Regenerated automatically. Nothing you own is lost.
echo     %CY%CARE%C0%  Harmless but has a side effect, such as a slower first launch.
echo     %CR%RISK%C0%  Forces long rebuilds or downloads, or needs a reboot. Off by default.
echo(
echo   %CY%One-time setup for Disk Cleanup preset 50%C0%
echo     1. Open Terminal as Administrator.
echo     2. Run  %CY%cleanmgr /sageset:50%C0%
echo     3. Tick what you want and click OK. The Maintenance item then runs it silently.
echo(
echo   %CY%Developer%C0%
echo     %CY%infusiblecoder%C0%
echo(
echo   %CD%Catalogue based on TempCleaner v1.5.0 by Prashant Thakur.%C0%
goto :eof

rem ============================================================
rem  COMMAND HANDLING
rem ============================================================
:handle
set "cmd=%~1"
if not defined cmd goto :eof
if /i "%cmd%"=="q" (set "ACT=BYE" & goto :eof)
if /i "%cmd%"=="r" (set "ACT=RUN" & goto :eof)
if /i "%cmd%"=="s" (set "ACT=SCAN" & goto :eof)
if /i "%cmd%"=="c" (call :save_config & goto :eof)
if /i "%cmd%"=="d" (set "ACT=ROOT" & goto :eof)
if /i "%cmd%"=="a" (
    for /L %%i in (1,1,%IC%) do if "!I%%i_TAB!"=="%TAB%" if not "!I%%i_RISK!"=="RISK" set "I%%i_SEL=1"
    set "MSG=%CG%Every SAFE and CARE item in this tab is ticked%C0%"
    goto :eof
)
if /i "%cmd%"=="n" (
    for /L %%i in (1,1,%IC%) do if "!I%%i_TAB!"=="%TAB%" set "I%%i_SEL=0"
    goto :eof
)
if /i "%cmd:~0,1%"=="t" (
    set "tn=%cmd:~1%"
    call :is_num "!tn!"
    if "!ISNUM!"=="1" if !tn! GEQ 1 if !tn! LEQ 7 (set "TAB=!tn!" & goto :eof)
    set "MSG=%CR%Tabs are t1 to t7%C0%"
    goto :eof
)
if /i "%cmd:~0,1%"=="p" (
    call :preset "%cmd:~1%"
    goto :eof
)
call :is_num "%cmd%"
if not "%ISNUM%"=="1" (set "MSG=%CR%Unknown command%C0%" & goto :eof)
set /a num=%cmd%
if %num% GEQ 1 if %num% LEQ %IC% (
    if "!I%num%_SEL!"=="1" (set "I%num%_SEL=0") else (set "I%num%_SEL=1")
    goto :eof
)
set "MSG=%CR%No item with that ID%C0%"
goto :eof

:is_num
set "ISNUM=0"
set "s=%~1"
if not defined s goto :eof
for /f "delims=0123456789" %%x in ("%s%") do goto :eof
set "ISNUM=1"
goto :eof

:preset
set "p=%~1"
for /L %%i in (1,1,%IC%) do set "I%%i_SEL=0"
if "%p%"=="0" (set "MSG=%CY%All items cleared%C0%" & goto :eof)
if "%p%"=="1" (
    for /L %%i in (1,1,%IC%) do if "!I%%i_RISK!"=="SAFE" if not "!I%%i_TAB!"=="6" set "I%%i_SEL=1"
    set "MSG=%CG%Safe preset applied%C0%"
    goto :eof
)
if "%p%"=="2" (
    for /L %%i in (1,1,%IC%) do if not "!I%%i_RISK!"=="RISK" if not "!I%%i_TAB!"=="6" set "I%%i_SEL=1"
    set "MSG=%CG%Standard preset applied%C0%"
    goto :eof
)
if "%p%"=="3" (
    for /L %%i in (1,1,%IC%) do if not "!I%%i_TAB!"=="6" set "I%%i_SEL=1"
    set "MSG=%CY%Deep preset applied - review the RISK items before running%C0%"
    goto :eof
)
set "MSG=%CR%Presets are p0 p1 p2 p3%C0%"
goto :eof

:setroot
echo(
echo   %CW%Current dev root:%C0% %DEVROOT%
set "nr="
set /p "nr=  Folder to sweep for project caches: "
if not defined nr goto :eof
set "nr=!nr:"=!"
if not exist "!nr!\" (set "MSG=%CR%Folder not found%C0%" & goto :eof)
set "DEVROOT=!nr!"
call :retarget_all
set "MSG=%CG%Dev root updated and sweep sizes reset%C0%"
goto :eof

:retarget_all
for /L %%i in (1,1,%IC%) do if "!I%%i_TYPE!"=="SWEEP" call :retarget %%i
goto :eof

:retarget
set "idx=%~1"
set "src=!I%idx%_TGT!"
set "out="
:retarget_loop
for /f "tokens=1* delims=|" %%a in ("!src!") do (
    set "one=%%a"
    set "src=%%b"
    for %%z in (1) do (
        set "tail=!one:*::=!"
        if defined out (set "out=!out!|SWEEP:!DEVROOT!::!tail!") else (set "out=SWEEP:!DEVROOT!::!tail!")
    )
)
if defined src goto retarget_loop
set "I%idx%_TGT=!out!"
set "SZ_%idx%=-1"
goto :eof

rem ============================================================
rem  SIZE SCANNING
rem ============================================================
:extract_ps
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content -LiteralPath '%~f0') -match '^#PS#' -replace '^#PS#','' | Set-Content -LiteralPath '%SCANPS%' -Encoding ASCII" >nul 2>&1
goto :eof

:scan_all
set "SCOPE=ALL"
call :scan
goto main

:scan
cls
call :draw_header
echo(
echo   %CY%Measuring targets. Large developer folders can take a moment...%C0%
echo(
break > "%SCANIN%"
set /a cnt=0
for /L %%i in (1,1,%IC%) do (
    call :scannable %%i
    if "!DOIT!"=="1" (
        set /a cnt+=1
        call :emit_targets %%i
        set "SZ_%%i=0"
    ) else (
        set "SZ_%%i=-1"
    )
)
if %cnt%==0 goto :eof
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCANPS%" -ListFile "%SCANIN%" -OutFile "%SCANOUT%" >nul 2>&1
if exist "%SCANOUT%" for /f "usebackq tokens=1,2 delims=|" %%a in ("%SCANOUT%") do set /a SZ_%%a+=%%b
goto :eof

:scannable
set "DOIT=0"
set "ix=%~1"
if "%SCOPE%"=="ALL" set "DOIT=1"
if "%SCOPE%"=="SEL" if "!I%ix%_SEL!"=="1" set "DOIT=1"
if "!I%ix%_TYPE!"=="CMD" set "DOIT=0"
if "!I%ix%_TYPE!"=="EXPL" set "DOIT=0"
if "!I%ix%_TYPE!"=="NET" set "DOIT=0"
if "!I%ix%_TYPE!"=="EVT" set "DOIT=0"
goto :eof

:emit_targets
set "idx=%~1"
set "rest=!I%idx%_TGT!"
:emit_loop
for /f "tokens=1* delims=|" %%a in ("!rest!") do (
    >> "%SCANIN%" echo %idx%^|%%a
    set "rest=%%b"
)
if defined rest goto emit_loop
goto :eof

:fmtkb
set "v=%~1"
if not defined v set "v=0"
if %v% LSS 0 (set "FMT=--" & goto :eof)
if %v% LSS 1024 (set "FMT=%v% KB" & goto :eof)
set /a _mb=%v%/1024
if %_mb% LSS 1024 (set "FMT=%_mb% MB" & goto :eof)
set /a _gb=%v%/1048576
set /a _gd=(%v%*10/1048576) %% 10
set "FMT=%_gb%.%_gd% GB"
goto :eof

rem ============================================================
rem  PROGRESS BAR
rem ============================================================
:bar
set "t=%~2"
if "%t%"=="0" set "t=1"
if "%t%"=="" set "t=1"
set /a PCT=(%~1*100)/%t%
if %PCT% GTR 100 set "PCT=100"
if %PCT% LSS 0 set "PCT=0"
set /a _fill=(PCT*30)/100
set /a _rest=30-_fill
set "_A="
set "_B="
for %%a in (!_fill!) do if not "%%a"=="0" set "_A=!SPACES:~0,%%a!"
for %%b in (!_rest!) do if not "%%b"=="0" set "_B=!SPACES:~0,%%b!"
set "BARTXT=%ESC%[42m!_A!%ESC%[100m!_B!%C0%"
goto :eof

:line
<nul set /p "=%ESC%[1G%ESC%[2K%~1"
goto :eof

rem ============================================================
rem  RUN
rem ============================================================
:run
set /a total=0
set "hasrisk=0"
for /L %%i in (1,1,%IC%) do (
    if "!I%%i_SEL!"=="1" (
        set /a total+=1
        if "!I%%i_RISK!"=="RISK" set "hasrisk=1"
    )
)
if %total%==0 (set "MSG=%CR%Nothing ticked. Select at least one item.%C0%" & goto main)

cls
call :draw_header
echo(
if "%hasrisk%"=="1" (
    echo  %CR%WARNING%C0%
    echo  Your selection contains items marked RISK. Depending on which ones are ticked,
    echo  this can delete a previous Windows installation, wipe package caches that must
    echo  be downloaded again, erase every Windows event log, or reset the network stack
    echo  so that a reboot is required. Nothing goes to the Recycle Bin and this script
    echo  cannot undo it.
    echo(
    echo  RISK items currently ticked:
    for /L %%i in (1,1,%IC%) do if "!I%%i_SEL!"=="1" if "!I%%i_RISK!"=="RISK" echo    %CR%-%C0% !I%%i_NAME!
    echo(
    set "ok="
    set /p "ok=  Type YES in capitals to continue, anything else cancels: "
    if not "!ok!"=="YES" (set "MSG=%CY%Cancelled. Nothing was deleted.%C0%" & goto main)
)

echo(
echo   %CY%Measuring before cleanup...%C0%
set "SCOPE=SEL"
call :scan
for /L %%i in (1,1,%IC%) do set "BEF_%%i=!SZ_%%i!"
set "FREE0="
for /f %%a in ('powershell -NoProfile -Command "[math]::Round((Get-PSDrive %SystemDrive:~0,1%).Free/1KB)" 2^>nul') do set "FREE0=%%a"

cls
call :draw_header
echo(
echo   %CY%Cleaning %total% items. Leave this window open.%C0%
echo(
set /a done=0
for /L %%i in (1,1,%IC%) do (
    if "!I%%i_SEL!"=="1" (
        set /a done+=1
        call :bar !done! %total%
        echo   !BARTXT! %CW%!done!/%total%%C0%  %CW%!I%%i_NAME!%C0%
        call :do_item %%i
    )
)
echo(
echo   %CY%Measuring after cleanup...%C0%
set "SCOPE=SEL"
call :scan
set "FREE1="
for /f %%a in ('powershell -NoProfile -Command "[math]::Round((Get-PSDrive %SystemDrive:~0,1%).Free/1KB)" 2^>nul') do set "FREE1=%%a"

cls
call :draw_header
echo(
echo  %CB%===============================================================================%C0%
echo   %CG%Cleanup complete%C0%
echo  %CB%===============================================================================%C0%
echo(
echo   %CD%ITEM                                                  RECLAIMED%C0%
set /a grand=0
for /L %%i in (1,1,%IC%) do (
    if "!I%%i_SEL!"=="1" (
        set /a fr=0
        if !BEF_%%i! GEQ 0 if !SZ_%%i! GEQ 0 set /a fr=!BEF_%%i!-!SZ_%%i!
        if !fr! LSS 0 set /a fr=0
        set /a grand+=!fr!
        set "nm=!I%%i_NAME! %DOTS%"
        set "nm=!nm:~0,50!"
        call :fmtkb !fr!
        set "sz=%SPACES%!FMT!"
        set "sz=!sz:~-12!"
        echo   %CW%!nm!%C0% %CG%!sz!%C0%
    )
)
echo(
call :fmtkb %grand%
echo   %CY%Total reclaimed by the ticked items:%C0% %CG%%FMT%%C0%
if defined FREE1 if defined FREE0 (
    set /a delta=FREE1-FREE0
    if !delta! LSS 0 set /a delta=0
    call :fmtkb !delta!
    echo   %CY%Free space gained on %SystemDrive%%C0% %CG%!FMT!%C0%
)
echo(
pause
goto main

:do_item
set "idx=%~1"
set "ty=!I%idx%_TYPE!"
if "%ty%"=="CMD" goto do_cmd
if "%ty%"=="EXPL" goto do_explorer
if "%ty%"=="NET" goto do_net
if "%ty%"=="EVT" goto do_evt
if "%ty%"=="WU" goto do_wu
goto do_paths

:do_cmd
call :line "   %CD%running command...%C0%"
cmd /c !I%idx%_TGT! >nul 2>&1
call :line "   %CG%done%C0%"
echo(
goto :eof

:do_explorer
call :line "   %CD%restarting Explorer...%C0%"
taskkill /f /im explorer.exe >nul 2>&1
del /f /s /q /a "%LOCALAPPDATA%\Microsoft\Windows\Explorer\thumbcache_*.db" >nul 2>&1
del /f /s /q /a "%LOCALAPPDATA%\Microsoft\Windows\Explorer\iconcache_*.db" >nul 2>&1
start explorer.exe
call :line "   %CG%Explorer restarted%C0%"
echo(
goto :eof

:do_net
call :line "   %CD%releasing and renewing IP...%C0%"
ipconfig /release >nul 2>&1
ipconfig /renew >nul 2>&1
call :line "   %CG%done%C0%"
echo(
goto :eof

:do_evt
set /a en=0
for /f "delims=" %%L in ('wevtutil el 2^>nul') do set /a en+=1
if !en!==0 goto :eof
set /a ed=0
for /f "delims=" %%L in ('wevtutil el 2^>nul') do (
    set /a ed+=1
    wevtutil cl "%%L" >nul 2>&1
    call :bar !ed! !en!
    call :line "   !BARTXT! !PCT!%%"
)
call :line "   !BARTXT! 100%% %CD%!en! logs cleared%C0%"
echo(
goto :eof

:do_wu
call :line "   %CD%stopping Windows Update services...%C0%"
net stop wuauserv >nul 2>&1
net stop bits >nul 2>&1
call :purge "!I%idx%_TGT!"
call :line "   %CD%restarting Windows Update services...%C0%"
net start wuauserv >nul 2>&1
net start bits >nul 2>&1
call :line "   %CG%done%C0%"
echo(
goto :eof

:do_paths
set "rest=!I%idx%_TGT!"
:do_paths_loop
for /f "tokens=1* delims=|" %%a in ("!rest!") do (
    call :purge "%%a"
    set "rest=%%b"
)
if defined rest goto do_paths_loop
goto :eof

rem ============================================================
rem  DELETION
rem ============================================================
:purge
set "tgt=%~1"
if not defined tgt goto :eof
if "!tgt:~0,6!"=="SWEEP:" goto purge_sweep
echo(!tgt!| findstr /c:"*" >nul && goto purge_glob
echo(!tgt!| findstr /c:"?" >nul && goto purge_glob
if exist "!tgt!\" goto purge_dir
if exist "!tgt!" (
    call :line "   %CD%deleting file...%C0%"
    del /f /q /a "!tgt!" >nul 2>&1
    call :line "   %CG%done%C0%"
    echo(
)
goto :eof

:purge_glob
set "short=!tgt!"
if not "!short:~46!"=="" set "short=...!short:~-43!"
call :line "   %CD%!short!%C0%"
del /f /q /s /a "!tgt!" >nul 2>&1
call :line "   %CG%cleared%C0% %CD%!short!%C0%"
echo(
goto :eof

:purge_dir
set "dir=!tgt!"
set /a cn=0
for /f "delims=" %%c in ('dir /b /a "!dir!" 2^>nul') do set /a cn+=1
if !cn!==0 (
    call :line "   %CD%nothing to remove%C0%"
    echo(
    goto :eof
)
set /a cd=0
for /f "delims=" %%c in ('dir /b /a "!dir!" 2^>nul') do (
    set /a cd+=1
    if exist "!dir!\%%c\" (rd /s /q "!dir!\%%c" >nul 2>&1) else (del /f /q /a "!dir!\%%c" >nul 2>&1)
    call :bar !cd! !cn!
    set "lbl=%%c"
    if not "!lbl:~28!"=="" set "lbl=!lbl:~0,25!..."
    call :line "   !BARTXT! !PCT!%% %CD%!lbl!%C0%"
)
call :line "   !BARTXT! 100%% %CD%!cn! entries removed%C0%"
echo(
goto :eof

:purge_sweep
set "body=!tgt:~6!"
set "swap=!body:::=|!"
for /f "tokens=1,* delims=|" %%a in ("!swap!") do (
    set "sr=%%a"
    set "sn=%%b"
)
if not exist "!sr!\" goto :eof
if not defined sn goto :eof
call :line "   %CD%searching !sr! for !sn! folders...%C0%"
set /a fn=0
for /f "delims=" %%d in ('dir /b /s /ad "!sr!\!sn!" 2^>nul') do set /a fn+=1
if !fn!==0 (
    call :line "   %CD%no !sn! folders found%C0%"
    echo(
    goto :eof
)
set /a fd=0
for /f "delims=" %%d in ('dir /b /s /ad "!sr!\!sn!" 2^>nul') do (
    set /a fd+=1
    rd /s /q "%%d" >nul 2>&1
    call :bar !fd! !fn!
    set "lbl=%%d"
    if not "!lbl:~42!"=="" set "lbl=...!lbl:~-39!"
    call :line "   !BARTXT! !PCT!%% %CD%!lbl!%C0%"
)
call :line "   !BARTXT! 100%% %CD%!fn! !sn! folders removed%C0%"
echo(
goto :eof

:bye
cls
echo(
echo   %CG%TempCleaner Pro closed.%C0%
echo(
endlocal
exit /b 0

rem ============================================================
rem  Embedded PowerShell size scanner. cmd never executes these.
rem ============================================================
#PS#param([string]$ListFile,[string]$OutFile)
#PS#$ErrorActionPreference = 'SilentlyContinue'
#PS#$out = New-Object System.Collections.Generic.List[string]
#PS#foreach($line in (Get-Content -LiteralPath $ListFile)){
#PS#  if([string]::IsNullOrWhiteSpace($line)){ continue }
#PS#  $parts = $line.Split('|',2)
#PS#  if($parts.Count -lt 2){ continue }
#PS#  $id = $parts[0].Trim(); $target = $parts[1].Trim(); $bytes = 0
#PS#  try{
#PS#    if($target.StartsWith('SWEEP:')){
#PS#      $sp = $target.Substring(6) -split '::',2
#PS#      if($sp.Count -eq 2 -and (Test-Path -LiteralPath $sp[0])){
#PS#        $bytes = (Get-ChildItem -LiteralPath $sp[0] -Recurse -Force -Directory -Filter $sp[1] | ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Recurse -Force -File } | Measure-Object -Property Length -Sum).Sum
#PS#      }
#PS#    }
#PS#    elseif($target -match '[*?]'){
#PS#      $bytes = (Get-ChildItem -Path $target -Recurse -Force -File | Measure-Object -Property Length -Sum).Sum
#PS#    }
#PS#    elseif(Test-Path -LiteralPath $target){
#PS#      $item = Get-Item -LiteralPath $target -Force
#PS#      if($item.PSIsContainer){
#PS#        $bytes = (Get-ChildItem -LiteralPath $target -Recurse -Force -File | Measure-Object -Property Length -Sum).Sum
#PS#      } else {
#PS#        $bytes = $item.Length
#PS#      }
#PS#    }
#PS#  } catch { $bytes = 0 }
#PS#  if(-not $bytes){ $bytes = 0 }
#PS#  $kb = [math]::Round($bytes / 1KB)
#PS#  $out.Add(("{0}|{1}" -f $id, $kb))
#PS#}
#PS#Set-Content -LiteralPath $OutFile -Value $out -Encoding ASCII
