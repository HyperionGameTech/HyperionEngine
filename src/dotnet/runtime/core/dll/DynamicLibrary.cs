using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "DynamicLibrary")]
    [StructLayout(LayoutKind.Explicit, Size = 8, Pack = 8)]
    public struct DynamicLibrary
    {
        [FieldOffset(0)]
        private RefCountedPtr impl;

        public DynamicLibrary()
        {
            impl = RefCountedPtr.Null;
        }

        public DynamicLibrary(string path)
        {
            impl = RefCountedPtr.Null;

            // extension method
            this.SetPath(path);
        }
    }
}