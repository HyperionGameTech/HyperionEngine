using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct ObjectReference : IDisposable
    {
        public IntPtr weakHandle;
        public IntPtr strongHandle;

        public bool IsValid
        {
            get
            {
                return weakHandle != IntPtr.Zero;
            }
        }
        
        public void Dispose()
        {
            if (strongHandle != IntPtr.Zero)
            {
                GCHandle.FromIntPtr(strongHandle).Free();
                strongHandle = IntPtr.Zero;
            }

            if (weakHandle != IntPtr.Zero)
            {
                GCHandle.FromIntPtr(weakHandle).Free();
                weakHandle = IntPtr.Zero;
            }
        }

        public object? LoadObject()
        {
            return weakHandle == IntPtr.Zero ? null : GCHandle.FromIntPtr(weakHandle).Target;
        }
    }
}