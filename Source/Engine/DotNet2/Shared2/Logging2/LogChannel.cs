using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class LogChannel
    {
        internal IntPtr ptr;
        internal bool isDynamic;

        public static LogChannel ByName(string name)
        {
            Name nameStruct = new Name(name, weak: true);
            IntPtr ptr = Logger_FindLogChannel(ref nameStruct);

            if (ptr != IntPtr.Zero)
            {
                return new LogChannel(ptr, isDynamic: false);
            }

            return new LogChannel(name);
        }

        internal LogChannel(IntPtr ptr, bool isDynamic)
        {
            this.ptr = ptr;
            this.isDynamic = isDynamic;
        }

        public LogChannel(string name)
        {
            ptr = Logger_CreateLogChannel(name);
            isDynamic = true;
        }

        ~LogChannel()
        {
            if (isDynamic && ptr != IntPtr.Zero)
            {
                Logger_DestroyLogChannel(ptr);
            }
        }

        [DllImport("hyperion", EntryPoint = "Logger_FindLogChannel")]
        private static extern IntPtr Logger_FindLogChannel(ref Name name);

        [DllImport("hyperion", EntryPoint = "Logger_CreateLogChannel")]
        private static extern IntPtr Logger_CreateLogChannel([MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport("hyperion", EntryPoint = "Logger_DestroyLogChannel")]
        private static extern void Logger_DestroyLogChannel(IntPtr logChannelPtr);
    }
}