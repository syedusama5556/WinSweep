#define UNICODE
#define _UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")

// TempCleaner Pro - pick what goes, watch it go.
// Developer: infusiblecoder
//
// Build (MSVC):  build-tempcleaner-msvc.bat
// Build (MinGW): build-tempcleaner-mingw.bat
//
// Nothing is removed until you press Clean. Sizes are measured first, so the
// reclaimed total at the end is real rather than an estimate.

constexpr wchar_t APP_TITLE[] = L"TempCleaner Pro";
constexpr wchar_t APP_VERSION[] = L"2.0.0";

// COLORREF is 0x00BBGGRR.
constexpr COLORREF C_BG        = 0x001C1715;   // window background
constexpr COLORREF C_PANEL     = 0x00241E1B;   // sidebar
constexpr COLORREF C_CARD      = 0x0028211E;   // list background
constexpr COLORREF C_CARD_ALT  = 0x002E2724;   // hover / selected row
constexpr COLORREF C_BORDER    = 0x00372E2A;
constexpr COLORREF C_TEXT      = 0x00EDEAE8;
constexpr COLORREF C_MUTED     = 0x00A6A09A;
constexpr COLORREF C_ACCENT    = 0x00F6823B;   // blue
constexpr COLORREF C_ACCENT_HI = 0x00FAA560;
constexpr COLORREF C_OK        = 0x0099D334;   // green
constexpr COLORREF C_WARN      = 0x0024BFFB;   // amber
constexpr COLORREF C_DANGER    = 0x007171F8;   // red

enum {
    ID_NAV = 1001, ID_LIST, ID_PROGRESS, ID_STATUS, ID_SUMMARY, ID_HEADLINE, ID_SUBHEAD,
    ID_SEARCH, ID_SEARCH_LABEL, ID_CLEAN, ID_MEASURE, ID_STOP, ID_ALL, ID_NONE,
    ID_PRESET_SAFE, ID_PRESET_STD, ID_PRESET_DEEP, ID_ROOT_EDIT, ID_ROOT_BROWSE,
    ID_ROOT_LABEL, ID_RECYCLE, ID_SAVE, ID_HELP
};

constexpr UINT MSG_SIZE = WM_APP + 1, MSG_PROGRESS = WM_APP + 2,
               MSG_SCAN_DONE = WM_APP + 3, MSG_CLEAN_DONE = WM_APP + 4;

enum class Risk { Safe, Care, Danger };

enum class Act {
    Contents,    // delete everything inside the folder, keep the folder
    Folder,      // delete the folder itself
    Glob,        // delete files matching a mask, recursively
    Sweep,       // find project folders by name under the sweep root and delete them
    Command,     // run a shell command
    RecycleBin,  // empty the recycle bin
    WinUpdate,   // stop update services, clear the download cache, start them again
    Explorer,    // restart Explorer and rebuild the icon cache
    EventLogs    // clear every Windows event log
};

struct Item {
    int tab = 0;
    std::wstring name;
    std::wstring note;
    Risk risk = Risk::Safe;
    Act act = Act::Contents;
    std::vector<std::wstring> targets;   // paths, globs, or sweep folder names
    std::wstring command;
    bool selected = false;
    bool measured = false;
    unsigned long long bytes = 0;
    unsigned long long freed = 0;
};

struct ProgressMsg { int percent = 0; std::wstring line; };
struct SizeMsg { int index = 0; unsigned long long bytes = 0; };
struct CleanDone {
    int cleaned = 0, failed = 0;
    unsigned long long freed = 0;
    bool cancelled = false;
    double seconds = 0;
};

struct TabInfo { const wchar_t* name; const wchar_t* blurb; };
static const TabInfo TABS[] = {
    { L"Windows",          L"Temp files, dumps, caches and update leftovers Windows keeps for itself." },
    { L"Browsers",         L"Cache, code cache and GPU cache. Your logins, history and passwords stay." },
    { L"Developer",        L"Package manager caches, IDE caches and generated project folders." },
    { L"Apps and Games",   L"Chat, music, launcher and creative app caches." },
    { L"Logs and Network", L"Servicing logs, defender logs and network cache resets." },
    { L"Maintenance",      L"One-shot tools: restore point, Disk Cleanup, DISM, TRIM, Explorer restart." },
};
constexpr int TAB_COUNT = 6;

static HWND win, nav, listv, progress, status, summary, headline, subhead, searchEdit,
            cleanBtn, measureBtn, stopBtn, allBtn, noneBtn, safeBtn, stdBtn, deepBtn,
            rootEdit, browseBtn, rootLabel, recycleCheck, saveBtn, helpBtn;
static HBRUSH bgBrush, panelBrush, cardBrush;
static HFONT font, boldFont, titleFont, smallFont;
static std::vector<Item> items;
static std::wstring devRoot;
static std::wstring filterText;
static int activeTab = 0;
static bool filling = false;
static std::atomic_bool cancelWork{ false }, working{ false };
// ----------------------------------------------------------------- helpers

static std::wstring env(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (!n) return L"";
    std::vector<wchar_t> buf(n);
    GetEnvironmentVariableW(name, buf.data(), n);
    return buf.data();
}

static std::wstring join(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return a.back() == L'\\' ? a + b : a + L"\\" + b;
}

static bool dirExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool fileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring fmtBytes(unsigned long long n) {
    const wchar_t* unit[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    double v = (double)n;
    int i = 0;
    while (v >= 1024 && i < 4) { v /= 1024; i++; }
    std::wostringstream s;
    s << std::fixed << std::setprecision(i && v < 10 ? 2 : i ? 1 : 0) << v << L" " << unit[i];
    return s.str();
}

static bool markerIn(const std::wstring& dir, std::initializer_list<const wchar_t*> names) {
    for (auto n : names) if (fileExists(join(dir, n))) return true;
    return false;
}

static bool hasExtension(const std::wstring& dir, const wchar_t* ext) {
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(dir, std::wstring(L"*") + ext).c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { found = true; break; }
    } while (FindNextFileW(h, &f));
    FindClose(h);
    return found;
}

static unsigned long long folderSize(const std::wstring& root) {
    if (cancelWork) return 0;
    unsigned long long total = 0;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(root, L"*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (cancelWork) break;
        if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total += folderSize(join(root, f.cFileName));
        } else {
            ULARGE_INTEGER n{};
            n.HighPart = f.nFileSizeHigh;
            n.LowPart = f.nFileSizeLow;
            total += n.QuadPart;
        }
    } while (FindNextFileW(h, &f));
    FindClose(h);
    return total;
}

static void splitGlob(const std::wstring& target, std::wstring& dir, std::wstring& mask) {
    size_t n = target.find_last_of(L'\\');
    if (n == std::wstring::npos) { dir = L"."; mask = target; return; }
    dir = target.substr(0, n);
    mask = target.substr(n + 1);
}

static unsigned long long globSize(const std::wstring& dir, const std::wstring& mask) {
    if (cancelWork || !dirExists(dir)) return 0;
    unsigned long long total = 0;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(dir, mask).c_str(), &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (cancelWork) break;
            if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ULARGE_INTEGER n{};
            n.HighPart = f.nFileSizeHigh;
            n.LowPart = f.nFileSizeLow;
            total += n.QuadPart;
        } while (FindNextFileW(h, &f));
        FindClose(h);
    }
    h = FindFirstFileW(join(dir, L"*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return total;
    do {
        if (cancelWork) break;
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
        total += globSize(join(dir, f.cFileName), mask);
    } while (FindNextFileW(h, &f));
    FindClose(h);
    return total;
}

// ------------------------------------------------------- project sweeping

static bool sweepMarkerOk(const std::wstring& parentDir, const std::wstring& name) {
    if (!_wcsicmp(name.c_str(), L"node_modules")) return markerIn(parentDir, { L"package.json" });
    if (!_wcsicmp(name.c_str(), L"build"))
        return markerIn(parentDir, { L"pubspec.yaml", L"build.gradle", L"build.gradle.kts", L"CMakeLists.txt" });
    if (!_wcsicmp(name.c_str(), L".dart_tool")) return markerIn(parentDir, { L"pubspec.yaml" });
    if (!_wcsicmp(name.c_str(), L".gradle"))
        return markerIn(parentDir, { L"settings.gradle", L"settings.gradle.kts", L"build.gradle", L"build.gradle.kts" });
    if (!_wcsicmp(name.c_str(), L"target")) return markerIn(parentDir, { L"Cargo.toml" });
    if (!_wcsicmp(name.c_str(), L"bin") || !_wcsicmp(name.c_str(), L"obj"))
        return hasExtension(parentDir, L".csproj") || hasExtension(parentDir, L".fsproj") ||
               hasExtension(parentDir, L".vbproj");
    return true;   // __pycache__, .pytest_cache, .mypy_cache, .ruff_cache
}

static bool skipTreeName(const std::wstring& n) {
    static const wchar_t* skip[] = { L"Windows", L"WinSxS", L"Program Files", L"Program Files (x86)",
                                     L"ProgramData", L"$Recycle.Bin", L"System Volume Information",
                                     L"Recovery", L".git", L".svn" };
    for (auto s : skip) if (!_wcsicmp(n.c_str(), s)) return true;
    return false;
}

static void findSweepTargets(const std::wstring& root, const std::vector<std::wstring>& names,
                             std::vector<std::wstring>& out) {
    if (cancelWork) return;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(root, L"*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (cancelWork) break;
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
        std::wstring name = f.cFileName, path = join(root, name);
        bool hit = false;
        for (const auto& want : names) {
            if (!_wcsicmp(name.c_str(), want.c_str()) && sweepMarkerOk(root, name)) {
                out.push_back(path);
                hit = true;
                break;
            }
        }
        if (!hit && !skipTreeName(name)) findSweepTargets(path, names, out);
    } while (FindNextFileW(h, &f));
    FindClose(h);
}

// ------------------------------------------------------------- catalogue

static void add(int tab, Risk risk, Act act, const wchar_t* name, const wchar_t* note,
                std::vector<std::wstring> targets, bool selected,
                const wchar_t* command = L"") {
    Item it;
    it.tab = tab;
    it.risk = risk;
    it.act = act;
    it.name = name;
    it.note = note;
    it.command = command;
    it.selected = selected;
    for (auto& t : targets) if (!t.empty()) it.targets.push_back(t);
    items.push_back(std::move(it));
}

static void buildCatalogue() {
    const std::wstring local = env(L"LOCALAPPDATA"), roaming = env(L"APPDATA"),
                       profile = env(L"USERPROFILE"), windir = env(L"WINDIR"),
                       progData = env(L"PROGRAMDATA"), temp = env(L"TEMP"),
                       sysDrive = env(L"SystemDrive"), pf86 = env(L"ProgramFiles(x86)");

    // ---- Windows -------------------------------------------------------
    add(0, Risk::Safe, Act::Contents, L"User TEMP files",
        L"Everything your account dumped in TEMP. In-use files stay.",
        { temp, join(local, L"LocalLow\\Temp") }, true);
    add(0, Risk::Safe, Act::Contents, L"Windows TEMP files",
        L"System scratch folder. Needs administrator rights.",
        { join(windir, L"Temp") }, true);
    add(0, Risk::Safe, Act::Glob, L"Crash and memory dumps",
        L"Kernel and app crash dumps. Only useful while debugging a crash.",
        { join(windir, L"Minidump\\*.*"), join(local, L"CrashDumps\\*.*") }, true);
    add(0, Risk::Safe, Act::Contents, L"Windows Error Reporting queue",
        L"Queued and archived error reports waiting to be sent.",
        { join(progData, L"Microsoft\\Windows\\WER\\ReportQueue"),
          join(progData, L"Microsoft\\Windows\\WER\\ReportArchive"),
          join(local, L"Microsoft\\Windows\\WER") }, true);
    add(0, Risk::Safe, Act::Glob, L"Explorer thumbnail and icon cache",
        L"Rebuilt on demand. Explorer may hold some files open.",
        { join(local, L"Microsoft\\Windows\\Explorer\\thumbcache_*.db"),
          join(local, L"Microsoft\\Windows\\Explorer\\iconcache_*.db") }, true);
    add(0, Risk::Safe, Act::Contents, L"Windows internet cache",
        L"INetCache. Close browsers and Store apps first.",
        { join(local, L"Microsoft\\Windows\\INetCache") }, true);
    add(0, Risk::Safe, Act::Contents, L"GPU shader caches",
        L"D3D, NVIDIA, AMD and Intel shader caches. Games rebuild them.",
        { join(local, L"D3DSCache"), join(local, L"NVIDIA\\DXCache"),
          join(local, L"NVIDIA\\GLCache"), join(local, L"AMD\\DxCache"),
          join(local, L"Intel\\ShaderCache") }, true);
    add(0, Risk::Safe, Act::Contents, L"System icon and font caches",
        L"Rebuilt at next sign-in. Icons may flicker once.",
        { join(local, L"Microsoft\\Windows\\Caches") }, false);
    add(0, Risk::Safe, Act::RecycleBin, L"Empty Recycle Bin",
        L"Permanently removes everything currently in the bin.", {}, false);
    add(0, Risk::Care, Act::WinUpdate, L"Windows Update download cache",
        L"Stops wuauserv and BITS, clears downloads, starts them again.",
        { join(windir, L"SoftwareDistribution\\Download") }, true);
    add(0, Risk::Care, Act::Contents, L"Delivery Optimization cache",
        L"Peer update cache. Windows refills it when it needs to.",
        { join(windir, L"SoftwareDistribution\\DeliveryOptimization"),
          join(local, L"Microsoft\\Windows\\DeliveryOptimization\\Cache") }, true);
    add(0, Risk::Care, Act::Contents, L"Prefetch data",
        L"First launch of each app gets slower until Windows relearns.",
        { join(windir, L"Prefetch") }, false);
    add(0, Risk::Care, Act::Contents, L"Recent items and jump lists",
        L"Clears your recent file history in Explorer and taskbar.",
        { join(roaming, L"Microsoft\\Windows\\Recent") }, false);
    add(0, Risk::Danger, Act::Folder, L"Windows.old previous install",
        L"Removes the rollback copy of your previous Windows version. No going back.",
        { join(sysDrive, L"Windows.old") }, false);

    // ---- Browsers ------------------------------------------------------
    struct Chromium { const wchar_t* label; std::wstring base; };
    const Chromium chromium[] = {
        { L"Edge",    join(local, L"Microsoft\\Edge\\User Data\\Default") },
        { L"Chrome",  join(local, L"Google\\Chrome\\User Data\\Default") },
        { L"Brave",   join(local, L"BraveSoftware\\Brave-Browser\\User Data\\Default") },
        { L"Vivaldi", join(local, L"Vivaldi\\User Data\\Default") },
        { L"Opera",   join(local, L"Opera Software\\Opera Stable") },
        { L"Opera GX",join(local, L"Opera Software\\Opera GX Stable") },
    };
    for (const auto& c : chromium) {
        std::wstring label = std::wstring(c.label) + L" cache";
        add(1, Risk::Safe, Act::Contents, label.c_str(),
            L"HTTP, code, GPU and media cache. Close the browser first. Logins stay.",
            { join(c.base, L"Cache"), join(c.base, L"Code Cache"), join(c.base, L"GPUCache"),
              join(c.base, L"Media Cache"), join(c.base, L"Service Worker\\CacheStorage") }, true);
    }
    add(1, Risk::Safe, Act::Glob, L"Firefox profile caches",
        L"Cache entries inside every Firefox profile.",
        { join(local, L"Mozilla\\Firefox\\Profiles\\*.*") }, true);
    add(1, Risk::Care, Act::Glob, L"Legacy WebCache logs",
        L"Old Internet Explorer and WinINet log files.",
        { join(local, L"Microsoft\\Windows\\WebCache\\*.log") }, false);

    // ---- Developer -----------------------------------------------------
    add(2, Risk::Safe, Act::Contents, L"Gradle build cache and transforms",
        L"Rebuilt on the next Gradle build.",
        { join(profile, L".gradle\\caches\\build-cache-1"),
          join(profile, L".gradle\\caches\\transforms-3"),
          join(profile, L".gradle\\caches\\transforms-4") }, true);
    add(2, Risk::Care, Act::Contents, L"Gradle daemon and native cache",
        L"Stop running Gradle daemons first.",
        { join(profile, L".gradle\\daemon"), join(profile, L".gradle\\native") }, true);
    add(2, Risk::Danger, Act::Contents, L"Gradle whole caches folder",
        L"Every dependency is downloaded again on the next build.",
        { join(profile, L".gradle\\caches") }, false);
    add(2, Risk::Safe, Act::Contents, L"Android SDK and build cache",
        L"SDK downloads and the legacy build cache.",
        { join(profile, L".android\\cache"), join(profile, L".android\\build-cache"),
          join(local, L"Android\\Sdk\\.temp") }, true);
    add(2, Risk::Care, Act::Contents, L"JetBrains and Android Studio caches",
        L"Indexes rebuild on the next IDE start, which takes a while.",
        { join(local, L"JetBrains"), join(local, L"Google\\AndroidStudio2024.1\\caches"),
          join(local, L"Google\\AndroidStudio2023.3\\caches") }, false);
    add(2, Risk::Safe, Act::Contents, L"VS Code caches and workspace storage",
        L"Close VS Code first. Settings and extensions are untouched.",
        { join(roaming, L"Code\\Cache"), join(roaming, L"Code\\CachedData"),
          join(roaming, L"Code\\Code Cache"), join(roaming, L"Code\\logs"),
          join(roaming, L"Code\\User\\workspaceStorage") }, true);
    add(2, Risk::Care, Act::Contents, L"Visual Studio local cache",
        L"Component model cache and telemetry spool.",
        { join(local, L"Microsoft\\VSApplicationInsights") }, false);
    add(2, Risk::Safe, Act::Contents, L"npm cache",
        L"npm refills this by itself.",
        { join(roaming, L"npm-cache\\_cacache"), join(local, L"npm-cache\\_cacache") }, true);
    add(2, Risk::Safe, Act::Contents, L"Yarn cache",
        L"Packages are downloaded again on the next install.",
        { join(local, L"Yarn\\Cache") }, true);
    add(2, Risk::Safe, Act::Contents, L"pnpm store",
        L"Packages are downloaded again on the next install.",
        { join(local, L"pnpm-cache"), join(local, L"pnpm\\store") }, true);
    add(2, Risk::Safe, Act::Contents, L"Python pip cache",
        L"Same effect as pip cache purge.",
        { join(local, L"pip\\Cache") }, true);
    add(2, Risk::Safe, Act::Contents, L"NuGet http cache",
        L"Package downloads are repeated, restore still works.",
        { join(local, L"NuGet\\v3-cache"), join(local, L"Temp\\NuGetScratch") }, true);
    add(2, Risk::Safe, Act::Contents, L"Go build cache",
        L"Same effect as go clean -cache.",
        { join(local, L"go-build") }, true);
    add(2, Risk::Safe, Act::Contents, L"Cargo registry cache and sources",
        L"Crates are downloaded again on the next build.",
        { join(profile, L".cargo\\registry\\cache"), join(profile, L".cargo\\registry\\src") }, true);
    add(2, Risk::Care, Act::Contents, L"Dart and Flutter pub cache",
        L"Packages are downloaded again. Run flutter pub get afterwards.",
        { join(local, L"Pub\\Cache\\hosted") }, false);
    add(2, Risk::Danger, Act::Contents, L"Maven local repository",
        L"Every dependency is downloaded again, which can take a long time.",
        { join(profile, L".m2\\repository") }, false);
    add(2, Risk::Safe, Act::Contents, L"Unity and Unreal derived caches",
        L"Assets and shaders are regenerated on the next open.",
        { join(local, L"Unity\\cache"), join(roaming, L"Unity\\Asset Store-5.x"),
          join(local, L"UnrealEngine\\Common\\DerivedDataCache") }, true);
    add(2, Risk::Care, Act::Command, L"Docker dangling build cache prune",
        L"Runs docker builder prune -f. Docker must be running.",
        {}, false, L"docker builder prune -f");
    add(2, Risk::Safe, Act::Sweep, L"Project __pycache__ folders",
        L"Searches the dev root. Python recreates these automatically.",
        { L"__pycache__" }, true);
    add(2, Risk::Safe, Act::Sweep, L"Project pytest, mypy and ruff caches",
        L"Searches the dev root. Tools recreate these automatically.",
        { L".pytest_cache", L".mypy_cache", L".ruff_cache" }, true);
    add(2, Risk::Care, Act::Sweep, L"Project build output folders",
        L"Only folders next to pubspec.yaml, build.gradle or CMakeLists.txt.",
        { L"build" }, false);
    add(2, Risk::Care, Act::Sweep, L"Project .NET bin and obj folders",
        L"Only folders next to a .csproj, .fsproj or .vbproj file.",
        { L"bin", L"obj" }, false);
    add(2, Risk::Care, Act::Sweep, L"Project .gradle and .dart_tool folders",
        L"Only folders inside a real Gradle or Flutter project.",
        { L".gradle", L".dart_tool" }, false);
    add(2, Risk::Danger, Act::Sweep, L"Project node_modules folders",
        L"Only next to package.json. Reinstall with npm, yarn or pnpm afterwards.",
        { L"node_modules" }, false);
    add(2, Risk::Danger, Act::Sweep, L"Project Rust target folders",
        L"Only next to Cargo.toml. The next build is a full rebuild.",
        { L"target" }, false);

    // ---- Apps and Games ------------------------------------------------
    add(3, Risk::Safe, Act::Contents, L"Discord cache",
        L"Close Discord first.",
        { join(roaming, L"discord\\Cache"), join(roaming, L"discord\\Code Cache"),
          join(roaming, L"discord\\GPUCache") }, true);
    add(3, Risk::Safe, Act::Contents, L"Spotify cache",
        L"Offline downloads are removed and can be re-synced.",
        { join(local, L"Spotify\\Data"), join(local, L"Spotify\\Storage") }, true);
    add(3, Risk::Safe, Act::Contents, L"Teams cache",
        L"Classic and new Teams. You stay signed in.",
        { join(roaming, L"Microsoft\\Teams\\Cache"), join(roaming, L"Microsoft\\Teams\\Code Cache"),
          join(roaming, L"Microsoft\\Teams\\GPUCache"),
          join(local, L"Packages\\MSTeams_8wekyb3d8bbwe\\LocalCache") }, true);
    add(3, Risk::Safe, Act::Contents, L"Slack cache",
        L"Close Slack first.",
        { join(roaming, L"Slack\\Cache"), join(roaming, L"Slack\\Code Cache"),
          join(roaming, L"Slack\\GPUCache") }, true);
    add(3, Risk::Safe, Act::Contents, L"Zoom cache", L"Close Zoom first.",
        { join(roaming, L"Zoom\\data\\Cache") }, true);
    add(3, Risk::Safe, Act::Contents, L"Steam shader cache and part downloads",
        L"Shaders rebuild. Unfinished downloads restart.",
        { join(pf86, L"Steam\\steamapps\\shadercache"),
          join(pf86, L"Steam\\steamapps\\downloading"),
          join(pf86, L"Steam\\appcache\\httpcache") }, true);
    add(3, Risk::Safe, Act::Contents, L"Epic Games web cache and logs",
        L"Launcher rebuilds these at startup.",
        { join(local, L"EpicGamesLauncher\\Saved\\webcache"),
          join(local, L"EpicGamesLauncher\\Saved\\Logs") }, true);
    add(3, Risk::Safe, Act::Contents, L"Battle.net and EA cache",
        L"Launchers rebuild these at startup.",
        { join(local, L"Battle.net\\Cache"),
          join(local, L"Electronic Arts\\EA Desktop\\Logs") }, true);
    add(3, Risk::Safe, Act::Contents, L"Adobe media cache",
        L"Premiere and After Effects re-conform media as needed.",
        { join(roaming, L"Adobe\\Common\\Media Cache Files"),
          join(roaming, L"Adobe\\Common\\Media Cache") }, true);
    add(3, Risk::Care, Act::Contents, L"Office document cache",
        L"Unsynced Office edits can be lost. Close Office apps first.",
        { join(local, L"Microsoft\\Office\\16.0\\OfficeFileCache") }, false);
    add(3, Risk::Care, Act::Contents, L"Telegram and WhatsApp media cache",
        L"Downloaded chat media is fetched again when you open it.",
        { join(roaming, L"Telegram Desktop\\tdata\\user_data\\cache"),
          join(local, L"WhatsApp\\Cache") }, false);

    // ---- Logs and Network ----------------------------------------------
    add(4, Risk::Safe, Act::Glob, L"Windows setup and CBS logs",
        L"Servicing logs. Only useful when diagnosing a failed update.",
        { join(windir, L"Logs\\CBS\\CbsPersist*.log"), join(windir, L"Logs\\MoSetup\\*.log"),
          join(windir, L"Panther\\*.log"), join(windir, L"Logs\\*.log") }, true);
    add(4, Risk::Safe, Act::Glob, L"Windows Defender logs",
        L"Scan history log files. Detections history stays intact.",
        { join(progData, L"Microsoft\\Windows Defender\\*.log") }, true);
    add(4, Risk::Safe, Act::Glob, L"WebCache and INetCache logs",
        L"Transaction logs left by WinINet.",
        { join(local, L"Microsoft\\Windows\\WebCache\\*.log"),
          join(local, L"Microsoft\\Windows\\INetCache\\*.log") }, true);
    add(4, Risk::Safe, Act::Glob, L"DISM logs",
        L"Component servicing logs.", { join(windir, L"Logs\\DISM\\*.log") }, false);
    add(4, Risk::Safe, Act::Command, L"Flush DNS resolver cache",
        L"Fixes stale DNS answers. No downside.", {}, true, L"ipconfig /flushdns");
    add(4, Risk::Care, Act::Command, L"Flush ARP cache",
        L"Briefly interrupts local network traffic.", {}, false,
        L"netsh interface ip delete arpcache");
    add(4, Risk::Care, Act::Command, L"Release and renew IP address",
        L"Drops your connection for a few seconds.", {}, false,
        L"ipconfig /release && ipconfig /renew");
    add(4, Risk::Danger, Act::Command, L"Reset Winsock catalog",
        L"Requires a reboot and resets network layered providers.", {}, false,
        L"netsh winsock reset");
    add(4, Risk::Danger, Act::EventLogs, L"Clear all Windows event logs",
        L"Erases the audit trail on this machine. Cannot be undone.", {}, false);

    // ---- Maintenance ---------------------------------------------------
    add(5, Risk::Safe, Act::Command, L"Create system restore point",
        L"Do this before a deep clean if System Protection is on.", {}, false,
        L"powershell -NoProfile -Command \"Checkpoint-Computer -Description 'TempCleaner Pro' "
        L"-RestorePointType MODIFY_SETTINGS\"");
    add(5, Risk::Safe, Act::Command, L"Run Disk Cleanup preset 50",
        L"Run cleanmgr /sageset:50 once to choose what this clears.", {}, false,
        L"cleanmgr /sagerun:50");
    add(5, Risk::Care, Act::Command, L"DISM component store cleanup",
        L"Slow. Removes superseded component versions permanently.", {}, false,
        L"dism /online /cleanup-image /startcomponentcleanup");
    add(5, Risk::Care, Act::Command, L"Optimize and TRIM system drive",
        L"Runs defrag /O on the system drive.", {}, false,
        L"defrag %SystemDrive% /O");
    add(5, Risk::Safe, Act::Explorer, L"Restart Explorer and rebuild icon cache",
        L"Closes all Explorer windows and starts it again.", {}, false);
}

// ------------------------------------------------------------ measuring

static unsigned long long measure(Item& it) {
    unsigned long long total = 0;
    switch (it.act) {
    case Act::Contents:
    case Act::Folder:
    case Act::WinUpdate:
        for (const auto& t : it.targets) if (dirExists(t)) total += folderSize(t);
        break;
    case Act::Glob:
        for (const auto& t : it.targets) {
            std::wstring dir, mask;
            splitGlob(t, dir, mask);
            total += globSize(dir, mask);
        }
        break;
    case Act::Sweep: {
        if (!dirExists(devRoot)) break;
        std::vector<std::wstring> found;
        findSweepTargets(devRoot, it.targets, found);
        for (const auto& f : found) total += folderSize(f);
        break;
    }
    default:
        break;   // commands, recycle bin, explorer, event logs: nothing to measure
    }
    return total;
}

// ------------------------------------------------------------- deleting

static bool shellDelete(const std::wstring& path, bool recycle) {
    std::vector<wchar_t> from(path.begin(), path.end());
    from.push_back(0);
    from.push_back(0);
    SHFILEOPSTRUCTW op{};
    op.hwnd = nullptr;
    op.wFunc = FO_DELETE;
    op.pFrom = from.data();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMMKDIR;
    if (recycle) op.fFlags |= FOF_ALLOWUNDO;
    return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}

static bool removeTree(const std::wstring& path) {
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(path, L"*").c_str(), &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
            std::wstring child = join(path, f.cFileName);
            if (f.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
            if ((f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                removeTree(child);
            else if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                RemoveDirectoryW(child.c_str());
            else
                DeleteFileW(child.c_str());
        } while (FindNextFileW(h, &f));
        FindClose(h);
    }
    return RemoveDirectoryW(path.c_str()) != 0;
}

// Deletes one entry and reports the bytes that went away.
static unsigned long long removeEntry(const std::wstring& path, bool isDir, bool recycle,
                                      unsigned long long knownSize) {
    unsigned long long size = knownSize;
    if (size == 0 && isDir) size = folderSize(path);
    bool ok = recycle ? shellDelete(path, true)
                      : (isDir ? removeTree(path) : (DeleteFileW(path.c_str()) != 0));
    if (!ok && !recycle && isDir) ok = shellDelete(path, false);
    if (!ok && !isDir) {
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        ok = DeleteFileW(path.c_str()) != 0;
    }
    if (!ok) return 0;
    return size;
}

static void report(int percent, const std::wstring& line) {
    auto* p = new ProgressMsg{ percent, line };
    if (!PostMessageW(win, MSG_PROGRESS, 0, (LPARAM)p)) delete p;
}

static unsigned long long clearContents(const std::wstring& dir, bool recycle,
                                        int basePercent, int span) {
    if (!dirExists(dir)) return 0;
    std::vector<std::pair<std::wstring, bool>> entries;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(dir, L"*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
        entries.emplace_back(join(dir, f.cFileName),
                             (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
    } while (FindNextFileW(h, &f));
    FindClose(h);

    unsigned long long freed = 0;
    for (size_t i = 0; i < entries.size(); i++) {
        if (cancelWork) break;
        unsigned long long size = 0;
        if (!entries[i].second) {
            WIN32_FILE_ATTRIBUTE_DATA d{};
            if (GetFileAttributesExW(entries[i].first.c_str(), GetFileExInfoStandard, &d)) {
                ULARGE_INTEGER n{};
                n.HighPart = d.nFileSizeHigh;
                n.LowPart = d.nFileSizeLow;
                size = n.QuadPart;
            }
        }
        freed += removeEntry(entries[i].first, entries[i].second, recycle, size);
        int pct = basePercent + (int)((span * (i + 1)) / (entries.size() ? entries.size() : 1));
        report(pct, entries[i].first);
    }
    return freed;
}

static void collectGlob(const std::wstring& dir, const std::wstring& mask,
                        std::vector<std::pair<std::wstring, unsigned long long>>& out) {
    if (cancelWork || !dirExists(dir)) return;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(join(dir, mask).c_str(), &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ULARGE_INTEGER n{};
            n.HighPart = f.nFileSizeHigh;
            n.LowPart = f.nFileSizeLow;
            out.emplace_back(join(dir, f.cFileName), n.QuadPart);
        } while (FindNextFileW(h, &f));
        FindClose(h);
    }
    h = FindFirstFileW(join(dir, L"*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
        collectGlob(join(dir, f.cFileName), mask, out);
    } while (FindNextFileW(h, &f));
    FindClose(h);
}

static bool runCommand(const std::wstring& command, bool waitForIt = true) {
    std::wstring line = L"cmd.exe /d /c " + command;
    std::vector<wchar_t> buf(line.begin(), line.end());
    buf.push_back(0);
    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    DWORD code = 0;
    if (waitForIt) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

static unsigned long long cleanItem(Item& it, bool recycle) {
    unsigned long long freed = 0;
    const int span = it.targets.empty() ? 100 : (int)(100 / it.targets.size());

    switch (it.act) {
    case Act::Contents: {
        int base = 0;
        for (const auto& t : it.targets) {
            if (cancelWork) break;
            freed += clearContents(t, recycle, base, span);
            base += span;
        }
        break;
    }
    case Act::Folder:
        for (const auto& t : it.targets) {
            if (cancelWork) break;
            if (!dirExists(t)) continue;
            report(20, t);
            freed += removeEntry(t, true, recycle, folderSize(t));
            report(100, t);
        }
        break;
    case Act::Glob: {
        std::vector<std::pair<std::wstring, unsigned long long>> files;
        for (const auto& t : it.targets) {
            std::wstring dir, mask;
            splitGlob(t, dir, mask);
            collectGlob(dir, mask, files);
        }
        for (size_t i = 0; i < files.size(); i++) {
            if (cancelWork) break;
            freed += removeEntry(files[i].first, false, recycle, files[i].second);
            report((int)((100 * (i + 1)) / (files.size() ? files.size() : 1)), files[i].first);
        }
        break;
    }
    case Act::Sweep: {
        if (!dirExists(devRoot)) break;
        report(0, L"Searching " + devRoot + L" ...");
        std::vector<std::wstring> found;
        findSweepTargets(devRoot, it.targets, found);
        for (size_t i = 0; i < found.size(); i++) {
            if (cancelWork) break;
            freed += removeEntry(found[i], true, recycle, folderSize(found[i]));
            report((int)((100 * (i + 1)) / (found.size() ? found.size() : 1)), found[i]);
        }
        break;
    }
    case Act::Command:
        report(30, L"Running: " + it.command);
        runCommand(it.command);
        report(100, it.command);
        break;
    case Act::RecycleBin:
        report(30, L"Emptying the Recycle Bin...");
        SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        report(100, L"Recycle Bin emptied");
        break;
    case Act::WinUpdate: {
        report(5, L"Stopping Windows Update services...");
        runCommand(L"net stop wuauserv");
        runCommand(L"net stop bits");
        for (const auto& t : it.targets) freed += clearContents(t, recycle, 10, 80);
        report(95, L"Starting Windows Update services...");
        runCommand(L"net start wuauserv");
        runCommand(L"net start bits");
        report(100, L"Windows Update cache cleared");
        break;
    }
    case Act::Explorer: {
        report(20, L"Stopping Explorer...");
        runCommand(L"taskkill /f /im explorer.exe");
        const std::wstring dir = join(env(L"LOCALAPPDATA"), L"Microsoft\\Windows\\Explorer");
        std::vector<std::pair<std::wstring, unsigned long long>> files;
        collectGlob(dir, L"thumbcache_*.db", files);
        collectGlob(dir, L"iconcache_*.db", files);
        for (auto& f : files) freed += removeEntry(f.first, false, false, f.second);
        report(70, L"Starting Explorer...");
        ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOW);
        report(100, L"Explorer restarted");
        break;
    }
    case Act::EventLogs:
        report(20, L"Clearing every event log...");
        runCommand(L"powershell -NoProfile -Command \"wevtutil el | ForEach-Object "
                   L"{ wevtutil cl \\\"$_\\\" 2>$null }\"");
        report(100, L"Event logs cleared");
        break;
    }
    return freed;
}

// ------------------------------------------------------------------- state

static std::wstring riskText(Risk r) {
    return r == Risk::Safe ? L"Safe" : r == Risk::Care ? L"Care" : L"Risk";
}

static COLORREF riskColor(Risk r) {
    return r == Risk::Safe ? C_OK : r == Risk::Care ? C_WARN : C_DANGER;
}

static bool managed(const Item& it) {
    return it.act == Act::Command || it.act == Act::RecycleBin ||
           it.act == Act::Explorer || it.act == Act::EventLogs;
}

static std::wstring sizeText(const Item& it) {
    if (managed(it)) return L"tool";
    if (!it.measured) return L"-";
    return fmtBytes(it.bytes);
}

static std::wstring lowerOf(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}

static bool passesFilter(const Item& it) {
    if (filterText.empty()) return true;
    const std::wstring needle = lowerOf(filterText);
    return lowerOf(it.name).find(needle) != std::wstring::npos ||
           lowerOf(it.note).find(needle) != std::wstring::npos;
}

static void tabTotals(int tab, int& selectedCount, unsigned long long& bytes) {
    selectedCount = 0;
    bytes = 0;
    for (const auto& it : items) {
        if (it.tab != tab) continue;
        if (it.selected) selectedCount++;
        if (it.measured) bytes += it.bytes;
    }
}

// --------------------------------------------------------------- list view

static int modelIndex(int row) {
    LVITEMW x{};
    x.mask = LVIF_PARAM;
    x.iItem = row;
    ListView_GetItem(listv, &x);
    return (int)x.lParam;
}

static void updateSummary();

static void fillList() {
    filling = true;
    SendMessageW(listv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(listv);
    int row = 0;
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].tab != activeTab || !passesFilter(items[i])) continue;
        LVITEMW x{};
        x.mask = LVIF_TEXT | LVIF_PARAM;
        x.iItem = row;
        x.pszText = (wchar_t*)items[i].name.c_str();
        x.lParam = (LPARAM)i;
        ListView_InsertItem(listv, &x);
        ListView_SetItemText(listv, row, 1, (wchar_t*)riskText(items[i].risk).c_str());
        std::wstring size = sizeText(items[i]);
        ListView_SetItemText(listv, row, 2, (wchar_t*)size.c_str());
        ListView_SetItemText(listv, row, 3, (wchar_t*)items[i].note.c_str());
        ListView_SetCheckState(listv, row, items[i].selected);
        row++;
    }
    SendMessageW(listv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(listv, nullptr, TRUE);
    filling = false;

    SetWindowTextW(headline, TABS[activeTab].name);
    SetWindowTextW(subhead, TABS[activeTab].blurb);
    const bool devTab = activeTab == 2;
    for (HWND w : { rootLabel, rootEdit, browseBtn }) ShowWindow(w, devTab ? SW_SHOW : SW_HIDE);
    InvalidateRect(nav, nullptr, TRUE);
    updateSummary();
}

static void refreshRow(int modelIdx) {
    if (items[modelIdx].tab != activeTab) return;
    for (int r = 0; r < ListView_GetItemCount(listv); r++) {
        if (modelIndex(r) != modelIdx) continue;
        ListView_SetItemText(listv, r, 2, (wchar_t*)sizeText(items[modelIdx]).c_str());
        break;
    }
}

static void updateSummary() {
    int count = 0;
    unsigned long long bytes = 0;
    bool pending = false;
    for (const auto& it : items) {
        if (!it.selected) continue;
        count++;
        if (it.measured) bytes += it.bytes;
        else if (!managed(it)) pending = true;
    }

    std::wostringstream s;
    if (!count) s << L"Nothing selected yet. Tick the rows you want cleared.";
    else {
        s << count << (count == 1 ? L" item selected" : L" items selected")
          << L"   \x2022   " << fmtBytes(bytes) << L" ready to reclaim";
        if (pending) s << L" (some rows still need measuring)";
    }
    SetWindowTextW(summary, s.str().c_str());

    std::wostringstream btn;
    btn << L"Clean selected";
    if (count) btn << L"  (" << count << L")";
    SetWindowTextW(cleanBtn, btn.str().c_str());
    EnableWindow(cleanBtn, count > 0 && !working);
    InvalidateRect(nav, nullptr, TRUE);
}

// ----------------------------------------------------------------- actions

static bool useRecycleBinFlag() {
    return SendMessageW(recycleCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void setBusy(bool busy) {
    for (HWND w : { measureBtn, allBtn, noneBtn, safeBtn, stdBtn, deepBtn, rootEdit,
                    browseBtn, recycleCheck, saveBtn, nav, searchEdit })
        EnableWindow(w, !busy);
    EnableWindow(stopBtn, busy);
    EnableWindow(cleanBtn, !busy);
    if (!busy) updateSummary();
}

static void readRoot() {
    wchar_t buf[MAX_PATH]{};
    GetWindowTextW(rootEdit, buf, MAX_PATH);
    devRoot = buf;
}

// includeSweeps is false for the quiet measure that runs at startup: walking the
// whole sweep root would hammer the disk before the user has asked for anything.
static void startScan(bool includeSweeps, bool quiet) {
    if (working.exchange(true)) return;
    cancelWork = false;
    readRoot();
    if (!quiet) setBusy(true);
    SetWindowTextW(status, includeSweeps ? L"Measuring every location, including project folders..."
                                         : L"Measuring known locations...");
    std::thread([includeSweeps] {
        size_t done = 0;
        for (size_t i = 0; i < items.size(); i++) {
            if (cancelWork) break;
            done++;
            if (!includeSweeps && items[i].act == Act::Sweep) continue;
            if (managed(items[i])) continue;
            report((int)((100 * done) / items.size()), items[i].name);
            unsigned long long b = measure(items[i]);
            auto* m = new SizeMsg{ (int)i, b };
            if (!PostMessageW(win, MSG_SIZE, 0, (LPARAM)m)) delete m;
        }
        working = false;
        PostMessageW(win, MSG_SCAN_DONE, 0, 0);
    }).detach();
}

static bool confirmClean(const std::vector<int>& chosen) {
    unsigned long long known = 0;
    int riskCount = 0;
    std::wostringstream risky;
    for (int i : chosen) {
        if (items[i].measured) known += items[i].bytes;
        if (items[i].risk != Risk::Danger) continue;
        riskCount++;
        risky << L"    \x2022  " << items[i].name << L"\n";
    }

    std::wostringstream q;
    q << L"Clean " << chosen.size() << (chosen.size() == 1 ? L" item" : L" items") << L"?\n\n"
      << L"Measured so far: " << fmtBytes(known) << L"\n"
      << (useRecycleBinFlag()
              ? L"Deleted items go to the Recycle Bin where Windows allows it.\n"
              : L"Items are deleted permanently and do not go to the Recycle Bin.\n");
    if (riskCount) {
        q << L"\nWARNING: " << riskCount << (riskCount == 1 ? L" item is" : L" items are")
          << L" marked Risk:\n" << risky.str()
          << L"\nThese can force long rebuilds or downloads, erase the audit trail, or need a "
             L"reboot. This program cannot undo them.\n";
    }
    UINT flags = MB_YESNO | MB_ICONWARNING | (riskCount ? MB_DEFBUTTON2 : MB_DEFBUTTON1);
    return MessageBoxW(win, q.str().c_str(), APP_TITLE, flags) == IDYES;
}

static void startClean() {
    std::vector<int> chosen;
    for (size_t i = 0; i < items.size(); i++) if (items[i].selected) chosen.push_back((int)i);
    if (chosen.empty()) {
        MessageBoxW(win, L"Tick at least one row first.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!confirmClean(chosen)) return;
    if (working.exchange(true)) return;

    cancelWork = false;
    readRoot();
    const bool recycle = useRecycleBinFlag();
    setBusy(true);
    SendMessageW(progress, PBM_SETPOS, 0, 0);
    SetWindowTextW(status, L"Cleaning...");

    std::thread([chosen, recycle] {
        const ULONGLONG began = GetTickCount64();
        CleanDone done{};
        for (size_t n = 0; n < chosen.size(); n++) {
            if (cancelWork) { done.cancelled = true; break; }
            Item& it = items[chosen[n]];
            std::wostringstream head;
            head << L"[" << (n + 1) << L" of " << chosen.size() << L"]  " << it.name;
            report((int)((100 * n) / chosen.size()), head.str());

            const unsigned long long before = it.measured ? it.bytes : 0;
            const unsigned long long freed = cleanItem(it, recycle);
            it.freed = freed;
            it.bytes = before > freed ? before - freed : 0;
            done.freed += freed;
            if (freed || managed(it)) done.cleaned++;
            else done.failed++;
        }
        done.seconds = (GetTickCount64() - began) / 1000.0;
        working = false;
        auto* d = new CleanDone(done);
        if (!PostMessageW(win, MSG_CLEAN_DONE, 0, (LPARAM)d)) delete d;
    }).detach();
}

static void applyPreset(int which) {   // 0 none, 1 safe, 2 standard, 3 deep
    for (auto& it : items) {
        if (which == 0) { it.selected = false; continue; }
        if (it.tab == 5) { it.selected = false; continue; }   // maintenance stays manual
        if (which == 1) it.selected = it.risk == Risk::Safe && !managed(it);
        else if (which == 2) it.selected = it.risk != Risk::Danger;
        else it.selected = true;
    }
    fillList();
}

static void tickVisible(bool on) {
    for (int r = 0; r < ListView_GetItemCount(listv); r++) {
        Item& it = items[modelIndex(r)];
        if (on && it.risk == Risk::Danger) continue;   // never bulk-tick the risky ones
        it.selected = on;
    }
    fillList();
}

// ------------------------------------------------------------------ config

static std::wstring configPath() {
    return join(join(env(L"APPDATA"), L"TempCleanerPro"), L"selection.cfg");
}

static bool writeTextFile(const std::wstring& path, const std::wstring& text) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const wchar_t bom = 0xFEFF;
    DWORD written = 0;
    WriteFile(h, &bom, sizeof(bom), &written, nullptr);
    WriteFile(h, text.data(), (DWORD)(text.size() * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(h);
    return true;
}

static bool readTextFile(const std::wstring& path, std::wstring& text) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(h, &size);
    std::vector<wchar_t> buf((size_t)(size.QuadPart / sizeof(wchar_t)) + 2, 0);
    DWORD read = 0;
    ReadFile(h, buf.data(), (DWORD)size.QuadPart, &read, nullptr);
    CloseHandle(h);
    text.assign(buf.data(), read / sizeof(wchar_t));
    if (!text.empty() && text[0] == 0xFEFF) text.erase(0, 1);
    return true;
}

static void saveConfig() {
    const std::wstring dir = join(env(L"APPDATA"), L"TempCleanerPro");
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wostringstream out;
    out << L"root=" << devRoot << L"\r\n";
    for (const auto& it : items) out << (it.selected ? 1 : 0) << L"=" << it.name << L"\r\n";
    SetWindowTextW(status, writeTextFile(configPath(), out.str())
                               ? L"Selection saved. It loads again next time you start."
                               : L"Could not save the selection.");
}

static void loadConfig() {
    std::wstring text;
    if (!readTextFile(configPath(), text)) return;
    std::wistringstream in(text);
    std::wstring line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n')) line.pop_back();
        if (line.rfind(L"root=", 0) == 0) { devRoot = line.substr(5); continue; }
        if (line.size() < 3 || line[1] != L'=') continue;
        const bool on = line[0] == L'1';
        const std::wstring name = line.substr(2);
        for (auto& it : items) if (it.name == name) it.selected = on;
    }
}

static void browseForRoot() {
    BROWSEINFOW bi{};
    bi.hwndOwner = win;
    bi.lpszTitle = L"Choose the folder that project sweeps should search";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH]{};
    if (SHGetPathFromIDListW(pidl, path)) {
        SetWindowTextW(rootEdit, path);
        devRoot = path;
        for (auto& it : items) if (it.act == Act::Sweep) { it.measured = false; it.bytes = 0; }
        fillList();
    }
    CoTaskMemFree(pidl);
}

static void showHelp() {
    std::wostringstream s;
    s << APP_TITLE << L" " << APP_VERSION << L"\n\n"
      << L"1.  Pick a category on the left.\n"
      << L"2.  Tick the rows you want cleared, or use a preset.\n"
      << L"3.  Press Measure sizes to see what each row really holds.\n"
      << L"4.  Press Clean selected. Progress and reclaimed space are shown live.\n\n"
      << L"Presets\n"
      << L"    Safe        only rows that regenerate on their own.\n"
      << L"    Standard    safe rows plus ones with a small side effect.\n"
      << L"    Deep        everything except the Maintenance tools.\n\n"
      << L"Risk labels\n"
      << L"    Safe    regenerated automatically, nothing you own is lost.\n"
      << L"    Care    harmless but has a side effect, such as a slower first launch.\n"
      << L"    Risk    forces long rebuilds or downloads, or needs a reboot.\n\n"
      << L"Project sweeps only delete folders sitting next to a real project marker such as "
         L"package.json, Cargo.toml, pubspec.yaml or a .csproj file, so unrelated folders "
         L"named build or target are left alone.\n\n"
      << L"Disk Cleanup preset: run  cleanmgr /sageset:50  once to choose what it clears.\n\n"
      << L"Developer\n"
      << L"    infusiblecoder\n\n"
      << L"Original TempCleaner v1.5.0 catalogue by Prashant Thakur.";
    MessageBoxW(win, s.str().c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
}

// ------------------------------------------------------------- owner draw

enum class Style { Primary, Normal, Ghost };

static Style styleOf(int id) {
    if (id == ID_CLEAN) return Style::Primary;
    if (id == ID_MEASURE || id == ID_STOP) return Style::Normal;
    return Style::Ghost;
}

static void drawButton(LPDRAWITEMSTRUCT d) {
    const int id = (int)d->CtlID;
    const Style style = styleOf(id);
    const bool disabled = (d->itemState & ODS_DISABLED) != 0;
    const bool pressed = (d->itemState & ODS_SELECTED) != 0;

    COLORREF fill = C_PANEL, edge = C_BORDER, text = C_TEXT;
    if (style == Style::Primary) {
        fill = pressed ? C_ACCENT_HI : C_ACCENT;
        edge = fill;
        text = 0x00FFFFFF;
    } else if (style == Style::Normal) {
        fill = pressed ? C_CARD_ALT : C_CARD;
        edge = C_BORDER;
    } else {
        fill = pressed ? C_CARD_ALT : C_PANEL;
        edge = C_BORDER;
        text = C_MUTED;
    }
    if (disabled) { fill = C_PANEL; edge = C_BORDER; text = 0x00706A66; }

    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(d->hDC, b), op = SelectObject(d->hDC, p);
    RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 8, 8);
    SelectObject(d->hDC, ob);
    SelectObject(d->hDC, op);
    DeleteObject(b);
    DeleteObject(p);

    wchar_t caption[128]{};
    GetWindowTextW(d->hwndItem, caption, 127);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, text);
    SelectObject(d->hDC, style == Style::Primary ? boldFont : font);
    DrawTextW(d->hDC, caption, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void drawNavItem(LPDRAWITEMSTRUCT d) {
    if ((int)d->itemID < 0 || (int)d->itemID >= TAB_COUNT) return;
    const bool active = (int)d->itemID == activeTab;

    HBRUSH b = CreateSolidBrush(active ? C_CARD_ALT : C_PANEL);
    FillRect(d->hDC, &d->rcItem, b);
    DeleteObject(b);

    if (active) {
        RECT bar = d->rcItem;
        bar.right = bar.left + 3;
        HBRUSH a = CreateSolidBrush(C_ACCENT);
        FillRect(d->hDC, &bar, a);
        DeleteObject(a);
    }

    int count = 0;
    unsigned long long bytes = 0;
    tabTotals((int)d->itemID, count, bytes);

    SetBkMode(d->hDC, TRANSPARENT);
    RECT r = d->rcItem;
    r.left += 18;
    r.top += 9;
    SetTextColor(d->hDC, active ? C_TEXT : C_MUTED);
    SelectObject(d->hDC, active ? boldFont : font);
    DrawTextW(d->hDC, TABS[d->itemID].name, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE);

    std::wostringstream sub;
    if (count) sub << count << L" ticked";
    else sub << L"none ticked";
    if (bytes) sub << L"  \x2022  " << fmtBytes(bytes);
    r.top += 20;
    SetTextColor(d->hDC, count ? C_ACCENT_HI : 0x00807A76);
    SelectObject(d->hDC, smallFont);
    DrawTextW(d->hDC, sub.str().c_str(), -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE);
}

static LRESULT listCustomDraw(LPNMLVCUSTOMDRAW cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        cd->clrTextBk = (cd->nmcd.dwItemSpec % 2) ? C_CARD_ALT : C_CARD;
        cd->clrText = C_TEXT;
        return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        const int index = (int)cd->nmcd.lItemlParam;
        cd->clrTextBk = (cd->nmcd.dwItemSpec % 2) ? C_CARD_ALT : C_CARD;
        if (index < 0 || index >= (int)items.size()) return CDRF_DODEFAULT;
        switch (cd->iSubItem) {
        case 0: cd->clrText = C_TEXT; break;
        case 1: cd->clrText = riskColor(items[index].risk); break;
        case 2: cd->clrText = items[index].measured ? C_TEXT : C_MUTED; break;
        default: cd->clrText = C_MUTED; break;
        }
        return CDRF_NEWFONT;
    }
    }
    return CDRF_DODEFAULT;
}

// --------------------------------------------------------------- creation

static HWND makeButton(HWND parent, const wchar_t* text, int id) {
    HWND w = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(w, WM_SETFONT, (WPARAM)font, TRUE);
    return w;
}

static HWND makeLabel(HWND parent, const wchar_t* text, int id, HFONT f) {
    HWND w = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(w, WM_SETFONT, (WPARAM)f, TRUE);
    return w;
}

static void layout(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    const int W = rc.right, H = rc.bottom;
    const int navW = 230, pad = 20, footerH = 96;

    MoveWindow(nav, 0, 0, navW, H - footerH, TRUE);

    int x = navW + pad, y = 18;
    const int contentW = W - navW - pad * 2;

    MoveWindow(headline, x, y, contentW - 360, 30, TRUE);
    MoveWindow(helpBtn, W - pad - 90, y, 90, 30, TRUE);
    MoveWindow(saveBtn, W - pad - 90 - 8 - 130, y, 130, 30, TRUE);
    y += 30;
    MoveWindow(subhead, x, y, contentW, 20, TRUE);
    y += 28;

    // preset row + filter box
    int bx = x;
    auto place = [&](HWND w, int width, int height = 30) {
        MoveWindow(w, bx, y, width, height, TRUE);
        bx += width + 8;
    };
    place(safeBtn, 90);
    place(stdBtn, 105);
    place(deepBtn, 90);
    bx += 10;
    place(allBtn, 130);
    place(noneBtn, 110);

    const int searchW = std::max(180, W - pad - bx);
    MoveWindow(searchEdit, W - pad - searchW, y, searchW, 30, TRUE);
    y += 38;

    // sweep root row, only visible on the Developer tab
    MoveWindow(rootLabel, x, y + 6, 120, 20, TRUE);
    MoveWindow(browseBtn, W - pad - 100, y, 100, 30, TRUE);
    MoveWindow(rootEdit, x + 124, y + 2, std::max(160, W - pad - 100 - 8 - (x + 124)), 26, TRUE);
    const bool devTab = activeTab == 2;
    if (devTab) y += 38;

    const int listH = std::max(140, H - footerH - y - 12);
    MoveWindow(listv, x, y, contentW, listH, TRUE);
    SetWindowPos(listv, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    const int lw = contentW;
    ListView_SetColumnWidth(listv, 0, std::max(240, (int)(lw * 0.28)));
    ListView_SetColumnWidth(listv, 1, 70);
    ListView_SetColumnWidth(listv, 2, 110);
    ListView_SetColumnWidth(listv, 3, std::max(220, lw - (int)(lw * 0.28) - 205));

    // footer
    const int fy = H - footerH;
    MoveWindow(summary, pad, fy + 12, W - pad * 2 - 470, 22, TRUE);
    MoveWindow(recycleCheck, pad, fy + 40, 260, 22, TRUE);
    MoveWindow(progress, pad, fy + 68, W - pad * 2, 6, TRUE);
    MoveWindow(status, pad, fy + 76, W - pad * 2, 18, TRUE);

    MoveWindow(cleanBtn, W - pad - 200, fy + 14, 200, 44, TRUE);
    MoveWindow(measureBtn, W - pad - 200 - 8 - 150, fy + 14, 150, 44, TRUE);
    MoveWindow(stopBtn, W - pad - 200 - 8 - 150 - 8 - 90, fy + 14, 90, 44, TRUE);
}

static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        win = h;
        font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
        boldFont = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                               CLEARTYPE_QUALITY, 0, L"Segoe UI");
        titleFont = CreateFontW(-21, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                CLEARTYPE_QUALITY, 0, L"Segoe UI");
        smallFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                CLEARTYPE_QUALITY, 0, L"Segoe UI");
        bgBrush = CreateSolidBrush(C_BG);
        panelBrush = CreateSolidBrush(C_PANEL);
        cardBrush = CreateSolidBrush(C_CARD);

        nav = CreateWindowW(L"LISTBOX", L"",
                            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LBS_OWNERDRAWFIXED |
                            LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                            0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_NAV, nullptr, nullptr);
        SendMessageW(nav, WM_SETFONT, (WPARAM)font, TRUE);
        for (int i = 0; i < TAB_COUNT; i++)
            SendMessageW(nav, LB_ADDSTRING, 0, (LPARAM)TABS[i].name);
        SendMessageW(nav, LB_SETCURSEL, 0, 0);

        headline = makeLabel(h, TABS[0].name, ID_HEADLINE, titleFont);
        subhead = makeLabel(h, TABS[0].blurb, ID_SUBHEAD, font);

        safeBtn = makeButton(h, L"Safe set", ID_PRESET_SAFE);
        stdBtn = makeButton(h, L"Standard set", ID_PRESET_STD);
        deepBtn = makeButton(h, L"Deep set", ID_PRESET_DEEP);
        allBtn = makeButton(h, L"Tick everything here", ID_ALL);
        noneBtn = makeButton(h, L"Untick all", ID_NONE);
        saveBtn = makeButton(h, L"Save selection", ID_SAVE);
        helpBtn = makeButton(h, L"How it works", ID_HELP);
        browseBtn = makeButton(h, L"Browse...", ID_ROOT_BROWSE);
        cleanBtn = makeButton(h, L"Clean selected", ID_CLEAN);
        measureBtn = makeButton(h, L"Measure sizes", ID_MEASURE);
        stopBtn = makeButton(h, L"Stop", ID_STOP);

        searchEdit = CreateWindowW(L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                   0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_SEARCH, nullptr, nullptr);
        SendMessageW(searchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter these items...");

        rootLabel = makeLabel(h, L"Project sweep root", ID_ROOT_LABEL, font);
        rootEdit = CreateWindowW(L"EDIT", devRoot.c_str(),
                                 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                 0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_ROOT_EDIT, nullptr, nullptr);

        recycleCheck = CreateWindowW(L"BUTTON", L"Send to Recycle Bin when possible",
                                     WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_RECYCLE, nullptr, nullptr);

        listv = CreateWindowW(WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_REPORT |
                              LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                              0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_LIST, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(listv, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES |
                                                 LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        ListView_SetBkColor(listv, C_CARD);
        ListView_SetTextBkColor(listv, C_CARD);
        ListView_SetTextColor(listv, C_TEXT);
        struct Col { const wchar_t* title; int width; int fmt; };
        const Col cols[] = { { L"Item", 320, LVCFMT_LEFT }, { L"Risk", 70, LVCFMT_LEFT },
                             { L"Size", 110, LVCFMT_RIGHT }, { L"What happens", 460, LVCFMT_LEFT } };
        for (int i = 0; i < 4; i++) {
            LVCOLUMNW c{};
            c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            c.pszText = (wchar_t*)cols[i].title;
            c.cx = cols[i].width;
            c.fmt = cols[i].fmt;
            ListView_InsertColumn(listv, i, &c);
        }

        summary = makeLabel(h, L"", ID_SUMMARY, boldFont);
        status = makeLabel(h, L"Measuring in the background. You can start ticking right away.",
                           ID_STATUS, smallFont);
        progress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, h,
                                 (HMENU)(INT_PTR)ID_PROGRESS, nullptr, nullptr);
        SendMessageW(progress, PBM_SETRANGE32, 0, 100);
        SendMessageW(progress, PBM_SETBARCOLOR, 0, C_ACCENT);
        SendMessageW(progress, PBM_SETBKCOLOR, 0, C_PANEL);

        SendMessageW(searchEdit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(rootEdit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(recycleCheck, WM_SETFONT, (WPARAM)font, TRUE);

        EnableWindow(stopBtn, FALSE);
        fillList();
        layout(h);
        // No scan on startup: measuring is a deliberate click, so opening the app
        // never spins the disk behind your back.
        SetWindowTextW(status, L"Press Measure sizes when you want the disk scanned. "
                               L"Nothing is read until then.");
        return 0;
    }
    case WM_SIZE:
        layout(h);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)l;
        mm->ptMinTrackSize.x = 1100;
        mm->ptMinTrackSize.y = 660;
        return 0;
    }
    case WM_MEASUREITEM: {
        auto* mi = (MEASUREITEMSTRUCT*)l;
        if (mi->CtlID == ID_NAV) { mi->itemHeight = 56; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        auto* d = (LPDRAWITEMSTRUCT)l;
        if (d->CtlID == ID_NAV) { drawNavItem(d); return TRUE; }
        if (d->CtlType == ODT_BUTTON) { drawButton(d); return TRUE; }
        break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)w;
        HWND ctl = (HWND)l;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, ctl == subhead || ctl == status ? C_MUTED : C_TEXT);
        SetBkColor(dc, C_BG);
        return (LRESULT)bgBrush;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)w;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_PANEL);
        return (LRESULT)panelBrush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_CARD);
        return (LRESULT)cardBrush;
    }
    case WM_COMMAND: {
        const int id = LOWORD(w);
        const int code = HIWORD(w);
        if (id == ID_NAV && code == LBN_SELCHANGE) {
            activeTab = (int)SendMessageW(nav, LB_GETCURSEL, 0, 0);
            fillList();
            layout(h);
            return 0;
        }
        if (id == ID_SEARCH && code == EN_CHANGE) {
            wchar_t buf[128]{};
            GetWindowTextW(searchEdit, buf, 127);
            filterText = buf;
            fillList();
            return 0;
        }
        switch (id) {
        case ID_MEASURE: startScan(code != 0xFFFF, code == 0xFFFF); return 0;
        case ID_CLEAN: startClean(); return 0;
        case ID_STOP:
            cancelWork = true;
            SetWindowTextW(status, L"Stopping after the current item...");
            return 0;
        case ID_PRESET_SAFE: applyPreset(1); return 0;
        case ID_PRESET_STD: applyPreset(2); return 0;
        case ID_PRESET_DEEP: applyPreset(3); return 0;
        case ID_ALL: tickVisible(true); return 0;
        case ID_NONE: applyPreset(0); return 0;
        case ID_ROOT_BROWSE: browseForRoot(); return 0;
        case ID_SAVE: readRoot(); saveConfig(); return 0;
        case ID_HELP: showHelp(); return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        auto* n = (NMHDR*)l;
        if (n->hwndFrom == listv && n->code == NM_CUSTOMDRAW)
            return listCustomDraw((LPNMLVCUSTOMDRAW)l);
        if (n->hwndFrom == listv && n->code == LVN_ITEMCHANGED && !filling) {
            auto* v = (NMLISTVIEW*)l;
            if ((v->uChanged & LVIF_STATE) &&
                ((v->uOldState & LVIS_STATEIMAGEMASK) != (v->uNewState & LVIS_STATEIMAGEMASK))) {
                items[(int)v->lParam].selected = ListView_GetCheckState(listv, v->iItem) != 0;
                updateSummary();
            }
        }
        break;
    }
    case MSG_SIZE: {
        std::unique_ptr<SizeMsg> m((SizeMsg*)l);
        items[m->index].bytes = m->bytes;
        items[m->index].measured = true;
        refreshRow(m->index);
        updateSummary();
        return 0;
    }
    case MSG_PROGRESS: {
        std::unique_ptr<ProgressMsg> p((ProgressMsg*)l);
        SendMessageW(progress, PBM_SETPOS, p->percent, 0);
        SetWindowTextW(status, p->line.c_str());
        return 0;
    }
    case MSG_SCAN_DONE:
        setBusy(false);
        SendMessageW(progress, PBM_SETPOS, 100, 0);
        fillList();
        SetWindowTextW(status, cancelWork
            ? L"Measuring stopped."
            : L"Sizes are up to date. Project sweeps are measured when you press Measure sizes.");
        return 0;
    case MSG_CLEAN_DONE: {
        std::unique_ptr<CleanDone> d((CleanDone*)l);
        setBusy(false);
        fillList();

        std::wostringstream line;
        line << (d->cancelled ? L"Stopped early" : L"Done") << L"   \x2022   reclaimed "
             << fmtBytes(d->freed) << L"   \x2022   " << d->cleaned << L" processed";
        if (d->failed) line << L", " << d->failed << L" had nothing to remove or were locked";
        line << L"   \x2022   " << std::fixed << std::setprecision(1) << d->seconds << L" s";
        SetWindowTextW(status, line.str().c_str());

        std::wostringstream box;
        box << (d->cancelled ? L"Cleanup stopped early.\n\n" : L"Cleanup complete.\n\n")
            << L"Space reclaimed: " << fmtBytes(d->freed) << L"\n\n";
        int listed = 0;
        for (const auto& it : items) {
            if (!it.freed) continue;
            box << L"    " << it.name << L"  -  " << fmtBytes(it.freed) << L"\n";
            listed++;
        }
        if (!listed) box << L"    Nothing was removable this time.\n";
        MessageBoxW(h, box.str().c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
        for (auto& it : items) it.freed = 0;
        return 0;
    }
    case WM_CLOSE:
        if (working) {
            cancelWork = true;
            if (MessageBoxW(h, L"Work is still running. Close anyway?", APP_TITLE,
                            MB_YESNO | MB_ICONQUESTION) != IDYES)
                return 0;
        }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        cancelWork = true;
        DeleteObject(bgBrush);
        DeleteObject(panelBrush);
        DeleteObject(cardBrush);
        DeleteObject(font);
        DeleteObject(boldFont);
        DeleteObject(titleFont);
        DeleteObject(smallFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

// --------------------------------------------------------------- startup

static bool elevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD size = sizeof(e);
    const bool ok = GetTokenInformation(token, TokenElevation, &e, sizeof(e), &size) &&
                    e.TokenIsElevated;
    CloseHandle(token);
    return ok;
}

static bool relaunchElevated() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    SHELLEXECUTEINFOW s{ sizeof(s) };
    s.lpVerb = L"runas";
    s.lpFile = path;
    s.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&s) != FALSE;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR cmdLine, int show) {
    const bool skipElevation = cmdLine && wcsstr(cmdLine, L"-noadmin") != nullptr;
    if (!skipElevation && !elevated()) {
        if (relaunchElevated()) return 0;
        MessageBoxW(nullptr,
                    L"Running without administrator rights.\n\n"
                    L"System locations such as the Windows TEMP folder and the Windows Update "
                    L"cache will be skipped.",
                    APP_TITLE, MB_OK | MB_ICONWARNING);
    }

    INITCOMMONCONTROLSEX ic{ sizeof(ic), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS |
                                         ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&ic);

    devRoot = env(L"USERPROFILE");
    buildCatalogue();
    loadConfig();

    WNDCLASSEXW c{ sizeof(c) };
    c.lpfnWndProc = proc;
    c.hInstance = hi;
    c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    // Resource id 1 is the app icon from app.rc; fall back when built without it.
    c.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    if (!c.hIcon) c.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    c.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), 0);
    if (!c.hIconSm) c.hIconSm = c.hIcon;
    c.hbrBackground = CreateSolidBrush(C_BG);
    c.lpszClassName = L"TempCleanerProWindow";
    if (!RegisterClassExW(&c)) return 1;

    HWND h = CreateWindowExW(0, c.lpszClassName,
                             (std::wstring(APP_TITLE) + L"  " + APP_VERSION).c_str(),
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1320, 800,
                             nullptr, nullptr, hi, nullptr);
    if (!h) return 1;
    ShowWindow(h, show);
    UpdateWindow(h);

    MSG m{};
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(h, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
