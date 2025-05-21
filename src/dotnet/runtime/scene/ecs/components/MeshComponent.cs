using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MeshComponentFlags : uint
    {
        None = 0x0,
        Dirty = 0x1
    }

    [HypClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 256)]
    public unsafe struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Mesh> meshHandle;

        [FieldOffset(8)]
        private Handle<Material> materialHandle;
        
        [FieldOffset(16)]
        private Handle<Skeleton> skeletonHandle;

        [FieldOffset(24)]
        private MeshInstanceData instanceData;

        [FieldOffset(128)]
        private IntPtr proxyPtr;

        [FieldOffset(136)]
        private uint meshComponentFlags;

        [FieldOffset(144)]
        private Matrix4 previousModelMatrix;

        [FieldOffset(208)]
        private IntPtr raytracingDataPtr;

        [FieldOffset(224)]
        private fixed byte userData[32];

        public void Dispose()
        {
            meshHandle.Dispose();
            materialHandle.Dispose();
            skeletonHandle.Dispose();
        }

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