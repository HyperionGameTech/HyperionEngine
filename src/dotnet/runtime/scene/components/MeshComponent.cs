using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 208, Pack = 16)]
    public unsafe struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Mesh> _meshHandle;

        [FieldOffset(8)]
        private Handle<Material> _materialHandle;
        
        [FieldOffset(16)]
        private Handle<Skeleton> _skeletonHandle;

        [FieldOffset(24)]
        private MeshInstanceData _instanceData;

        [FieldOffset(112)]
        private Mat4f _previousModelMatrix;

        [FieldOffset(176)] // aligned by 16
        private fixed byte _userData[32];

        public void Dispose()
        {
            _meshHandle.Dispose();
            _materialHandle.Dispose();
            _skeletonHandle.Dispose();
        }

        public Mesh? Mesh
        {
            get => _meshHandle.GetValue();
            set
            {
                _meshHandle.Dispose();

                if (value == null)
                {
                    _meshHandle = Handle<Mesh>.Empty;
                    
                    return;
                }

                _meshHandle = new Handle<Mesh>(value);
            }
        }

        public Material? Material
        {
            get => _materialHandle.GetValue();
            set
            {
                _materialHandle.Dispose();

                if (value == null)
                {
                    _materialHandle = Handle<Material>.Empty;
                    
                    return;
                }

                _materialHandle = new Handle<Material>(value);
            }
        }

        public Skeleton? Skeleton
        {
            get => _skeletonHandle.GetValue();
            set
            {
                _skeletonHandle.Dispose();

                if (value == null)
                {
                    _skeletonHandle = Handle<Skeleton>.Empty;
                    
                    return;
                }

                _skeletonHandle = new Handle<Skeleton>(value);
            }
        }

        public ref MeshInstanceData InstanceData => ref _instanceData;
    }
}