using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Platform;
using Avalonia.Threading;
using System;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;

        public MainWindow()
        {
            InitializeComponent();

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
