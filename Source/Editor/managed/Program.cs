using Avalonia;
using Avalonia.Vulkan;
using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        }

        public static AppBuilder BuildAvaloniaApp()
            => AppBuilder.Configure<App>()
                .With(new MacOSPlatformOptions { ShowInDock = true })
                .With(new Win32PlatformOptions { OverlayPopups = true })
                .UsePlatformDetect()
                .LogToTrace();
    }
}
