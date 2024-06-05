using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class LogChannel
    {
        internal IntPtr ptr;

        internal LogChannel(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public LogChannel(string name)
        {
            ptr = Logger_CreateLogChannel(name);
        }

        ~LogChannel()
        {
            if (ptr != IntPtr.Zero)
            {
                Logger_DestroyLogChannel(ptr);
            }
        }

        [DllImport("hyperion", EntryPoint = "Logger_CreateLogChannel")]
        private static extern IntPtr Logger_CreateLogChannel([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "Logger_DestroyLogChannel")]
        private static extern void Logger_DestroyLogChannel(IntPtr logChannelPtr);
    }
}