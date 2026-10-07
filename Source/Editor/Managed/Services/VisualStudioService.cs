using System;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Services
{
    public static class VisualStudioService
    {
        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly Lazy<string?> s_devEnvPath = new Lazy<string?>(FindDevEnv);

        public static bool IsAvailable => OperatingSystem.IsWindows() && s_devEnvPath.Value != null;

        public static async Task OpenGameProjectAsync(Func<string?> projectFilePathProvider, GameProjectLanguage language)
        {
            if (!IsAvailable)
            {
                return;
            }

            DateTime deadline = DateTime.UtcNow.AddMinutes(2);

            while (DateTime.UtcNow < deadline)
            {
                await Task.Delay(500);

                string? target = FindOpenTarget(projectFilePathProvider(), language);

                if (target == null)
                {
                    continue;
                }

                try
                {
                    if (language == GameProjectLanguage.Native)
                    {
                        WriteCMakeUserPresets(target);
                    }

                    ProcessStartInfo startInfo = new ProcessStartInfo
                    {
                        FileName = s_devEnvPath.Value!,
                        UseShellExecute = false
                    };

                    startInfo.ArgumentList.Add(target);

                    Process.Start(startInfo)?.Dispose();
                }
                catch (Exception ex)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "Could not open {0} in Visual Studio: {1}", target, ex.Message);
                }

                return;
            }
        }

        private static string? FindOpenTarget(string? projectFilePath, GameProjectLanguage language)
        {
            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));

            if (projectDirectory == null || !Directory.Exists(projectDirectory))
            {
                return null;
            }

            if (language == GameProjectLanguage.Managed)
            {
                string[] solutions = Directory.GetFiles(projectDirectory, "*.slnx");

                return solutions.Length > 0 ? solutions[0] : null;
            }

            string sourceDirectory = Path.Combine(projectDirectory, "Source");

            return File.Exists(Path.Combine(sourceDirectory, "CMakeLists.txt")) ? sourceDirectory : null;
        }

        private static void WriteCMakeUserPresets(string sourceDirectory)
        {
            string presetsPath = Path.Combine(sourceDirectory, "CMakeUserPresets.json");

            string editorDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;
            string sdkDirectory = Path.Combine(editorDirectory, "Sdk");
            string sdkFile = Path.Combine(sdkDirectory, "CMake", "HyperionSdk.cmake");

            if (!File.Exists(sdkFile))
            {
                return;
            }

            Match compilerMatch = Regex.Match(File.ReadAllText(sdkFile), "set\\(HYPERION_SDK_COMPILER \"([^\"]*)\"\\)");
            string compiler = compilerMatch.Success ? compilerMatch.Groups[1].Value : "clang-cl";

            File.WriteAllLines(presetsPath,
            [
                "{",
                "  \"version\": 3,",
                "  \"configurePresets\": [",
                "    {",
                "      \"name\": \"hyperion-release\",",
                "      \"displayName\": \"Hyperion (Release)\",",
                "      \"generator\": \"Ninja\",",
                "      \"binaryDir\": \"${sourceDir}/../Build/VisualStudio\",",
                "      \"architecture\": { \"value\": \"x64\", \"strategy\": \"external\" },",
                "      \"cacheVariables\": {",
                "        \"CMAKE_BUILD_TYPE\": \"Release\",",
                $"        \"CMAKE_CXX_COMPILER\": \"{compiler}\",",
                $"        \"HYPERION_SDK_DIR\": \"{sdkDirectory.Replace('\\', '/')}\"",
                "      }",
                "    }",
                "  ]",
                "}"
            ]);
        }

        private static string? FindDevEnv()
        {
            if (!OperatingSystem.IsWindows())
            {
                return null;
            }

            foreach (string installPath in NativeGameBuildService.FindVisualStudioInstalls())
            {
                string devEnvPath = Path.Combine(installPath, "Common7", "IDE", "devenv.exe");

                if (File.Exists(devEnvPath))
                {
                    return devEnvPath;
                }
            }

            return null;
        }
    }
}
