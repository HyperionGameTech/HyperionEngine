using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 208, Pack = 16)]
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

        [FieldOffset(112)]
        private Mat4f previousModelMatrix;

        [FieldOffset(176)] // aligned by 16
        private fixed byte userData[32];

        public void Dispose()
        {
            meshHandle.Dispose();
            materialHandle.Dispose();
            skeletonHandle.Dispose();
        }

        public Mesh? Mesh
        {
            get
            {
                return meshHandle.GetValue();
            }
            set
            {
                meshHandle.Dispose();

                if (value == null)
                {
                    meshHandle = Handle<Mesh>.Empty;
                    
                    return;
                }

                meshHandle = new Handle<Mesh>(value);
            }
        }

        public Material? Material
        {
            get
            {
                return materialHandle.GetValue();
            }
            set
            {
                materialHandle.Dispose();

                if (value == null)
                {
                    materialHandle = Handle<Material>.Empty;
                    
                    return;
                }

                materialHandle = new Handle<Material>(value);
            }
        }

        public Skeleton? Skeleton
        {
            get
            {
                return skeletonHandle.GetValue();
            }
            set
            {
                skeletonHandle.Dispose();

                if (value == null)
                {
                    skeletonHandle = Handle<Skeleton>.Empty;
                    
                    return;
                }

                skeletonHandle = new Handle<Skeleton>(value);
            }
        }

        public ref MeshInstanceData InstanceData
        {
            get
            {
                return ref instanceData;
            }
        }
    }
}