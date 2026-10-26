using System.Collections.Generic;

namespace Hyperion.Editor.Services
{
    public sealed partial class LinuxGameBuildPlatform
    {
        public override void PromptInstallNativeToolchain(IReadOnlyList<string> missing)
        {
            ShowMessage(MessageBox.Warning("C++ build tools required",
                    FormatMissing(missing) + "\n\nInstall using your distribution's package manager, for example:\n  sudo apt install cmake ninja-build g++\n  sudo dnf install cmake ninja-build gcc-c++")
                .Button("OK", () => { }));
        }
    }
}
