using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using Avalonia.Threading;
using DynamicData;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.Services
{
    public struct LogEntry
    {
        private static readonly string[] LogLevelColorTable =
        {
            "#4c0b0b",  // Fatal
            "#FF0000",  // Error
            "#ffc65d",  // Warning
            "#FFFFFF",  // Info
            "#FFFFFF",  // Verbose
            "#FFFFFF",  // Debug
        };

        public string Channel { get; set; }
        public LogLevel Level { get; set; }
        public double Timestamp { get; set; }
        public string FileName { get; set; }
        public int LineNumber { get; set; }
        public string Message { get; set; }

        private string _color;
        private string _fileLocationText;

        public string Color => _color;
        public bool HasFileLocation => !string.IsNullOrEmpty(FileName);
        public string FileLocationText => _fileLocationText;

        public NavigateToFileCommand NavigateToFileCommand => NavigateToFileCommand.DefaultInstance;

        public LogEntry()
        {
            Channel = string.Empty;
            Level = LogLevel.Info;
            Timestamp = 0.0;
            FileName = string.Empty;
            LineNumber = 0;
            Message = string.Empty;
            _color = "#FFFFFF";
            _fileLocationText = string.Empty;
        }

        internal static LogEntry Create(string channel, LogLevel level, double timestamp, string fileName, int lineNumber, string message)
        {
            LogEntry entry = new LogEntry
            {
                Channel = channel,
                Level = level,
                Timestamp = timestamp,
                FileName = fileName,
                LineNumber = lineNumber,
                Message = message
            };

            int idx = (int)level;
            entry._color = (uint)idx < (uint)LogLevelColorTable.Length ? LogLevelColorTable[idx] : "#FFFFFF";
            entry._fileLocationText = ComputeFileLocationText(fileName, lineNumber);
            return entry;
        }

        private static string ComputeFileLocationText(string fileName, int lineNumber)
        {
            if (string.IsNullOrEmpty(fileName))
                return string.Empty;

            int lastSep = Math.Max(fileName.LastIndexOf('/'), fileName.LastIndexOf('\\'));
            string displayName = lastSep >= 0 && lastSep + 1 < fileName.Length
                ? fileName[(lastSep + 1)..]
                : fileName;

            return $"[{displayName}:{lineNumber}]";
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

        private readonly LogEntryRingBuffer _ringBuffer = new LogEntryRingBuffer(4096);

        private List<LogEntry> _pendingEntries = new List<LogEntry>(256);
        private List<LogEntry> _submittingEntries = new List<LogEntry>(256);
        private int _isSubmittingPendingEntries = 0; // atomic

        private readonly List<string> _cvarNames = new List<string>(256);
        private readonly List<string> _commandletNames = new List<string>(128);
        private readonly List<string> _editorCommandNames = new List<string>(128);
        private readonly List<string> _allCompletionNames = new List<string>(512);
        private bool _completionNamesLoaded = false;

        public ConsoleService()
        {
            _logsSource.Connect()
                .Bind(out _logs)
                .Subscribe();

            _logCallback = OnLogMessage;

            try
            {
                NativeBindings.Hyp_RegisterLogCallback(_logCallback);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Failed to register log callback: {ex.Message}");
            }
        }

        private void OnLogMessage(string channel, LogLevel level, double timestamp, string fileName, int lineNumber, string message)
        {
            // avoid TrimEnd allocation when the string doesn't need trimming
            string msg = message.Length > 0 && char.IsWhiteSpace(message[^1]) ? message.TrimEnd() : message;
            _ringBuffer.Enqueue(LogEntry.Create(channel, level, timestamp, fileName, lineNumber, msg));
        }

        public void ExecuteCommand(ReadOnlySpan<string> args)
        {
            Debug.Assert(args.Length > 0, "Command arguments cannot be empty.");

            // make char** from List<string>
            int argc = args.Length;

            unsafe
            {
                Span<IntPtr> argvPtr = stackalloc IntPtr[argc];

                fixed (IntPtr* argv = &argvPtr[0])
                {
                    try
                    {
                        for (int i = 0; i < argc; i++)
                        {
                            byte[] utf8Bytes = System.Text.Encoding.UTF8.GetBytes(args[i]);

                            IntPtr stringPtr = Marshal.AllocHGlobal(utf8Bytes.Length + 1);
                            Marshal.Copy(utf8Bytes, 0, stringPtr, utf8Bytes.Length);
                            Marshal.WriteByte(stringPtr + utf8Bytes.Length, 0); // null terminator

                            argv[i] = stringPtr;
                        }

                        // execute with int argc, char** argv
                        int returnValue = NativeBindings.Hyp_ExecuteConsoleCommand(argc, (nint)argv);
                        if (returnValue != 0)
                        {
                            throw new Exception("The command returned with error code: " + returnValue);
                        }
                    }
                    finally
                    {
                        // free the strings we allocated
                        for (int i = 0; i < argc; i++)
                        {
                            Marshal.FreeHGlobal(argvPtr[i]);
                        }
                    }
                }
            }
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
            _ringBuffer.DrainTo(_pendingEntries);

            if (_pendingEntries.Count == 0)
                return;

            if (Interlocked.CompareExchange(ref _isSubmittingPendingEntries, 1, 0) == 1)
            {
                // Another submission is in progress
                return;
            }

            List<LogEntry> toSubmit = _pendingEntries;
            _pendingEntries = _submittingEntries;
            _submittingEntries = toSubmit;

            Dispatcher.UIThread.Post(() =>
            {
                _logsSource.Edit(list =>
                {
                    list.AddRange(toSubmit);

                    if (list.Count > 1000)
                    {
                        list.RemoveRange(0, list.Count - 1000);
                    }
                });

                toSubmit.Clear(); // return to pool for next swap
                _isSubmittingPendingEntries = 0;
            });
        }

        public void LoadCompletionNames()
        {
            if (_completionNamesLoaded)
                return;

            try
            {
                NativeBindings.Hyp_GetAllCVarNames(OnCVarNameCallback, IntPtr.Zero);
                NativeBindings.Hyp_GetAllCommandletNames(OnCommandletNameCallback, IntPtr.Zero);
                NativeBindings.Hyp_GetAllEditorCommandNames(OnEditorCommandNameCallback, IntPtr.Zero);

                _cvarNames.Sort();
                _commandletNames.Sort();
                _editorCommandNames.Sort();

                _allCompletionNames.Clear();
                _allCompletionNames.AddRange(_cvarNames);
                _allCompletionNames.AddRange(_commandletNames);
                _allCompletionNames.AddRange(_editorCommandNames);
                _allCompletionNames.Sort();

                _completionNamesLoaded = true;
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Failed to load completion names: {ex.Message}");
            }
        }

        private void OnCVarNameCallback(string name, IntPtr userData)
        {
            _cvarNames.Add(name);
        }

        private void OnCommandletNameCallback(string name, IntPtr userData)
        {
            _commandletNames.Add(name);
        }

        private void OnEditorCommandNameCallback(string name, IntPtr userData)
        {
            _editorCommandNames.Add(name);
        }

        public IReadOnlyList<string> GetCompletions(string prefix)
        {
            if (!_completionNamesLoaded)
                LoadCompletionNames();

            if (string.IsNullOrEmpty(prefix))
                return _allCompletionNames;

            var matches = new List<string>();
            int prefixLen = prefix.Length;

            foreach (var name in _allCompletionNames)
            {
                if (prefixLen <= name.Length &&
                    string.Compare(name, 0, prefix, 0, prefixLen, StringComparison.OrdinalIgnoreCase) == 0)
                {
                    matches.Add(name);
                }
            }

            return matches;
        }

        private sealed class LogEntryRingBuffer
        {
            private readonly LogEntry[] _entries;
            private readonly int _mask;
            private int _head;
            private int _tail;
            private SpinLock _spinLock = new SpinLock(enableThreadOwnerTracking: false);

            public LogEntryRingBuffer(int capacity)
            {
                Debug.Assert(capacity > 0 && (capacity & (capacity - 1)) == 0,
                    "Ring buffer capacity must be a power of 2.");
                _entries = new LogEntry[capacity];
                _mask = capacity - 1;
            }

            public void Enqueue(in LogEntry entry)
            {
                bool lockTaken = false;
                try
                {
                    _spinLock.Enter(ref lockTaken);
                    _entries[_tail & _mask] = entry;
                    _tail++;
                    
                    // buffer full
                    if (_tail - _head > _entries.Length)
                        _head = _tail - _entries.Length;
                }
                finally
                {
                    if (lockTaken)
                        _spinLock.Exit(useMemoryBarrier: false);
                }
            }

            public void DrainTo(List<LogEntry> target)
            {
                bool lockTaken = false;
                try
                {
                    _spinLock.Enter(ref lockTaken);
                    int count = _tail - _head;
                    for (int i = 0; i < count; i++)
                        target.Add(_entries[(_head + i) & _mask]);
                    _head = _tail;
                }
                finally
                {
                    if (lockTaken)
                        _spinLock.Exit(useMemoryBarrier: false);
                }
            }
        }
    }
}
