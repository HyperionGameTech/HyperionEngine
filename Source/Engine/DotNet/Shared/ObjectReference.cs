using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct ObjectReference : IDisposable
    {
        public IntPtr WeakHandle;
        public IntPtr StrongHandle;

        public bool IsValid
        {
            get
            {
                return WeakHandle != IntPtr.Zero;
            }
        }
        
        public void Dispose()
        {
            if (StrongHandle != IntPtr.Zero)
            {
                GCHandle.FromIntPtr(StrongHandle).Free();
                StrongHandle = IntPtr.Zero;
            }

            if (WeakHandle != IntPtr.Zero)
            {
                GCHandle.FromIntPtr(WeakHandle).Free();
                WeakHandle = IntPtr.Zero;
            }
        }

        public object? LoadObject()
        {
            return WeakHandle == IntPtr.Zero
                ? null
                : GCHandle.FromIntPtr(WeakHandle).Target;
        }
    }
}