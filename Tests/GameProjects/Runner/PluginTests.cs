using System.Text.RegularExpressions;

using Xunit;

namespace Hyperion.GameProjectTests;

/// Projects using the PluginTest plugin (Tests/Plugins/PluginTest), whose libraries and C# the editor loads only for the projects that enable it
public class PluginTests
{
    private const int NumFrames = 10;
    private const string PluginName = "PluginTest";
    private const string PluginSystemName = "PluginTestSystem";

    [Fact]
    public void ManagedGame_ReferencingPluginType_Loads()
    {
        GameProject project = GameProject.CreateInEditor("PluginReference", GameProjectLanguage.Managed);

        File.WriteAllText(Path.Combine(project.SourceDirectory, project.ClassName + ".cs"),
            "using Hyperion;\n\n"
            + $"public class {project.ClassName} : Game\n"
            + "{\n"
            + "    protected override void OnLaunch()\n"
            + "    {\n"
            + $"        Logger.Log(LogLevel.Info, \"{project.ClassName} launched with \" + typeof({PluginSystemName}).FullName);\n"
            + "    }\n"
            + "}\n");

        GameAssert.Succeeded(project.Build());

        SimulationRunResult result = EditorTestHost.Simulate(project, NumFrames, "simulate");
        SimulationReport report = AssertSimulated(project, result.Report, result.Process);

        Assert.Contains($"{project.ClassName} launched with Hyperion.{PluginSystemName}", result.Process.Output);
        Assert.Contains(PluginSystemName, report.WorldSystems ?? []);
        Assert.True(result.Process.Succeeded, $"The editor did not exit cleanly.\n{result.Process.Summary}");
    }

    [Fact]
    public void SwitchingProjects_WithDifferentPlugins_LoadsAndUnloadsTheirModules()
    {
        GameProject withPlugin = GameProject.CreateInEditor("PluginSwitchWith", GameProjectLanguage.Managed);
        GameProject withoutPlugin = GameProject.CreateInEditor("PluginSwitchWithout", GameProjectLanguage.Managed, PluginName);

        GameAssert.Succeeded(withPlugin.Build());
        GameAssert.Succeeded(withoutPlugin.Build());

        GameProject[] order = [withPlugin, withoutPlugin, withPlugin];

        SwitchRunResult result = EditorTestHost.Switch(order, NumFrames, "switch");

        Assert.True(result.Report != null, $"The editor wrote no results.\n{result.Process.Summary}");
        Assert.Equal(order.Length, result.Report!.Steps.Count);

        for (int stepIndex = 0; stepIndex < order.Length; stepIndex++)
        {
            GameProject project = order[stepIndex];
            SimulationReport step = AssertSimulated(project, result.Report.Steps[stepIndex], result.Process);

            bool expectPlugin = project == withPlugin;
            string context = $"step {stepIndex} ({project.Name})";

            Assert.True(expectPlugin == (step.EnabledPlugins ?? []).Contains(PluginName),
                $"{context}: enabled plugins were [{string.Join(", ", step.EnabledPlugins ?? [])}]");
            Assert.True(expectPlugin == (step.WorldSystems ?? []).Contains(PluginSystemName),
                $"{context}: world systems were [{string.Join(", ", step.WorldSystems ?? [])}]");
        }

        string output = result.Process.Output;

        Assert.Equal(2, CountModuleLogLines(output, "Loaded", PluginName));
        Assert.True(CountModuleLogLines(output, "Unloaded", PluginName) >= 1,
            $"{PluginName} was never unloaded when the project without it opened.\n{result.Process.Summary}");
        Assert.True(result.Process.Succeeded, $"The editor did not exit cleanly.\n{result.Process.Summary}");
    }

    private static SimulationReport AssertSimulated(GameProject project, SimulationReport? report, ProcessResult process)
    {
        Assert.True(report != null, $"The editor wrote no results for {project.Name}.\n{process.Summary}");
        Assert.True(report!.Loaded, $"{project.Name} didn't open: {report.LoadError}\n{process.Summary}");
        Assert.Equal(project.ClassName, report.GameClass);
        Assert.True(report.Launched, $"{project.ClassName} never launched.\n{process.Summary}");
        Assert.Equal(NumFrames, report.SimulatedFrames);
        Assert.True(report.Stopped, $"{project.Name} didn't stop simulating");

        return report;
    }

    // "<Verb> plugin module <Plugin>.<Plugin>", not its <Plugin>Editor module
    private static int CountModuleLogLines(string output, string verb, string pluginName)
    {
        return Regex.Matches(output, $@"\b{verb} plugin module {pluginName}\.{pluginName}\b").Count;
    }
}
