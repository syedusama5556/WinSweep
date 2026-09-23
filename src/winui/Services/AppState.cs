using System.ComponentModel;
using System.Runtime.CompilerServices;
using TempCleanerWinUI.Models;

namespace TempCleanerWinUI.Services;

/// <summary>Shared selection state. One instance for the whole app.</summary>
public sealed class AppState : INotifyPropertyChanged
{
    public static AppState Current { get; } = new();

    public List<CleanItem> Items { get; }

    private AppState()
    {
        Items = Catalog.Build();
        foreach (var item in Items) item.SelectionChanged += (_, _) => RaiseTotals();
        SweepRoot = Environment.GetEnvironmentVariable("USERPROFILE") ?? "";
        LoadSelection();
    }

    private string _sweepRoot = "";
    public string SweepRoot
    {
        get => _sweepRoot;
        set { _sweepRoot = value; Notify(); }
    }

    private bool _useRecycleBin;
    public bool UseRecycleBin
    {
        get => _useRecycleBin;
        set { _useRecycleBin = value; Notify(); }
    }

    private bool _busy;
    public bool Busy
    {
        get => _busy;
        set { _busy = value; Notify(); Notify(nameof(NotBusy)); }
    }

    public bool NotBusy => !_busy;

    public int SelectedCount => Items.Count(i => i.Selected);
    public long SelectedBytes => Items.Where(i => i.Selected && i.Measured).Sum(i => i.Bytes);

    public string SelectionSummary => SelectedCount == 0
        ? "Nothing selected yet. Tick the rows you want cleared."
        : $"{SelectedCount} selected  •  {CleanItem.Format(SelectedBytes)} ready to reclaim";

    public string CleanButtonText => SelectedCount == 0 ? "Clean selected" : $"Clean {SelectedCount} selected";

    public IEnumerable<CleanItem> InCategory(int category) => Items.Where(i => i.Category == category);

    public int SelectedIn(int category) => Items.Count(i => i.Category == category && i.Selected);

    public long BytesIn(int category) =>
        Items.Where(i => i.Category == category && i.Measured).Sum(i => i.Bytes);

    public void RaiseTotals()
    {
        Notify(nameof(SelectedCount));
        Notify(nameof(SelectedBytes));
        Notify(nameof(SelectionSummary));
        Notify(nameof(CleanButtonText));
        TotalsChanged?.Invoke(this, EventArgs.Empty);
    }

    /// <summary>0 clears everything, 1 safe, 2 standard, 3 deep.</summary>
    public void ApplyPreset(int preset)
    {
        foreach (var item in Items)
        {
            if (preset == 0 || item.Category == 5) { item.Selected = false; continue; }
            item.Selected = preset switch
            {
                1 => item.Risk == RiskLevel.Safe && !item.IsTool,
                2 => item.Risk != RiskLevel.Risk,
                _ => true
            };
        }
        RaiseTotals();
    }

    // ------------------------------------------------------------- storage

    private static string ConfigPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "TempCleanerPro", "selection-winui.cfg");

    public void SaveSelection()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(ConfigPath)!);
            var lines = new List<string> { "root=" + SweepRoot, "recycle=" + (UseRecycleBin ? 1 : 0) };
            lines.AddRange(Items.Select(i => (i.Selected ? "1=" : "0=") + i.Name));
            File.WriteAllLines(ConfigPath, lines);
        }
        catch { /* saving preferences is best effort */ }
    }

    private void LoadSelection()
    {
        try
        {
            if (!File.Exists(ConfigPath)) return;
            foreach (var line in File.ReadAllLines(ConfigPath))
            {
                if (line.StartsWith("root=")) { SweepRoot = line[5..]; continue; }
                if (line.StartsWith("recycle=")) { UseRecycleBin = line[8..] == "1"; continue; }
                if (line.Length < 3 || line[1] != '=') continue;
                var name = line[2..];
                var item = Items.FirstOrDefault(i => i.Name == name);
                if (item is not null) item.Selected = line[0] == '1';
            }
        }
        catch { /* a broken config just means defaults */ }
    }

    /// <summary>Raised by a page asking the shell to measure one category, or all when null.</summary>
    public event EventHandler<int?>? MeasureRequested;

    public void RequestMeasure(int? category) => MeasureRequested?.Invoke(this, category);

    public event EventHandler? TotalsChanged;
    public event PropertyChangedEventHandler? PropertyChanged;

    private void Notify([CallerMemberName] string? property = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(property));
}
