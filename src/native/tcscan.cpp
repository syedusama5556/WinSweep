// tcscan - native directory scanner for TempCleaner Pro.
// Developer: infusiblecoder
//
// The managed enumeration in .NET spends most of its time allocating FileInfo
// objects and strings. This walks the same trees with raw FindFirstFileExW and
// a large fetch hint, which is several times faster on cold caches and avoids
// per-entry allocation entirely.
//
// Build: build-tempcleaner-native.bat  ->  tcscan.dll

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <string>
#include <vector>

#define TCSCAN_API extern "C" __declspec(dllexport)

namespace {

bool cancelled(const volatile long* cancel) { return cancel && *cancel != 0; }

std::wstring join(const std::wstring& dir, const wchar_t* leaf) {
    if (dir.empty()) return leaf;
    return dir.back() == L'\\' ? dir + leaf : dir + L"\\" + leaf;
}

bool isDot(const wchar_t* name) {
    return name[0] == L'.' && (name[1] == 0 || (name[1] == L'.' && name[2] == 0));
}

HANDLE openFind(const std::wstring& pattern, WIN32_FIND_DATAW& data) {
    return FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data,
                            FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
}

unsigned long long fileSize(const WIN32_FIND_DATAW& f) {
    ULARGE_INTEGER n{};
    n.HighPart = f.nFileSizeHigh;
    n.LowPart = f.nFileSizeLow;
    return n.QuadPart;
}

// Iterative walk: deep node_modules trees blow a recursive stack.
unsigned long long walk(const std::wstring& root, const wchar_t* mask,
                        const volatile long* cancel) {
    unsigned long long total = 0;
    std::vector<std::wstring> pending{ root };

    while (!pending.empty()) {
        if (cancelled(cancel)) break;
        std::wstring dir = std::move(pending.back());
        pending.pop_back();

        // Pass 1: files matching the mask in this directory.
        WIN32_FIND_DATAW f{};
        HANDLE h = openFind(join(dir, mask), f);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                total += fileSize(f);
            } while (FindNextFileW(h, &f));
            FindClose(h);
        }

        // Pass 2: subdirectories to visit.
        HANDLE hd = openFind(join(dir, L"*"), f);
        if (hd == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (isDot(f.cFileName)) continue;
            pending.push_back(join(dir, f.cFileName));
        } while (FindNextFileW(hd, &f));
        FindClose(hd);
    }
    return total;
}

}   // namespace

/// Total bytes under a folder. Returns 0 for a missing folder.
TCSCAN_API long long tc_folder_size(const wchar_t* path, const volatile long* cancel) {
    if (!path || !*path) return 0;
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    return (long long)walk(path, L"*", cancel);
}

/// Total bytes of files matching a mask anywhere under a folder.
TCSCAN_API long long tc_glob_size(const wchar_t* dir, const wchar_t* mask,
                                  const volatile long* cancel) {
    if (!dir || !*dir || !mask || !*mask) return 0;
    DWORD attributes = GetFileAttributesW(dir);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    return (long long)walk(dir, mask, cancel);
}

/// Finds folders whose name matches any entry in a newline separated list.
/// Writes newline separated full paths into `out` and returns the character
/// count written, or the count required when `out` is too small (negative).
TCSCAN_API int tc_find_dirs(const wchar_t* root, const wchar_t* names,
                            wchar_t* out, int capacity, const volatile long* cancel) {
    if (!root || !*root || !names) return 0;

    std::vector<std::wstring> wanted;
    {
        std::wstring current;
        for (const wchar_t* p = names; ; p++) {
            if (*p == L'\n' || *p == 0) {
                if (!current.empty()) wanted.push_back(current);
                current.clear();
                if (*p == 0) break;
            } else {
                current.push_back(*p);
            }
        }
    }
    if (wanted.empty()) return 0;

    static const wchar_t* skip[] = {
        L"Windows", L"WinSxS", L"Program Files", L"Program Files (x86)", L"ProgramData",
        L"$Recycle.Bin", L"System Volume Information", L"Recovery", L".git", L".svn"
    };

    std::wstring found;
    std::vector<std::wstring> pending{ root };
    while (!pending.empty()) {
        if (cancelled(cancel)) break;
        std::wstring dir = std::move(pending.back());
        pending.pop_back();

        WIN32_FIND_DATAW f{};
        HANDLE h = openFind(join(dir, L"*"), f);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (isDot(f.cFileName)) continue;

            bool hit = false;
            for (const auto& name : wanted) {
                if (_wcsicmp(f.cFileName, name.c_str()) == 0) { hit = true; break; }
            }
            if (hit) {
                found += join(dir, f.cFileName);
                found += L'\n';
                continue;   // never descend into something that is about to be deleted
            }

            bool skipThis = false;
            for (auto s : skip) {
                if (_wcsicmp(f.cFileName, s) == 0) { skipThis = true; break; }
            }
            if (!skipThis) pending.push_back(join(dir, f.cFileName));
        } while (FindNextFileW(h, &f));
        FindClose(h);
    }

    const int needed = (int)found.size();
    if (!out || capacity < needed + 1) return -needed;
    memcpy(out, found.c_str(), (size_t)needed * sizeof(wchar_t));
    out[needed] = 0;
    return needed;
}
