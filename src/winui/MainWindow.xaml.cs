using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using TempCleanerWinUI.Models;
using TempCleanerWinUI.Pages;
using TempCleanerWinUI.Services;

namespace TempCleanerWinUI;

public sealed partial class MainWindow : Window
{
    private readonly AppState _state = AppState.Current;
    private CancellationTokenSource? _work;

    public MainWindow()
    {
        InitializeComponent();

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        AppWindow.TitleBar.PreferredHeightOption = TitleBarHeightOption.Tall;
        AppWindow.SetIcon("Assets/AppIcon.ico");
        AppWindow.Resize(new Windows.Graphics.SizeInt32(1320, 860));

        BuildNavigation();
        RecycleSwitch.IsOn = _state.UseRecycleBin;
        ElevationBar.IsOpen = !IsElevated();
        _state.TotalsChanged += (_, _) => DispatcherQueue.TryEnqueue(RefreshTotals);
        _state.MeasureRequested += (_, category) => _ = MeasureAsync(includeSweeps: true, category);
        RefreshTotals();

        Closed += (_, _) => { _work?.Cancel(); _state.SaveSelection(); };

        // No scanning on startup: measuring is a deliberate click, so opening the app
        // never spins the disk behind your back.
        StatusText.Text = "Press Measure sizes when you want the disk scanned. Nothing is read until then.";
    }

    private void BuildNavigation()
    {
        for (int i = 0; i < Catalog.Categories.Length; i++)
        {
            var category = Catalog.Categories[i];
            NavView.MenuItems.Add(new NavigationViewItem
            {
                Content = category.Name,
                Tag = i,
                Icon = new FontIcon { Glyph = category.Glyph },
                InfoBadge = new InfoBadge { Value = _state.SelectedIn(i), Visibility = Visibility.Collapsed }
            });
        }
        NavView.SelectedItem = NavView.MenuItems[0];
    }

    private void RefreshTotals()
    {
        SummaryText.Text = _state.SelectionSummary;
        CleanButton.Content = _state.CleanButtonText;
        CleanButton.IsEnabled = _state.SelectedCount > 0 && !_state.Busy;

        foreach (var entry in NavView.MenuItems.OfType<NavigationViewItem>())
        {
            if (entry.Tag is not int category || entry.InfoBadge is not InfoBadge badge) continue;
            int count = _state.SelectedIn(category);
            badge.Value = count;
            badge.Visibility = count > 0 ? Visibility.Visible : Visibility.Collapsed;
        }
    }

    private void SetBusy(bool busy)
    {
        _state.Busy = busy;
        MeasureButton.IsEnabled = !busy;
        StopButton.IsEnabled = busy;
        CleanButton.IsEnabled = !busy && _state.SelectedCount > 0;
        RecycleSwitch.IsEnabled = !busy;
    }

    private void TitleBar_PaneToggleRequested(TitleBar sender, object args) =>
        NavView.IsPaneOpen = !NavView.IsPaneOpen;

    private static bool IsElevated()
    {
        using var identity = System.Security.Principal.WindowsIdentity.GetCurrent();
        return new System.Security.Principal.WindowsPrincipal(identity)
            .IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
    }

    private void Elevate_Click(object sender, RoutedEventArgs e)
    {
        var exe = Environment.ProcessPath;
        if (string.IsNullOrEmpty(exe)) return;
        try
        {
            _state.SaveSelection();
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(exe)
            {
                UseShellExecute = true,
                Verb = "runas"
            });
            Close();
        }
        catch
        {
            ElevationBar.Message = "The elevation prompt was dismissed, so the app is still running as a standard user.";
        }
    }

    private void NavView_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    {
        if (args.IsSettingsSelected)
        {
            NavFrame.Navigate(typeof(SettingsPage));
            return;
        }
        if (args.SelectedItem is not NavigationViewItem item) return;
        if (item.Tag is int category) NavFrame.Navigate(typeof(CategoryPage), category);
        else NavFrame.Navigate(typeof(AboutPage));
    }

    // --------------------------------------------------------------- measuring

    private void Measure_Click(object sender, RoutedEventArgs e) => _ = MeasureAsync(includeSweeps: true);

    /// <param name="category">Measure one category only, or every category when null.</param>
    private async Task MeasureAsync(bool includeSweeps, int? category = null)
    {
        if (_state.Busy) return;
        SetBusy(true);
        _work = new CancellationTokenSource();
        var token = _work.Token;
        var root = _state.SweepRoot;

        StatusText.Text = category is int only
            ? $"Measuring {Catalog.Categories[only].Name}..."
            : "Measuring every category, including project folders...";

        var progress = new Progress<CleanProgress>(p =>
        {
            Progress.Value = p.Percent;
            StatusText.Text = p.Line;
        });

        // Measuring is disk bound but dominated by directory enumeration latency, so a
        // handful of parallel workers finishes several times faster than a single pass.
        await Task.Run(() =>
        {
            var queue = _state.Items
                .Where(i => !i.IsTool
                            && (includeSweeps || i.Action != ActionKind.Sweep)
                            && (category is null || i.Category == category))
                .ToList();
            if (queue.Count == 0) return;
            int done = 0;
            var options = new ParallelOptions
            {
                CancellationToken = token,
                MaxDegreeOfParallelism = Math.Clamp(Environment.ProcessorCount / 2, 2, 6)
            };
            try
            {
                Parallel.ForEach(queue, options, item =>
                {
                    long bytes = Cleaner.Measure(item, root, token);
                    int seen = Interlocked.Increment(ref done);
                    ((IProgress<CleanProgress>)progress).Report(
                        new CleanProgress(100 * seen / queue.Count, $"Measured {item.Name}"));
                    DispatcherQueue.TryEnqueue(() =>
                    {
                        item.Bytes = bytes;
                        item.Measured = true;
                        _state.RaiseTotals();   // the footer total grows as results land
                    });
                });
            }
            catch (OperationCanceledException) { /* Stop was pressed */ }
        }, token);

        Progress.Value = 100;
        var scope = category is int done ? Catalog.Categories[done].Name : "Every category";
        StatusText.Text = token.IsCancellationRequested
            ? "Measuring stopped."
            : $"{scope} measured" +
              (NativeScanner.IsAvailable ? "." : ". (native scanner missing, using the slower managed walk)");
        SetBusy(false);
        _state.RaiseTotals();
    }

    // ---------------------------------------------------------------- cleaning

    private void Stop_Click(object sender, RoutedEventArgs e)
    {
        _work?.Cancel();
        StatusText.Text = "Stopping after the current item...";
    }

    private void RecycleSwitch_Toggled(object sender, RoutedEventArgs e) =>
        _state.UseRecycleBin = RecycleSwitch.IsOn;

    private async void Clean_Click(object sender, RoutedEventArgs e)
    {
        var chosen = _state.Items.Where(i => i.Selected).ToList();
        if (chosen.Count == 0) return;

        if (!await ConfirmAsync(chosen)) return;

        SetBusy(true);
        _work = new CancellationTokenSource();
        var token = _work.Token;
        var root = _state.SweepRoot;
        var recycle = _state.UseRecycleBin;
        var started = DateTime.UtcNow;

        var progress = new Progress<CleanProgress>(p =>
        {
            Progress.Value = p.Percent;
            StatusText.Text = p.Line;
        });

        long freed = 0;
        int cleaned = 0, skipped = 0;

        await Task.Run(() =>
        {
            for (int i = 0; i < chosen.Count; i++)
            {
                if (token.IsCancellationRequested) break;
                var item = chosen[i];
                ((IProgress<CleanProgress>)progress).Report(
                    new CleanProgress(100 * i / chosen.Count, $"[{i + 1} of {chosen.Count}]  {item.Name}"));

                long before = item.Measured ? item.Bytes : 0;
                long removed = Cleaner.CleanOne(item, root, recycle, progress, token);
                freed += removed;
                if (removed > 0 || item.IsTool) cleaned++; else skipped++;

                DispatcherQueue.TryEnqueue(() =>
                {
                    item.Freed = removed;
                    item.Bytes = Math.Max(0, before - removed);
                });
            }
        }, token);

        var result = new CleanResult(cleaned, skipped, freed, token.IsCancellationRequested,
                                     DateTime.UtcNow - started);
        Progress.Value = 100;
        StatusText.Text = $"{(result.Cancelled ? "Stopped early" : "Done")}  •  reclaimed " +
                          $"{CleanItem.Format(result.Freed)}  •  {result.Cleaned} processed" +
                          (result.Skipped > 0 ? $", {result.Skipped} had nothing to remove" : "") +
                          $"  •  {result.Elapsed.TotalSeconds:0.0} s";
        SetBusy(false);
        _state.RaiseTotals();
        await ShowResultAsync(result, chosen);
        foreach (var item in chosen) item.Freed = 0;
    }

    private async Task<bool> ConfirmAsync(List<CleanItem> chosen)
    {
        long known = chosen.Where(i => i.Measured).Sum(i => i.Bytes);
        var risky = chosen.Where(i => i.Risk == RiskLevel.Risk).ToList();

        var body = new StackPanel { Spacing = 10 };
        body.Children.Add(new TextBlock
        {
            TextWrapping = TextWrapping.Wrap,
            Text = $"{chosen.Count} item{(chosen.Count == 1 ? "" : "s")} will be cleaned. " +
                   $"Measured so far: {CleanItem.Format(known)}."
        });
        body.Children.Add(new TextBlock
        {
            TextWrapping = TextWrapping.Wrap,
            Text = _state.UseRecycleBin
                ? "Items go to the Recycle Bin where Windows allows it."
                : "Items are deleted permanently and do not go to the Recycle Bin."
        });

        if (risky.Count > 0)
        {
            var warning = new InfoBar
            {
                IsOpen = true,
                IsClosable = false,
                Severity = InfoBarSeverity.Warning,
                Title = $"{risky.Count} item{(risky.Count == 1 ? " is" : "s are")} marked Risk",
                Message = string.Join("\n", risky.Select(i => "•  " + i.Name)) +
                          "\n\nThese can force long rebuilds or downloads, erase the audit trail, " +
                          "or need a reboot. This cannot be undone."
            };
            body.Children.Add(warning);
        }

        var dialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = "Clean now?",
            Content = body,
            PrimaryButtonText = "Clean",
            CloseButtonText = "Cancel",
            DefaultButton = risky.Count > 0 ? ContentDialogButton.Close : ContentDialogButton.Primary
        };
        return await dialog.ShowAsync() == ContentDialogResult.Primary;
    }

    private async Task ShowResultAsync(CleanResult result, List<CleanItem> chosen)
    {
        var body = new StackPanel { Spacing = 8 };
        body.Children.Add(new TextBlock
        {
            Style = (Style)Application.Current.Resources["SubtitleTextBlockStyle"],
            Text = CleanItem.Format(result.Freed) + " reclaimed"
        });

        var detail = chosen.Where(i => i.Freed > 0)
                           .OrderByDescending(i => i.Freed)
                           .Select(i => $"{i.Name}  —  {CleanItem.Format(i.Freed)}")
                           .ToList();
        body.Children.Add(new TextBlock
        {
            TextWrapping = TextWrapping.Wrap,
            Foreground = (Microsoft.UI.Xaml.Media.Brush)Application.Current.Resources["TextFillColorSecondaryBrush"],
            Text = detail.Count > 0
                ? string.Join("\n", detail)
                : "Nothing was removable this time. Files that are open stay put."
        });

        var dialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = result.Cancelled ? "Stopped early" : "Cleanup complete",
            Content = body,
            CloseButtonText = "Done"
        };
        await dialog.ShowAsync();
    }
}
