using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Assert
    {
        public static void Throw(bool condition, string? message = null, params object[] args)
        {
            if (!condition)
            {
                var frame = new System.Diagnostics.StackFrame(1, true);

                if (message == null || ((string)message).Length == 0)
                {
                    Assert_Throw(null, frame.GetMethod()?.Name ?? null, (uint)frame.GetFileLineNumber());
                }
                else
                {
                    if (args == null)
                    {
                        args = Array.Empty<object>();
                    }

                    try
                    {
                        string formattedMessage = string.Format((string)message, args);

                        Assert_Throw((string)message, frame.GetMethod()?.Name ?? null, (uint)frame.GetFileLineNumber());
                    }
                    catch (FormatException)
                    {
                        // Do nothing, just log as is
                        Assert_Throw((string)message, frame.GetMethod()?.Name ?? null, (uint)frame.GetFileLineNumber());
                    }
                }
            }
        }

        [DllImport("hyperion", EntryPoint = "Assert_Throw")]
        private static extern void Assert_Throw([MarshalAs(UnmanagedType.LPStr)] string message, [MarshalAs(UnmanagedType.LPStr)] string funcName, uint line);
    }
}