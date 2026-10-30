using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// The HYP_EDITOR_TESTs (plugins' Tests/Editor), run by EditorTestHost with an editor and a new project open
public class EditorTests : IClassFixture<EditorTestFixture>
{
    private const string DiscoveryFailedPrefix = "<discovery failed> ";

    private readonly EditorTestFixture _fixture;
    private readonly ITestOutputHelper _output;

    public EditorTests(EditorTestFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    public static IEnumerable<object[]> TestNames()
    {
        try
        {
            EngineTestRunResult listing = EditorTestHost.RunEditorTests("list", listOnly: true);

            if (listing.Report == null)
            {
                throw new InvalidOperationException($"EditorTestHost wrote no test list.\n{listing.Process.Summary}");
            }

            return listing.Report.Tests.Select(test => new object[] { test.Name }).ToList();
        }
        catch (Exception exception)
        {
            // reported as a failing test, rather than a discovery error that hides the other tests in the assembly
            return [[DiscoveryFailedPrefix + exception.Message]];
        }
    }

    [Theory]
    [MemberData(nameof(TestNames))]
    public void EditorTest(string name)
    {
        Assert.False(name.StartsWith(DiscoveryFailedPrefix, StringComparison.Ordinal), name);

        EngineTestRunResult run = _fixture.Run;

        _output.WriteLine($"Log: {run.Process.LogPath}");

        Assert.True(run.Report != null, $"The editor tests wrote no results.\n{run.Process.Summary}");

        GameTestResult? result = run.Report!.Tests.FirstOrDefault(test => test.Name == name);

        Assert.True(result != null, $"{name} did not run; the editor probably crashed during an earlier test.\n{run.Process.Summary}");
        Assert.True(result!.Passed, result.Message);
    }

    [Fact]
    public void EditorTestsExitCleanly()
    {
        EngineTestRunResult run = _fixture.Run;

        Assert.True(run.Report != null, $"The editor tests wrote no results.\n{run.Process.Summary}");

        int failed = run.Report!.Tests.Count(test => !test.Passed);

        Assert.True(run.Process.Succeeded || failed > 0, $"The editor did not exit cleanly after the tests ran.\n{run.Process.Summary}");
    }
}

public sealed class EditorTestFixture
{
    private readonly Lazy<EngineTestRunResult> _run = new Lazy<EngineTestRunResult>(() => EditorTestHost.RunEditorTests("run", listOnly: false));

    public EngineTestRunResult Run => _run.Value;
}
