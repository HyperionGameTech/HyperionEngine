using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MeshComponentFlags : uint
    {
        None = 0x0,
        Dirty = 0x1
    }

    [StructLayout(LayoutKind.Explicit, Size = 96)]
    public unsafe struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private ManagedHandle meshHandle;
        [FieldOffset(4)]
        private ManagedHandle materialHandle;
        [FieldOffset(8)]
        private ManagedHandle skeletonHandle;
        [FieldOffset(12)]
        private RefCountedPtr proxyRc;
        [FieldOffset(20)]
        private MeshComponentFlags meshComponentFlags;
        [FieldOffset(32)] // align to 16 byte boundary
        private Matrix4 previousModelMatrix;

        public Mesh Mesh
        {
            get
            {
                return new Mesh(meshHandle);
            }
            set
            {
                meshHandle = value.Handle;
            }
        }

        public Material Material
        {
            get
            {
                return new Material(materialHandle);
            }
            set
            {
                materialHandle = value.Handle;
            }
        }
    }
}