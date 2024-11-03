using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MeshComponentFlags : uint
    {
        None = 0x0,
        Dirty = 0x1
    }

    [HypClassBinding(Name="MeshInstanceData")]
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct MeshInstanceData
    {
        [FieldOffset(0)]
        private unsafe fixed byte arrayData[32];
    }

    [HypClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 160)]
    public unsafe struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Mesh> meshHandle;
        [FieldOffset(4)]
        private Handle<Material> materialHandle;
        [FieldOffset(8)]
        private Handle<Skeleton> skeletonHandle;
        [FieldOffset(12)]
        private MeshInstanceData instanceData;
        [FieldOffset(48)]
        private RefCountedPtr proxyRc;
        [FieldOffset(56)]
        private uint meshComponentFlags;
        [FieldOffset(64)] // align to 16 byte boundary
        private Matrix4 previousModelMatrix;
        // 16 bytes of user data
        [FieldOffset(128)]
        private fixed byte userData[32];

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

        public ref MeshInstanceData InstanceData
        {
            get
            {
                return ref instanceData;
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