using System.Text.Json;

namespace Hyperion.GameProjectTests;

public enum GameProjectLanguage
{
    Managed,
    Native
}

public sealed record GameTestResult(string Name, bool Passed, string? Message, double DurationMs);

public sealed record GameTestReport(string Phase, List<GameTestResult> Tests);

public sealed record GameRunResult(ProcessResult Process, GameTestReport? Report);

public sealed record EditorTestStep(string Build, string Phase, bool Loaded, string? LoadError, GameTestReport? Report);

public sealed record EditorTestReport(List<EditorTestStep> Steps, bool RetiredBeforeRelease, GameTestReport? Release, bool Unloaded, bool Saved, string? SaveError);

public sealed record EditorTestRunResult(ProcessResult Process, EditorTestReport? Report);

/// A game project generated from Source/Templates the way the editor's Generate C# / C++ Project does, optionally with
/// a fixture from Tests/GameProjects/Fixtures copied over it
public sealed class GameProject
{
    // the editor's projects keep their content in a Content subfolder, which the templates reach from Binaries/<Platform>/
    private const string ContentDirFromBinaries = "../../Content/";

    private static readonly JsonSerializerOptions s_reportJsonOptions = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

    public string Name { get; }
    public GameProjectLanguage Language { get; }
    public string Directory { get; }

    public string ClassName => Name + "Game";
    public string SourceDirectory => Path.Combine(Directory, "Source");
    public string ContentDirectory => Path.Combine(Directory, "Content");
    public string BinariesDirectory => Path.Combine(Directory, "Binaries", HarnessEnvironment.PlatformName);
    public string NativeBuildDirectory => Path.Combine(Directory, "Build", HarnessEnvironment.PlatformName);
    public string LogsDirectory => Path.Combine(Directory, "Logs");
    public string ExecutablePath => Path.Combine(BinariesDirectory, Name + HarnessEnvironment.ExecutableExtension);
    public string ProjectFilePath => Path.Combine(Directory, Name + ".hypproject");

    public string NativeModulePath => Path.Combine(BinariesDirectory,
        OperatingSystem.IsWindows() ? $"{Name}Game.dll"
        : OperatingSystem.IsMacOS() ? $"lib{Name}Game.dylib"
        : $"lib{Name}Game.so");

    // what a C++ game's CodeGen run writes, and the C# bindings assembly its build compiles from it
    public string NativeGeneratedDirectory => Path.Combine(NativeBuildDirectory, "Generated");
    public string BindingsAssemblyPath => Path.Combine(BinariesDirectory, $"{Name}Game.Bindings.dll");
    public string EngineVersionStampPath => Path.Combine(BinariesDirectory, $"{Name}Game.EngineVersion.txt");
    public string ScriptsDirectory => Path.Combine(ContentDirectory, "Scripts");

    private GameProject(string name, GameProjectLanguage language, string directory)
    {
        Name = name;
        Language = language;
        Directory = directory;
    }

    /// <param name="directoryName">The project's folder under the work directory, when it isn't named after the project</param>
    public static GameProject Create(string name, GameProjectLanguage language, string? fixtureName = null, string? directoryName = null)
    {
        string directory = Path.Combine(HarnessEnvironment.WorkDir, directoryName ?? name);

        if (System.IO.Directory.Exists(directory))
        {
            System.IO.Directory.Delete(directory, recursive: true);
        }

        GameProject project = new GameProject(name, language, directory);

        System.IO.Directory.CreateDirectory(project.ContentDirectory);

        foreach ((string templatePath, string outputPath) in project.GetTemplateFiles())
        {
            System.IO.Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
            File.WriteAllText(outputPath, project.RenderTemplate(templatePath));
        }

        if (language == GameProjectLanguage.Managed)
        {
            project.WriteManagedProjectProps();
        }

        project.WriteProjectFile();

        if (fixtureName != null)
        {
            project.ApplyFixture(fixtureName);
        }

        return project;
    }

    /// Copies a fixture over the project, replacing the files it has
    public void ApplyFixture(string fixtureName)
    {
        string fixtureDirectory = Path.Combine(HarnessEnvironment.FixturesDir, fixtureName);

        CopyDirectory(fixtureDirectory, Directory);

        // copies keep the fixture's timestamps, which are older than the last build's outputs
        foreach (string sourcePath in System.IO.Directory.EnumerateFiles(fixtureDirectory, "*", SearchOption.AllDirectories))
        {
            File.SetLastWriteTimeUtc(Path.Combine(Directory, Path.GetRelativePath(fixtureDirectory, sourcePath)), DateTime.UtcNow);
        }
    }

    /// Copies the game module as built (with its bindings and debug info) aside, so a later build doesn't replace it
    public string SaveNativeModuleBuild(string label)
    {
        string buildDirectory = Path.Combine(Directory, "Build", "ModuleBuilds", label);

        if (System.IO.Directory.Exists(buildDirectory))
        {
            System.IO.Directory.Delete(buildDirectory, recursive: true);
        }

        System.IO.Directory.CreateDirectory(buildDirectory);

        string moduleStem = Path.GetFileNameWithoutExtension(NativeModulePath);

        foreach (string path in System.IO.Directory.EnumerateFiles(BinariesDirectory, moduleStem + ".*"))
        {
            File.Copy(path, Path.Combine(buildDirectory, Path.GetFileName(path)));
        }

        foreach (string pattern in new[] { ClassName + ".Bindings.*", ClassName + ".EngineVersion.*" })
        {
            foreach (string path in System.IO.Directory.EnumerateFiles(BinariesDirectory, pattern))
            {
                File.Copy(path, Path.Combine(buildDirectory, Path.GetFileName(path)), overwrite: true);
            }
        }

        return buildDirectory;
    }

    public EditorTestRunResult RunEditorTests(
        string label,
        IReadOnlyList<string> buildDirectories,
        IReadOnlyList<string> phases,
        string testClassName,
        string releasePhase = "",
        bool saveProject = false,
        string rebuildScript = "")
    {
        string resultsPath = Path.Combine(LogsDirectory, $"results-{label}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        System.IO.Directory.CreateDirectory(LogsDirectory);

        List<string> arguments = new List<string>
        {
            "--project=" + Directory,
            "--builds=" + string.Join(',', buildDirectories),
            "--phases=" + string.Join(',', phases),
            "--test-class=" + testClassName,
            "--results=" + resultsPath
        };

        if (releasePhase.Length > 0)
        {
            arguments.Add("--release-phase=" + releasePhase);
        }

        if (saveProject)
        {
            arguments.Add("--save-project=true");
        }

        if (rebuildScript.Length > 0)
        {
            arguments.Add("--rebuild-script=" + rebuildScript);
        }

        ProcessResult process = ProcessRunner.Run(
            $"{Name} in the editor ({label})",
            HarnessEnvironment.GetCommandletPath("EditorGameTestCommandlet"),
            arguments,
            HarnessEnvironment.EngineBinDir,
            Path.Combine(LogsDirectory, $"{label}.log"),
            HarnessEnvironment.RunTimeout);

        EditorTestReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<EditorTestReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new EditorTestRunResult(process, report);
    }

    /// Cooks the project into a package directory with the same arguments as the editor's Package Game (CookPackage() in
    /// Source/Editor/Managed/Services/ManagedGameBuildService.cs), less the shaders, which a headless game doesn't use
    public ProcessResult Cook(string packageDirectory)
    {
        string cookCacheDirectory = Path.Combine(Directory, "Build", "PackageCook");
        System.IO.Directory.CreateDirectory(cookCacheDirectory);

        string cookTargetPlatform = OperatingSystem.IsWindows() ? "windows" : OperatingSystem.IsMacOS() ? "mac" : "linux";

        return ProcessRunner.Run(
            $"Cooking {Name}",
            HarnessEnvironment.GetCommandletPath("BlobStorageCookCommandlet"),
            new[]
            {
                "--project=" + Directory,
                "--out-cache=" + Path.Combine(packageDirectory, "Cache"),
                "--out-content=" + Path.Combine(packageDirectory, "Content"),
                "--out-engine-content=" + Path.Combine(packageDirectory, "Content", "Engine"),
                "--target-platform=" + cookTargetPlatform,
                "--target-config=" + Path.Combine(HarnessEnvironment.EngineBinDir, "Config", "EngineConfig.json"),
                "--cachedir=" + cookCacheDirectory
            },
            HarnessEnvironment.EngineBinDir,
            Path.Combine(LogsDirectory, "cook.log"),
            HarnessEnvironment.BuildTimeout);
    }

    /// Builds the game into a package directory as Package Game does (NativeGameBuildService.BuildPackage()), and writes the
    /// config StagePackagedGame() in Source/Editor/Commands/EditorCommandsNativeProject.cpp writes
    public ProcessResult BuildPackage(string packageDirectory, string startupWorldName)
    {
        ProcessResult build = BuildNative(
            Path.Combine(Directory, "Build", HarnessEnvironment.PlatformName + "Package"),
            "package",
            "-DHYPERION_PACKAGE=ON",
            "-DGAME_OUTPUT_DIR=" + packageDirectory.Replace('\\', '/'),
            "-DHYPERION_SCRIPTS_LINK_INPUTS=");

        if (!build.Succeeded)
        {
            return build;
        }

        string configDirectory = Path.Combine(packageDirectory, "Config");
        System.IO.Directory.CreateDirectory(configDirectory);

        File.WriteAllText(Path.Combine(configDirectory, "GlobalConfig.json"),
            "{\n  \"App\": {\n"
            + $"    \"Name\": \"{Name}\",\n"
            + $"    \"Args\": \"--basedir=./ --startupworld={startupWorldName} --singleplayer\"\n"
            + "  }\n}\n");

        return build;
    }

    /// Launches the packaged game headless; it runs the tests for <paramref name="phase"/> and quits
    public GameRunResult RunPackaged(string packageDirectory, string phase)
    {
        return RunGame(Path.Combine(packageDirectory, Name + HarnessEnvironment.ExecutableExtension), packageDirectory, phase);
    }

    public ProcessResult Build()
    {
        return Language == GameProjectLanguage.Managed ? BuildManaged() : BuildNative();
    }

    /// Builds Content/Scripts as the editor's script compiler does: a module of its own, against the engine assemblies and the
    /// game's (a C++ game's bindings assembly, or a C# game's own assembly)
    public ProcessResult BuildScripts(string referenceAssemblyPath)
    {
        string scriptsProjectDirectory = Path.Combine(Directory, "Build", "Scripts");
        System.IO.Directory.CreateDirectory(scriptsProjectDirectory);

        string scriptsProjectPath = Path.Combine(scriptsProjectDirectory, "Scripts.csproj");

        File.WriteAllText(scriptsProjectPath, RenderScriptsProject(referenceAssemblyPath, Path.Combine(scriptsProjectDirectory, "bin")));

        return ProcessRunner.Run(
            $"Building {Name}'s scripts",
            "dotnet",
            new[] { "build", scriptsProjectPath, "--disable-build-servers", "--nologo" },
            scriptsProjectDirectory,
            Path.Combine(LogsDirectory, "scripts-build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    // mirrors GenerateCSharpProjectFile() in Source/Engine/DotNET/Scripting/CSharpScriptCompiler.cs
    private string RenderScriptsProject(string referenceAssemblyPath, string outputDirectory)
    {
        string Reference(string name, string path) =>
            $"    <Reference Include=\"{name}\">\n      <HintPath>{path}</HintPath>\n      <Private>false</Private>\n    </Reference>\n";

        string EngineAssembly(string name) => Path.Combine(HarnessEnvironment.EngineBinDir, name + ".dll");

        string generatorPath = EngineAssembly("Hyperion.NET.Generators");
        string scripts = string.Join("", System.IO.Directory.EnumerateFiles(ScriptsDirectory, "*.cs", SearchOption.AllDirectories)
            .Select(script => $"    <Compile Include=\"{script}\" />\n"));

        return "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
            + "  <PropertyGroup>\n"
            + "    <OutputType>Library</OutputType>\n"
            + "    <TargetFramework>net10.0</TargetFramework>\n"
            + "    <ImplicitUsings>enable</ImplicitUsings>\n"
            + "    <Nullable>enable</Nullable>\n"
            + "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n"
            + "    <EnableDynamicLoading>true</EnableDynamicLoading>\n"
            + "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n"
            + $"    <AssemblyName>{Name}.Scripts</AssemblyName>\n"
            + $"    <OutputPath>{outputDirectory}/</OutputPath>\n"
            + "  </PropertyGroup>\n"
            + "  <ItemGroup>\n"
            + Reference("Hyperion.NET.Shared", EngineAssembly("Hyperion.NET.Shared"))
            + Reference("Hyperion.NET.Runtime", EngineAssembly("Hyperion.NET.Runtime"))
            + Reference(Path.GetFileNameWithoutExtension(referenceAssemblyPath), referenceAssemblyPath)
            + (File.Exists(generatorPath) ? $"    <Analyzer Include=\"{generatorPath}\" />\n" : "")
            + scripts
            + "  </ItemGroup>\n"
            + "</Project>\n";
    }

    /// Launches the built game headless; the test game runs the tests for <paramref name="phase"/> and quits
    public GameRunResult Run(string phase)
    {
        return RunGame(ExecutablePath, BinariesDirectory, phase);
    }

    private GameRunResult RunGame(string executablePath, string workingDirectory, string phase)
    {
        string resultsPath = Path.Combine(LogsDirectory, $"results-{phase}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        System.IO.Directory.CreateDirectory(LogsDirectory);

        Dictionary<string, string> environment = new Dictionary<string, string>
        {
            ["HYP_GAME_TEST_PHASE"] = phase,
            ["HYP_GAME_TEST_RESULTS"] = resultsPath
        };

        ProcessResult process = ProcessRunner.Run(
            $"{Name} ({phase} phase)",
            executablePath,
            new[] { "--headless" },
            workingDirectory,
            Path.Combine(LogsDirectory, $"run-{phase}.log"),
            HarnessEnvironment.RunTimeout,
            environment);

        GameTestReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<GameTestReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new GameRunResult(process, report);
    }

    // mirrors GetGameProjectFiles() in Source/Editor/EditorNativeModule.cpp
    private IEnumerable<(string TemplatePath, string OutputPath)> GetTemplateFiles()
    {
        if (Language == GameProjectLanguage.Managed)
        {
            string templateDir = Path.Combine(HarnessEnvironment.TemplatesDir, "ManagedGame");

            yield return (Path.Combine(templateDir, "Game.csproj.in"), Path.Combine(SourceDirectory, Name + ".csproj"));
            yield return (Path.Combine(templateDir, "Game.cs.in"), Path.Combine(SourceDirectory, ClassName + ".cs"));
            yield return (Path.Combine(templateDir, "Program.cs.in"), Path.Combine(SourceDirectory, "Program.cs"));
            yield return (Path.Combine(templateDir, "Game.slnx.in"), Path.Combine(Directory, Name + ".slnx"));
        }
        else
        {
            string templateDir = Path.Combine(HarnessEnvironment.TemplatesDir, "NativeGame");

            yield return (Path.Combine(templateDir, "CMakeLists.txt.in"), Path.Combine(SourceDirectory, "CMakeLists.txt"));
            yield return (Path.Combine(templateDir, "Game", "Game.hpp.in"), Path.Combine(SourceDirectory, "Game", ClassName + ".hpp"));
            yield return (Path.Combine(templateDir, "Game", "Game.cpp.in"), Path.Combine(SourceDirectory, "Game", ClassName + ".cpp"));
            yield return (Path.Combine(templateDir, "Launcher", "main.cpp.in"), Path.Combine(SourceDirectory, "Launcher", "main.cpp"));
        }
    }

    // mirrors RenderGameProjectTemplate() in Source/Editor/EditorNativeModule.cpp
    private string RenderTemplate(string templatePath)
    {
        return File.ReadAllText(templatePath)
            .Replace("@NAME@", Name)
            .Replace("@CLASS@", ClassName)
            .Replace("@CONTENT_DIR@", ContentDirFromBinaries);
    }

    // the editor's project file, with only what differs from a new project's; the editor tools (cook, reload) open the project through it
    private void WriteProjectFile()
    {
        File.WriteAllText(ProjectFilePath,
            $"EditorProject {Name} {{\n"
            + "    ContentSubfolder = \"Content\"\n"
            + "    EditWorldName = MainWorld\n"
            + "}\n");
    }

    // mirrors WriteManagedProjectProps() in Source/Editor/EditorNativeModule.cpp
    private void WriteManagedProjectProps()
    {
        string binDir = HarnessEnvironment.EngineBinDir.Replace('\\', '/').TrimEnd('/');
        string baseDir = HarnessEnvironment.RepositoryRoot.Replace('\\', '/').TrimEnd('/');

        File.WriteAllText(Path.Combine(SourceDirectory, "Hyperion.Local.props"),
            "<Project>\n  <PropertyGroup>\n"
            + $"    <HyperionBinDir>{binDir}/</HyperionBinDir>\n"
            + $"    <HyperionBaseDir>{baseDir}/</HyperionBaseDir>\n"
            + $"    <HyperionSdkDir>{baseDir}/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
            + "  </PropertyGroup>\n</Project>\n");
    }

    // the same command the editor's ManagedGameBuildService runs
    private ProcessResult BuildManaged()
    {
        return ProcessRunner.Run(
            $"Building {Name}",
            "dotnet",
            new[] { "build", Path.Combine(SourceDirectory, Name + ".csproj"), "--configuration", "Release", "--disable-build-servers", "--nologo" },
            SourceDirectory,
            Path.Combine(LogsDirectory, "build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    private ProcessResult BuildNative()
    {
        return BuildNative(NativeBuildDirectory, "");
    }

    /// A script that builds the game again as Build() did, for the editor host to run where the editor rebuilds the game
    public string WriteRebuildScript()
    {
        // RunCMake() left the build in a script that sets up the compiler environment first
        if (HarnessEnvironment.VisualStudioDir != null)
        {
            return Path.Combine(Directory, "Build", "build.bat");
        }

        string command = $"cmake --build \"{NativeBuildDirectory}\" --config Release";
        string scriptPath = Path.Combine(Directory, "Build", OperatingSystem.IsWindows() ? "rebuild.bat" : "rebuild.sh");

        if (OperatingSystem.IsWindows())
        {
            File.WriteAllText(scriptPath, command + "\r\nexit /b %errorlevel%\r\n");
        }
        else
        {
            File.WriteAllText(scriptPath, "#!/bin/sh\n" + command + "\n");
            File.SetUnixFileMode(scriptPath, File.GetUnixFileMode(scriptPath) | UnixFileMode.UserExecute);
        }

        return scriptPath;
    }

    // the same configure + build as the editor's NativeGameBuildService: on Windows inside vcvars64, with the compiler the SDK
    // was built with; elsewhere the compiler comes from CXX, as for any CMake project
    private ProcessResult BuildNative(string buildDirectory, string label, params string[] cmakeArguments)
    {
        string logSuffix = label.Length > 0 ? "-" + label : "";

        List<string> configureArguments = new List<string>
        {
            "-S", SourceDirectory,
            "-B", buildDirectory,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DHYPERION_SDK_DIR=" + HarnessEnvironment.SdkDir.Replace('\\', '/')
        };

        if (HarnessEnvironment.VisualStudioDir != null)
        {
            configureArguments.AddRange(new[] { "-G", "Ninja", "-DCMAKE_CXX_COMPILER=" + FindWindowsCompiler().Replace('\\', '/') });
        }
        else if (FindOnPath("ninja") != null)
        {
            configureArguments.AddRange(new[] { "-G", "Ninja" });
        }

        configureArguments.AddRange(cmakeArguments);

        ProcessResult configure = RunCMake(
            $"Configuring {Name}",
            configureArguments,
            Path.Combine(LogsDirectory, $"configure{logSuffix}.log"));

        if (!configure.Succeeded)
        {
            return configure;
        }

        return RunCMake(
            $"Building {Name}",
            new[] { "--build", buildDirectory, "--config", "Release", "--parallel", Environment.ProcessorCount.ToString() },
            Path.Combine(LogsDirectory, $"build{logSuffix}.log"));
    }

    // mirrors WindowsGameBuildPlatform.FindNativeToolchain() in Source/Editor/Managed/Services/Build
    private static string FindWindowsCompiler()
    {
        if (HarnessEnvironment.SdkCompiler == "cl")
        {
            return "cl.exe";
        }

        string clangCl = Path.Combine(HarnessEnvironment.VisualStudioDir!, "VC", "Tools", "Llvm", "x64", "bin", "clang-cl.exe");

        return File.Exists(clangCl) ? clangCl : "clang-cl.exe";
    }

    private ProcessResult RunCMake(string description, IReadOnlyList<string> arguments, string logPath)
    {
        if (HarnessEnvironment.VisualStudioDir == null)
        {
            return ProcessRunner.Run(description, "cmake", arguments, Directory, logPath, HarnessEnvironment.BuildTimeout);
        }

        string vcVarsPath = Path.Combine(HarnessEnvironment.VisualStudioDir, "VC", "Auxiliary", "Build", "vcvars64.bat");
        string scriptPath = Path.Combine(Directory, "Build", Path.GetFileNameWithoutExtension(logPath) + ".bat");

        System.IO.Directory.CreateDirectory(Path.GetDirectoryName(scriptPath)!);

        File.WriteAllText(scriptPath,
            "@echo off\r\n"
            + $"call \"{vcVarsPath}\" >nul\r\n"
            + "if errorlevel 1 exit /b 1\r\n"
            + "cmake " + string.Join(" ", arguments.Select(argument => "\"" + argument + "\"")) + "\r\n"
            + "exit /b %errorlevel%\r\n");

        return ProcessRunner.Run(description, "cmd.exe", new[] { "/d", "/c", scriptPath }, Directory, logPath, HarnessEnvironment.BuildTimeout);
    }

    private static string? FindOnPath(string program)
    {
        string[] extensions = OperatingSystem.IsWindows() ? new[] { ".exe", ".cmd", ".bat" } : new[] { "" };

        foreach (string directory in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            foreach (string extension in extensions)
            {
                string candidate = Path.Combine(directory, program + extension);

                if (File.Exists(candidate))
                {
                    return candidate;
                }
            }
        }

        return null;
    }

    private static void CopyDirectory(string sourceDirectory, string destinationDirectory)
    {
        if (!System.IO.Directory.Exists(sourceDirectory))
        {
            throw new DirectoryNotFoundException($"Fixture directory {sourceDirectory} does not exist");
        }

        foreach (string sourcePath in System.IO.Directory.EnumerateFiles(sourceDirectory, "*", SearchOption.AllDirectories))
        {
            string destinationPath = Path.Combine(destinationDirectory, Path.GetRelativePath(sourceDirectory, sourcePath));

            System.IO.Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
            File.Copy(sourcePath, destinationPath, overwrite: true);
        }
    }
}
