using System.Diagnostics;
using System.Text;

namespace Hyperion.GameProjectTests;

public sealed record ProcessResult(string Description, int ExitCode, bool TimedOut, string Output, string LogPath)
{
    public bool Succeeded => !TimedOut && ExitCode == 0;

    public string Summary
    {
        get
        {
            string status = TimedOut ? "timed out" : $"exited with code {ExitCode}";

            return $"{Description} {status} (full log: {LogPath})\n{Tail(Output, 80)}";
        }
    }

    private static string Tail(string text, int lineCount)
    {
        string[] lines = text.Split('\n');

        return lines.Length <= lineCount ? text : string.Join('\n', lines[^lineCount..]);
    }
}

/// A process started by ProcessRunner.Start() that is still running, so a test can run something else alongside it
public sealed class RunningProcess
{
    private readonly string _description;
    private readonly Process _process;
    private readonly StreamWriter _logWriter;
    private readonly StringBuilder _output = new();

    private ProcessResult? _result;

    internal RunningProcess(string description, Process process, StreamWriter logWriter, string logPath)
    {
        _description = description;
        _process = process;
        _logWriter = logWriter;
        LogPath = logPath;
    }

    public string LogPath { get; }

    public bool HasExited => _result != null || _process.HasExited;

    /// What the process has printed so far
    public string OutputSoFar
    {
        get
        {
            lock (_output)
            {
                return _output.ToString();
            }
        }
    }

    internal void AppendLine(string? line)
    {
        if (line == null)
        {
            return;
        }

        lock (_output)
        {
            _output.AppendLine(line);
            _logWriter.WriteLine(line);
        }
    }

    /// Kills the process tree if it is still running. Wait() then collects what it printed.
    public void Kill()
    {
        if (_result == null && !_process.HasExited)
        {
            _process.Kill(entireProcessTree: true);
        }
    }

    public ProcessResult Wait(TimeSpan timeout)
    {
        if (_result != null)
        {
            return _result;
        }

        bool timedOut = !_process.WaitForExit(timeout);

        if (timedOut)
        {
            _process.Kill(entireProcessTree: true);
            _process.WaitForExit();
        }
        else
        {
            // flushes the redirected streams
            _process.WaitForExit();
        }

        int exitCode = timedOut
            ? -1
            : _process.ExitCode;

        _process.Dispose();
        _logWriter.Dispose();

        _result = new ProcessResult(
            _description,
            exitCode,
            timedOut,
            OutputSoFar,
            LogPath);

        return _result;
    }
}

public static class ProcessRunner
{
    public static ProcessResult Run(
        string description,
        string fileName,
        IEnumerable<string> arguments,
        string workingDirectory,
        string logPath,
        TimeSpan timeout,
        IReadOnlyDictionary<string, string>? environment = null)
    {
        return Start(description, fileName, arguments, workingDirectory, logPath, environment).Wait(timeout);
    }

    public static RunningProcess Start(
        string description,
        string fileName,
        IEnumerable<string> arguments,
        string workingDirectory,
        string logPath,
        IReadOnlyDictionary<string, string>? environment = null)
    {
        ProcessStartInfo startInfo = new ProcessStartInfo(fileName)
        {
            WorkingDirectory = workingDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        foreach (string argument in arguments)
        {
            startInfo.ArgumentList.Add(argument);
        }

        if (environment != null)
        {
            foreach ((string key, string value) in environment)
            {
                startInfo.Environment[key] = value;
            }
        }

        Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);

        // written as it arrives, so a game that hangs until the timeout still leaves a log to look at
        StreamWriter logWriter = new StreamWriter(logPath) { AutoFlush = true };

        Process process = new Process { StartInfo = startInfo };
        RunningProcess running = new RunningProcess(description, process, logWriter, logPath);

        running.AppendLine($"> {fileName} {string.Join(' ', startInfo.ArgumentList)}");

        process.OutputDataReceived += (_, e) => running.AppendLine(e.Data);
        process.ErrorDataReceived += (_, e) => running.AppendLine(e.Data);

        process.Start();
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        return running;
    }
}
