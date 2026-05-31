using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;

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
            if (!AlreadyInitialized)
            {
                EngineManager.Initialize();
            }

            EngineManager.InitializeEditor();

            // Initialize Console Service
            _ = Services.ConsoleService.Instance;

            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.MainWindow = new MainWindow();
            }

            base.OnFrameworkInitializationCompleted();
        }
    }
}
