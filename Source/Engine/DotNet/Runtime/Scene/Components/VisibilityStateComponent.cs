using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum VisibilityStateFlags : uint
    {
        None = 0x0,
        AlwaysVisible = 0x1,
        Invalidated = 0x2
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct OctantId
    {
        public ulong IndexBits;
        public byte Depth;
    }

    [ClassBinding(Name="VisibilityStateComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public ref struct VisibilityStateComponent : IComponent
    {
        public VisibilityStateFlags VisibilityStateFlags;
        public OctantId OctantId;
        public IntPtr pVisibilityState;

        public void Dispose()
        {
        }
        
        public static Class Class => Class.GetClass(typeof(VisibilityStateComponent));

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (VisibilityStateComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}
