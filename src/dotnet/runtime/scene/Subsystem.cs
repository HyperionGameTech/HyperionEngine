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
                IntPtr stringPtr = Subsystem_GetName(ptr);
                
                return Marshal.PtrToStringAnsi(stringPtr);
            }
        }

        [DllImport("hyperion", EntryPoint = "Subsystem_GetName")]
        private static extern IntPtr Subsystem_GetName(IntPtr subsystemPtr);
    }
}