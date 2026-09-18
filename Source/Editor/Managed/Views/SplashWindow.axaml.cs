using System;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Threading;

namespace Hyperion.Editor.Views
{
    public partial class SplashWindow : Window
    {
        public SplashWindow()
        {
            InitializeComponent();
        }

        public void SetStatus(string status)
        {
            StatusText.Text = status;
        }

        // Engine startup blocks the UI thread, so the splash needs to have presented a frame before each blocking step
        public Task WaitForNextFrameAsync()
        {
            TaskCompletionSource frameCompletion = new TaskCompletionSource();

            RequestAnimationFrame(_ => Dispatcher.UIThread.Post(() => frameCompletion.SetResult(), DispatcherPriority.Background));

            return frameCompletion.Task;
        }

        protected override void OnClosing(WindowClosingEventArgs e)
        {
            // Closing mid-startup would leave the engine half initialized
            if (!e.IsProgrammatic)
            {
                e.Cancel = true;
            }

            base.OnClosing(e);
        }
    }
}
