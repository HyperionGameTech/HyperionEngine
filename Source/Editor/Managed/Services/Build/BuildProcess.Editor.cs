using System;
using System.Diagnostics;
using System.Threading.Tasks;

namespace Hyperion.Editor.Services
{
    internal static partial class BuildProcess
    {
        public static async Task<int> RunAsync(ProcessStartInfo startInfo, LogChannel logChannel, Action<string> onOutputLine)
        {
            using Process process = new Process { StartInfo = startInfo };

            void OnLine(string? line)
            {
                if (line == null)
                {
                    return;
                }

                Logger.Log(logChannel, LogLevel.Info, line);

                onOutputLine(line);
            }

            process.OutputDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);
            process.ErrorDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);

            process.Start();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync().ConfigureAwait(false);

            return process.ExitCode;
        }
    }
}
