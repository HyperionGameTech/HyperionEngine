using System.Collections.Generic;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public abstract partial class GameBuildPlatform
    {
        public abstract void PromptInstallNativeToolchain(IReadOnlyList<string> missing);

        protected static void ShowMessage(MessageBox messageBox)
        {
            Dispatcher.UIThread.Post(() => messageBox.Show());
        }
    }
}
