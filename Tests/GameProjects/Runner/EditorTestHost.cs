using System.Runtime.InteropServices;
using System.Text.Json;

namespace Hyperion.GameProjectTests;

public sealed record SimulationReport(
    bool Loaded,
    string? LoadError,
    string? GameClass,
    bool Launched,
    int SimulatedFrames,
    double GameTime,
    bool Stopped,
    List<string>? EnabledPlugins,
    List<string>? WorldSystems);

public sealed record SimulationRunResult(ProcessResult Process, SimulationReport? Report);

public sealed record SwitchReport(List<SimulationReport> Steps);

public sealed record SwitchRunResult(ProcessResult Process, SwitchReport? Report);

/// What the editor's Play did, as the `play` command reports it
public sealed record PlayReport(
    bool Loaded,
    string? LoadError,
    bool EditorLaunched,
    bool Started,
    string? GameClass,
    bool Launched,
    int SimulatedFrames,
    double GameTime,
    bool ServerGame,
    string NetStatus,
    bool Hosting,
    int ClientConnects,
    int ClientDisconnects,
    bool TimedOut,
    bool Stopped,
    bool ListeningAfterStop,
    bool EditProjectRestored);

public sealed record PlayRunResult(ProcessResult Process, PlayReport? Report);

/// A `play` run that was started but not waited on yet, for running two editors at once (a server and a client)
public sealed class RunningPlay
{
    private readonly RunningProcess _process;
    private readonly string _resultsPath;

    internal RunningPlay(RunningProcess process, string resultsPath)
    {
        _process = process;
        _resultsPath = resultsPath;
    }

    public RunningProcess Process => _process;

    public PlayRunResult Wait()
    {
        ProcessResult result = _process.Wait(HarnessEnvironment.RunTimeout);

        return new PlayRunResult(result, EditorTestHost.ReadPlayReport(_resultsPath));
    }
}

public static class EditorTestHost
{
    private static readonly JsonSerializerOptions s_reportJsonOptions = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

    private static readonly Lazy<ProcessResult> s_build = new(Build);

    private static string ProjectDirectory => Path.Combine(HarnessEnvironment.RepositoryRoot, "Tests", "GameProjects", "EditorTestHost");

    public static string ExecutablePath => Path.Combine(HarnessEnvironment.EngineBinDir, "EditorTestHost" + HarnessEnvironment.ExecutableExtension);

    public static ProcessResult CreateProject(GameProject project, IReadOnlyList<string> disabledPlugins)
    {
        string typeStr = project.Language == GameProjectLanguage.Managed
            ? "managed"
            : "native";

        return Run(
            $"Creating {project.Name} in the editor",
            project,
            "create",
            [project.ProjectFilePath, typeStr, disabledPlugins.Count > 0 ? string.Join(",", disabledPlugins) : "-"]);
    }

    public static SimulationRunResult Simulate(GameProject project, int numFrames, string label)
    {
        string resultsPath = Path.Combine(project.LogsDirectory, $"results-{label}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        ProcessResult process = Run(
            $"Simulating {project.Name} in the editor ({label})",
            project,
            label,
            [project.Directory, numFrames.ToString(), resultsPath],
            command: "simulate");

        SimulationReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<SimulationReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new SimulationRunResult(process, report);
    }

    /// Simulates \p projects one after another in one editor process, the way the editor opens one project after another
    public static SwitchRunResult Switch(IReadOnlyList<GameProject> projects, int numFrames, string label)
    {
        GameProject logProject = projects[0];
        string resultsPath = Path.Combine(logProject.LogsDirectory, $"results-{label}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        ProcessResult process = Run(
            $"Switching between {string.Join(", ", projects.Select(project => project.Name))} in the editor ({label})",
            logProject,
            label,
            [string.Join(";", projects.Select(project => project.Directory)), numFrames.ToString(), resultsPath],
            command: "switch");

        SwitchReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<SwitchReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new SwitchRunResult(process, report);
    }

    /// Plays the project the way the editor's Play does, in \p netMode (Standalone, Client or DedicatedServer), and waits for it
    public static PlayRunResult Play(GameProject project, string label, string netMode, int numFrames, string host = "127.0.0.1", int port = 0, int expectedClients = 0)
    {
        return StartPlay(project, label, netMode, numFrames, host, port, expectedClients).Wait();
    }

    /// Starts a `play` run without waiting for it, so another can run against it
    public static RunningPlay StartPlay(GameProject project, string label, string netMode, int numFrames, string host = "127.0.0.1", int port = 0, int expectedClients = 0)
    {
        if (!s_build.Value.Succeeded)
        {
            throw new InvalidOperationException($"EditorTestHost didn't build\n{s_build.Value.Summary}");
        }

        string resultsPath = Path.Combine(project.LogsDirectory, $"results-{label}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        Directory.CreateDirectory(project.LogsDirectory);

        RunningProcess process = ProcessRunner.Start(
            $"Playing {project.Name} in the editor ({label}, {netMode})",
            ExecutablePath,
            new[] { "play", project.Directory, numFrames.ToString(), netMode, host, port.ToString(), expectedClients.ToString(), resultsPath },
            HarnessEnvironment.EngineBinDir,
            Path.Combine(project.LogsDirectory, $"editor-{label}.log"),
            new Dictionary<string, string> { ["DOTNET_ROOT"] = FindDotNetRoot() });

        return new RunningPlay(process, resultsPath);
    }

    internal static PlayReport? ReadPlayReport(string resultsPath)
    {
        return File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<PlayReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;
    }

    private static ProcessResult Run(string description, GameProject project, string label, string[] arguments, string? command = null)
    {
        if (!s_build.Value.Succeeded)
        {
            return s_build.Value;
        }

        Directory.CreateDirectory(project.LogsDirectory);

        return ProcessRunner.Run(
            description,
            ExecutablePath,
            new[] { command ?? label }.Concat(arguments),
            HarnessEnvironment.EngineBinDir,
            Path.Combine(project.LogsDirectory, $"editor-{label}.log"),
            HarnessEnvironment.RunTimeout,
            new Dictionary<string, string> { ["DOTNET_ROOT"] = FindDotNetRoot() });
    }

    private static ProcessResult Build()
    {
        string binDir = HarnessEnvironment.EngineBinDir.Replace('\\', '/').TrimEnd('/');
        string baseDir = HarnessEnvironment.RepositoryRoot.Replace('\\', '/').TrimEnd('/');

        return ProcessRunner.Run(
            "Building EditorTestHost",
            "dotnet",
            [
                "build", Path.Combine(ProjectDirectory, "EditorTestHost.csproj"), "--configuration", "Release", "--disable-build-servers", "--nologo",
                $"-p:HyperionBinDir={binDir}/", $"-p:HyperionBaseDir={baseDir}/"
            ],
            ProjectDirectory,
            Path.Combine(HarnessEnvironment.WorkDir, "EditorTestHost-build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    private static string FindDotNetRoot()
    {
        string? dotnetRoot = Environment.GetEnvironmentVariable("DOTNET_ROOT");

        if (!string.IsNullOrEmpty(dotnetRoot))
        {
            return dotnetRoot;
        }

        // <root>/shared/Microsoft.NETCore.App/<version>/
        return Path.GetFullPath(Path.Combine(RuntimeEnvironment.GetRuntimeDirectory(), "..", "..", ".."));
    }
}
