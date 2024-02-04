using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedMethod
    {
        public Guid guid;
    }
}