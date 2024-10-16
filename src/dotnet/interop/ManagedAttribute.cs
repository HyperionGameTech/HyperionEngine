using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 32, Pack = 8)]
    public struct ManagedAttribute
    {
        [FieldOffset(0)]
        public IntPtr classObjectPtr;

        [FieldOffset(8)]
        public ObjectReference objectReference;
    }

    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 8)]
    public struct ManagedAttributeHolder : IDisposable
    {
        [FieldOffset(0)]
        public uint managedAttributesSize;

        [FieldOffset(8)]
        public IntPtr managedAttributesPtr;

        public void Dispose()
        {
            if (managedAttributesPtr != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(managedAttributesPtr);
            }
        }
    }
}