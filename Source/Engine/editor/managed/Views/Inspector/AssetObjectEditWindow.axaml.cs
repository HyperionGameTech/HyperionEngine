using System;
using System.Runtime.InteropServices;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AssetObjectEditWindow : Window
    {
        public AssetObjectEditWindow(string label, string assetPath, ComponentSubObjectViewModel subObject)
        {
            InitializeComponent();

            Title = $"Edit {label}";
            DataContext = subObject;

            PART_Heading.Text = label;

            bool hasPath = !string.IsNullOrEmpty(assetPath) && assetPath != "(None)";
            PART_SubHeading.Text = hasPath ? assetPath : string.Empty;
            PART_SubHeading.IsVisible = hasPath;
        }

        protected override void OnOpened(EventArgs e)
        {
            base.OnOpened(e);
            RemoveMinimizeButton();
        }

        private void RemoveMinimizeButton()
        {
            if (!OperatingSystem.IsWindows())
            {
                return;
            }

            IntPtr hWnd = TryGetPlatformHandle()?.Handle ?? IntPtr.Zero;

            if (hWnd == IntPtr.Zero)
            {
                return;
            }

            const int GWL_STYLE = -16;
            const uint WS_MINIMIZEBOX = 0x00020000;
            const uint WS_MAXIMIZEBOX = 0x00010000;

            uint style = (uint)GetWindowLong(hWnd, GWL_STYLE);
            SetWindowLong(hWnd, GWL_STYLE, (int)(style & ~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX));
        }

        private void OnClose(object? sender, RoutedEventArgs e)
        {
            Close();
        }

        [DllImport("user32.dll")]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);
    }
}
