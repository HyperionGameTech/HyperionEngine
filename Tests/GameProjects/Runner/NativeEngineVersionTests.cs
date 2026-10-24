using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

public sealed class NativeEngineVersionFixture
{
    public const string OtherEngineVersion = "0.0.0";

    private readonly Lazy<EditorTestRunResult> _run = new Lazy<EditorTestRunResult>(BuildAndLoad);

    public EditorTestRunResult Run => _run.Value;

    private static EditorTestRunResult BuildAndLoad()
    {
        GameProject project = GameProject.Create("NativeTest", GameProjectLanguage.Native, fixtureName: "NativeTest", directoryName: "NativeEngineVersion");

        GameAssert.Succeeded(project.Build());

        string build = project.SaveNativeModuleBuild("OtherEngine");

        File.WriteAllText(Path.Combine(build, Path.GetFileName(project.EngineVersionStampPath)), OtherEngineVersion + "\n");

        return project.RunEditorTests("engine-version", new[] { build }, new[] { "engine-version" }, "NativeTestEditorTests");
    }
}

public class NativeEngineVersionTests : IClassFixture<NativeEngineVersionFixture>
{
    private readonly NativeEngineVersionFixture _fixture;
    private readonly ITestOutputHelper _output;

    public NativeEngineVersionTests(NativeEngineVersionFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    private EditorTestReport Report
    {
        get
        {
            _output.WriteLine($"Log: {_fixture.Run.Process.LogPath}");

            Assert.True(_fixture.Run.Report != null, $"The editor host wrote no results.\n{_fixture.Run.Process.Summary}");
            Assert.True(_fixture.Run.Report!.Steps.Count == 1, $"The editor host ran {_fixture.Run.Report.Steps.Count} builds, expected 1.\n{_fixture.Run.Process.Summary}");

            return _fixture.Run.Report;
        }
    }

    [Fact]
    public void OtherEngineBuild_IsRefusedBeforeLoading()
    {
        EditorTestStep step = Report.Steps[0];

        Assert.False(step.Loaded, $"{step.Build} is stamped for engine {NativeEngineVersionFixture.OtherEngineVersion}, but it loaded");
        Assert.Contains($"built for engine {NativeEngineVersionFixture.OtherEngineVersion}", step.LoadError ?? "");
        Assert.Contains("Rebuild the game", step.LoadError ?? "");

        string log = File.ReadAllText(_fixture.Run.Process.LogPath);

        Assert.DoesNotContain("Loaded game module", log);
    }

    [Fact]
    public void EditorHost_ExitsCleanly()
    {
        Assert.NotNull(Report);

        GameAssert.Succeeded(_fixture.Run.Process);
    }
}
