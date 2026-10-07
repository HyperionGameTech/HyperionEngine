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
    public sealed class NativeGameBuildService
    {
        public static NativeGameBuildService Instance { get; } = new NativeGameBuildService();

        private const string BuildToolsPackageId = "Microsoft.VisualStudio.BuildTools";

        private const string BuildToolsInstallerArguments = "--passive --wait"
            + " --add Microsoft.VisualStudio.Workload.VCTools"
            + " --add Microsoft.VisualStudio.Component.VC.Llvm.Clang"
            + " --add Microsoft.VisualStudio.Component.VC.CMake.Project"
            + " --includeRecommended";

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"(\berror\b|CMake Error)", RegexOptions.Compiled);

        private int _isBuilding;

        private NativeGameBuildService()
        {
        }

        private sealed class Toolchain
        {
            public string? VcVarsPath;
            public string? CompilerPath;
            public string? CMakePath;
            public string? NinjaPath;

            public List<string> Missing { get; } = new List<string>();
        }

        public async Task<bool> BuildAsync(string? projectFilePath)
        {
            if (!OperatingSystem.IsWindows())
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Building a C++ game from the editor is only supported on Windows for now");

                return false;
            }

            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));

            if (projectDirectory == null || !File.Exists(Path.Combine(projectDirectory, "Source", "CMakeLists.txt")))
            {
                ShowMessage(MessageBox.Info("No game project", "Generate a game project first (Build > Generate Game Project...).").Button("OK", () => { }));

                return false;
            }

            string editorDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;
            string sdkDirectory = Path.Combine(editorDirectory, "Sdk");
            string sdkFile = Path.Combine(sdkDirectory, "CMake", "HyperionSdk.cmake");

            if (!File.Exists(sdkFile))
            {
                ShowMessage(MessageBox.Critical("SDK not found", $"This editor install has no SDK at {sdkDirectory}.").Button("OK", () => { }));

                return false;
            }

            Match compilerMatch = Regex.Match(File.ReadAllText(sdkFile), "set\\(HYPERION_SDK_COMPILER \"([^\"]*)\"\\)");
            string compiler = compilerMatch.Success ? compilerMatch.Groups[1].Value : "clang-cl";

            Toolchain toolchain = FindToolchain(compiler);

            if (toolchain.Missing.Count > 0)
            {
                PromptInstallBuildTools(toolchain.Missing);

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                string buildDirectory = Path.Combine(projectDirectory, "Build", "Windows");
                Directory.CreateDirectory(buildDirectory);

                string scriptPath = Path.Combine(buildDirectory, "Build.bat");

                File.WriteAllLines(scriptPath,
                [
                    "@echo off",
                    $"call \"{toolchain.VcVarsPath}\" >nul",
                    "if errorlevel 1 exit /b 1",
                    $"\"{toolchain.CMakePath}\" -S \"{Path.Combine(projectDirectory, "Source")}\" -B \"{buildDirectory}\" -G Ninja"
                        + $" -DCMAKE_MAKE_PROGRAM=\"{ToCMakePath(toolchain.NinjaPath!)}\""
                        + " -DCMAKE_BUILD_TYPE=Release"
                        + $" -DCMAKE_CXX_COMPILER=\"{ToCMakePath(toolchain.CompilerPath!)}\""
                        + $" -DHYPERION_SDK_DIR=\"{ToCMakePath(sdkDirectory)}\"",
                    "if errorlevel 1 exit /b 1",
                    $"\"{toolchain.CMakePath}\" --build \"{buildDirectory}\""
                ]);

                Logger.Log(BuildLogChannel, LogLevel.Info, "Building game in {0}", projectDirectory);

                string? firstError = null;

                int exitCode = await RunAsync(scriptPath, buildDirectory, line =>
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

                Logger.Log(BuildLogChannel, LogLevel.Info, "Game build succeeded: {0}", Path.Combine(projectDirectory, "Binaries", "Windows"));

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

        private static Toolchain FindToolchain(string compiler)
        {
            Toolchain? best = null;

            foreach (string installPath in FindVisualStudioInstalls())
            {
                Toolchain candidate = new Toolchain();

                string vcVarsPath = Path.Combine(installPath, "VC", "Auxiliary", "Build", "vcvars64.bat");
                string cmakeRoot = Path.Combine(installPath, "Common7", "IDE", "CommonExtensions", "Microsoft", "CMake");

                candidate.VcVarsPath = File.Exists(vcVarsPath) ? vcVarsPath : null;
                candidate.CMakePath = FirstExisting(Path.Combine(cmakeRoot, "CMake", "bin", "cmake.exe")) ?? FindOnPath("cmake.exe");
                candidate.NinjaPath = FirstExisting(Path.Combine(cmakeRoot, "Ninja", "ninja.exe")) ?? FindOnPath("ninja.exe");

                if (compiler == "cl")
                {
                    // on PATH once vcvars has run
                    candidate.CompilerPath = candidate.VcVarsPath != null ? "cl.exe" : null;
                }
                else
                {
                    candidate.CompilerPath = FirstExisting(Path.Combine(installPath, "VC", "Tools", "Llvm", "x64", "bin", "clang-cl.exe")) ?? FindOnPath("clang-cl.exe");
                }

                if (candidate.VcVarsPath == null)
                {
                    candidate.Missing.Add("MSVC C++ build tools");
                }

                if (candidate.CompilerPath == null)
                {
                    candidate.Missing.Add("C++ Clang tools for Windows (clang-cl)");
                }

                if (candidate.CMakePath == null)
                {
                    candidate.Missing.Add("CMake");
                }

                if (candidate.NinjaPath == null)
                {
                    candidate.Missing.Add("Ninja");
                }

                if (best == null || candidate.Missing.Count < best.Missing.Count)
                {
                    best = candidate;
                }
            }

            if (best == null)
            {
                best = new Toolchain();
                best.Missing.Add("Visual Studio or Visual Studio Build Tools with the C++ workload");
            }

            return best;
        }

        internal static List<string> FindVisualStudioInstalls()
        {
            List<string> installPaths = new List<string>();

            string vswherePath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
                "Microsoft Visual Studio", "Installer", "vswhere.exe");

            if (!File.Exists(vswherePath))
            {
                return installPaths;
            }

            try
            {
                ProcessStartInfo startInfo = new ProcessStartInfo
                {
                    FileName = vswherePath,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true
                };

                foreach (string argument in new[] { "-products", "*", "-prerelease", "-sort", "-format", "value", "-property", "installationPath" })
                {
                    startInfo.ArgumentList.Add(argument);
                }

                using Process process = Process.Start(startInfo)!;

                string output = process.StandardOutput.ReadToEnd();
                process.WaitForExit();

                foreach (string line in output.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                {
                    if (Directory.Exists(line))
                    {
                        installPaths.Add(line);
                    }
                }
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "Could not run vswhere: {0}", ex.Message);
            }

            return installPaths;
        }

        private static void PromptInstallBuildTools(List<string> missing)
        {
            string text = "Building the game needs:\n  - " + string.Join("\n  - ", missing)
                + "\n\nInstall Visual Studio Build Tools now? This is a multi-GB download and asks for administrator access.";

            ShowMessage(MessageBox.Warning("C++ build tools required", text)
                .Button("Install", StartBuildToolsInstall)
                .Button("Cancel", () => { }));
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

        private static async Task<int> RunAsync(string scriptPath, string workingDirectory, Action<string> onOutputLine)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe",
                WorkingDirectory = workingDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            startInfo.ArgumentList.Add("/d");
            startInfo.ArgumentList.Add("/c");
            startInfo.ArgumentList.Add(scriptPath);

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

        private static string ToCMakePath(string path)
        {
            return path.Replace('\\', '/');
        }

        private static string? FirstExisting(string path)
        {
            return File.Exists(path) ? path : null;
        }

        private static string? FindOnPath(string fileName)
        {
            foreach (string directory in (Environment.GetEnvironmentVariable("PATH") ?? string.Empty).Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
            {
                try
                {
                    string candidate = Path.Combine(directory.Trim(), fileName);

                    if (File.Exists(candidate))
                    {
                        return candidate;
                    }
                }
                catch (ArgumentException)
                {
                }
            }

            return null;
        }
    }
}
