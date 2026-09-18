using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.Views;

namespace Hyperion.Editor
{
    public partial class App : Application
    {
        public static bool AlreadyInitialized { get; set; } = false;

        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                SplashWindow splashWindow = new SplashWindow();
                splashWindow.Show();

                StartEditor(desktop, splashWindow);
            }
            else
            {
                InitializeEngine();
            }

            base.OnFrameworkInitializationCompleted();
        }

        private static async void StartEditor(IClassicDesktopStyleApplicationLifetime desktop, SplashWindow splashWindow)
        {
            splashWindow.SetStatus("Initializing engine...");
            await splashWindow.WaitForNextFrameAsync();

            InitializeEngine();

            splashWindow.SetStatus("Loading editor...");
            await splashWindow.WaitForNextFrameAsync();

            MainWindow mainWindow = new MainWindow();
            desktop.MainWindow = mainWindow;
            mainWindow.Show();

            // Main window must be open before the splash closes, otherwise the lifetime sees the last window close and shuts down
            splashWindow.Close();
            mainWindow.Activate();
        }

        private static void InitializeEngine()
        {
            if (!AlreadyInitialized)
            {
                EngineManager.Initialize();
            }

            EngineManager.InitializeEditor();

            // Initialize Console Service
            _ = Services.ConsoleService.Instance;
        }
    }
}
