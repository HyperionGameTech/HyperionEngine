using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Hyperion.Editor.Services
{
    public enum LocalServerLaunchResult
    {
        Ready,
        Failed,
        Cancelled
    }

    public sealed class PlayInEditorServerService : IDisposable
    {
        public static PlayInEditorServerService Instance { get; } = new PlayInEditorServerService();

        private const string CacheServerExecutableName = "CacheServerCommandlet";
        private const string GameServerExecutableName = "hyperion-sample";

        private const string CacheServerReadyMarker = "CacheServer listening on port";
        private const string GameServerReadyMarker = "Game server ready";

        private static readonly TimeSpan CacheServerStartTimeout = TimeSpan.FromSeconds(60);
        private static readonly TimeSpan CacheServerRefreshTimeout = TimeSpan.FromMinutes(10);
        private static readonly TimeSpan GameServerStartTimeout = TimeSpan.FromMinutes(10);
        private static readonly TimeSpan ProcessExitTimeout = TimeSpan.FromSeconds(5);

        private static readonly HttpClient CacheServerHttpClient = new HttpClient { Timeout = Timeout.InfiniteTimeSpan };

        private static readonly LogChannel GameServerLogChannel = LogChannel.ByName("LocalGameServer");
        private static readonly LogChannel CacheServerLogChannel = LogChannel.ByName("LocalCacheServer");

        private readonly Lock _lock = new();

        private Process? _cacheServerProcess;
        private string? _cacheServerProjectDirectory;
        private uint _cacheServerPort;

        private Process? _gameServerProcess;
        private CancellationTokenSource? _launchCancellation;

        private PlayInEditorServerService()
        {
        }

        public async Task<LocalServerLaunchResult> LaunchGameServerAsync(string projectDirectory, uint gamePort, uint cachePort)
        {
            CancellationTokenSource launchCancellation = new();
            Process? previousGameServer;

            lock (_lock)
            {
                _launchCancellation?.Cancel();
                _launchCancellation = launchCancellation;

                previousGameServer = _gameServerProcess;
                _gameServerProcess = null;
            }

            CancellationToken cancellationToken = launchCancellation.Token;

            try
            {
                if (previousGameServer != null)
                {
                    // The old server still holds the game port until it has fully exited
                    await KillAndWaitAsync(previousGameServer).ConfigureAwait(false);
                }

                if (string.IsNullOrEmpty(projectDirectory) || !Directory.Exists(projectDirectory))
                {
                    Logger.Log(GameServerLogChannel, LogLevel.Error, "Cannot launch local server: project directory '{0}' does not exist", projectDirectory);

                    return LocalServerLaunchResult.Failed;
                }

                await EnsureCacheServerAsync(projectDirectory, cachePort, cancellationToken).ConfigureAwait(false);
                await RefreshCacheServerAsync(projectDirectory, cachePort, cancellationToken).ConfigureAwait(false);

                string stagingDirectory = GetStagingDirectory(projectDirectory);
                string contentDirectory = Path.Combine(stagingDirectory, "Content");
                string cacheDirectory = Path.Combine(stagingDirectory, "Cache");

                Directory.CreateDirectory(contentDirectory);
                Directory.CreateDirectory(cacheDirectory);

                TaskCompletionSource<bool> gameServerReady = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);

                Process gameServer = StartProcess(
                    GameServerExecutableName,
                    [
                        "--server",
                        $"--gameport={gamePort}",
                        $"--cacheserver=http://127.0.0.1:{cachePort}",
                        $"--contentdir={contentDirectory}",
                        $"--cachedir={cacheDirectory}"
                    ],
                    GameServerLogChannel,
                    LogLevel.Info,
                    line =>
                    {
                        if (line.Contains(GameServerReadyMarker, StringComparison.Ordinal))
                        {
                            gameServerReady.TrySetResult(true);
                        }
                    },
                    () => gameServerReady.TrySetResult(false));

                lock (_lock)
                {
                    if (cancellationToken.IsCancellationRequested)
                    {
                        _ = KillAndWaitAsync(gameServer);

                        return LocalServerLaunchResult.Cancelled;
                    }

                    _gameServerProcess = gameServer;
                }

                Logger.Log(GameServerLogChannel, LogLevel.Info, "Waiting for local server on port {0} to sync content and load its world...", gamePort);

                bool isReady;

                try
                {
                    isReady = await gameServerReady.Task.WaitAsync(GameServerStartTimeout, cancellationToken).ConfigureAwait(false);
                }
                catch (TimeoutException)
                {
                    Logger.Log(GameServerLogChannel, LogLevel.Error, "Local server was not ready within {0} minutes", GameServerStartTimeout.TotalMinutes);

                    isReady = false;
                }

                if (!isReady)
                {
                    Logger.Log(GameServerLogChannel, LogLevel.Error, "Local server failed to start (exit code {0})", SafeExitCode(gameServer));

                    StopGameServerProcess(gameServer);

                    return LocalServerLaunchResult.Failed;
                }

                Logger.Log(GameServerLogChannel, LogLevel.Info, "Local server is ready on port {0}", gamePort);

                return LocalServerLaunchResult.Ready;
            }
            catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
            {
                return LocalServerLaunchResult.Cancelled;
            }
            catch (Exception ex)
            {
                Logger.Log(GameServerLogChannel, LogLevel.Error, "Failed to launch local server: {0}", ex.Message);

                return LocalServerLaunchResult.Failed;
            }
        }

        /// <summary>
        /// Makes sure the cache server serves the freshly saved project, for external clients joining an editor-hosted server.
        /// </summary>
        public async Task<LocalServerLaunchResult> StartCacheServerAsync(string projectDirectory, uint cachePort)
        {
            CancellationTokenSource launchCancellation = new CancellationTokenSource();

            lock (_lock)
            {
                _launchCancellation?.Cancel();
                _launchCancellation = launchCancellation;
            }

            CancellationToken cancellationToken = launchCancellation.Token;

            try
            {
                if (string.IsNullOrEmpty(projectDirectory) || !Directory.Exists(projectDirectory))
                {
                    Logger.Log(CacheServerLogChannel, LogLevel.Error, "Cannot start cache server: project directory '{0}' does not exist", projectDirectory);

                    return LocalServerLaunchResult.Failed;
                }

                await EnsureCacheServerAsync(projectDirectory, cachePort, cancellationToken).ConfigureAwait(false);
                await RefreshCacheServerAsync(projectDirectory, cachePort, cancellationToken).ConfigureAwait(false);

                Logger.Log(CacheServerLogChannel, LogLevel.Info, "Clients can sync this project with --cacheserver=http://127.0.0.1:{0}", cachePort);

                return LocalServerLaunchResult.Ready;
            }
            catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
            {
                return LocalServerLaunchResult.Cancelled;
            }
            catch (Exception ex)
            {
                Logger.Log(CacheServerLogChannel, LogLevel.Error, "Failed to start cache server: {0}", ex.Message);

                return LocalServerLaunchResult.Failed;
            }
        }

        private static async Task RefreshCacheServerAsync(string projectDirectory, uint cachePort, CancellationToken cancellationToken)
        {
            Logger.Log(CacheServerLogChannel, LogLevel.Info, "Refreshing cache server manifest for '{0}'...", projectDirectory);

            using CancellationTokenSource refreshTimeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            refreshTimeout.CancelAfter(CacheServerRefreshTimeout);

            try
            {
                using HttpResponseMessage response = await CacheServerHttpClient.GetAsync($"http://127.0.0.1:{cachePort}/refresh?id=0", refreshTimeout.Token).ConfigureAwait(false);
                response.EnsureSuccessStatusCode();
            }
            catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
            {
                throw new TimeoutException($"Cache server did not finish refreshing its manifest within {CacheServerRefreshTimeout.TotalMinutes} minutes");
            }
        }

        private void StopGameServerProcess(Process gameServer)
        {
            lock (_lock)
            {
                if (ReferenceEquals(_gameServerProcess, gameServer))
                {
                    _gameServerProcess = null;
                }
            }

            _ = KillAndWaitAsync(gameServer);
        }

        /// <summary>
        /// Cancels a launch or refresh in progress and kills the session's server, if any. The cache server is left running for the next Play.
        /// </summary>
        public void StopGameServer()
        {
            Process? gameServer;

            lock (_lock)
            {
                _launchCancellation?.Cancel();
                _launchCancellation = null;

                gameServer = _gameServerProcess;
                _gameServerProcess = null;
            }

            if (gameServer != null)
            {
                _ = KillAndWaitAsync(gameServer);
            }
        }

        public void Dispose()
        {
            Process? gameServer;
            Process? cacheServer;

            lock (_lock)
            {
                _launchCancellation?.Cancel();
                _launchCancellation = null;

                gameServer = _gameServerProcess;
                cacheServer = _cacheServerProcess;

                _gameServerProcess = null;
                _cacheServerProcess = null;
                _cacheServerProjectDirectory = null;
            }

            KillNow(gameServer);
            KillNow(cacheServer);
        }

        private async Task EnsureCacheServerAsync(string projectDirectory, uint cachePort, CancellationToken cancellationToken)
        {
            string fullProjectDirectory = Path.GetFullPath(projectDirectory);
            Process? staleCacheServer = null;

            lock (_lock)
            {
                bool isReusable = _cacheServerProcess != null
                    && !_cacheServerProcess.HasExited
                    && _cacheServerPort == cachePort
                    && string.Equals(_cacheServerProjectDirectory, fullProjectDirectory, StringComparison.OrdinalIgnoreCase);

                if (isReusable)
                {
                    return;
                }

                staleCacheServer = _cacheServerProcess;

                _cacheServerProcess = null;
                _cacheServerProjectDirectory = null;
            }

            if (staleCacheServer != null)
            {
                await KillAndWaitAsync(staleCacheServer).ConfigureAwait(false);
            }

            TaskCompletionSource<bool> cacheServerReady = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);

            Process cacheServer = StartProcess(
                CacheServerExecutableName,
                [
                    $"--dir={fullProjectDirectory}",
                    "--dev=true",
                    $"--port={cachePort}"
                ],
                CacheServerLogChannel,
                LogLevel.Verbose,
                line =>
                {
                    if (line.Contains(CacheServerReadyMarker, StringComparison.Ordinal))
                    {
                        cacheServerReady.TrySetResult(true);
                    }
                },
                () => cacheServerReady.TrySetResult(false));

            lock (_lock)
            {
                _cacheServerProcess = cacheServer;
                _cacheServerProjectDirectory = fullProjectDirectory;
                _cacheServerPort = cachePort;
            }

            bool isListening = false;

            try
            {
                isListening = await cacheServerReady.Task.WaitAsync(CacheServerStartTimeout, cancellationToken).ConfigureAwait(false);
            }
            catch (TimeoutException)
            {
            }
            finally
            {
                if (!isListening)
                {
                    lock (_lock)
                    {
                        if (ReferenceEquals(_cacheServerProcess, cacheServer))
                        {
                            _cacheServerProcess = null;
                            _cacheServerProjectDirectory = null;
                        }
                    }

                    _ = KillAndWaitAsync(cacheServer);
                }
            }

            if (!isListening)
            {
                throw new InvalidOperationException($"Cache server did not start listening on port {cachePort} (exit code {SafeExitCode(cacheServer)}). Is the port already in use?");
            }
        }

        private static Process StartProcess(
            string executableName,
            IEnumerable<string> arguments,
            LogChannel logChannel,
            LogLevel outputLogLevel,
            Action<string> onOutputLine,
            Action onExited)
        {
            string binariesDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;
            string executablePath = Path.Combine(binariesDirectory, OperatingSystem.IsWindows() ? executableName + ".exe" : executableName);

            if (!File.Exists(executablePath))
            {
                throw new FileNotFoundException($"'{executableName}' was not found next to the editor. Build it first.", executablePath);
            }

            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = executablePath,
                WorkingDirectory = binariesDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            foreach (string argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

            Process process = new Process { StartInfo = startInfo, EnableRaisingEvents = true };

            process.OutputDataReceived += (_, eventArgs) =>
            {
                if (eventArgs.Data == null)
                {
                    return;
                }

                // No format args, so braces in the child's output are logged as-is
                Logger.Log(logChannel, outputLogLevel, eventArgs.Data);

                onOutputLine(eventArgs.Data);
            };

            process.ErrorDataReceived += (_, eventArgs) =>
            {
                if (eventArgs.Data == null)
                {
                    return;
                }

                // The engine logger writes warnings and errors here, but each line already carries its level, so it isn't promoted
                Logger.Log(logChannel, outputLogLevel, eventArgs.Data);

                onOutputLine(eventArgs.Data);
            };

            process.Exited += (_, _) =>
            {
                Logger.Log(logChannel, LogLevel.Info, "Process exited (exit code {0})", SafeExitCode(process));

                onExited();
            };

            Logger.Log(logChannel, LogLevel.Info, "Starting {0} {1}", executablePath, string.Join(" ", startInfo.ArgumentList));

            process.Start();

            // Closed stdin makes the headless server's console input thread see EOF and exit
            process.StandardInput.Close();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            ChildProcessJob.Assign(process);

            return process;
        }

        private static async Task KillAndWaitAsync(Process process)
        {
            try
            {
                if (!process.HasExited)
                {
                    process.Kill(entireProcessTree: true);
                }

                using CancellationTokenSource exitTimeout = new CancellationTokenSource(ProcessExitTimeout);

                await process.WaitForExitAsync(exitTimeout.Token).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, "Timed out or failed waiting for server process to exit: {0}", ex.Message);
            }
            finally
            {
                process.Dispose();
            }
        }

        private static void KillNow(Process? process)
        {
            if (process == null)
            {
                return;
            }

            try
            {
                if (!process.HasExited)
                {
                    process.Kill(entireProcessTree: true);
                }
            }
            catch (Exception)
            {
                // exiting anyway; the job object kills anything left behind
            }
            finally
            {
                process.Dispose();
            }
        }

        private static string SafeExitCode(Process process)
        {
            try
            {
                return process.HasExited ? process.ExitCode.ToString() : "still running";
            }
            catch (Exception ex) when (ex is InvalidOperationException or ObjectDisposedException)
            {
                return "unknown";
            }
        }

        // Stable per project so repeat launches only sync what changed
        private static string GetStagingDirectory(string projectDirectory)
        {
            string fullProjectDirectory = Path.GetFullPath(projectDirectory).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            string binariesDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;

            uint hash = 2166136261;

            foreach (char character in fullProjectDirectory.ToLowerInvariant())
            {
                hash = (hash ^ character) * 16777619;
            }

            return Path.Combine(binariesDirectory, "PlayInEditorServer", $"{Path.GetFileName(fullProjectDirectory)}_{hash:x8}");
        }

        /// <summary>
        /// Windows job object with kill-on-close, so server processes die with the editor even if it crashes.
        /// </summary>
        private static class ChildProcessJob
        {
            private const int JobObjectExtendedLimitInformation = 9;
            private const uint JobObjectLimitKillOnJobClose = 0x2000;

            private static readonly Lazy<IntPtr> JobHandle = new Lazy<IntPtr>(CreateJob);

            public static void Assign(Process process)
            {
                if (!OperatingSystem.IsWindows())
                {
                    return;
                }

                IntPtr jobHandle = JobHandle.Value;

                if (jobHandle == IntPtr.Zero || !AssignProcessToJobObject(jobHandle, process.Handle))
                {
                    Logger.Log(LogLevel.Warning, "Could not tie server process {0} to the editor's lifetime; it may outlive an editor crash", process.Id);
                }
            }

            private static IntPtr CreateJob()
            {
                IntPtr jobHandle = CreateJobObjectW(IntPtr.Zero, null);

                if (jobHandle == IntPtr.Zero)
                {
                    return IntPtr.Zero;
                }

                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInformation = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
                limitInformation.BasicLimitInformation.LimitFlags = JobObjectLimitKillOnJobClose;

                int length = Marshal.SizeOf<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>();
                IntPtr limitInformationPointer = Marshal.AllocHGlobal(length);

                try
                {
                    Marshal.StructureToPtr(limitInformation, limitInformationPointer, false);

                    if (!SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation, limitInformationPointer, (uint)length))
                    {
                        return IntPtr.Zero;
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(limitInformationPointer);
                }

                // Intentionally never closed: the handle closing when the editor process exits is what kills the children
                return jobHandle;
            }

            [StructLayout(LayoutKind.Sequential)]
            private struct JOBOBJECT_BASIC_LIMIT_INFORMATION
            {
                public long PerProcessUserTimeLimit;
                public long PerJobUserTimeLimit;
                public uint LimitFlags;
                public UIntPtr MinimumWorkingSetSize;
                public UIntPtr MaximumWorkingSetSize;
                public uint ActiveProcessLimit;
                public UIntPtr Affinity;
                public uint PriorityClass;
                public uint SchedulingClass;
            }

            [StructLayout(LayoutKind.Sequential)]
            private struct IO_COUNTERS
            {
                public ulong ReadOperationCount;
                public ulong WriteOperationCount;
                public ulong OtherOperationCount;
                public ulong ReadTransferCount;
                public ulong WriteTransferCount;
                public ulong OtherTransferCount;
            }

            [StructLayout(LayoutKind.Sequential)]
            private struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
            {
                public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
                public IO_COUNTERS IoInfo;
                public UIntPtr ProcessMemoryLimit;
                public UIntPtr JobMemoryLimit;
                public UIntPtr PeakProcessMemoryUsed;
                public UIntPtr PeakJobMemoryUsed;
            }

            [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            private static extern IntPtr CreateJobObjectW(IntPtr jobAttributes, string? name);

            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern bool SetInformationJobObject(IntPtr job, int infoClass, IntPtr info, uint infoLength);

            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
        }
    }
}
