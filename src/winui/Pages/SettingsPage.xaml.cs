using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using TempCleanerWinUI.Models;
using TempCleanerWinUI.Services;
using Windows.Storage.Pickers;

namespace TempCleanerWinUI.Pages;

public sealed partial class SettingsPage : Page
{
    private readonly AppState _state = AppState.Current;

    public SettingsPage() => InitializeComponent();

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        SweepRootBox.Text = _state.SweepRoot;
        RecycleToggle.IsOn = _state.UseRecycleBin;
    }

    private void SweepRootBox_LostFocus(object sender, RoutedEventArgs e) => Apply(SweepRootBox.Text);

    private async void Browse_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FolderPicker();
        picker.FileTypeFilter.Add("*");
        WinRT.Interop.InitializeWithWindow.Initialize(picker, App.WindowHandle);

        var folder = await picker.PickSingleFolderAsync();
        if (folder is null) return;
        SweepRootBox.Text = folder.Path;
        Apply(folder.Path);
    }

    private void Apply(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || path == _state.SweepRoot) return;
        _state.SweepRoot = path;
        foreach (var item in _state.Items.Where(i => i.Action == ActionKind.Sweep))
        {
            item.Measured = false;
            item.Bytes = 0;
        }
        _state.RaiseTotals();
    }

    private void RecycleToggle_Toggled(object sender, RoutedEventArgs e) =>
        _state.UseRecycleBin = RecycleToggle.IsOn;

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        _state.SaveSelection();
        SavedNote.Text = "Saved. These ticks load again next time you start.";
    }

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        _state.ApplyPreset(2);
        SavedNote.Text = "Reset to the standard set. Save it if you want to keep it.";
    }
}
