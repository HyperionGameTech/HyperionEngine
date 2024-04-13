using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LogType : int
    {
        Info,
        Warn,
        Error,
        Fatal,
        Debug
    }
    
    public class Logger
    {
        // Log with variadic arguments
        public static void Log(LogType logLevel, string message, params object[] args)
        {
            var frame = new System.Diagnostics.StackFrame(1, true);

            string formattedMessage = message;

            try
            {
                formattedMessage = string.Format(message + "\n", args);
            }
            catch (FormatException)
            {
                // Do nothing, just log as is
            }

            IntPtr formattedMessagePtr = Marshal.StringToHGlobalAnsi(formattedMessage);

            if (frame == null)
            {
                Logger_Log((int)logLevel, IntPtr.Zero, 0, formattedMessagePtr);

                Marshal.FreeHGlobal(formattedMessagePtr);

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

            IntPtr funcNamePtr = Marshal.StringToHGlobalAnsi(fileName + ":" + funcName);

            Logger_Log((int)logLevel, funcNamePtr, line, formattedMessagePtr);

            Marshal.FreeHGlobal(funcNamePtr);
            Marshal.FreeHGlobal(formattedMessagePtr);
        }

        [DllImport("hyperion", EntryPoint = "Logger_Log")]
        private static extern void Logger_Log(int logLevel, IntPtr funcNamePtr, uint line, IntPtr messagePtr);
    }
}