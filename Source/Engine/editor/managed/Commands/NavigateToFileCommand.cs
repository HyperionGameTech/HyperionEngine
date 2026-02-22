using System;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.Commands
{

    public class NavigateToFileCommand : ICommand
    {
        public static readonly NavigateToFileCommand DefaultInstance = new NavigateToFileCommand();

        public NavigateToFileCommand()
        {
        }

        public bool CanExecute(object? parameter) => parameter is LogEntry logEntry && !string.IsNullOrEmpty(logEntry.FileName);

        public void Execute(object? parameter)
        {
            if (parameter is not LogEntry logEntry)
            {
                return;
            }

            try
            {
                ProcessStartInfo? startInfo = null;
                if (OperatingSystem.IsWindows())
                {
                    startInfo = new ProcessStartInfo
                    {
                        FileName = "cmd.exe",
                        Arguments = $"/C code --goto \"{logEntry.FileName}:{logEntry.LineNumber}\"",
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                }
                else if (OperatingSystem.IsLinux())
                {
                    startInfo = new ProcessStartInfo
                    {
                        FileName = "/bin/bash",
                        Arguments = $"-c \"code --goto \\\"{logEntry.FileName}:{logEntry.LineNumber}\\\"\"",
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                }
                else if (OperatingSystem.IsMacOS())
                {
                    startInfo = new ProcessStartInfo
                    {
                        FileName = "/bin/zsh",
                        Arguments = $"-l -c \"code --goto \\\"{logEntry.FileName}:{logEntry.LineNumber}\\\"\"",
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                }

                if (startInfo != null)
                {
                    using Process process = Process.Start(startInfo)!;
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Failed to navigate to file: {ex.Message}");
            }
        }

        public event EventHandler? CanExecuteChanged;

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}