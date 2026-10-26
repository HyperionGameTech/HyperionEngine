using System.Diagnostics;
using System.Text.Json;
using Hyperion.Editor.Services;

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

public sealed class GameProject
{
    private const string ContentDirFromBinaries = "../../Content/";

    private static readonly JsonSerializerOptions s_reportJsonOptions = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

    public string Name { get; }
    public GameProjectLanguage Language { get; }
    public string Directory { get; }

    public string ClassName => Name + "Game";
    public string SourceDirectory => Path.Combine(Directory, "Source");
    public string ContentDirectory => Path.Combine(Directory, "Content");
    public string BinariesDirectory => Path.Combine(Directory, "Binaries", HarnessEnvironment.PlatformName);
    public string NativeBuildDirectory => NativeGameBuild.GetBuildDirectory(Directory, GameBuildPlatform.Current, isPackage: false);
    public string LogsDirectory => Path.Combine(Directory, "Logs");
    public string ExecutablePath => Path.Combine(BinariesDirectory, Name + HarnessEnvironment.ExecutableExtension);
    public string ProjectFilePath => Path.Combine(Directory, Name + ".hypproject");

    public string NativeModulePath => Path.Combine(
        BinariesDirectory,
        OperatingSystem.IsWindows()
            ? $"{Name}Game.dll" : OperatingSystem.IsMacOS()
                ? $"lib{Name}Game.dylib" : $"lib{Name}Game.so");

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

    public static GameProject Create(string name, GameProjectLanguage language, string? fixtureName = null, string? directoryName = null, string? engineBinDir = null)
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
            project.WriteManagedProjectProps(engineBinDir ?? HarnessEnvironment.EngineBinDir);
        }

        project.WriteProjectFile();

        if (fixtureName != null)
        {
            project.ApplyFixture(fixtureName);
        }

        return project;
    }

    public static GameProject CreateInEditor(string name, GameProjectLanguage language)
    {
        string directory = Path.Combine(HarnessEnvironment.WorkDir, name);

        if (System.IO.Directory.Exists(directory))
        {
            System.IO.Directory.Delete(directory, recursive: true);
        }

        GameProject project = new GameProject(name, language, directory);

        GameAssert.Succeeded(EditorTestHost.CreateProject(project));

        return project;
    }

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

    public string PackageBuildDirectory => NativeGameBuild.GetBuildDirectory(Directory, GameBuildPlatform.Current, isPackage: true);

    public ProcessResult BuildPackage(string packageDirectory, string startupWorldName)
    {
        List<string> cmakeArguments = NativeGameBuild.GetPackageArguments(packageDirectory, scriptsLinkInputsFile: null);
        cmakeArguments.Add("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON");

        ProcessResult build = BuildNative(isPackage: true, cmakeArguments);

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

    public ProcessResult PublishManagedPackage(string packageDirectory)
    {
        HyperionSdk shippingSdk = HyperionSdk.Load(HarnessEnvironment.ShippingSdkDir, isShipping: true)
            ?? throw new InvalidOperationException($"No shipping SDK at {HarnessEnvironment.ShippingSdkDir}; build it with BuildHyperion sdk or set HYPERION_SHIPPING_SDK_DIR");

        string intermediateDirectory = Path.Combine(Directory, "Build", "AotPackage", HarnessEnvironment.RuntimeIdentifier);
        string publishDirectory = Path.Combine(intermediateDirectory, "publish");

        if (System.IO.Directory.Exists(publishDirectory))
        {
            System.IO.Directory.Delete(publishDirectory, recursive: true);
        }

        List<string> arguments = new List<string>
        {
            "publish", Path.Combine(SourceDirectory, Name + ".csproj"),
            "--configuration", "Release",
            "--runtime", HarnessEnvironment.RuntimeIdentifier,
            "--output", publishDirectory,
            "--disable-build-servers", "--nologo",
            "-p:PublishAot=true",
            "-p:HyperionOutputToBinDir=false",
            "-p:HyperionIncludeScripts=true",
            "-p:HyperionProjectContentDir=" + ContentDirectory + Path.DirectorySeparatorChar,
            "-p:HyperionWriteGlobalConfig=false",
            "-p:OutputPath=" + Path.Combine(intermediateDirectory, "bin") + Path.DirectorySeparatorChar,
            "-p:IntermediateOutputPath=" + Path.Combine(intermediateDirectory, "obj") + Path.DirectorySeparatorChar
        };

        arguments.AddRange(shippingSdk.GetStaticEnginePublishProperties(intermediateDirectory, linksExecutable: true));

        // NativeAOT finds the MSVC linker through vswhere (WindowsGameBuildPlatform.FindNativeAotToolchainError())
        Dictionary<string, string> environment = new Dictionary<string, string>();

        if (OperatingSystem.IsWindows())
        {
            string installerDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Microsoft Visual Studio", "Installer");
            environment["PATH"] = installerDirectory + Path.PathSeparator + Environment.GetEnvironmentVariable("PATH");
        }

        ProcessResult publish = ProcessRunner.Run(
            $"Publishing {Name} with NativeAOT",
            "dotnet",
            arguments,
            SourceDirectory,
            Path.Combine(LogsDirectory, "publish-aot.log"),
            HarnessEnvironment.BuildTimeout,
            environment);

        if (!publish.Succeeded)
        {
            return publish;
        }

        System.IO.Directory.CreateDirectory(packageDirectory);

        foreach (string file in System.IO.Directory.GetFiles(publishDirectory).Where(file => Path.GetExtension(file) is not ".pdb" and not ".dbg"))
        {
            File.Copy(file, Path.Combine(packageDirectory, Path.GetFileName(file)), overwrite: true);
        }

        foreach (string file in shippingSdk.GetRuntimeLibraries())
        {
            File.Copy(file, Path.Combine(packageDirectory, Path.GetFileName(file)), overwrite: true);
        }

        string configDirectory = Path.Combine(packageDirectory, "Config");
        System.IO.Directory.CreateDirectory(configDirectory);

        string sourceConfigDirectory = Path.Combine(shippingSdk.RuntimeDirectory, "Config");

        if (!System.IO.Directory.Exists(sourceConfigDirectory))
        {
            sourceConfigDirectory = Path.Combine(HarnessEnvironment.EngineBinDir, "Config");
        }

        foreach (string file in System.IO.Directory.GetFiles(sourceConfigDirectory).Where(file => Path.GetFileName(file) != "GlobalConfig.json"))
        {
            File.Copy(file, Path.Combine(configDirectory, Path.GetFileName(file)), overwrite: true);
        }

        File.WriteAllText(Path.Combine(configDirectory, "GlobalConfig.json"),
            "{\n  \"App\": {\n"
            + $"    \"Name\": \"{Name}\",\n"
            + "    \"Args\": \"--basedir=./ --singleplayer\"\n"
            + "  }\n}\n");

        return publish;
    }

    public GameRunResult RunPackaged(string packageDirectory, string phase)
    {
        return RunGame(Path.Combine(packageDirectory, Name + HarnessEnvironment.ExecutableExtension), packageDirectory, phase);
    }

    public ProcessResult Build(params string[] managedBuildArguments)
    {
        return Language == GameProjectLanguage.Managed
            ? BuildManaged(managedBuildArguments)
            : BuildNative();
    }

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

    private string RenderTemplate(string templatePath)
    {
        return File.ReadAllText(templatePath)
            .Replace("@NAME@", Name)
            .Replace("@CLASS@", ClassName)
            .Replace("@CONTENT_DIR@", ContentDirFromBinaries);
    }

    private void WriteProjectFile()
    {
        File.WriteAllText(ProjectFilePath,
            $"EditorProject {Name} {{\n"
            + "    ContentSubfolder = \"Content\"\n"
            + "    EditWorldName = MainWorld\n"
            + "}\n");
    }

    private void WriteManagedProjectProps(string engineBinDir)
    {
        string binDir = engineBinDir.Replace('\\', '/').TrimEnd('/');
        string baseDir = HarnessEnvironment.RepositoryRoot.Replace('\\', '/').TrimEnd('/');

        File.WriteAllText(Path.Combine(SourceDirectory, "Hyperion.Local.props"),
            "<Project>\n  <PropertyGroup>\n"
            + $"    <HyperionBinDir>{binDir}/</HyperionBinDir>\n"
            + $"    <HyperionBaseDir>{baseDir}/</HyperionBaseDir>\n"
            + $"    <HyperionSdkDir>{baseDir}/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
            + "  </PropertyGroup>\n</Project>\n");
    }

    private ProcessResult BuildManaged(string[] extraArguments)
    {
        return ProcessRunner.Run(
            $"Building {Name}",
            "dotnet",
            new[] { "build", Path.Combine(SourceDirectory, Name + ".csproj"), "--configuration", "Release", "--disable-build-servers", "--nologo" }.Concat(extraArguments),
            SourceDirectory,
            Path.Combine(LogsDirectory, "build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    private ProcessResult BuildNative()
    {
        return BuildNative(isPackage: false, Array.Empty<string>());
    }

    /// The script the last native build ran, which configures and builds the game again, as the editor's rebuild does
    public string? NativeBuildScriptPath { get; private set; }

    // the editor's NativeGameBuildService configure + build, with the toolchain it would pick for the SDK
    private ProcessResult BuildNative(bool isPackage, IReadOnlyList<string> cmakeArguments)
    {
        GameBuildPlatform platform = GameBuildPlatform.Current;

        string sdkDirectory = isPackage ? HarnessEnvironment.ShippingSdkDir : HarnessEnvironment.SdkDir;
        HyperionSdk sdk = HyperionSdk.Load(sdkDirectory, isShipping: isPackage)
            ?? throw new InvalidOperationException($"No {(isPackage ? "shipping " : "")}SDK at {sdkDirectory}");

        NativeToolchain toolchain = platform.FindNativeToolchain(sdk.Compiler);

        if (toolchain.Missing.Count > 0)
        {
            throw new InvalidOperationException($"Building a C++ game with the {sdk.Compiler} SDK needs: {string.Join(", ", toolchain.Missing)}");
        }

        string buildDirectory = NativeGameBuild.GetBuildDirectory(Directory, platform, isPackage);
        string scriptPath = NativeGameBuild.WriteBuildScript(platform, toolchain, sdk, Directory, buildDirectory, cmakeArguments);

        if (!isPackage)
        {
            NativeBuildScriptPath = scriptPath;
        }

        ProcessStartInfo startInfo = platform.CreateBuildScriptStartInfo(scriptPath, buildDirectory);

        return ProcessRunner.Run(
            $"Building {Name}",
            startInfo.FileName,
            startInfo.ArgumentList,
            buildDirectory,
            Path.Combine(LogsDirectory, isPackage ? "build-package.log" : "build.log"),
            HarnessEnvironment.BuildTimeout);
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
