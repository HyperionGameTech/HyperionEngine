using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Platform;
using Avalonia.Threading;
using System;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Services;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;
        private const bool IsRenderingOnMainThread = true;

        public MainWindow()
        {
            InitializeComponent();

            // Provide engine window to the viewport control via factory
            EditorViewportControl? evc = this.FindControl<EditorViewportControl>("EditorViewportControl");

            if (evc == null)
            {
                throw new Exception("EditorViewportControl control not found in MainWindow.");
            }

            evc.Focus();

            DataContext = new MainWindowViewModel();

            if (IsRenderingOnMainThread)
            {
                Opened += (s, e) =>
                {
                    var topLevel = TopLevel.GetTopLevel(this);
                    topLevel?.RequestAnimationFrame(OnFrame);
                };
            }
        }

        // need to destroy the engine window when MainWindow is closed
        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
        }

        private void OnFrame(TimeSpan time)
        {
            NativeBindings.Hyp_MainThreadUpdate();

            ConsoleService.Instance.ProcessLogQueue();

            var topLevel = GetTopLevel(this);
            topLevel?.RequestAnimationFrame(OnFrame);
        }

        // protected override void OnKeyDown(Avalonia.Input.KeyEventArgs e)
        // {
        //     base.OnKeyDown(e);

        //     var vm = DataContext as MainWindowViewModel;
        //     vm?.HandleKeyDown(e);
        // }
    }
}
