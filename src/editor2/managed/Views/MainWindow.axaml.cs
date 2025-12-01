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

            this.DataContext = new MainWindowViewModel();

            if (IsRenderingOnMainThread)
            {
                this.Opened += (s, e) =>
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

        private void OnFrame(TimeSpan time)
        {
            if (CappedFrameRate)
            {
                NativeBindings.Hyp_MainThreadUpdate();

                var topLevel = TopLevel.GetTopLevel(this);
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
