using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum VisibilityStateFlags : uint
    {
        None = 0x0,
        AlwaysVisible = 0x1
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct OctantID
    {
        public ulong indexBits;
        public byte depth;
    }

    [HypClassBinding(Name="VisibilityStateComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public struct VisibilityStateComponent : IComponent
    {
        public VisibilityStateFlags visibilityStateFlags;
        public OctantID octantID;
        public IntPtr visibilityStatePtr;
    }
}
