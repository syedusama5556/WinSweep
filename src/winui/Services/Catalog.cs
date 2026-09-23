using TempCleanerWinUI.Models;

namespace TempCleanerWinUI.Services;

public sealed record Category(string Name, string Blurb, string Glyph);

/// <summary>Every location the cleaner knows about, grouped into the six categories.</summary>
public static class Catalog
{
    public static readonly Category[] Categories =
    [
        new("Windows", "Temp files, dumps, caches and update leftovers Windows keeps for itself.", ""),
        new("Browsers", "Cache, code cache and GPU cache. Logins, history and passwords stay.", ""),
        new("Developer", "Package manager caches, IDE caches and generated project folders.", ""),
        new("Apps and Games", "Chat, music, launcher and creative app caches.", ""),
        new("Logs and Network", "Servicing logs, defender logs and network cache resets.", ""),
        new("Maintenance", "One-shot tools: restore point, Disk Cleanup, DISM, TRIM, Explorer restart.", ""),
    ];

    private static string Env(string name) => Environment.GetEnvironmentVariable(name) ?? "";
    private static string Local => Env("LOCALAPPDATA");
    private static string Roaming => Env("APPDATA");
    private static string Profile => Env("USERPROFILE");
    private static string WinDir => Env("WINDIR");
    private static string ProgData => Env("PROGRAMDATA");

    private static string P(string root, string tail) =>
        string.IsNullOrEmpty(root) ? "" : Path.Combine(root, tail);

    public static List<CleanItem> Build()
    {
        var items = new List<CleanItem>();

        void Add(int category, RiskLevel risk, ActionKind action, string name, string note,
                 string[] targets, bool selected, string command = "")
        {
            items.Add(new CleanItem
            {
                Category = category,
                Risk = risk,
                Action = action,
                Name = name,
                Note = note,
                Targets = targets.Where(t => !string.IsNullOrWhiteSpace(t)).ToArray(),
                Command = command,
                Selected = selected
            });
        }

        // ---------------------------------------------------------- Windows
        Add(0, RiskLevel.Safe, ActionKind.Contents, "User TEMP files",
            "Everything your account dumped in TEMP. Files in use stay.",
            [Env("TEMP"), P(Local, @"LocalLow\Temp")], true);
        Add(0, RiskLevel.Safe, ActionKind.Contents, "Windows TEMP files",
            "System scratch folder. Needs administrator rights.",
            [P(WinDir, "Temp")], true);
        Add(0, RiskLevel.Safe, ActionKind.Glob, "Crash and memory dumps",
            "Kernel and app crash dumps. Only useful while debugging a crash.",
            [P(WinDir, @"Minidump\*.*"), P(Local, @"CrashDumps\*.*")], true);
        Add(0, RiskLevel.Safe, ActionKind.Contents, "Windows Error Reporting queue",
            "Queued and archived error reports waiting to be sent.",
            [P(ProgData, @"Microsoft\Windows\WER\ReportQueue"),
             P(ProgData, @"Microsoft\Windows\WER\ReportArchive"),
             P(Local, @"Microsoft\Windows\WER")], true);
        Add(0, RiskLevel.Safe, ActionKind.Glob, "Explorer thumbnail and icon cache",
            "Rebuilt on demand. Explorer may hold some files open.",
            [P(Local, @"Microsoft\Windows\Explorer\thumbcache_*.db"),
             P(Local, @"Microsoft\Windows\Explorer\iconcache_*.db")], true);
        Add(0, RiskLevel.Safe, ActionKind.Contents, "Windows internet cache",
            "INetCache. Close browsers and Store apps first.",
            [P(Local, @"Microsoft\Windows\INetCache")], true);
        Add(0, RiskLevel.Safe, ActionKind.Contents, "GPU shader caches",
            "D3D, NVIDIA, AMD and Intel shader caches. Games rebuild them.",
            [P(Local, "D3DSCache"), P(Local, @"NVIDIA\DXCache"), P(Local, @"NVIDIA\GLCache"),
             P(Local, @"AMD\DxCache"), P(Local, @"Intel\ShaderCache")], true);
        Add(0, RiskLevel.Safe, ActionKind.Contents, "System icon and font caches",
            "Rebuilt at next sign-in. Icons may flicker once.",
            [P(Local, @"Microsoft\Windows\Caches")], false);
        Add(0, RiskLevel.Safe, ActionKind.RecycleBin, "Empty Recycle Bin",
            "Permanently removes everything currently in the bin.", [], false);
        Add(0, RiskLevel.Care, ActionKind.WinUpdate, "Windows Update download cache",
            "Stops wuauserv and BITS, clears downloads, starts them again.",
            [P(WinDir, @"SoftwareDistribution\Download")], true);
        Add(0, RiskLevel.Care, ActionKind.Contents, "Delivery Optimization cache",
            "Peer update cache. Windows refills it when it needs to.",
            [P(WinDir, @"SoftwareDistribution\DeliveryOptimization"),
             P(Local, @"Microsoft\Windows\DeliveryOptimization\Cache")], true);
        Add(0, RiskLevel.Care, ActionKind.Contents, "Prefetch data",
            "First launch of each app gets slower until Windows relearns.",
            [P(WinDir, "Prefetch")], false);
        Add(0, RiskLevel.Care, ActionKind.Contents, "Recent items and jump lists",
            "Clears your recent file history in Explorer and the taskbar.",
            [P(Roaming, @"Microsoft\Windows\Recent")], false);
        Add(0, RiskLevel.Risk, ActionKind.Folder, "Windows.old previous install",
            "Removes the rollback copy of your previous Windows version. No going back.",
            [P(Env("SystemDrive") + @"\", "Windows.old")], false);

        // --------------------------------------------------------- Browsers
        (string Label, string Base)[] chromium =
        [
            ("Edge", P(Local, @"Microsoft\Edge\User Data\Default")),
            ("Chrome", P(Local, @"Google\Chrome\User Data\Default")),
            ("Brave", P(Local, @"BraveSoftware\Brave-Browser\User Data\Default")),
            ("Vivaldi", P(Local, @"Vivaldi\User Data\Default")),
            ("Opera", P(Local, @"Opera Software\Opera Stable")),
            ("Opera GX", P(Local, @"Opera Software\Opera GX Stable")),
        ];
        foreach (var (label, dir) in chromium)
        {
            Add(1, RiskLevel.Safe, ActionKind.Contents, $"{label} cache",
                "HTTP, code, GPU and media cache. Close the browser first. Logins stay.",
                [P(dir, "Cache"), P(dir, "Code Cache"), P(dir, "GPUCache"),
                 P(dir, "Media Cache"), P(dir, @"Service Worker\CacheStorage")], true);
        }
        Add(1, RiskLevel.Safe, ActionKind.Glob, "Firefox profile caches",
            "Cache entries inside every Firefox profile.",
            [P(Local, @"Mozilla\Firefox\Profiles\*.*")], true);
        Add(1, RiskLevel.Care, ActionKind.Glob, "Legacy WebCache logs",
            "Old Internet Explorer and WinINet log files.",
            [P(Local, @"Microsoft\Windows\WebCache\*.log")], false);

        // -------------------------------------------------------- Developer
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Gradle build cache and transforms",
            "Rebuilt on the next Gradle build.",
            [P(Profile, @".gradle\caches\build-cache-1"), P(Profile, @".gradle\caches\transforms-3"),
             P(Profile, @".gradle\caches\transforms-4")], true);
        Add(2, RiskLevel.Care, ActionKind.Contents, "Gradle daemon and native cache",
            "Stop running Gradle daemons first.",
            [P(Profile, @".gradle\daemon"), P(Profile, @".gradle\native")], true);
        Add(2, RiskLevel.Risk, ActionKind.Contents, "Gradle whole caches folder",
            "Every dependency is downloaded again on the next build.",
            [P(Profile, @".gradle\caches")], false);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Android SDK and build cache",
            "SDK downloads and the legacy build cache.",
            [P(Profile, @".android\cache"), P(Profile, @".android\build-cache"),
             P(Local, @"Android\Sdk\.temp")], true);
        Add(2, RiskLevel.Care, ActionKind.Contents, "JetBrains and Android Studio caches",
            "Indexes rebuild on the next IDE start, which takes a while.",
            [P(Local, "JetBrains"), P(Local, @"Google\AndroidStudio2024.1\caches")], false);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "VS Code caches and workspace storage",
            "Close VS Code first. Settings and extensions are untouched.",
            [P(Roaming, @"Code\Cache"), P(Roaming, @"Code\CachedData"), P(Roaming, @"Code\Code Cache"),
             P(Roaming, @"Code\logs"), P(Roaming, @"Code\User\workspaceStorage")], true);
        Add(2, RiskLevel.Care, ActionKind.Contents, "Visual Studio local cache",
            "Component model cache and telemetry spool.",
            [P(Local, @"Microsoft\VSApplicationInsights")], false);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "npm cache",
            "npm refills this by itself.",
            [P(Roaming, @"npm-cache\_cacache"), P(Local, @"npm-cache\_cacache")], true);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Yarn cache",
            "Packages are downloaded again on the next install.",
            [P(Local, @"Yarn\Cache")], true);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "pnpm store",
            "Packages are downloaded again on the next install.",
            [P(Local, "pnpm-cache"), P(Local, @"pnpm\store")], true);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Python pip cache",
            "Same effect as pip cache purge.", [P(Local, @"pip\Cache")], true);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "NuGet http cache",
            "Package downloads are repeated, restore still works.",
            [P(Local, @"NuGet\v3-cache"), P(Local, @"Temp\NuGetScratch")], true);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Go build cache",
            "Same effect as go clean -cache.", [P(Local, "go-build")], true);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Cargo registry cache and sources",
            "Crates are downloaded again on the next build.",
            [P(Profile, @".cargo\registry\cache"), P(Profile, @".cargo\registry\src")], true);
        Add(2, RiskLevel.Care, ActionKind.Contents, "Dart and Flutter pub cache",
            "Packages are downloaded again. Run flutter pub get afterwards.",
            [P(Local, @"Pub\Cache\hosted")], false);
        Add(2, RiskLevel.Risk, ActionKind.Contents, "Maven local repository",
            "Every dependency is downloaded again, which can take a long time.",
            [P(Profile, @".m2\repository")], false);
        Add(2, RiskLevel.Safe, ActionKind.Contents, "Unity and Unreal derived caches",
            "Assets and shaders are regenerated on the next open.",
            [P(Local, @"Unity\cache"), P(Roaming, @"Unity\Asset Store-5.x"),
             P(Local, @"UnrealEngine\Common\DerivedDataCache")], true);
        Add(2, RiskLevel.Care, ActionKind.Command, "Docker dangling build cache prune",
            "Runs docker builder prune -f. Docker must be running.",
            [], false, "docker builder prune -f");
        Add(2, RiskLevel.Safe, ActionKind.Sweep, "Project __pycache__ folders",
            "Searches the sweep root. Python recreates these automatically.",
            ["__pycache__"], true);
        Add(2, RiskLevel.Safe, ActionKind.Sweep, "Project pytest, mypy and ruff caches",
            "Searches the sweep root. Tools recreate these automatically.",
            [".pytest_cache", ".mypy_cache", ".ruff_cache"], true);
        Add(2, RiskLevel.Care, ActionKind.Sweep, "Project build output folders",
            "Only folders next to pubspec.yaml, build.gradle or CMakeLists.txt.",
            ["build"], false);
        Add(2, RiskLevel.Care, ActionKind.Sweep, "Project .NET bin and obj folders",
            "Only folders next to a .csproj, .fsproj or .vbproj file.",
            ["bin", "obj"], false);
        Add(2, RiskLevel.Care, ActionKind.Sweep, "Project .gradle and .dart_tool folders",
            "Only folders inside a real Gradle or Flutter project.",
            [".gradle", ".dart_tool"], false);
        Add(2, RiskLevel.Risk, ActionKind.Sweep, "Project node_modules folders",
            "Only next to package.json. Reinstall with npm, yarn or pnpm afterwards.",
            ["node_modules"], false);
        Add(2, RiskLevel.Risk, ActionKind.Sweep, "Project Rust target folders",
            "Only next to Cargo.toml. The next build is a full rebuild.",
            ["target"], false);

        // --------------------------------------------------- Apps and Games
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Discord cache", "Close Discord first.",
            [P(Roaming, @"discord\Cache"), P(Roaming, @"discord\Code Cache"),
             P(Roaming, @"discord\GPUCache")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Spotify cache",
            "Offline downloads are removed and can be re-synced.",
            [P(Local, @"Spotify\Data"), P(Local, @"Spotify\Storage")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Teams cache",
            "Classic and new Teams. You stay signed in.",
            [P(Roaming, @"Microsoft\Teams\Cache"), P(Roaming, @"Microsoft\Teams\Code Cache"),
             P(Roaming, @"Microsoft\Teams\GPUCache"),
             P(Local, @"Packages\MSTeams_8wekyb3d8bbwe\LocalCache")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Slack cache", "Close Slack first.",
            [P(Roaming, @"Slack\Cache"), P(Roaming, @"Slack\Code Cache"),
             P(Roaming, @"Slack\GPUCache")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Zoom cache", "Close Zoom first.",
            [P(Roaming, @"Zoom\data\Cache")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Steam shader cache and part downloads",
            "Shaders rebuild. Unfinished downloads restart.",
            [P(Env("ProgramFiles(x86)"), @"Steam\steamapps\shadercache"),
             P(Env("ProgramFiles(x86)"), @"Steam\steamapps\downloading"),
             P(Env("ProgramFiles(x86)"), @"Steam\appcache\httpcache")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Epic Games web cache and logs",
            "Launcher rebuilds these at startup.",
            [P(Local, @"EpicGamesLauncher\Saved\webcache"),
             P(Local, @"EpicGamesLauncher\Saved\Logs")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Battle.net and EA cache",
            "Launchers rebuild these at startup.",
            [P(Local, @"Battle.net\Cache"), P(Local, @"Electronic Arts\EA Desktop\Logs")], true);
        Add(3, RiskLevel.Safe, ActionKind.Contents, "Adobe media cache",
            "Premiere and After Effects re-conform media as needed.",
            [P(Roaming, @"Adobe\Common\Media Cache Files"),
             P(Roaming, @"Adobe\Common\Media Cache")], true);
        Add(3, RiskLevel.Care, ActionKind.Contents, "Office document cache",
            "Unsynced Office edits can be lost. Close Office apps first.",
            [P(Local, @"Microsoft\Office\16.0\OfficeFileCache")], false);
        Add(3, RiskLevel.Care, ActionKind.Contents, "Telegram and WhatsApp media cache",
            "Downloaded chat media is fetched again when you open it.",
            [P(Roaming, @"Telegram Desktop\tdata\user_data\cache"), P(Local, @"WhatsApp\Cache")], false);

        // ------------------------------------------------- Logs and Network
        Add(4, RiskLevel.Safe, ActionKind.Glob, "Windows setup and CBS logs",
            "Servicing logs. Only useful when diagnosing a failed update.",
            [P(WinDir, @"Logs\CBS\CbsPersist*.log"), P(WinDir, @"Logs\MoSetup\*.log"),
             P(WinDir, @"Panther\*.log"), P(WinDir, @"Logs\*.log")], true);
        Add(4, RiskLevel.Safe, ActionKind.Glob, "Windows Defender logs",
            "Scan history log files. Detection history stays intact.",
            [P(ProgData, @"Microsoft\Windows Defender\*.log")], true);
        Add(4, RiskLevel.Safe, ActionKind.Glob, "WebCache and INetCache logs",
            "Transaction logs left by WinINet.",
            [P(Local, @"Microsoft\Windows\WebCache\*.log"),
             P(Local, @"Microsoft\Windows\INetCache\*.log")], true);
        Add(4, RiskLevel.Safe, ActionKind.Glob, "DISM logs", "Component servicing logs.",
            [P(WinDir, @"Logs\DISM\*.log")], false);
        Add(4, RiskLevel.Safe, ActionKind.Command, "Flush DNS resolver cache",
            "Fixes stale DNS answers. No downside.", [], true, "ipconfig /flushdns");
        Add(4, RiskLevel.Care, ActionKind.Command, "Flush ARP cache",
            "Briefly interrupts local network traffic.", [], false,
            "netsh interface ip delete arpcache");
        Add(4, RiskLevel.Care, ActionKind.Command, "Release and renew IP address",
            "Drops your connection for a few seconds.", [], false,
            "ipconfig /release && ipconfig /renew");
        Add(4, RiskLevel.Risk, ActionKind.Command, "Reset Winsock catalog",
            "Requires a reboot and resets network layered providers.", [], false,
            "netsh winsock reset");
        Add(4, RiskLevel.Risk, ActionKind.EventLogs, "Clear all Windows event logs",
            "Erases the audit trail on this machine. Cannot be undone.", [], false);

        // ------------------------------------------------------ Maintenance
        Add(5, RiskLevel.Safe, ActionKind.Command, "Create system restore point",
            "Do this before a deep clean if System Protection is on.", [], false,
            "powershell -NoProfile -Command \"Checkpoint-Computer -Description 'TempCleaner Pro' " +
            "-RestorePointType MODIFY_SETTINGS\"");
        Add(5, RiskLevel.Safe, ActionKind.Command, "Run Disk Cleanup preset 50",
            "Run cleanmgr /sageset:50 once to choose what this clears.", [], false,
            "cleanmgr /sagerun:50");
        Add(5, RiskLevel.Care, ActionKind.Command, "DISM component store cleanup",
            "Slow. Removes superseded component versions permanently.", [], false,
            "dism /online /cleanup-image /startcomponentcleanup");
        Add(5, RiskLevel.Care, ActionKind.Command, "Optimize and TRIM system drive",
            "Runs defrag /O on the system drive.", [], false,
            $"defrag {Env("SystemDrive")} /O");
        Add(5, RiskLevel.Safe, ActionKind.Explorer, "Restart Explorer and rebuild icon cache",
            "Closes all Explorer windows and starts it again.", [], false);

        return items;
    }
}
