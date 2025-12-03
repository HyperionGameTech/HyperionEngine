using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Platform;
using Avalonia.Threading;
using System;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;
        private const bool IsRenderingOnMainThread = true;

        private IntPtr AppContext { get; set; } = IntPtr.Zero;

        public MainWindow()
        {
            InitializeComponent();

            // Provide engine window to the viewport control via factory
            var viewport = this.FindControl<EditorViewportControl>("EditorViewportControl");

            if (viewport == null)
                throw new Exception("EditorViewportControl control not found in MainWindow.");

            IntPtr window = IntPtr.Zero;
            AppContext = NativeBindings.Hyp_GetAppContext();
            if (AppContext == IntPtr.Zero)
                throw new Exception("Failed to get AppContext from Hyperion");

            DataContext = new MainWindowViewModel();

            if (IsRenderingOnMainThread)
            {
                Opened += (s, e) =>
                {
                    if (CappedFrameRate)
                    {
                        var topLevel = TopLevel.GetTopLevel(this);
                        topLevel?.RequestAnimationFrame(OnFrame);
                    }
                    else
                    {
                        OnFrame(TimeSpan.Zero);
                    }
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
            if (CappedFrameRate)
            {
                NativeBindings.Hyp_MainThreadUpdate();

                var topLevel = GetTopLevel(this);
                topLevel?.RequestAnimationFrame(OnFrame);

                return;
            }

            Dispatcher.UIThread.Post(() =>
            {
                NativeBindings.Hyp_MainThreadUpdate();

                OnFrame(TimeSpan.Zero);
            }, DispatcherPriority.Render);
        }
    }
}
