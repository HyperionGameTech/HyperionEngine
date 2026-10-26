using System.Collections.Generic;
using System.Diagnostics;

namespace Hyperion.Editor.Services
{
    internal static partial class BuildProcess
    {
        public static ProcessStartInfo CreateStartInfo(string fileName, string workingDirectory, IEnumerable<string> arguments)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = fileName,
                WorkingDirectory = workingDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            foreach (string argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

            return startInfo;
        }
    }
}
