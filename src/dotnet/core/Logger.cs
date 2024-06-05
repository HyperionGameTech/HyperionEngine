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
    
    public class Logger
    {
        private static LogChannel defaultChannel = new LogChannel("Default");

        public static void Log(LogType logLevel, string message, params object[] args)
        {
            Log(defaultChannel, logLevel, message, args);
        }

        public static void Log(LogChannel channel, LogType logLevel, string message, params object[] args)
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

            string? funcName = frame.GetMethod()?.Name;

            if (funcName == null)
            {
                funcName = "<Unknown>";
            }

            string? fileName = frame.GetFileName();

            if (fileName != null)
            {
                fileName = fileName.Replace("\\", "/").Substring(fileName.LastIndexOf('/') + 1);
            }
            
            uint line = (uint)frame.GetFileLineNumber();

            Logger_Log(channel.ptr, (uint)logLevel, funcName, line, formattedMessage);
        }

        [DllImport("hyperion", EntryPoint = "Logger_Log")]
        private static extern void Logger_Log(IntPtr logChannelPtr, uint logLevel, [MarshalAs(UnmanagedType.LPStr)] string funcName, uint line, [MarshalAs(UnmanagedType.LPStr)] string message);
    }
}