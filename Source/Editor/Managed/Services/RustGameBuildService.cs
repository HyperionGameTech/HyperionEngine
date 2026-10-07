using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public sealed class RustGameBuildService
    {
        public static RustGameBuildService Instance { get; } = new RustGameBuildService();

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"^error(\[E\d+\])?:", RegexOptions.Compiled);

        private int _isBuilding;

        private RustGameBuildService()
        {
        }

        public static string? FindProjectFile(string? projectFilePath)
        {
            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));
            string? manifestPath = projectDirectory == null ? null : Path.Combine(projectDirectory, "Source", "Cargo.toml");

            return manifestPath != null && File.Exists(manifestPath) ? manifestPath : null;
        }

        public async Task<bool> BuildAsync(string? projectFilePath)
        {
            string? manifestPath = FindProjectFile(projectFilePath);

            if (manifestPath == null)
            {
                ShowMessage(MessageBox.Info("No Rust project", "Generate the Rust project first.").Button("OK", () => { }));

                return false;
            }

            string? cargoPath = FindCargo();

            if (cargoPath == null)
            {
                ShowMessage(MessageBox.Critical("Rust toolchain required", "cargo was not found. Install Rust from https://rustup.rs to build a Rust game.").Button("OK", () => { }));

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                string projectDirectory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath!))!;
                string targetDirectory = Path.Combine(projectDirectory, "Build", "Rust");

                Logger.Log(BuildLogChannel, LogLevel.Info, "Building game project {0}", manifestPath);

                string? firstError = null;

                int exitCode = await RunCargoAsync(cargoPath, manifestPath, targetDirectory, line =>
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

                string outputDirectory = Path.Combine(projectDirectory, "Binaries", PlatformDirectory);

                StageGame(Path.Combine(targetDirectory, "release"), outputDirectory, Path.GetFileNameWithoutExtension(projectFilePath!));

                Logger.Log(BuildLogChannel, LogLevel.Info, "Game build succeeded: {0}", outputDirectory);

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

        // matches GetModulePlatformDirectory in EditorNativeModule.cpp
        private static string PlatformDirectory => OperatingSystem.IsWindows() ? "Windows" : OperatingSystem.IsMacOS() ? "Darwin" : "Linux";

        // The built game next to the engine runtime, set up to run against the project's uncooked content
        // (what hyperion_copy_runtime does for a C++ game)
        private static void StageGame(string releaseDirectory, string outputDirectory, string appName)
        {
            Directory.CreateDirectory(outputDirectory);

            foreach (string file in Directory.GetFiles(releaseDirectory))
            {
                string extension = Path.GetExtension(file).ToLowerInvariant();

                if (extension is ".exe" or ".dll" or ".so" or ".dylib" or ".pdb" || (extension.Length == 0 && !OperatingSystem.IsWindows()))
                {
                    File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
                }
            }

            string editorDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;
            string sdkCMakeDirectory = Path.Combine(editorDirectory, "Sdk", "CMake");

            string sdkText = ReadIfExists(Path.Combine(sdkCMakeDirectory, "HyperionSdk.cmake"));
            string devPathsText = ReadIfExists(Path.Combine(sdkCMakeDirectory, "HyperionSdkDevPaths.cmake"));

            List<string> runtimeFiles = new List<string>();

            // a packaged editor lists the libraries a game needs; a dev build takes everything next to the editor
            string runtimeLibraries = ReadCMakeVariable(sdkText, "HYPERION_SDK_RUNTIME_LIBRARIES") ?? string.Empty;

            if (runtimeLibraries.Length != 0)
            {
                foreach (string runtimeLibrary in runtimeLibraries.Split(';', StringSplitOptions.RemoveEmptyEntries))
                {
                    runtimeFiles.Add(Path.Combine(editorDirectory, runtimeLibrary));
                }
            }
            else
            {
                foreach (string pattern in new[] { "*.dll", "*.so", "*.dylib" })
                {
                    runtimeFiles.AddRange(Directory.GetFiles(editorDirectory, pattern));
                }
            }

            foreach (string runtimeFile in runtimeFiles)
            {
                if (File.Exists(runtimeFile))
                {
                    CopyIfDifferent(runtimeFile, Path.Combine(outputDirectory, Path.GetFileName(runtimeFile)));
                }
            }

            string configDirectory = Path.Combine(outputDirectory, "Config");
            Directory.CreateDirectory(configDirectory);

            string editorConfigDirectory = Path.Combine(editorDirectory, "Config");

            if (Directory.Exists(editorConfigDirectory))
            {
                foreach (string configFile in Directory.GetFiles(editorConfigDirectory))
                {
                    if (!Path.GetFileName(configFile).Equals("GlobalConfig.json", StringComparison.OrdinalIgnoreCase))
                    {
                        CopyIfDifferent(configFile, Path.Combine(configDirectory, Path.GetFileName(configFile)));
                    }
                }
            }

            string globalConfigPath = Path.Combine(configDirectory, "GlobalConfig.json");

            if (!File.Exists(globalConfigPath))
            {
                string baseDirectory = (ReadCMakeVariable(devPathsText, "HYPERION_SDK_BASE_DIR") ?? editorDirectory).Replace('\\', '/').TrimEnd('/');

                File.WriteAllLines(globalConfigPath,
                [
                    "{",
                    "  \"App\": {",
                    $"    \"Name\": \"{appName}\",",
                    $"    \"Args\": \"--basedir=\\\"{baseDirectory}\\\" --contentdir=../../ --cooked=false --singleplayer\"",
                    "  }",
                    "}"
                ]);
            }
        }

        private static string ReadIfExists(string path)
        {
            return File.Exists(path) ? File.ReadAllText(path) : string.Empty;
        }

        private static string? ReadCMakeVariable(string text, string name)
        {
            Match match = Regex.Match(text, "set\\(" + Regex.Escape(name) + " \"([^\"]*)\"\\)");

            return match.Success ? match.Groups[1].Value : null;
        }

        private static void CopyIfDifferent(string source, string destination)
        {
            FileInfo sourceInfo = new FileInfo(source);
            FileInfo destinationInfo = new FileInfo(destination);

            if (destinationInfo.Exists && destinationInfo.Length == sourceInfo.Length && destinationInfo.LastWriteTimeUtc == sourceInfo.LastWriteTimeUtc)
            {
                return;
            }

            File.Copy(source, destination, overwrite: true);
        }

        private static string? FindCargo()
        {
            string executable = OperatingSystem.IsWindows() ? "cargo.exe" : "cargo";

            foreach (string directory in (Environment.GetEnvironmentVariable("PATH") ?? string.Empty).Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
            {
                try
                {
                    string candidate = Path.Combine(directory.Trim(), executable);

                    if (File.Exists(candidate))
                    {
                        return candidate;
                    }
                }
                catch (ArgumentException)
                {
                }
            }

            // rustup's default location, for an editor started before PATH picked it up
            string rustupCandidate = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".cargo", "bin", executable);

            return File.Exists(rustupCandidate) ? rustupCandidate : null;
        }

        private static async Task<int> RunCargoAsync(string cargoPath, string manifestPath, string targetDirectory, Action<string> onOutputLine)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = cargoPath,
                WorkingDirectory = Path.GetDirectoryName(manifestPath)!,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            foreach (string argument in new[] { "build", "--release", "--manifest-path", manifestPath, "--target-dir", targetDirectory, "--color", "never" })
            {
                startInfo.ArgumentList.Add(argument);
            }

            using Process process = new Process { StartInfo = startInfo };

            void OnLine(string? line)
            {
                if (line == null)
                {
                    return;
                }

                // No format args, so braces in compiler output are logged as-is
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
