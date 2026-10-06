using System;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public sealed class ManagedGameBuildService
    {
        public static ManagedGameBuildService Instance { get; } = new ManagedGameBuildService();

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"\berror\b", RegexOptions.Compiled);

        private int _isBuilding;

        private ManagedGameBuildService()
        {
        }

        public static string? FindProjectFile(string? projectFilePath)
        {
            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));
            string? sourceDirectory = projectDirectory == null ? null : Path.Combine(projectDirectory, "Source");

            if (sourceDirectory == null || !Directory.Exists(sourceDirectory))
            {
                return null;
            }

            string[] projectFiles = Directory.GetFiles(sourceDirectory, "*.csproj");

            return projectFiles.Length > 0 ? projectFiles[0] : null;
        }

        public async Task<bool> BuildAsync(string? projectFilePath)
        {
            string? managedProjectFile = FindProjectFile(projectFilePath);

            if (managedProjectFile == null)
            {
                ShowMessage(MessageBox.Info("No C# project", "Generate the C# project first.").Button("OK", () => { }));

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                Logger.Log(BuildLogChannel, LogLevel.Info, "Building game project {0}", managedProjectFile);

                string? firstError = null;

                int exitCode = await RunDotNetAsync(managedProjectFile, line =>
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

                Logger.Log(BuildLogChannel, LogLevel.Info, "Game build succeeded");

                return true;
            }
            catch (System.ComponentModel.Win32Exception)
            {
                ShowMessage(MessageBox.Critical(".NET SDK required", "dotnet was not found. Install the .NET 10 SDK to build a C# game.").Button("OK", () => { }));

                return false;
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

        private static async Task<int> RunDotNetAsync(string managedProjectFile, Action<string> onOutputLine)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = OperatingSystem.IsMacOS() && File.Exists("/usr/local/share/dotnet/dotnet") ? "/usr/local/share/dotnet/dotnet" : "dotnet",
                WorkingDirectory = Path.GetDirectoryName(managedProjectFile)!,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            foreach (string argument in new[] { "build", managedProjectFile, "--configuration", "Release", "--disable-build-servers", "--nologo" })
            {
                startInfo.ArgumentList.Add(argument);
            }

            // a stale MSBuildSdksPath breaks resolution of Microsoft.NET.Sdk
            startInfo.Environment.Remove("MSBuildSdksPath");

            using Process process = new Process { StartInfo = startInfo };

            void OnLine(string? line)
            {
                if (line == null)
                {
                    return;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, line);

                onOutputLine(line);
            }

            process.OutputDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);
            process.ErrorDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);

            process.Start();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync().ConfigureAwait(false);

            return process.ExitCode;
        }

        private static void ShowMessage(MessageBox messageBox)
        {
            Dispatcher.UIThread.Post(() => messageBox.Show());
        }
    }
}
