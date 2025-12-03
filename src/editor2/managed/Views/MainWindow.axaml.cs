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

        public MainWindow()
        {
            InitializeComponent();

            // Provide engine window to the viewport control via factory
            EditorViewportControl evc = this.FindControl<EditorViewportControl>("EditorViewportControl");

            if (evc == null)
            {
                throw new Exception("EditorViewportControl control not found in MainWindow.");
            }

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
