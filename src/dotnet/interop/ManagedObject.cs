using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 24)]
    public struct ObjectReference
    {
        public Guid guid;
        public IntPtr ptr;
    }
}