using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace Hyperion.Editor.Services
{
    public sealed partial class WindowsGameBuildPlatform
    {
        private const string BuildToolsPackageId = "Microsoft.VisualStudio.BuildTools";

        private const string BuildToolsInstallerArguments = "--passive --wait"
            + " --add Microsoft.VisualStudio.Workload.VCTools"
            + " --add Microsoft.VisualStudio.Component.VC.Llvm.Clang"
            + " --add Microsoft.VisualStudio.Component.VC.CMake.Project"
            + " --includeRecommended";

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        public override void PromptInstallNativeToolchain(IReadOnlyList<string> missing)
        {
            string text = "Building the game needs:\n  - " + string.Join("\n  - ", missing)
                + "\n\nInstall Visual Studio Build Tools?";

            ShowMessage(MessageBox.Warning("C++ build tools required", text)
                .Button("Install", StartBuildToolsInstall)
                .Button("Cancel", () => { }));
        }

        static partial void OnVsWhereFailed(Exception ex)
        {
            Logger.Log(BuildLogChannel, LogLevel.Warning, "Could not run vswhere: {0}", ex.Message);
        }

        private static void StartBuildToolsInstall()
        {
            try
            {
                ProcessStartInfo startInfo = new ProcessStartInfo
                {
                    FileName = "winget",
                    UseShellExecute = true
                };

                foreach (string argument in new[] { "install", "--id", BuildToolsPackageId, "--exact", "--override", BuildToolsInstallerArguments })
                {
                    startInfo.ArgumentList.Add(argument);
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Starting winget {0}", string.Join(" ", startInfo.ArgumentList));

                Process.Start(startInfo)?.Dispose();
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Could not start winget: {0}", ex.Message);

                ShowMessage(MessageBox.Critical("Could not start the install", "winget was not found. Install Visual Studio Build Tools with the C++ workload, Clang and CMake components manually.").Button("OK", () => { }));
            }
        }
    }
}
