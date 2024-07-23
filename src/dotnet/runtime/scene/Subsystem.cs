using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Subsystem
    {
        internal IntPtr ptr;

        internal Subsystem(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public string Name
        {
            get
            {
                return Subsystem_GetName(ptr);
            }
        }

        [DllImport("hyperion", EntryPoint = "Subsystem_GetName")]
        [return: MarshalAs(UnmanagedType.LPStr)]
        private static extern string Subsystem_GetName(IntPtr subsystemPtr);
    }
}