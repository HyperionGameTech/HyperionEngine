using System;
using System.Runtime.InteropServices;
using Avalonia.Controls;
using Avalonia.Input;
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

            AddHandler(InputElement.LostFocusEvent, OnTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnTextBoxKeyDown, RoutingStrategies.Bubble);
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

        private void OnTextBoxLostFocus(object? sender, RoutedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.CommitValue();
            }
        }

        private void OnTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return && e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.CommitValue();
                e.Handled = true;
            }
        }

        [DllImport("user32.dll")]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);
    }
}
