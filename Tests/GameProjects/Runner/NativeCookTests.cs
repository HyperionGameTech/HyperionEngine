using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// Packages Fixtures/NativeTest the way the editor's Package Game does, then runs the packaged game headless on its cooked content.
/// The content is made in the editor project and saved by the editor (NativeTestEditorTests' cook-author phase): a World with a scene
/// holding an entity of a game class, a game component and a game asset
public sealed class NativeCookFixture
{
    private const string StartupWorldName = "MainWorld";

    private readonly Lazy<GameProject> _project;
    private readonly Lazy<ProcessResult> _build;
    private readonly Lazy<EditorTestRunResult> _author;
    private readonly Lazy<ProcessResult> _cook;
    private readonly Lazy<ProcessResult> _package;
    private readonly Lazy<GameRunResult> _packagedRun;

    public NativeCookFixture()
    {
        _project = new Lazy<GameProject>(CreateProject);
        _build = new Lazy<ProcessResult>(() => Project.Build());
        _author = new Lazy<EditorTestRunResult>(() =>
        {
            GameAssert.Succeeded(Build);

            return Project.RunEditorTests("cook-author", new[] { Project.SaveNativeModuleBuild("Author") }, new[] { "cook-author" }, "NativeTestEditorTests", saveProject: true);
        });
        _cook = new Lazy<ProcessResult>(() =>
        {
            Assert.True(Author.Report?.Saved == true, $"The editor didn't save the content to cook.\n{Author.Process.Summary}");

            return Project.Cook(PackageDirectory);
        });
        _package = new Lazy<ProcessResult>(() =>
        {
            Assert.True(Cook.Output.Contains("Blob storage cook complete."), $"The cook didn't complete.\n{Cook.Summary}");

            return Project.BuildPackage(PackageDirectory, StartupWorldName);
        });
        _packagedRun = new Lazy<GameRunResult>(() =>
        {
            GameAssert.Succeeded(Package);

            return Project.RunPackaged(PackageDirectory, "cooked");
        });
    }

    public GameProject Project => _project.Value;

    public string PackageDirectory => Path.Combine(Project.Directory, "Packaged");

    public ProcessResult Build => _build.Value;

    public EditorTestRunResult Author => _author.Value;

    public ProcessResult Cook => _cook.Value;

    public ProcessResult Package => _package.Value;

    public GameRunResult PackagedRun => _packagedRun.Value;

    private static GameProject CreateProject()
    {
        GameProject project = GameProject.Create("NativeTest", GameProjectLanguage.Native, fixtureName: "NativeTest", directoryName: "NativeCook");

        // a packaged game's scripts are linked into it with NativeAOT, which this doesn't cover
        Directory.Delete(project.ScriptsDirectory, recursive: true);

        // a project made in the editor starts out with its bucket folders
        foreach (string bucket in new[] { "Worlds", "Scenes", "Misc" })
        {
            Directory.CreateDirectory(Path.Combine(project.ContentDirectory, bucket));
        }

        return project;
    }
}

public class NativeCookTests : IClassFixture<NativeCookFixture>
{
    private readonly NativeCookFixture _fixture;
    private readonly ITestOutputHelper _output;

    public NativeCookTests(NativeCookFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    [Fact]
    public void Editor_SavesWorldWithGameObjects()
    {
        EditorTestRunResult author = _fixture.Author;

        _output.WriteLine($"Log: {author.Process.LogPath}");

        Assert.True(author.Report != null, $"The editor wrote no results.\n{author.Process.Summary}");

        EditorTestStep step = Assert.Single(author.Report!.Steps);

        Assert.True(step.Loaded, $"The game didn't load in the editor: {step.LoadError}");
        GameAssert.TestsPassed(step.Report!, _output);

        Assert.True(author.Report.Saved, $"The project didn't save: {author.Report.SaveError}");

        string worldPath = Path.Combine(_fixture.Project.ContentDirectory, "Worlds", "MainWorld.hmf");
        Assert.True(File.Exists(worldPath), $"{worldPath} was not saved");
    }

    // the editor reads the cook's log rather than its exit code, since it can crash shutting down after a finished cook
    [Fact]
    public void Cook_Completes()
    {
        ProcessResult cook = _fixture.Cook;

        Assert.True(cook.Output.Contains("Blob storage cook complete."), $"The cook didn't complete.\n{cook.Summary}");
        Assert.True(cook.Succeeded, $"The cook completed, but didn't exit cleanly.\n{cook.Summary}");
    }

    [Fact]
    public void Package_Builds()
    {
        GameAssert.Succeeded(_fixture.Package);

        string executablePath = Path.Combine(_fixture.PackageDirectory, _fixture.Project.Name + HarnessEnvironment.ExecutableExtension);
        Assert.True(File.Exists(executablePath), $"{executablePath} was not built");
    }

    [Fact]
    public void Package_LinksShippingEngineStatically()
    {
        GameAssert.Succeeded(_fixture.Package);

        GameAssert.NoEngineLibraries(_fixture.PackageDirectory);

        string compileCommandsPath = Path.Combine(_fixture.Project.PackageBuildDirectory, "compile_commands.json");
        Assert.True(File.Exists(compileCommandsPath), $"{compileCommandsPath} was not written");
        Assert.Contains("HYP_SHIPPING=1", File.ReadAllText(compileCommandsPath));
    }

    [Fact]
    public void PackagedGame_LoadsCookedContent()
    {
        GameAssert.Passed(_fixture.PackagedRun, _output);
    }
}
