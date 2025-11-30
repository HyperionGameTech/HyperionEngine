using Avalonia;
using Avalonia.Vulkan;
using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    class Program
    {
        // Initialization code. Don't use any Avalonia, third-party APIs or any
        // SynchronizationContext-reliant code before AppMain is called: things aren't initialized
        // yet and stuff might break.
        [STAThread]
        public static void Main(string[] args)
        {
            BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        }

        // Avalonia configuration, don't remove; also used by visual designer.
        public static AppBuilder BuildAvaloniaApp()
            => AppBuilder.Configure<App>()
                .UsePlatformDetect()
                .With(new MacOSPlatformOptions { ShowInDock = true })
                .With(new VulkanOptions
                {
                    VulkanInstanceCreationOptions = new()
                    {
                        UseDebug = true
                    }
                })
                .UseSkia()
                .WithInterFont()
                .LogToTrace();
    }
}
