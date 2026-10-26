using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection.PortableExecutable;
using System.IO;
using System.Linq;
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

        private const string ScriptsLibraryName = "HyperionScripts";

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

                string[] arguments = { "build", managedProjectFile, "--configuration", "Release", "--disable-build-servers", "--nologo" };

                int exitCode = await RunDotNetAsync(managedProjectFile, arguments, null, line =>
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

        public static bool HasScripts(string projectFilePath)
        {
            string scriptsDirectory = Path.Combine(ProjectLayout.GetContentDirectory(projectFilePath), "Scripts");

            return Directory.Exists(scriptsDirectory) && Directory.EnumerateFiles(scriptsDirectory, "*.cs", SearchOption.AllDirectories).Any();
        }

        public bool BuildPackage(string projectFilePath, string binariesDirectory, string outputDirectory)
        {
            if (!CookPackage(projectFilePath, outputDirectory))
            {
                return false;
            }

            bool isBuilt = FindProjectFile(projectFilePath) == null && !HasScripts(projectFilePath)
                ? NativeGameBuildService.Instance.BuildPackage(projectFilePath, outputDirectory, scriptsLinkInputsFile: null)
                : PublishAot(projectFilePath, binariesDirectory, outputDirectory);

            if (isBuilt)
            {
                StripDebugInfo(outputDirectory);
            }

            return isBuilt;
        }

        private static void StripDebugInfo(string outputDirectory)
        {
            List<string> binaries = Directory.GetFiles(outputDirectory).Where(HasEmbeddedDebugInfo).ToList();

            if (binaries.Count == 0)
            {
                return;
            }

            string? stripTool = GameBuildPlatform.Current.FindStripTool();

            if (stripTool == null)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "No strip or llvm-strip found");

                return;
            }

            foreach (string binary in binaries)
            {
                long sizeBefore = new FileInfo(binary).Length;

                int exitCode = BuildProcess.RunAsync(BuildProcess.CreateStartInfo(stripTool, outputDirectory, new[] { "--strip-unneeded", binary }), BuildLogChannel, _ => { }).GetAwaiter().GetResult();

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Warning, "Stripping {0} failed (exit code {1})", Path.GetFileName(binary), exitCode);

                    continue;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Debug Info Stripped {0}: {1} MB -> {2} MB",
                    Path.GetFileName(binary),
                    sizeBefore / (1024 * 1024),
                    new FileInfo(binary).Length / (1024 * 1024));
            }
        }

        private static bool HasEmbeddedDebugInfo(string path)
        {
            try
            {
                using FileStream stream = File.OpenRead(path);

                Span<byte> magic = stackalloc byte[4];

                if (stream.Read(magic) != magic.Length)
                {
                    return false;
                }

                if (magic[0] == 0x7F && magic[1] == (byte)'E' && magic[2] == (byte)'L' && magic[3] == (byte)'F')
                {
                    return true;
                }

                if (magic[0] != (byte)'M' || magic[1] != (byte)'Z')
                {
                    return false;
                }

                stream.Position = 0;

                using PEReader reader = new PEReader(stream);

                return !reader.HasMetadata
                    && reader.PEHeaders.SectionHeaders.Any(section => section.Name.StartsWith(".debug_") || section.Name.StartsWith("/"));
            }
            catch (Exception ex) when (ex is IOException || ex is BadImageFormatException || ex is UnauthorizedAccessException)
            {
                return false;
            }
        }

        private static bool CookPackage(string projectFilePath, string outputDirectory)
        {
            GameBuildPlatform platform = GameBuildPlatform.Current;

            string editorDirectory = AppContext.BaseDirectory;
            string cookExecutable = Path.Combine(editorDirectory, platform.GetExecutableFileName("BlobStorageCookCommandlet"));

            if (!File.Exists(cookExecutable))
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "The cook commandlet is missing ({0})", cookExecutable);

                ShowMessage(MessageBox.Critical("Cook failed", $"The cook commandlet is missing ({cookExecutable}).").Button("OK", () => { }));

                return false;
            }

            string projectDirectory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;
            string cookCacheDirectory = Path.Combine(projectDirectory, "Build", "PackageCook");
            Directory.CreateDirectory(cookCacheDirectory);

            string[] arguments =
            {
                "--project=" + projectDirectory,
                "--out-cache=" + Path.Combine(outputDirectory, "Cache"),
                "--out-content=" + Path.Combine(outputDirectory, "Content"),
                "--out-engine-content=" + Path.Combine(outputDirectory, "Content", "Engine"),
                "--shader-target=host",
                "--target-platform=" + platform.CookTargetPlatform,
                "--target-config=" + Path.Combine(editorDirectory, "Config", "EngineConfig.json"),
                "--cachedir=" + cookCacheDirectory
            };

            Logger.Log(BuildLogChannel, LogLevel.Info, "Cooking {0} into {1}", projectDirectory, outputDirectory);

            bool isComplete = false;
            string? firstError = null;

            int exitCode = BuildProcess.RunAsync(BuildProcess.CreateStartInfo(cookExecutable, editorDirectory, arguments), BuildLogChannel, line =>
            {
                if (line.Contains("Blob storage cook complete."))
                {
                    isComplete = true;
                }
                else if (firstError == null && line.Contains("[Error]"))
                {
                    firstError = line;
                }
            }).GetAwaiter().GetResult();

            // the commandlet can crash shutting down after a finished cook, so its log decides
            if (!isComplete)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Cook failed (exit code {0})", exitCode);

                ShowMessage(MessageBox.Critical("Cook failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                return false;
            }

            return true;
        }

        private bool PublishAot(string projectFilePath, string binariesDirectory, string outputDirectory)
        {
            string projectDirectory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;

            HyperionSdk? shippingSdk = HyperionSdk.Find(shipping: true);

            if (shippingSdk == null)
            {
                string message = $"Hyperion SDK is invalid in {HyperionSdk.ExpectedShippingDirectory}";

                Logger.Log(BuildLogChannel, LogLevel.Error, "{0}", message);

                ShowMessage(MessageBox.Critical("Shipping SDK not found", message).Button("OK", () => { }));

                return false;
            }
            string? managedProjectFile = FindProjectFile(projectFilePath);
            bool isScriptsLibrary = false;

            if (managedProjectFile == null)
            {
                if (!HasScripts(projectFilePath))
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "No C# project or scripts found for {0}", projectFilePath);

                    return false;
                }

                string? sdkDirectory = FindManagedSdkDirectory();

                if (sdkDirectory == null)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "No C# SDK (Source/Engine/DotNET/Sdk) found near {0}", AppContext.BaseDirectory);

                    return false;
                }

                isScriptsLibrary = Directory.Exists(Path.Combine(projectDirectory, "Source", "Game"));

                string cmakeListsFile = Path.Combine(projectDirectory, "Source", "CMakeLists.txt");

                if (isScriptsLibrary && (!File.Exists(cmakeListsFile) || !File.ReadAllText(cmakeListsFile).Contains("hyperion_link_scripts")))
                {
                    string message = $"Source/CMakeLists.txt must call hyperion_link_scripts({ProjectLayout.GetModuleName(projectFilePath)}) after the launcher's target_link_libraries to link the scripts.";

                    Logger.Log(BuildLogChannel, LogLevel.Error, "{0}", message);

                    ShowMessage(MessageBox.Critical("Scripts can't be linked", message).Button("OK", () => { }));

                    return false;
                }

                string projectName = ProjectLayout.GetModuleName(projectFilePath);
                string scriptsProjectDirectory = Path.GetFullPath(Path.Combine(binariesDirectory, "..", "AotIntermediate", "ScriptsProject"));
                Directory.CreateDirectory(scriptsProjectDirectory);

                managedProjectFile = Path.Combine(scriptsProjectDirectory, (isScriptsLibrary ? ScriptsLibraryName : projectName) + ".csproj");

                File.WriteAllText(managedProjectFile,
                    "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                    + "  <PropertyGroup>\n"
                    + "    <HyperionProjectDir>" + projectDirectory.Replace('\\', '/') + "/</HyperionProjectDir>\n"
                    + "    <HyperionBinDir>" + HyperionSdk.EnsureTrailingSeparator(shippingSdk.RuntimeDirectory).Replace('\\', '/') + "</HyperionBinDir>\n"
                    + (isScriptsLibrary
                        ? "    <OutputType>Library</OutputType>\n    <NativeLib>Static</NativeLib>\n    <HyperionHostedEntry>true</HyperionHostedEntry>\n"
                        : "    <OutputType>Exe</OutputType>\n    <HyperionScriptsProgram>true</HyperionScriptsProgram>\n")
                    + "  </PropertyGroup>\n"
                    + "  <Import Project=\"" + Path.Combine(sdkDirectory, "HyperionScripts.props").Replace('\\', '/') + "\" />\n"
                    + "</Project>\n");
            }

            string? gameBindingsDirectory = null;

            if (isScriptsLibrary)
            {
                // the game's build runs CodeGenTool, which writes the C# of its classes that the scripts may use.
                if (!NativeGameBuildService.Instance.BuildAsync(projectFilePath).GetAwaiter().GetResult())
                {
                    return false;
                }

                gameBindingsDirectory = Path.Combine(
                    projectDirectory,
                    "Build",
                    GameBuildPlatform.Current.Name,
                    "Generated",
                    "CSharp");
            }

            string? toolchainError = GameBuildPlatform.Current.FindNativeAotToolchainError(out string? toolchainPath);

            if (toolchainError != null)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "{0}", toolchainError);

                ShowMessage(MessageBox.Critical("NativeAOT toolchain required", toolchainError).Button("OK", () => { }));

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                string runtimeIdentifier = GameBuildPlatform.Current.RuntimeIdentifier;
                string intermediateDirectory = Path.GetFullPath(Path.Combine(binariesDirectory, "..", "AotIntermediate", runtimeIdentifier));
                string publishDirectory = Path.Combine(intermediateDirectory, "publish");
                string scriptsLinkInputsFile = Path.Combine(intermediateDirectory, "ScriptsLinkInputs.txt");

                if (Directory.Exists(publishDirectory))
                {
                    Directory.Delete(publishDirectory, recursive: true);
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Publishing {0} with NativeAOT for {1}", managedProjectFile, runtimeIdentifier);

                List<string> arguments = [
                    "publish", managedProjectFile,
                    "--configuration", "Release",
                    "--runtime", runtimeIdentifier,
                    "--output", publishDirectory,
                    "--disable-build-servers", "--nologo",
                    "-p:PublishAot=true",
                    "-p:HyperionOutputToBinDir=false",
                    "-p:HyperionIncludeScripts=true",
                    "-p:HyperionProjectContentDir=" + ProjectLayout.GetContentDirectory(projectFilePath) + Path.DirectorySeparatorChar,
                    "-p:HyperionWriteGlobalConfig=false",
                    "-p:HyperionStaticLinkInputsFile=" + scriptsLinkInputsFile,
                    "-p:OutputPath=" + Path.Combine(intermediateDirectory, "bin") + Path.DirectorySeparatorChar,
                    "-p:IntermediateOutputPath=" + Path.Combine(intermediateDirectory, "obj") + Path.DirectorySeparatorChar
                ];

                if (gameBindingsDirectory != null && Directory.Exists(gameBindingsDirectory))
                {
                    arguments.Add("-p:HyperionGameBindingsDir=" + gameBindingsDirectory + Path.DirectorySeparatorChar);
                }

                // a C# game's executable is linked by NativeAOT, so it links the engine's archives too (a C++ launcher gets them from CMake)
                arguments.AddRange(shippingSdk.GetStaticEnginePublishProperties(intermediateDirectory, linksExecutable: !isScriptsLibrary));

                string? firstError = null;

                int exitCode = RunDotNetAsync(managedProjectFile, arguments.ToArray(), toolchainPath, line =>
                {
                    if (firstError == null && ErrorLineRegex.IsMatch(line))
                    {
                        firstError = line;
                    }
                }).GetAwaiter().GetResult();

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "NativeAOT publish failed (exit code {0})", exitCode);

                    ShowMessage(MessageBox.Critical("NativeAOT publish failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                    return false;
                }

                Directory.CreateDirectory(outputDirectory);

                if (isScriptsLibrary)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Info, "Linking the scripts into the game");

                    return NativeGameBuildService.Instance.BuildPackage(projectFilePath, outputDirectory, scriptsLinkInputsFile);
                }

                foreach (string file in Directory.GetFiles(publishDirectory))
                {
                    if (IsDebugFile(file))
                    {
                        continue;
                    }

                    File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
                }

                foreach (string file in shippingSdk.GetRuntimeLibraries())
                {
                    File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "NativeAOT publish succeeded");

                return true;
            }
            catch (System.ComponentModel.Win32Exception)
            {
                ShowMessage(MessageBox.Critical(".NET SDK required", "dotnet was not found. Install the .NET 10 SDK to package a C# game.").Button("OK", () => { }));

                return false;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "NativeAOT publish failed: {0}", ex.Message);

                return false;
            }
            finally
            {
                Interlocked.Exchange(ref _isBuilding, 0);
            }
        }

        private static string? FindManagedSdkDirectory()
        {
            string? directory = AppContext.BaseDirectory;

            for (int depth = 0; depth < 5 && directory != null; depth++)
            {
                string candidate = Path.Combine(directory, "Source", "Engine", "DotNET", "Sdk");

                if (File.Exists(Path.Combine(candidate, "HyperionScripts.props")))
                {
                    return candidate;
                }

                directory = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(directory));
            }

            return null;
        }

        private static bool IsDebugFile(string path)
        {
            string extension = Path.GetExtension(path);

            return extension == ".pdb" || extension == ".dbg" || extension == ".dSYM";
        }

        private static Task<int> RunDotNetAsync(string managedProjectFile, string[] arguments, string? extraPath, Action<string> onOutputLine)
        {
            ProcessStartInfo startInfo = BuildProcess.CreateStartInfo(GameBuildPlatform.Current.FindDotNet(), Path.GetDirectoryName(managedProjectFile)!, arguments);

            startInfo.Environment.Remove("MSBuildSdksPath");

            if (extraPath != null)
            {
                startInfo.Environment["PATH"] = extraPath + Path.PathSeparator + startInfo.Environment["PATH"];
            }

            return BuildProcess.RunAsync(startInfo, BuildLogChannel, onOutputLine);
        }

        private static void ShowMessage(MessageBox messageBox)
        {
            Dispatcher.UIThread.Post(() => messageBox.Show());
        }
    }
}
