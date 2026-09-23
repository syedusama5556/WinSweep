using System.Runtime.InteropServices;

namespace TempCleanerWinUI.Services;

/// <summary>
/// Thin wrapper over tcscan.dll, the native walker. Directory enumeration in .NET
/// allocates per entry; the native pass uses FindFirstFileEx with a large fetch hint
/// and no allocation, which is what makes a cold scan of a developer profile quick.
/// Every call falls back to the managed path when the DLL is missing.
/// </summary>
public static class NativeScanner
{
    private const string Dll = @"native\tcscan.dll";

    [DllImport(Dll, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern long tc_folder_size(string path, IntPtr cancel);

    [DllImport(Dll, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern long tc_glob_size(string dir, string mask, IntPtr cancel);

    [DllImport(Dll, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern int tc_find_dirs(string root, string names,
                                           [Out] char[]? buffer, int capacity, IntPtr cancel);

    private static readonly bool Available = Probe();

    public static bool IsAvailable => Available;

    private static bool Probe()
    {
        try
        {
            tc_folder_size(Path.GetTempPath(), IntPtr.Zero);
            return true;
        }
        catch (DllNotFoundException) { return false; }
        catch (EntryPointNotFoundException) { return false; }
        catch (BadImageFormatException) { return false; }
    }

    /// <summary>Cancellation is a pinned int the native walker polls between entries.</summary>
    public sealed class CancelFlag : IDisposable
    {
        private readonly IntPtr _memory = Marshal.AllocHGlobal(sizeof(int));
        private readonly CancellationTokenRegistration _registration;

        public CancelFlag(CancellationToken token)
        {
            Marshal.WriteInt32(_memory, 0);
            _registration = token.Register(() => Marshal.WriteInt32(_memory, 1));
            if (token.IsCancellationRequested) Marshal.WriteInt32(_memory, 1);
        }

        public IntPtr Pointer => _memory;

        public void Dispose()
        {
            _registration.Dispose();
            Marshal.FreeHGlobal(_memory);
        }
    }

    public static long FolderSize(string path, CancellationToken token)
    {
        using var flag = new CancelFlag(token);
        return tc_folder_size(path, flag.Pointer);
    }

    public static long GlobSize(string dir, string mask, CancellationToken token)
    {
        using var flag = new CancelFlag(token);
        return tc_glob_size(dir, mask, flag.Pointer);
    }

    /// <summary>Candidate folders by name. Project marker checks stay on the managed side.</summary>
    public static List<string> FindDirs(string root, string[] names, CancellationToken token)
    {
        using var flag = new CancelFlag(token);
        var joined = string.Join('\n', names);

        int needed = tc_find_dirs(root, joined, null, 0, flag.Pointer);
        if (needed == 0) return [];

        int capacity = Math.Abs(needed) + 1;
        var buffer = new char[capacity];
        int written = tc_find_dirs(root, joined, buffer, capacity, flag.Pointer);
        if (written <= 0) return [];

        return new string(buffer, 0, written)
            .Split('\n', StringSplitOptions.RemoveEmptyEntries)
            .ToList();
    }
}
