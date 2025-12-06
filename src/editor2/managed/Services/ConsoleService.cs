using System;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public class LogEntry
    {
        public string Channel { get; set; }
        public int Level { get; set; }
        public double Timestamp { get; set; }
        public string Message { get; set; }
        public string Color { get; set; } // For UI binding
    }

    public class ConsoleService
    {
        private static ConsoleService _instance;
        public static ConsoleService Instance => _instance ??= new ConsoleService();

        public ObservableCollection<LogEntry> Logs { get; } = new ObservableCollection<LogEntry>();

        private NativeBindings.LogCallbackDelegate _logCallback;

        public ConsoleService()
        {
            _logCallback = OnLogMessage;
            
            try
            {
                NativeBindings.Editor_RegisterLogCallback(_logCallback);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Failed to register log callback: {ex.Message}");
            }
        }

        private void OnLogMessage(string channel, int level, double timestamp, string message)
        {
            Dispatcher.UIThread.Post(() =>
            {
                string color = "#FFFFFF";
                switch (level)
                {
                    case 0: color = "#AAAAAA"; break; // Debug
                    case 1: color = "#FFFFFF"; break; // Info
                    case 2: color = "#FFCC00"; break; // Warning
                    case 3: color = "#FF3333"; break; // Error
                    case 4: color = "#FF0000"; break; // Fatal
                }

                Logs.Add(new LogEntry
                {
                    Channel = channel,
                    Level = level,
                    Timestamp = timestamp,
                    Message = message,
                    Color = color
                });

                // Limit history size
                if (Logs.Count > 1000)
                {
                    Logs.RemoveAt(0);
                }
            });
        }

        public void ExecuteCommand(string command)
        {
            NativeBindings.Editor_ExecuteConsoleCommand(command);
        }
    }
}
