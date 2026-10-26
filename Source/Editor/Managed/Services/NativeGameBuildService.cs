using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public sealed class NativeGameBuildService
    {
        public static NativeGameBuildService Instance { get; } = new NativeGameBuildService();

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"(\berror\b|CMake Error)", RegexOptions.Compiled);

        private int _isBuilding;

        private NativeGameBuildService()
        {
        }

        public Task<bool> BuildAsync(string? projectFilePath)
        {
            return BuildAsync(projectFilePath, isPackage: false, Array.Empty<string>());
        }

        public bool BuildPackage(string projectFilePath, string outputDirectory, string? scriptsLinkInputsFile)
        {
            return BuildAsync(projectFilePath, isPackage: true, NativeGameBuild.GetPackageArguments(outputDirectory, scriptsLinkInputsFile)).GetAwaiter().GetResult();
        }

        private async Task<bool> BuildAsync(
            string? projectFilePath,
            bool isPackage,
            IReadOnlyList<string> cmakeArguments)
        {
            GameBuildPlatform platform = GameBuildPlatform.Current;

            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));

            if (projectDirectory == null || !File.Exists(Path.Combine(projectDirectory, "Source", "CMakeLists.txt")))
            {
                ShowMessage(MessageBox.Info("No game project", "Generate a game project first (Build > Generate Game Project...)").Button("OK", () => { }));

                return false;
            }

            HyperionSdk? sdk = HyperionSdk.Find(shipping: isPackage);

            if (sdk == null)
            {
                ShowMessage(isPackage
                    ? MessageBox.Critical("Shipping SDK not found", $"Engine static libraries not found in {HyperionSdk.ExpectedShippingDirectory}").Button("OK", () => { })
                    : MessageBox.Critical("SDK not found", $"This editor install has no SDK at {HyperionSdk.ExpectedDirectory}.").Button("OK", () => { }));

                return false;
            }

            NativeToolchain toolchain = platform.FindNativeToolchain(sdk.Compiler);

            if (toolchain.Missing.Count > 0)
            {
                platform.PromptInstallNativeToolchain(toolchain.Missing);

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                string buildDirectory = NativeGameBuild.GetBuildDirectory(projectDirectory, platform, isPackage);
                string scriptPath = NativeGameBuild.WriteBuildScript(platform, toolchain, sdk, projectDirectory, buildDirectory, cmakeArguments);

                Logger.Log(BuildLogChannel, LogLevel.Info, "Building game in {0}", projectDirectory);

                string? firstError = null;

                int exitCode = await BuildProcess.RunAsync(platform.CreateBuildScriptStartInfo(scriptPath, buildDirectory), BuildLogChannel, line =>
                {
                    if (firstError == null && ErrorLineRegex.IsMatch(line))
                    {
                        firstError = line;
                    }
                }).ConfigureAwait(false);

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "Game build failed (exit code {0})", exitCode);

                    ShowMessage(MessageBox.Critical("Build failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                    return false;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Game build succeeded in {0}", buildDirectory);

                return true;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Game build failed: {0}", ex.Message);

                return false;
            }
            finally
            {
                Interlocked.Exchange(ref _isBuilding, 0);
            }
        }

        private static void ShowMessage(MessageBox messageBox)
        {
            Dispatcher.UIThread.Post(() => messageBox.Show());
        }
    }
}
