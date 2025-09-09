using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "DynamicLibrary")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct DynamicLibrary
    {
        [FieldOffset(0), MarshalAs(UnmanagedType.SysInt)]
        private IntPtr impl;

        public DynamicLibrary(string path)
        {
            impl = IntPtr.Zero;
            
            // extension method
            this.SetPath(path);
        }
    }
}