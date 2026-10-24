using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

public sealed class NativeEngineVersionFixture
{
    public const string OtherEngineVersion = "0.0.0";

    private readonly Lazy<GameProject> _project = new Lazy<GameProject>(CreateProject);

    private readonly Lazy<string> _otherEngineBuild;
    private readonly Lazy<EditorTestRunResult> _run;
    private readonly Lazy<EditorTestRunResult> _rebuildRun;

    public NativeEngineVersionFixture()
    {
        _otherEngineBuild = new Lazy<string>(SaveOtherEngineBuild);
        _run = new Lazy<EditorTestRunResult>(() => Load("engine-version", rebuildScript: ""));
        _rebuildRun = new Lazy<EditorTestRunResult>(() => Load("engine-version-rebuild", _project.Value.WriteRebuildScript()));
    }

    // loaded where nothing can rebuild the game, as in the editor without a C++ toolchain
    public EditorTestRunResult Run => _run.Value;

    // loaded where the game is rebuilt, as the editor does
    public EditorTestRunResult RebuildRun => _rebuildRun.Value;

    private static GameProject CreateProject()
    {
        GameProject project = GameProject.Create("NativeTest", GameProjectLanguage.Native, fixtureName: "NativeTest", directoryName: "NativeEngineVersion");

        GameAssert.Succeeded(project.Build());

        return project;
    }

    private string SaveOtherEngineBuild()
    {
        GameProject project = _project.Value;

        string build = project.SaveNativeModuleBuild("OtherEngine");

        File.WriteAllText(Path.Combine(build, Path.GetFileName(project.EngineVersionStampPath)), OtherEngineVersion + "\n");

        return build;
    }

    private EditorTestRunResult Load(string label, string rebuildScript)
    {
        return _project.Value.RunEditorTests(label, new[] { _otherEngineBuild.Value }, new[] { "engine-version" }, "NativeTestEditorTests",
            rebuildScript: rebuildScript);
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

    private EditorTestStep GetStep(EditorTestRunResult run)
    {
        _output.WriteLine($"Log: {run.Process.LogPath}");

        Assert.True(run.Report != null, $"The editor host wrote no results.\n{run.Process.Summary}");
        Assert.True(run.Report!.Steps.Count == 1, $"The editor host ran {run.Report.Steps.Count} builds, expected 1.\n{run.Process.Summary}");

        return run.Report.Steps[0];
    }

    [Fact]
    public void OtherEngineBuild_IsRefusedBeforeLoading()
    {
        EditorTestStep step = GetStep(_fixture.Run);

        Assert.False(step.Loaded, $"{step.Build} is stamped for engine {NativeEngineVersionFixture.OtherEngineVersion}, but it loaded");
        Assert.Contains($"built for engine version {NativeEngineVersionFixture.OtherEngineVersion}", step.LoadError ?? "");
        Assert.Contains("Rebuild the game", step.LoadError ?? "");

        Assert.DoesNotContain("Loaded game module", File.ReadAllText(_fixture.Run.Process.LogPath));
    }

    [Fact]
    public void OtherEngineBuild_IsRebuiltAndLoaded()
    {
        EditorTestStep step = GetStep(_fixture.RebuildRun);

        Assert.True(step.Loaded, $"{step.Build} wasn't rebuilt for this engine: {step.LoadError}");

        string log = File.ReadAllText(_fixture.RebuildRun.Process.LogPath);

        Assert.Contains($"built for engine version {NativeEngineVersionFixture.OtherEngineVersion}", log);
        Assert.Contains("Rebuilding the game with", log);
    }

    [Fact]
    public void EditorHost_ExitsCleanly()
    {
        GetStep(_fixture.Run);
        GameAssert.Succeeded(_fixture.Run.Process);

        GetStep(_fixture.RebuildRun);
        GameAssert.Succeeded(_fixture.RebuildRun.Process);
    }
}
