using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "LogLevel")]
    public enum LogLevel : byte
    {
        Fatal,
        Error,
        Warning,
        Info,
        Verbose,
        Debug
    }

    public class Logger : ObjectBase
    {
        private static LogChannel defaultChannel = LogChannel.ByName("Default");

        public static void Log(LogLevel logLevel, string message, params object?[] args)
        {
            var frame = new System.Diagnostics.StackFrame(1, true);

            string formattedMessage = message;

            try
            {
                formattedMessage = string.Format(message, args);
            }
            catch (FormatException)
            {
                // Do nothing, just log as is
            }

            if (frame == null)
            {
                Logger_Log(defaultChannel.ptr, (uint)logLevel, string.Empty, 0, formattedMessage + '\n');

                return;
            }

            string? fileName = frame.GetFileName();
            uint line = (uint)frame.GetFileLineNumber();

            Logger_Log(defaultChannel.ptr, (uint)logLevel, fileName ?? string.Empty, line, formattedMessage + '\n');
        }

        public static void Log(LogChannel channel, LogLevel logLevel, string message, params object?[] args)
        {
            var frame = new System.Diagnostics.StackFrame(1, true);

            string formattedMessage = message;

            try
            {
                formattedMessage = string.Format(message, args);
            }
            catch (FormatException)
            {
                // Do nothing, just log as is
            }

            if (frame == null)
            {
                Logger_Log(channel.ptr, (uint)logLevel, string.Empty, 0, formattedMessage + '\n');

                return;
            }

            string? fileName = frame.GetFileName();
            uint line = (uint)frame.GetFileLineNumber();

            Logger_Log(channel.ptr, (uint)logLevel, fileName ?? string.Empty, line, formattedMessage + '\n');
        }

        [DllImport("hyperion", EntryPoint = "Logger_Log")]
        private static extern void Logger_Log(IntPtr logChannelPtr, uint logLevel, [MarshalAs(UnmanagedType.LPStr)] string fileName, uint line, [MarshalAs(UnmanagedType.LPStr)] string message);
    }
}