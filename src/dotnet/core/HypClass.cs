using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypClass
    {
        private IntPtr ptr;

        internal HypClass(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        internal IntPtr Address
        {
            get
            {
                return ptr;
            }
            set
            {
                ptr = value;
            }
        }

        public static HypClass? GetClass(string name)
        {
            IntPtr ptr = HypClass_GetClassByName(name);

            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            return new HypClass(ptr);
        }
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);
    }
}