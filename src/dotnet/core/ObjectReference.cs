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

        public bool MakeStrong()
        {
            if (strongHandle != IntPtr.Zero)
                // Already allocated
                return true;

            if (weakHandle == IntPtr.Zero)
                return false;

            object? obj = LoadObject();

            if (obj == null)
                return false;

            strongHandle = GCHandle.ToIntPtr(GCHandle.Alloc(obj, GCHandleType.Normal));

            return true;
        }

        public bool MakeWeak()
        {
            if (weakHandle == IntPtr.Zero)
                return false;

            if (strongHandle == IntPtr.Zero)
                return true;

            if (strongHandle != IntPtr.Zero)
            {
                GCHandle.FromIntPtr(strongHandle).Free();
                strongHandle = IntPtr.Zero;
            }

            return true;
        }
    }
}