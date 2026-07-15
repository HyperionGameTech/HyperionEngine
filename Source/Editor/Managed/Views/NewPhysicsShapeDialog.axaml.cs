using System;
using System.Runtime.InteropServices;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class NewPhysicsShapeDialog : Window
    {
        public NewPhysicsShapeDialogViewModel ViewModel => (NewPhysicsShapeDialogViewModel)DataContext!;

        public bool Result { get; private set; }

        public NewPhysicsShapeDialog()
        {
            InitializeComponent();

            Title = "New Physics Shape";
            DataContext = new NewPhysicsShapeDialogViewModel();
        }

        protected override void OnOpened(EventArgs e)
        {
            base.OnOpened(e);

            RemoveMinimizeButton();
        }

        private void RemoveMinimizeButton()
        {
            if (!OperatingSystem.IsWindows())
                return;

            IntPtr hWnd = TryGetPlatformHandle()?.Handle ?? IntPtr.Zero;

            if (hWnd == IntPtr.Zero)
                return;

            const int GWL_STYLE = -16;
            const uint WS_MINIMIZEBOX = 0x00020000;
            const uint WS_MAXIMIZEBOX = 0x00010000;

            uint style = (uint)GetWindowLong(hWnd, GWL_STYLE);
            SetWindowLong(hWnd, GWL_STYLE, (int)(style & ~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX));
        }

        private void OnConfirm(object? sender, RoutedEventArgs e)
        {
            Result = true;
            Close();
        }

        private void OnCancel(object? sender, RoutedEventArgs e)
        {
            Result = false;
            Close();
        }

        [DllImport("user32.dll")]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);
    }
}
