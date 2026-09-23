using Microsoft.UI.Xaml;
using TempCleanerWinUI.Services;

namespace TempCleanerWinUI;

public partial class App : Application
{
    private Window? _window;

    /// <summary>Handle of the main window, needed by pickers and dialogs.</summary>
    public static IntPtr WindowHandle { get; private set; }

    public App() => InitializeComponent();

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _window = new MainWindow();
        WindowHandle = WinRT.Interop.WindowNative.GetWindowHandle(_window);
        _window.Closed += (_, _) => AppState.Current.SaveSelection();
        _window.Activate();

        // Size after activation: a resize in the constructor is overridden by the shell.
        var appWindow = _window.AppWindow;
        var area = Microsoft.UI.Windowing.DisplayArea.GetFromWindowId(
            appWindow.Id, Microsoft.UI.Windowing.DisplayAreaFallback.Primary);
        int width = Math.Min(1360, area.WorkArea.Width - 80);
        int height = Math.Min(900, area.WorkArea.Height - 80);
        appWindow.MoveAndResize(new Windows.Graphics.RectInt32(
            area.WorkArea.X + (area.WorkArea.Width - width) / 2,
            area.WorkArea.Y + (area.WorkArea.Height - height) / 2,
            width, height));
    }
}
