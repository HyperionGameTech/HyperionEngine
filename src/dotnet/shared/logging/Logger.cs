using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LogType : uint
    {
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct LogCategory
    {
        private uint value;

        public LogCategory(uint value)
        {
            this.value = value;
        }

        public uint Value
        {
            get { return value; }
        }
    }

    public class Logger : ObjectBase
    {
        private static LogChannel defaultChannel = LogChannel.ByName("Default");

        public static void Log(LogType logLevel, string message, params object?[] args)
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
                Logger_Log(defaultChannel.ptr, (uint)logLevel, string.Empty, 0, formattedMessage);

                return;
            }

            string? fileName = frame.GetFileName();
            uint line = (uint)frame.GetFileLineNumber();

            Logger_Log(defaultChannel.ptr, (uint)logLevel, fileName, line, formattedMessage);
        }

        public static void Log(LogChannel channel, LogType logLevel, string message, params object?[] args)
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
                Logger_Log(channel.ptr, (uint)logLevel, "", 0, formattedMessage);

                return;
            }

            string? fileName = frame.GetFileName();
            uint line = (uint)frame.GetFileLineNumber();

            Logger_Log(channel.ptr, (uint)logLevel, fileName, line, formattedMessage);
        }

        [DllImport("hyperion", EntryPoint = "Logger_Log")]
        private static extern void Logger_Log(IntPtr logChannelPtr, uint logLevel, [MarshalAs(UnmanagedType.LPStr)] string fileName, uint line, [MarshalAs(UnmanagedType.LPStr)] string message);
    }
}