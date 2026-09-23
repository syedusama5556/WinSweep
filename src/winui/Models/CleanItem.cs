using System.ComponentModel;
using System.Runtime.CompilerServices;
using Microsoft.UI;
using Microsoft.UI.Xaml.Media;
using Windows.UI;

namespace TempCleanerWinUI.Models;

public enum RiskLevel { Safe, Care, Risk }

public enum ActionKind
{
    Contents,    // empty the folder, keep the folder itself
    Folder,      // delete the folder
    Glob,        // delete files matching a mask, recursively
    Sweep,       // find project folders by name under the sweep root
    Command,     // run a shell command
    RecycleBin,  // empty the recycle bin
    WinUpdate,   // stop update services, clear downloads, start them again
    Explorer,    // restart Explorer and rebuild the icon cache
    EventLogs    // clear every Windows event log
}

public sealed class CleanItem : INotifyPropertyChanged
{
    public required int Category { get; init; }
    public required string Name { get; init; }
    public required string Note { get; init; }
    public required RiskLevel Risk { get; init; }
    public required ActionKind Action { get; init; }
    public string[] Targets { get; init; } = [];
    public string Command { get; init; } = "";

    /// <summary>Actions that run a tool instead of deleting a measurable folder.</summary>
    public bool IsTool => Action is ActionKind.Command or ActionKind.RecycleBin
                                 or ActionKind.Explorer or ActionKind.EventLogs;

    private bool _selected;
    public bool Selected
    {
        get => _selected;
        set
        {
            if (_selected == value) return;
            _selected = value;
            Notify();
            SelectionChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private bool _measured;
    public bool Measured
    {
        get => _measured;
        set { _measured = value; Notify(); Notify(nameof(SizeText)); }
    }

    private long _bytes;
    public long Bytes
    {
        get => _bytes;
        set { _bytes = value; Notify(); Notify(nameof(SizeText)); }
    }

    /// <summary>Bytes removed by the most recent clean, used for the results list.</summary>
    public long Freed { get; set; }

    public string SizeText => IsTool ? "tool" : Measured ? Format(Bytes) : "—";

    public string RiskText => Risk switch
    {
        RiskLevel.Safe => "Safe",
        RiskLevel.Care => "Care",
        _ => "Risk"
    };

    public Brush RiskBrush => new SolidColorBrush(Risk switch
    {
        RiskLevel.Safe => Color.FromArgb(255, 0x6C, 0xCB, 0x5F),
        RiskLevel.Care => Color.FromArgb(255, 0xF5, 0xC2, 0x42),
        _ => Color.FromArgb(255, 0xFF, 0x6B, 0x6B)
    });

    public Brush RiskBackground => new SolidColorBrush(Risk switch
    {
        RiskLevel.Safe => Color.FromArgb(38, 0x6C, 0xCB, 0x5F),
        RiskLevel.Care => Color.FromArgb(38, 0xF5, 0xC2, 0x42),
        _ => Color.FromArgb(38, 0xFF, 0x6B, 0x6B)
    });

    public static string Format(long bytes)
    {
        string[] units = ["B", "KB", "MB", "GB", "TB"];
        double value = bytes;
        int unit = 0;
        while (value >= 1024 && unit < units.Length - 1) { value /= 1024; unit++; }
        return unit == 0 ? $"{value:0} {units[unit]}"
             : value < 10 ? $"{value:0.00} {units[unit]}"
             : $"{value:0.0} {units[unit]}";
    }

    public event EventHandler? SelectionChanged;
    public event PropertyChangedEventHandler? PropertyChanged;

    private void Notify([CallerMemberName] string? property = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(property));
}
