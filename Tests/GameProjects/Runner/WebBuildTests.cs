using System.Text.Json;
using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// What building a project for the browser needs, as Tools/Scripts/BuildWebGame.ps1 checks it. The editor only builds for the web on Windows
public static class WebBuildEnvironment
{
    public static string ScriptPath => Path.Combine(HarnessEnvironment.RepositoryRoot, "Tools", "Scripts", "BuildWebGame.ps1");

    public static string WebSdkPath => Path.Combine(HarnessEnvironment.RepositoryRoot, "Binaries", "Web", "Release", "Sdk", "CMake", "HyperionWebSdk.cmake");

    public static string WebConfigPath => Path.Combine(HarnessEnvironment.RepositoryRoot, "Config", "EngineConfig.Web.json");

    public static TimeSpan BuildTimeout { get; } = TimeSpan.FromMinutes(45);

    /// Why a C++ game can't be built for the web here, or null when it can
    public static string? MissingForNative { get; } = FindMissing(managed: false);

    /// Why a C# game can't be built for the web here, or null when it can
    public static string? MissingForManaged { get; } = FindMissing(managed: true);

    private static string? FindMissing(bool managed)
    {
        if (!OperatingSystem.IsWindows())
        {
            return "building for the web is Windows only";
        }

        List<string> missing = new List<string>();

        string? emsdk = Environment.GetEnvironmentVariable("EMSDK");

        if (string.IsNullOrEmpty(emsdk) || !File.Exists(Path.Combine(emsdk, "emsdk_env.bat")))
        {
            missing.Add("emsdk (EMSDK)");
        }

        if (!File.Exists(WebSdkPath))
        {
            missing.Add("the engine's web build (BuildHyperion web)");
        }

        foreach (string tool in new[] { "ninja", "pwsh" })
        {
            if (!IsOnPath(tool))
            {
                missing.Add(tool);
            }
        }

        if (managed)
        {
            string? managedEmsdk = Environment.GetEnvironmentVariable("HYP_WEB_DOTNET_EMSDK");

            if (string.IsNullOrEmpty(managedEmsdk) && !string.IsNullOrEmpty(emsdk))
            {
                managedEmsdk = Path.Combine(Path.GetDirectoryName(emsdk.TrimEnd('\\', '/'))!, "emsdk-3.1.56");
            }

            if (string.IsNullOrEmpty(managedEmsdk) || !File.Exists(Path.Combine(managedEmsdk, "upstream", "bin", "clang.exe")))
            {
                missing.Add("emsdk 3.1.56 for NativeAOT-LLVM (HYP_WEB_DOTNET_EMSDK)");
            }

            if (!File.Exists(Path.Combine(HarnessEnvironment.RepositoryRoot, "Binaries", "Web", "Release", "Hyperion.NET.Runtime.dll")))
            {
                missing.Add("the C# assemblies of the engine's web build");
            }
        }

        return missing.Count > 0 ? "needs " + string.Join(", ", missing) : null;
    }

    private static bool IsOnPath(string tool)
    {
        foreach (string directory in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            if (File.Exists(Path.Combine(directory, tool + ".exe")))
            {
                return true;
            }
        }

        return false;
    }

    /// Runs BuildWebGame.ps1 the way the editor's Build for Web does (WebGameBuildService.BuildAsync())
    public static ProcessResult Build(GameProject project, string title, IReadOnlyList<string> extraArguments, string label)
    {
        List<string> arguments = new List<string>
        {
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ScriptPath,
            "-Project", project.Directory,
            "-ContentDir", project.ContentDirectory,
            "-Name", project.Name,
            "-Title", title
        };

        arguments.AddRange(extraArguments);

        return ProcessRunner.Run(
            $"Building {project.Name} for the web",
            "powershell.exe",
            arguments,
            project.Directory,
            Path.Combine(project.LogsDirectory, $"{label}.log"),
            BuildTimeout);
    }
}

public sealed class WindowsFactAttribute : FactAttribute
{
    public WindowsFactAttribute()
    {
        if (!OperatingSystem.IsWindows())
        {
            Skip = "Windows only, as building for the web from the editor is";
        }
    }
}

public sealed class WebBuildFactAttribute : FactAttribute
{
    public WebBuildFactAttribute(bool managed = false)
    {
        string? missing = managed ? WebBuildEnvironment.MissingForManaged : WebBuildEnvironment.MissingForNative;

        if (missing != null)
        {
            Skip = missing;
        }
    }
}

/// A C++ project made in the editor, with the AnimToolkit plugin on, cooked and built for the web as the editor's Build for Web does
public sealed class WebNativeFixture
{
    // the page title goes through cmd and into HTML, so it holds characters both treat specially
    public const string Title = "Web & <Native> 100%";

    private readonly Lazy<GameProject> _project;
    private readonly Lazy<ProcessResult> _cook;
    private readonly Lazy<ProcessResult> _build;

    public WebNativeFixture()
    {
        // PluginTest and MCPBridge are on by default too, but the first is a test plugin and the second editor only
        _project = new Lazy<GameProject>(() => GameProject.CreateInEditor("WebNative", GameProjectLanguage.Native, "PluginTest", "MCPBridge"));
        _cook = new Lazy<ProcessResult>(CookForWeb);

        // what the editor passes for a project with AnimToolkit on: the plugin, and its runtime module, linked into the page
        _build = new Lazy<ProcessResult>(() => WebBuildEnvironment.Build(Project, Title, new[] { "-FastLink", "-PluginModules", "Animation", "-Plugins", "AnimToolkit" }, "build-web"));
    }

    public GameProject Project => _project.Value;

    public string CookDirectory => Path.Combine(Project.Directory, "Build", "WebCookOnly");

    public ProcessResult Cook => _cook.Value;

    public ProcessResult Build => _build.Value;

    public string OutputDirectory => Path.Combine(Project.Directory, "Binaries", "Web");

    // BuildWebGame.ps1's cook step, which needs no emsdk
    private ProcessResult CookForWeb()
    {
        if (Directory.Exists(CookDirectory))
        {
            Directory.Delete(CookDirectory, recursive: true);
        }

        string shaderListsDirectory = Path.Combine(CookDirectory, "NoShaderLists");
        Directory.CreateDirectory(shaderListsDirectory);

        return ProcessRunner.Run(
            $"Cooking {Project.Name} for the web",
            HarnessEnvironment.GetCommandletPath("BlobStorageCookCommandlet"),
            new[]
            {
                "--project=" + Project.Directory,
                "--out-cache=" + Path.Combine(CookDirectory, "Cache"),
                "--out-content=" + Path.Combine(CookDirectory, "Content"),
                "--target-platform=web",
                "--shader-target=web",
                "--target-config=" + WebBuildEnvironment.WebConfigPath,
                "--cachedir=" + shaderListsDirectory,
                "--worlds=MainWorld",
                "--plugins=AnimToolkit"
            },
            HarnessEnvironment.EngineBinDir,
            Path.Combine(Project.LogsDirectory, "cook-web.log"),
            WebBuildEnvironment.BuildTimeout);
    }
}

public sealed class WebManagedFixture
{
    private readonly Lazy<GameProject> _project;
    private readonly Lazy<ProcessResult> _build;

    public WebManagedFixture()
    {
        _project = new Lazy<GameProject>(() => GameProject.CreateInEditor("WebManaged", GameProjectLanguage.Managed, "AnimToolkit", "PluginTest", "MCPBridge"));
        _build = new Lazy<ProcessResult>(() => WebBuildEnvironment.Build(Project, Project.Name, new[] { "-FastLink" }, "build-web"));
    }

    public GameProject Project => _project.Value;

    public ProcessResult Build => _build.Value;

    public string OutputDirectory => Path.Combine(Project.Directory, "Binaries", "Web");
}

public static class WebAssert
{
    public static readonly string[] PageFiles = { "index.html", "isolation-worker.js", "brotli_dec_wasm.js", "brotli_dec_wasm_bg.wasm", ".nojekyll" };

    /// Binaries/Web holds everything a static host serves, as BuildWebGame.ps1 stages it
    public static void Staged(GameProject project, string outputDirectory)
    {
        IEnumerable<string> expected = PageFiles.Concat(new[] { project.Name + ".js", project.Name + ".wasm", project.Name + ".data" });
        string[] missing = expected.Where(fileName => !File.Exists(Path.Combine(outputDirectory, fileName))).ToArray();

        Assert.True(missing.Length == 0, $"{outputDirectory} is missing {string.Join(", ", missing)}");
    }

    /// The page reads the cache's file names from index.txt (a static host lists no directories), and fetches the large ones
    /// as <name>.br, which brotli.json lists
    public static void CacheIndexed(string outputDirectory)
    {
        string cacheDirectory = Path.Combine(outputDirectory, "Cache");
        string indexPath = Path.Combine(cacheDirectory, "index.txt");

        Assert.True(File.Exists(indexPath), $"{indexPath} was not written");

        string[] indexed = File.ReadAllLines(indexPath).Where(line => line.Length > 0).ToArray();
        Assert.NotEmpty(indexed);

        string brotliPath = Path.Combine(cacheDirectory, "brotli.json");
        HashSet<string> compressed = File.Exists(brotliPath)
            ? JsonDocument.Parse(File.ReadAllText(brotliPath)).RootElement.EnumerateObject().Select(property => property.Name).ToHashSet()
            : new HashSet<string>();

        List<string> unreadable = new List<string>();

        foreach (string fileName in indexed)
        {
            bool isPlain = File.Exists(Path.Combine(cacheDirectory, fileName));
            bool isCompressed = compressed.Contains(fileName) && File.Exists(Path.Combine(cacheDirectory, fileName + ".br"));

            if (!isPlain && !isCompressed)
            {
                unreadable.Add(fileName);
            }
        }

        Assert.True(unreadable.Count == 0, $"index.txt lists files the page can't fetch: {string.Join(", ", unreadable)}");

        HashSet<string> indexedSet = indexed.ToHashSet();
        string[] unindexed = Directory.GetFiles(cacheDirectory)
            .Select(Path.GetFileName)
            .Select(fileName => fileName!.EndsWith(".br") ? fileName[..^3] : fileName!)
            .Where(fileName => fileName != "index.txt" && fileName != "brotli.json" && !indexedSet.Contains(fileName))
            .ToArray();

        Assert.True(unindexed.Length == 0, $"The cache has files index.txt doesn't list, which the page never loads: {string.Join(", ", unindexed)}");
    }

    /// The editor goes by Built.stamp to tell whether the web build is out of date
    public static void Stamped(GameProject project)
    {
        string buildDirectory = Path.Combine(project.Directory, "Build", "Web");

        Assert.True(File.Exists(Path.Combine(buildDirectory, "Built.stamp")), "A finished build wrote no Built.stamp");
        Assert.False(File.Exists(Path.Combine(buildDirectory, "Building.stamp")), "A finished build left Building.stamp");
    }
}

public class WebBuildTests : IClassFixture<WebNativeFixture>
{
    private readonly WebNativeFixture _fixture;
    private readonly ITestOutputHelper _output;

    public WebBuildTests(WebNativeFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    // the editor reads the cook's log rather than its exit code, since it can crash shutting down after a finished cook
    [WindowsFact]
    public void Cook_CompilesWebShaders()
    {
        ProcessResult cook = _fixture.Cook;

        _output.WriteLine($"Log: {cook.LogPath}");

        Assert.True(cook.Output.Contains("Blob storage cook complete."), $"The web cook didn't complete.\n{cook.Summary}");

        string cacheDirectory = Path.Combine(_fixture.CookDirectory, "Cache");
        Assert.True(Directory.Exists(cacheDirectory) && Directory.EnumerateFiles(cacheDirectory).Any(), $"The web cook wrote nothing to {cacheDirectory}");
    }

    [WebBuildFact]
    public void NativeGame_Builds()
    {
        _output.WriteLine($"Log: {_fixture.Build.LogPath}");

        GameAssert.Succeeded(_fixture.Build);
        WebAssert.Staged(_fixture.Project, _fixture.OutputDirectory);
        WebAssert.Stamped(_fixture.Project);
    }

    [WebBuildFact]
    public void NativeGame_CacheIsIndexed()
    {
        GameAssert.Succeeded(_fixture.Build);

        WebAssert.CacheIndexed(_fixture.OutputDirectory);
    }

    [WebBuildFact]
    public void NativeGame_PageHasTitle()
    {
        GameAssert.Succeeded(_fixture.Build);

        string page = File.ReadAllText(Path.Combine(_fixture.OutputDirectory, "index.html"));

        // % goes in as an entity, since cmd would expand it
        Assert.Contains(System.Net.WebUtility.HtmlEncode(WebNativeFixture.Title).Replace("%", "&#37;"), page);
    }

    [WebBuildFact]
    public void NativeGame_LinksPluginModule()
    {
        GameAssert.Succeeded(_fixture.Build);

        // the page's link line names each plugin library it links
        string linkCommands = string.Join("\n", Directory.EnumerateFiles(Path.Combine(_fixture.Project.Directory, "Build", "Web", "CMake"), "build.ninja", SearchOption.AllDirectories).Select(File.ReadAllText));

        Assert.Contains("plugin-Animation", linkCommands);
    }

    // Run in Browser on a project never built for the web: the editor offers to build it first, the script refuses to serve
    [WindowsFact]
    public void ServeOnly_WithoutBuild_Fails()
    {
        GameProject project = GameProject.Create("WebServeOnly", GameProjectLanguage.Native);

        ProcessResult serve = WebBuildEnvironment.Build(project, project.Name, new[] { "-ServeOnly" }, "serve-web");

        Assert.False(serve.Succeeded, $"Serving a project with no web build succeeded.\n{serve.Summary}");
        Assert.Contains("hasn't been built for the web yet", serve.Output);
    }
}

public class WebManagedBuildTests : IClassFixture<WebManagedFixture>
{
    private readonly WebManagedFixture _fixture;
    private readonly ITestOutputHelper _output;

    public WebManagedBuildTests(WebManagedFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    [WebBuildFact(managed: true)]
    public void ManagedGame_Builds()
    {
        _output.WriteLine($"Log: {_fixture.Build.LogPath}");

        GameAssert.Succeeded(_fixture.Build);
        WebAssert.Staged(_fixture.Project, _fixture.OutputDirectory);
        WebAssert.Stamped(_fixture.Project);
        WebAssert.CacheIndexed(_fixture.OutputDirectory);
    }

    // NativeAOT-LLVM compiles the C# game into a static library the page links
    [WebBuildFact(managed: true)]
    public void ManagedGame_LinksCompiledCSharp()
    {
        GameAssert.Succeeded(_fixture.Build);

        string linkInputsPath = Path.Combine(_fixture.Project.Directory, "Build", "Web", "Managed", "LinkInputs.txt");

        Assert.True(File.Exists(linkInputsPath), $"Compiling the C# game wrote no {linkInputsPath}");
        Assert.Contains(File.ReadAllLines(linkInputsPath), line => line.Length > 0);
    }
}
