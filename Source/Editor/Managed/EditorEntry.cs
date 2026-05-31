using System;
using Avalonia;

namespace Hyperion.Editor
{
    public static class EditorEntry
    {
        public static void Run()
        {
            App.AlreadyInitialized = true;

            Program.BuildAvaloniaApp().StartWithClassicDesktopLifetime(
                Environment.GetCommandLineArgs());
        }
    }
}
