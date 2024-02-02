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
    public struct VisibilityStateSnapshot
    {
        public ulong bits;
        public ushort nonce;
    }

    [StructLayout(LayoutKind.Sequential, Size = 48)]
    public struct VisibilityState
    {
        // C++ side has 3 snapshots
        private VisibilityStateSnapshot snapshot0;
        private VisibilityStateSnapshot snapshot1;
        private VisibilityStateSnapshot snapshot2;
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct OctantID
    {
        public ulong indexBits;
        public byte depth;
    }

    [StructLayout(LayoutKind.Sequential, Size = 80)]
    public struct VisibilityStateComponent : IComponent
    {
        public VisibilityStateFlags visibilityStateFlags;
        public VisibilityState visibilityState;
        public OctantID octantID;
        private HashCode lastAabbHash;
    }
}