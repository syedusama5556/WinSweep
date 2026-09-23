using System.Diagnostics;
using System.Runtime.InteropServices;
using TempCleanerWinUI.Models;

namespace TempCleanerWinUI.Services;

public readonly record struct CleanProgress(int Percent, string Line);

public sealed record CleanResult(int Cleaned, int Skipped, long Freed, bool Cancelled, TimeSpan Elapsed);

/// <summary>Measuring and deleting. Every method here runs off the UI thread.</summary>
public static class Cleaner
{
    // ------------------------------------------------------------ measuring

    public static long Measure(CleanItem item, string sweepRoot, CancellationToken token)
    {
        if (item.IsTool) return 0;
        long total = 0;
        switch (item.Action)
        {
            case ActionKind.Contents:
            case ActionKind.Folder:
            case ActionKind.WinUpdate:
                foreach (var target in item.Targets) total += FolderSize(target, token);
                break;

            case ActionKind.Glob:
                foreach (var target in item.Targets)
                {
                    var (dir, mask) = SplitGlob(target);
                    if (NativeScanner.IsAvailable)
                    {
                        total += NativeScanner.GlobSize(dir, mask, token);
                        continue;
                    }
                    foreach (var file in EnumerateFiles(dir, mask, token))
                        total += SafeLength(file);
                }
                break;

            case ActionKind.Sweep:
                foreach (var folder in FindSweepTargets(sweepRoot, item.Targets, token))
                    total += FolderSize(folder, token);
                break;
        }
        return total;
    }

    private static long SafeLength(string file)
    {
        try { return new FileInfo(file).Length; }
        catch { return 0; }
    }

    public static long FolderSize(string path, CancellationToken token)
    {
        if (string.IsNullOrEmpty(path) || !Directory.Exists(path)) return 0;
        if (NativeScanner.IsAvailable) return NativeScanner.FolderSize(path, token);

        long total = 0;
        foreach (var file in EnumerateFiles(path, "*", token)) total += SafeLength(file);
        return total;
    }

    /// <summary>Recursive enumeration that survives permission errors and reparse points.</summary>
    private static IEnumerable<string> EnumerateFiles(string dir, string mask, CancellationToken token)
    {
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) yield break;

        var pending = new Stack<string>();
        pending.Push(dir);
        while (pending.Count > 0)
        {
            if (token.IsCancellationRequested) yield break;
            var current = pending.Pop();

            string[] files;
            try { files = Directory.GetFiles(current, mask); }
            catch { files = []; }
            foreach (var file in files) yield return file;

            string[] subdirs;
            try { subdirs = Directory.GetDirectories(current); }
            catch { subdirs = []; }
            foreach (var sub in subdirs)
            {
                try
                {
                    var attributes = File.GetAttributes(sub);
                    if (attributes.HasFlag(FileAttributes.ReparsePoint)) continue;
                }
                catch { continue; }
                pending.Push(sub);
            }
        }
    }

    private static (string Dir, string Mask) SplitGlob(string target)
    {
        var slash = target.LastIndexOf('\\');
        return slash < 0 ? (".", target) : (target[..slash], target[(slash + 1)..]);
    }

    // ------------------------------------------------------ project sweeps

    private static readonly string[] SkipFolders =
    [
        "Windows", "WinSxS", "Program Files", "Program Files (x86)", "ProgramData",
        "$Recycle.Bin", "System Volume Information", "Recovery", ".git", ".svn"
    ];

    private static bool HasMarker(string dir, params string[] names) =>
        names.Any(n => File.Exists(Path.Combine(dir, n)));

    private static bool HasProjectFile(string dir, params string[] extensions)
    {
        try { return extensions.Any(ext => Directory.GetFiles(dir, "*" + ext).Length > 0); }
        catch { return false; }
    }

    /// <summary>A folder only counts as generated output when a real project sits beside it.</summary>
    private static bool MarkerOk(string parent, string name) => name.ToLowerInvariant() switch
    {
        "node_modules" => HasMarker(parent, "package.json"),
        "build" => HasMarker(parent, "pubspec.yaml", "build.gradle", "build.gradle.kts", "CMakeLists.txt"),
        ".dart_tool" => HasMarker(parent, "pubspec.yaml"),
        ".gradle" => HasMarker(parent, "settings.gradle", "settings.gradle.kts", "build.gradle", "build.gradle.kts"),
        "target" => HasMarker(parent, "Cargo.toml"),
        "bin" or "obj" => HasProjectFile(parent, ".csproj", ".fsproj", ".vbproj"),
        _ => true
    };

    public static List<string> FindSweepTargets(string root, string[] names, CancellationToken token)
    {
        var found = new List<string>();
        if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return found;

        if (NativeScanner.IsAvailable)
        {
            // The native walk returns candidates by name; the marker check is cheap
            // because it only runs on the handful of folders that actually matched.
            foreach (var candidate in NativeScanner.FindDirs(root, names, token))
            {
                var parent = Path.GetDirectoryName(candidate);
                if (parent is not null && MarkerOk(parent, Path.GetFileName(candidate)))
                    found.Add(candidate);
            }
            return found;
        }

        var pending = new Stack<string>();
        pending.Push(root);
        while (pending.Count > 0)
        {
            if (token.IsCancellationRequested) break;
            var current = pending.Pop();

            string[] subdirs;
            try { subdirs = Directory.GetDirectories(current); }
            catch { continue; }

            foreach (var sub in subdirs)
            {
                var name = Path.GetFileName(sub);
                try
                {
                    if (File.GetAttributes(sub).HasFlag(FileAttributes.ReparsePoint)) continue;
                }
                catch { continue; }

                if (names.Any(n => string.Equals(n, name, StringComparison.OrdinalIgnoreCase)) &&
                    MarkerOk(current, name))
                {
                    found.Add(sub);
                    continue;   // do not descend into something we are about to delete
                }
                if (!SkipFolders.Any(s => string.Equals(s, name, StringComparison.OrdinalIgnoreCase)))
                    pending.Push(sub);
            }
        }
        return found;
    }

    // ------------------------------------------------------------- deleting

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct SHFILEOPSTRUCT
    {
        public IntPtr hwnd;
        public uint wFunc;
        [MarshalAs(UnmanagedType.LPWStr)] public string pFrom;
        [MarshalAs(UnmanagedType.LPWStr)] public string? pTo;
        public ushort fFlags;
        [MarshalAs(UnmanagedType.Bool)] public bool fAnyOperationsAborted;
        public IntPtr hNameMappings;
        [MarshalAs(UnmanagedType.LPWStr)] public string? lpszProgressTitle;
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern int SHFileOperation(ref SHFILEOPSTRUCT op);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern int SHEmptyRecycleBin(IntPtr hwnd, string? rootPath, uint flags);

    private const uint FO_DELETE = 0x0003;
    private const ushort FOF_ALLOWUNDO = 0x0040, FOF_NOCONFIRMATION = 0x0010,
                         FOF_SILENT = 0x0004, FOF_NOERRORUI = 0x0400;

    private static bool ShellDelete(string path, bool recycle)
    {
        var op = new SHFILEOPSTRUCT
        {
            wFunc = FO_DELETE,
            pFrom = path + "\0\0",
            fFlags = (ushort)(FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI |
                              (recycle ? FOF_ALLOWUNDO : 0))
        };
        return SHFileOperation(ref op) == 0 && !op.fAnyOperationsAborted;
    }

    private static long RemoveDirectory(string path, bool recycle, CancellationToken token)
    {
        if (!Directory.Exists(path)) return 0;
        var size = FolderSize(path, token);
        try
        {
            if (recycle) { if (!ShellDelete(path, true)) return 0; }
            else Directory.Delete(path, recursive: true);
            return size;
        }
        catch
        {
            // Locked children are normal: fall back to the shell, which skips what it cannot touch.
            return ShellDelete(path, recycle) ? size : 0;
        }
    }

    private static long RemoveFile(string path, bool recycle)
    {
        long size = SafeLength(path);
        try
        {
            if (recycle) return ShellDelete(path, true) ? size : 0;
            File.SetAttributes(path, FileAttributes.Normal);
            File.Delete(path);
            return size;
        }
        catch { return 0; }
    }

    private static long ClearContents(string dir, bool recycle, IProgress<CleanProgress> progress,
                                      int basePercent, int span, CancellationToken token)
    {
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return 0;

        string[] entries;
        try { entries = Directory.GetFileSystemEntries(dir); }
        catch { return 0; }

        long freed = 0;
        for (int i = 0; i < entries.Length; i++)
        {
            if (token.IsCancellationRequested) break;
            var entry = entries[i];
            freed += Directory.Exists(entry) ? RemoveDirectory(entry, recycle, token)
                                             : RemoveFile(entry, recycle);
            progress.Report(new CleanProgress(basePercent + span * (i + 1) / entries.Length, entry));
        }
        return freed;
    }

    private static void Run(string command, bool wait = true)
    {
        try
        {
            var info = new ProcessStartInfo("cmd.exe", "/d /c " + command)
            {
                CreateNoWindow = true,
                UseShellExecute = false
            };
            using var process = Process.Start(info);
            if (wait) process?.WaitForExit();
        }
        catch { /* the tool is simply not installed */ }
    }

    public static long CleanOne(CleanItem item, string sweepRoot, bool recycle,
                                IProgress<CleanProgress> progress, CancellationToken token)
    {
        long freed = 0;
        int span = item.Targets.Length == 0 ? 100 : 100 / item.Targets.Length;

        switch (item.Action)
        {
            case ActionKind.Contents:
                for (int i = 0; i < item.Targets.Length && !token.IsCancellationRequested; i++)
                    freed += ClearContents(item.Targets[i], recycle, progress, i * span, span, token);
                break;

            case ActionKind.Folder:
                foreach (var target in item.Targets)
                {
                    if (token.IsCancellationRequested) break;
                    progress.Report(new CleanProgress(20, target));
                    freed += RemoveDirectory(target, recycle, token);
                    progress.Report(new CleanProgress(100, target));
                }
                break;

            case ActionKind.Glob:
            {
                var files = new List<string>();
                foreach (var target in item.Targets)
                {
                    var (dir, mask) = SplitGlob(target);
                    files.AddRange(EnumerateFiles(dir, mask, token));
                }
                for (int i = 0; i < files.Count && !token.IsCancellationRequested; i++)
                {
                    freed += RemoveFile(files[i], recycle);
                    progress.Report(new CleanProgress(100 * (i + 1) / Math.Max(1, files.Count), files[i]));
                }
                break;
            }

            case ActionKind.Sweep:
            {
                progress.Report(new CleanProgress(0, $"Searching {sweepRoot} ..."));
                var folders = FindSweepTargets(sweepRoot, item.Targets, token);
                for (int i = 0; i < folders.Count && !token.IsCancellationRequested; i++)
                {
                    freed += RemoveDirectory(folders[i], recycle, token);
                    progress.Report(new CleanProgress(100 * (i + 1) / Math.Max(1, folders.Count), folders[i]));
                }
                break;
            }

            case ActionKind.Command:
                progress.Report(new CleanProgress(30, "Running: " + item.Command));
                Run(item.Command);
                progress.Report(new CleanProgress(100, item.Command));
                break;

            case ActionKind.RecycleBin:
                progress.Report(new CleanProgress(30, "Emptying the Recycle Bin..."));
                SHEmptyRecycleBin(IntPtr.Zero, null, 0x7);   // no confirmation, progress or sound
                progress.Report(new CleanProgress(100, "Recycle Bin emptied"));
                break;

            case ActionKind.WinUpdate:
                progress.Report(new CleanProgress(5, "Stopping Windows Update services..."));
                Run("net stop wuauserv");
                Run("net stop bits");
                foreach (var target in item.Targets)
                    freed += ClearContents(target, recycle, progress, 10, 80, token);
                progress.Report(new CleanProgress(95, "Starting Windows Update services..."));
                Run("net start wuauserv");
                Run("net start bits");
                break;

            case ActionKind.Explorer:
            {
                progress.Report(new CleanProgress(20, "Stopping Explorer..."));
                Run("taskkill /f /im explorer.exe");
                var dir = Path.Combine(Environment.GetEnvironmentVariable("LOCALAPPDATA") ?? "",
                                       @"Microsoft\Windows\Explorer");
                foreach (var mask in new[] { "thumbcache_*.db", "iconcache_*.db" })
                    foreach (var file in EnumerateFiles(dir, mask, token))
                        freed += RemoveFile(file, false);
                progress.Report(new CleanProgress(70, "Starting Explorer..."));
                Run("start explorer.exe", wait: false);
                progress.Report(new CleanProgress(100, "Explorer restarted"));
                break;
            }

            case ActionKind.EventLogs:
                progress.Report(new CleanProgress(20, "Clearing every event log..."));
                Run("powershell -NoProfile -Command \"wevtutil el | ForEach-Object " +
                    "{ wevtutil cl \\\"$_\\\" 2>$null }\"");
                progress.Report(new CleanProgress(100, "Event logs cleared"));
                break;
        }
        return freed;
    }
}
