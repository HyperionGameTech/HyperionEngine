using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 24)]
    public struct ObjectReference
    {
        public Guid guid;
        public IntPtr ptr;

        public bool IsValid
        {
            get
            {
                return guid != Guid.Empty && ptr != IntPtr.Zero;
            }
        }

        public object? LoadObject()
        {
            return ptr == IntPtr.Zero ? null : GCHandle.FromIntPtr(ptr).Target;
        }
    }
}