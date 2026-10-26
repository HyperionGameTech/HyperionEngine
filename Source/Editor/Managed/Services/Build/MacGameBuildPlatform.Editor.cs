using System.Collections.Generic;

namespace Hyperion.Editor.Services
{
    public sealed partial class MacGameBuildPlatform
    {
        public override void PromptInstallNativeToolchain(IReadOnlyList<string> missing)
        {
            ShowMessage(MessageBox.Warning("C++ build tools required",
                    FormatMissing(missing) + "\n\nInstall with:\n  xcode-select --install\n  brew install cmake ninja")
                .Button("OK", () => { }));
        }
    }
}
