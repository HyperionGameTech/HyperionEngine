using System.Text.Json;

using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// The tests registered with HYP_ENGINE_TEST in Tests/Engine, run by the engine's EngineTestCommandlet. They need no game
/// project, so the whole set runs in one launch of the engine, and each test is reported as its own result
public class EngineTests : IClassFixture<EngineTestFixture>
{
    private const string DiscoveryFailedPrefix = "<discovery failed> ";

    private readonly EngineTestFixture _fixture;
    private readonly ITestOutputHelper _output;

    public EngineTests(EngineTestFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    public static IEnumerable<object[]> TestNames()
    {
        try
        {
            return EngineTestRunner.List().Select(name => new object[] { name }).ToList();
        }
        catch (Exception exception)
        {
            // reported as a failing test, rather than a discovery error that hides the other tests in the assembly
            return new[] { new object[] { DiscoveryFailedPrefix + exception.Message } };
        }
    }

    [Theory]
    [MemberData(nameof(TestNames))]
    public void EngineTest(string name)
    {
        Assert.False(name.StartsWith(DiscoveryFailedPrefix, StringComparison.Ordinal), name);

        EngineTestRunResult run = _fixture.Run;

        _output.WriteLine($"Log: {run.Process.LogPath}");

        Assert.True(run.Report != null, $"The engine tests wrote no results.\n{run.Process.Summary}");

        GameTestResult? result = run.Report!.Tests.FirstOrDefault(test => test.Name == name);

        Assert.True(result != null, $"{name} did not run; the engine probably crashed during an earlier test.\n{run.Process.Summary}");
        Assert.True(result!.Passed, result.Message);
    }

    [Fact]
    public void EngineTestsExitCleanly()
    {
        EngineTestRunResult run = _fixture.Run;

        Assert.True(run.Report != null, $"The engine tests wrote no results.\n{run.Process.Summary}");
        Assert.True(run.Report!.Tests.Count > 0, $"The engine ran no tests\n{run.Process.Summary}");

        int failed = run.Report.Tests.Count(test => !test.Passed);

        Assert.True(run.Process.Succeeded || failed > 0, $"The engine did not exit cleanly after the tests ran.\n{run.Process.Summary}");
    }
}

public sealed record EngineTestRunResult(ProcessResult Process, GameTestReport? Report);

public static class EngineTestRunner
{
    private const string CommandletName = "EngineTestCommandlet";

    private static readonly JsonSerializerOptions s_reportJsonOptions = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

    private static string LogsDirectory => Path.Combine(HarnessEnvironment.WorkDir, "EngineTests", "Logs");

    public static IReadOnlyList<string> List()
    {
        EngineTestRunResult listing = Execute("list", new[] { "--list=true" });

        if (listing.Report == null)
        {
            throw new InvalidOperationException($"The engine wrote no test list.\n{listing.Process.Summary}");
        }

        return listing.Report.Tests.Select(test => test.Name).ToList();
    }

    public static EngineTestRunResult Run(string? filter = null)
    {
        return Execute("run", filter == null ? Array.Empty<string>() : new[] { "--filter=" + filter });
    }

    private static EngineTestRunResult Execute(string label, IEnumerable<string> extraArguments)
    {
        string resultsPath = Path.Combine(LogsDirectory, $"engine-tests-{label}.json");

        Directory.CreateDirectory(LogsDirectory);

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        List<string> arguments = new List<string> { "--results=" + resultsPath };
        arguments.AddRange(extraArguments);

        ProcessResult process = ProcessRunner.Run(
            $"Engine tests ({label})",
            HarnessEnvironment.GetCommandletPath(CommandletName),
            arguments,
            HarnessEnvironment.EngineBinDir,
            Path.Combine(LogsDirectory, $"engine-tests-{label}.log"),
            HarnessEnvironment.RunTimeout);

        GameTestReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<GameTestReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new EngineTestRunResult(process, report);
    }
}

public sealed class EngineTestFixture
{
    private readonly Lazy<EngineTestRunResult> _run = new Lazy<EngineTestRunResult>(() => EngineTestRunner.Run());

    public EngineTestRunResult Run => _run.Value;
}
