using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MeshComponentFlags : uint
    {
        None = 0x0,
        Dirty = 0x1
    }

    [StructLayout(LayoutKind.Explicit, Size = 112)]
    public unsafe struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private ManagedHandle meshHandle;
        [FieldOffset(4)]
        private ManagedHandle materialHandle;
        [FieldOffset(8)]
        private ManagedHandle skeletonHandle;
        [FieldOffset(12)]
        private uint numInstances;
        [FieldOffset(16)]
        private RefCountedPtr proxyRc;
        [FieldOffset(24)]
        private uint meshComponentFlags;
        [FieldOffset(32)] // align to 16 byte boundary
        private Matrix4 previousModelMatrix;
        // 16 bytes of user data
        [FieldOffset(96)]
        private fixed byte userData[16];

        public Mesh Mesh
        {
            get
            {
                throw new NotImplementedException();
                // return new Mesh(meshHandle);
            }
            set
            {
                throw new NotImplementedException();
                // meshHandle = value.Handle;
            }
        }

        public Material Material
        {
            get
            {
                throw new NotImplementedException();
                // return new Material(materialHandle);
            }
            set
            {
                throw new NotImplementedException();
                // materialHandle = value.Handle;
            }
        }

        public uint Flags
        {
            get
            {
                return meshComponentFlags;
            }
            set
            {
                meshComponentFlags = value;
            }
        }
    }
}