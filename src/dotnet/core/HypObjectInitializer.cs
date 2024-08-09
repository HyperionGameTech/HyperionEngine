using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential)]
    public struct HypObjectInitializer
    {
        public static readonly HypObjectInitializer FromManaged = new HypObjectInitializer {
            hypClassPtr = IntPtr.Zero,
            nativeAddress = IntPtr.Zero
        };

        public IntPtr hypClassPtr = IntPtr.Zero;
        public IntPtr nativeAddress = IntPtr.Zero;

        public HypObjectInitializer()
        {
        }
    }
}