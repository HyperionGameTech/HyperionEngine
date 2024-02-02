using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MeshComponentFlags : uint
    {
        None = 0x0,
        Init = 0x1,
        Dirty = 0x2
    }

    [StructLayout(LayoutKind.Explicit, Size = 76)]
    public struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private ManagedHandle meshHandle;
        [FieldOffset(4)]
        private ManagedHandle materialHandle;
        [FieldOffset(8)]
        private Matrix4 previousModelMatrix;
        [FieldOffset(72)]
        private MeshComponentFlags meshComponentFlags;

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