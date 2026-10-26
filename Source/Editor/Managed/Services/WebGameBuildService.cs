using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public sealed class WebGameBuildService
    {
        public static WebGameBuildService Instance { get; } = new WebGameBuildService();

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("WebBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"(^ERROR:|\berror\b|CMake Error)", RegexOptions.Compiled);

        private static readonly Regex StepLineRegex = new Regex(@"^==> \[(\d+)%\] (.*)$", RegexOptions.Compiled);

        private static readonly HashSet<string> IgnoredProjectFolders = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "Binaries", "Build" };

        private int _isBuilding;

        private WebGameBuildService()
        {
        }

        public async Task<bool> BuildAsync(string? projectFilePath, bool run)
        {
            if (!TryFindBuildScript(projectFilePath, out string projectDirectory, out string scriptPath))
            {
                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A web build is already running");

                return false;
            }

            string gameTitle = ProjectLayout.GetGameTitle(projectFilePath!);

            ExternalEditorTask task = new ExternalEditorTask($"Building {gameTitle} for Web", "Starting");
            task.SetIsForegroundTask(true);
            task.Commit();

            try
            {
                Logger.Log(BuildLogChannel, LogLevel.Info, "Building {0} for the web", projectDirectory);

                string? firstError = null;
                bool isCancelled = false;

                List<string> arguments = new List<string> { "-ContentDir", ProjectLayout.GetContentDirectory(projectFilePath!) };
                arguments.AddRange(GetNameArguments(projectFilePath!));

                // the enabled built-in plugins' runtime modules, linked into the page
                List<string> pluginModules = EngineManager.CurrentProject?.GetEnabledPluginRuntimeModules().Cast<Name>().Select(moduleName => moduleName.ToString()).ToList() ?? new List<string>();

                if (pluginModules.Count > 0)
                {
                    arguments.Add("-PluginModules");
                    arguments.Add(string.Join(",", pluginModules));
                }

                if (run)
                {
                    arguments.Add("-Run");
                    arguments.Add("-FastLink");
                }

                int exitCode = await RunScriptAsync(scriptPath, projectDirectory, arguments.ToArray(), line =>
                {
                    Match stepMatch = StepLineRegex.Match(line);

                    if (stepMatch.Success)
                    {
                        task.SetStep(stepMatch.Groups[2].Value, int.Parse(stepMatch.Groups[1].Value) / 100.0f);
                    }
                    else if (firstError == null && ErrorLineRegex.IsMatch(line))
                    {
                        firstError = line;
                    }
                }, process =>
                {
                    task.OnCancelAction = () =>
                    {
                        isCancelled = true;

                        try
                        {
                            process.Kill(entireProcessTree: true);
                        }
                        catch (Exception)
                        {
                        }
                    };
                }).ConfigureAwait(false);

                if (isCancelled)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Warning, "Web build cancelled");

                    return false;
                }

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "Web build failed (exit code {0})", exitCode);

                    ShowMessage(MessageBox.Critical("Web build failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                    return false;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Web build succeeded: {0}", Path.Combine(projectDirectory, "Binaries", "Web"));

                return true;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Web build failed: {0}", ex.Message);

                return false;
            }
            finally
            {
                task.OnCancelAction = null;
                task.Complete();

                Interlocked.Exchange(ref _isBuilding, 0);
            }
        }

        public async Task RunInBrowserAsync(string? projectFilePath, bool hasUnsavedChanges)
        {
            if (!TryFindBuildScript(projectFilePath, out string projectDirectory, out string scriptPath))
            {
                return;
            }

            if (Volatile.Read(ref _isBuilding) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A web build is already running");

                return;
            }

            string gameTitle = ProjectLayout.GetGameTitle(projectFilePath!);
            DateTime? builtTime = GetBuiltTime(projectDirectory);

            if (builtTime == null)
            {
                ShowMessage(MessageBox.Info("Not built for the web yet", $"{gameTitle} hasn't been built for the web yet. Build it now and run it in the browser?")
                    .Button("Build and Run", BuildAndRun)
                    .Button("Cancel", () => { }));

                return;
            }

            string? outOfDateReason = GetOutOfDateReason(projectDirectory, scriptPath, builtTime.Value, hasUnsavedChanges);

            if (outOfDateReason != null)
            {
                ShowMessage(MessageBox.Warning("Web build out of date", $"{outOfDateReason} since {gameTitle} was last built for the web. Rebuild it before running?")
                    .Button("Rebuild and Run", BuildAndRun)
                    .Button("Run Anyway", () => _ = Task.Run(() => ServeAsync(scriptPath, projectFilePath!, projectDirectory)))
                    .Button("Cancel", () => { }));

                return;
            }

            await ServeAsync(scriptPath, projectFilePath!, projectDirectory).ConfigureAwait(false);
        }

        private static void BuildAndRun()
        {
            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommandBuildWebGame"), "--run");
        }

        private static async Task ServeAsync(string scriptPath, string projectFilePath, string projectDirectory)
        {
            string? firstError = null;

            int exitCode = await RunScriptAsync(scriptPath, projectDirectory, GetNameArguments(projectFilePath).Append("-ServeOnly").ToArray(), line =>
            {
                if (firstError == null && ErrorLineRegex.IsMatch(line))
                {
                    firstError = line;
                }
            }, _ => { }).ConfigureAwait(false);

            if (exitCode != 0)
            {
                ShowMessage(MessageBox.Critical("Couldn't run in the browser", firstError ?? "See the log for details.").Button("OK", () => { }));
            }
        }

        private static DateTime? GetBuiltTime(string projectDirectory)
        {
            string pagePath = Path.Combine(projectDirectory, "Binaries", "Web", "index.html");

            if (!File.Exists(pagePath))
            {
                return null;
            }

            string stampPath = Path.Combine(projectDirectory, "Build", "Web", "Built.stamp");

            // builds from before the stamp existed go by when the page was made
            return File.Exists(stampPath) ? File.GetLastWriteTimeUtc(stampPath) : File.GetLastWriteTimeUtc(pagePath);
        }

        private static string? GetOutOfDateReason(string projectDirectory, string scriptPath, DateTime builtTime, bool hasUnsavedChanges)
        {
            if (hasUnsavedChanges)
            {
                return "The project has unsaved changes";
            }

            if (GetNewestProjectFileTime(projectDirectory) > builtTime)
            {
                return "The project has changed";
            }

            string engineRoot = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(scriptPath)!, "..", ".."));
            string webSdkDirectory = Path.Combine(engineRoot, "Binaries", "Web", "Release", "Sdk");

            if (GetNewestFileTime(webSdkDirectory) > builtTime)
            {
                return "The engine has been rebuilt";
            }

            return null;
        }

        private static DateTime GetNewestProjectFileTime(string projectDirectory)
        {
            DateTime newest = GetNewestFileTime(projectDirectory, recursive: false);

            foreach (string folder in Directory.EnumerateDirectories(projectDirectory))
            {
                string folderName = Path.GetFileName(folder);

                if (IgnoredProjectFolders.Contains(folderName) || folderName.StartsWith('.'))
                {
                    continue;
                }

                DateTime folderNewest = GetNewestFileTime(folder);

                if (folderNewest > newest)
                {
                    newest = folderNewest;
                }
            }

            return newest;
        }

        private static DateTime GetNewestFileTime(string directory, bool recursive = true)
        {
            DateTime newest = DateTime.MinValue;

            if (!Directory.Exists(directory))
            {
                return newest;
            }

            EnumerationOptions options = new EnumerationOptions
            {
                RecurseSubdirectories = recursive,
                IgnoreInaccessible = true,
                AttributesToSkip = FileAttributes.Hidden | FileAttributes.System
            };

            foreach (FileInfo file in new DirectoryInfo(directory).EnumerateFiles("*", options))
            {
                if (file.LastWriteTimeUtc > newest)
                {
                    newest = file.LastWriteTimeUtc;
                }
            }

            return newest;
        }

        private static bool TryFindBuildScript(string? projectFilePath, out string projectDirectory, out string scriptPath)
        {
            projectDirectory = string.Empty;
            scriptPath = string.Empty;

            if (!OperatingSystem.IsWindows())
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Building for the web from the editor is only supported on Windows for now");

                return false;
            }

            if (string.IsNullOrEmpty(projectFilePath))
            {
                ShowMessage(MessageBox.Info("Save the project first", "The project needs to be saved before it can be built for the web.").Button("OK", () => { }));

                return false;
            }

            if (ManagedGameBuildService.FindProjectFile(projectFilePath) != null)
            {
                ShowMessage(MessageBox.Info("Not available for C# projects", "C# games can't be built for the web yet.").Button("OK", () => { }));

                return false;
            }

            string editorDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;

            projectDirectory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;
            scriptPath = Path.GetFullPath(Path.Combine(editorDirectory, "..", "..", "..", "Tools", "Scripts", "BuildWebGame.ps1"));

            if (!File.Exists(scriptPath))
            {
                ShowMessage(MessageBox.Info("Not available in this editor", "Building for the web needs an editor built from the engine's source for now.").Button("OK", () => { }));

                return false;
            }

            return true;
        }

        private static string[] GetNameArguments(string projectFilePath)
        {
            return [
                "-Name",
                ProjectLayout.GetModuleName(projectFilePath),
                "-Title",
                ProjectLayout.GetGameTitle(projectFilePath)
            ];
        }

        private static async Task<int> RunScriptAsync(
            string scriptPath,
            string projectDirectory,
            string[] arguments,
            Action<string> onOutputLine,
            Action<Process> onStarted)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                WorkingDirectory = projectDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            startInfo.ArgumentList.Add("-NoProfile");
            startInfo.ArgumentList.Add("-ExecutionPolicy");
            startInfo.ArgumentList.Add("Bypass");
            startInfo.ArgumentList.Add("-File");
            startInfo.ArgumentList.Add(scriptPath);
            startInfo.ArgumentList.Add("-Project");
            startInfo.ArgumentList.Add(projectDirectory);

            foreach (string argument in arguments)
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

                Logger.Log(BuildLogChannel, LogLevel.Info, line);

                onOutputLine(line);
            }

            process.OutputDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);
            process.ErrorDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);

            process.Start();

            onStarted(process);

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
