using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Immutable;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using DynamicData;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.Services
{
    public struct LogEntry
    {
        private static readonly ImmutableDictionary<int, string> LogLevelColors = new Dictionary<int, string>
        {
            { 0, "#FFFFFF" }, // Debug - White
            { 1, "#FFFFFF" }, // Info - White
            { 2, "#FFA500" }, // Warning - Orange
            { 3, "#FF0000" }, // Error - Red
            { 4, "#FF00FF" }  // Fatal - Magenta
        }.ToImmutableDictionary();

        public string Channel { get; set; }
        public int Level { get; set; }
        public double Timestamp { get; set; }
        public string FileName { get; set; }
        public int LineNumber { get; set; }
        public string Message { get; set; }

        public string Color => LogLevelColors.TryGetValue(Level, out string? color) ? color : "#FFFFFF";

        public bool HasFileLocation => !string.IsNullOrEmpty(FileName);
        private static ReadOnlySpan<char> GetDisplayFileName(string? filePath)
        {
            if (string.IsNullOrEmpty(filePath))
            {
                return ReadOnlySpan<char>.Empty;
            }

            int lastSeparatorIndex = Math.Max(filePath.LastIndexOf('/'), filePath.LastIndexOf('\\'));
            if (lastSeparatorIndex >= 0 && lastSeparatorIndex + 1 < filePath.Length)
            {
                return filePath.AsSpan(lastSeparatorIndex + 1);
            }
            return filePath.AsSpan();
        }

        public string FileLocationText => HasFileLocation ? $"[{GetDisplayFileName(FileName)}:{LineNumber}]" : string.Empty;

        public NavigateToFileCommand NavigateToFileCommand => NavigateToFileCommand.DefaultInstance;

        public LogEntry()
        {
            Channel = string.Empty;
            Level = 0;
            Timestamp = 0.0;
            FileName = string.Empty;
            LineNumber = 0;
            Message = string.Empty;
        }
    }

    public class ConsoleService
    {
        private static ConsoleService _instance;
        public static ConsoleService Instance => _instance ??= new ConsoleService();

        private readonly SourceList<LogEntry> _logsSource = new SourceList<LogEntry>();
        private readonly ReadOnlyObservableCollection<LogEntry> _logs;
        public ReadOnlyObservableCollection<LogEntry> Logs => _logs;

        private LogCallbackDelegate _logCallback;
        private ConcurrentQueue<LogEntry> _logQueue = new ConcurrentQueue<LogEntry>();
        private List<LogEntry> _pendingEntries = new List<LogEntry>();
        private int _isSubmittingPendingEntries = 0; // atomic

        public ConsoleService()
        {
            _logsSource.Connect()
                .Bind(out _logs)
                .Subscribe();

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

        private void OnLogMessage(string channel, int level, double timestamp, string fileName, int lineNumber, string message)
        {
            _logQueue.Enqueue(new LogEntry
            {
                Channel = channel,
                Level = level,
                Timestamp = timestamp,
                FileName = fileName,
                LineNumber = lineNumber,
                Message = message.TrimEnd(),
            });
        }

        public void ExecuteCommand(string command)
        {
            NativeBindings.Editor_ExecuteConsoleCommand(command);
        }

        public void ClearLogs()
        {
            Dispatcher.UIThread.Post(() =>
            {
                _logsSource.Clear();
            });
        }

        public void ProcessLogQueue()
        {
            LogEntry[]? entriesArray = null;

            while (_logQueue.TryDequeue(out LogEntry logEntry))
            {
                _pendingEntries.Add(logEntry);
            }

            if (_pendingEntries.Count == 0)
            {
                return;
            }

            if (Interlocked.CompareExchange(ref _isSubmittingPendingEntries, 1, 0) == 1)
            {
                // Another submission is in progress
                return;
            }

            entriesArray = _pendingEntries.ToArray();
            _pendingEntries.Clear();

            Dispatcher.UIThread.Post(() =>
                {
                    _logsSource.Edit(list =>
                    {
                        list.AddRange(entriesArray!);

                        if (list.Count > 1000)
                        {
                            list.RemoveRange(0, list.Count - 1000);
                        }
                    });

                    _isSubmittingPendingEntries = 0;
                });
        }
    }
}
