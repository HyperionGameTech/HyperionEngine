using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.Services;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Views;

namespace Hyperion.Editor
{
    public partial class App : Application
    {
        public static bool AlreadyInitialized { get; set; } = false;

        // On hold for now
        private static readonly bool IsStartScreenEnabled = false;

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

            if (IsStartScreenEnabled && !RecentProjectsService.Instance.WillOpenLastProjectOnStartup)
            {
                ShowStartScreen(desktop, splashWindow);

                return;
            }

            splashWindow.SetStatus("Loading editor...");
            await splashWindow.WaitForNextFrameAsync();

            ShowMainWindow(desktop);

            // Main window must be open before the splash closes, otherwise the lifetime sees the last window close and shuts down
            splashWindow.Close();
        }

        private static void ShowStartScreen(IClassicDesktopStyleApplicationLifetime desktop, SplashWindow splashWindow)
        {
            StartScreenViewModel startScreenViewModel = new StartScreenViewModel();
            StartScreenWindow startScreenWindow = new StartScreenWindow { DataContext = startScreenViewModel };

            bool mainWindowShown = false;

            // Same rule as the splash: the main window has to be open before the start screen closes, including via its close button
            void ContinueToEditor()
            {
                if (mainWindowShown)
                {
                    return;
                }

                mainWindowShown = true;

                ShowMainWindow(desktop);
            }

            startScreenViewModel.Finished += () =>
            {
                ContinueToEditor();
                startScreenWindow.Close();
            };

            startScreenWindow.Closing += (_, _) => ContinueToEditor();

            startScreenWindow.Show();
            splashWindow.Close();
            startScreenWindow.Activate();
        }

        private static void ShowMainWindow(IClassicDesktopStyleApplicationLifetime desktop)
        {
            MainWindow mainWindow = new MainWindow();
            desktop.MainWindow = mainWindow;
            mainWindow.Show();
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
