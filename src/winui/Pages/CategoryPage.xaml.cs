using System.Collections.ObjectModel;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using TempCleanerWinUI.Models;
using TempCleanerWinUI.Services;
using Windows.Storage.Pickers;

namespace TempCleanerWinUI.Pages;

public sealed partial class CategoryPage : Page
{
    private readonly AppState _state = AppState.Current;
    private readonly ObservableCollection<CleanItem> _visible = [];
    private int _category;
    private string _filter = "";

    public CategoryPage()
    {
        InitializeComponent();
        ItemList.ItemsSource = _visible;
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        _category = e.Parameter is int index ? index : 0;
        var category = Catalog.Categories[_category];
        Headline.Text = category.Name;
        Blurb.Text = category.Blurb;

        SweepRow.Visibility = _category == 2 ? Visibility.Visible : Visibility.Collapsed;
        SweepRootBox.Text = _state.SweepRoot;

        Refill();
    }

    private void Refill()
    {
        _visible.Clear();
        foreach (var item in _state.InCategory(_category))
        {
            if (_filter.Length > 0 &&
                !item.Name.Contains(_filter, StringComparison.OrdinalIgnoreCase) &&
                !item.Note.Contains(_filter, StringComparison.OrdinalIgnoreCase))
                continue;
            _visible.Add(item);
        }
    }

    private void FilterBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
    {
        if (args.Reason != AutoSuggestionBoxTextChangeReason.UserInput) return;
        _filter = sender.Text ?? "";
        Refill();
    }

    private void MeasureCategory_Click(object sender, RoutedEventArgs e) =>
        _state.RequestMeasure(_category);

    private void Safe_Click(object sender, RoutedEventArgs e) => _state.ApplyPreset(1);

    private void Standard_Click(object sender, RoutedEventArgs e) => _state.ApplyPreset(2);

    private void Deep_Click(object sender, RoutedEventArgs e) => _state.ApplyPreset(3);

    private void UntickAll_Click(object sender, RoutedEventArgs e) => _state.ApplyPreset(0);

    /// <summary>Ticks what is on screen, but never the Risk rows: those stay a deliberate choice.</summary>
    private void TickAll_Click(object sender, RoutedEventArgs e)
    {
        foreach (var item in _visible.Where(i => i.Risk != RiskLevel.Risk)) item.Selected = true;
        _state.RaiseTotals();
    }

    private void SweepRootBox_LostFocus(object sender, RoutedEventArgs e) => ApplySweepRoot(SweepRootBox.Text);

    private async void Browse_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FolderPicker();
        picker.FileTypeFilter.Add("*");
        WinRT.Interop.InitializeWithWindow.Initialize(picker, App.WindowHandle);

        var folder = await picker.PickSingleFolderAsync();
        if (folder is null) return;
        SweepRootBox.Text = folder.Path;
        ApplySweepRoot(folder.Path);
    }

    private void ApplySweepRoot(string path)
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
}
