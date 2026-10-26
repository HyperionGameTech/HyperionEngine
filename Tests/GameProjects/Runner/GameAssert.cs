using System.Text;

using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

public static class GameAssert
{
    public static void Succeeded(ProcessResult result)
    {
        Assert.True(result.Succeeded, result.Summary);
    }

    public static void Passed(GameRunResult result, ITestOutputHelper output)
    {
        output.WriteLine($"Log: {result.Process.LogPath}");

        Assert.True(result.Report != null, $"The game wrote no test results.\n{result.Process.Summary}");

        TestsPassed(result.Report!, output);

        Assert.True(result.Process.Succeeded, $"All game tests passed, but the game did not exit cleanly.\n{result.Process.Summary}");
    }

    public static void NoEngineLibraries(string packageDirectory)
    {
        string enginePrefix = Path.GetFileNameWithoutExtension(HarnessEnvironment.NativeLibraryName);

        string[] engineLibraries = Directory.GetFiles(packageDirectory)
            .Select(file => Path.GetFileName(file))
            .Where(name => (name.StartsWith(enginePrefix, StringComparison.OrdinalIgnoreCase) || name.Contains("dxcompiler", StringComparison.OrdinalIgnoreCase))
                && (name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) || name.Contains(".so") || name.EndsWith(".dylib")))
            .ToArray();

        Assert.True(engineLibraries.Length == 0, $"The package has engine libraries beside the game: {string.Join(", ", engineLibraries)}");
    }

    /// The tests the game ran for one phase all passed
    public static void TestsPassed(GameTestReport report, ITestOutputHelper output)
    {
        StringBuilder failures = new StringBuilder();

        foreach (GameTestResult test in report.Tests)
        {
            output.WriteLine($"  [{(test.Passed ? "PASS" : "FAIL")}] {test.Name} ({test.DurationMs:F0} ms){(test.Passed ? "" : ": " + test.Message)}");

            if (!test.Passed)
            {
                failures.AppendLine($"{test.Name}: {test.Message}");
            }
        }

        Assert.True(report.Tests.Count > 0, $"The game ran no tests for the {report.Phase} phase");
        Assert.True(failures.Length == 0, $"Game tests failed ({report.Phase} phase):\n{failures}");
    }
}
