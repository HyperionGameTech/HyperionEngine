using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 176, Pack = 16)]
    public unsafe ref struct MeshComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Mesh> _meshHandle;

        [FieldOffset(8)]
        private Handle<Material> _materialHandle;

        [FieldOffset(16)]
        private Handle<Skeleton> _skeletonHandle;

        [FieldOffset(24)]
        private bool _enableAutoInstancing;
        
        [FieldOffset(32)]
        private AssetReference _instanceData;

        [FieldOffset(56)]
        private uint _numInstances;

        [FieldOffset(64)]
        private Mat4f _previousModelMatrix;

        [FieldOffset(128)]
        private fixed byte _userData[32];

        [FieldOffset(160)]
        private byte _forcedLod;

        [FieldOffset(161)]
        private sbyte _lodBias;

        public MeshComponent()
        {
            _instanceData = new AssetReference();
            _previousModelMatrix = Mat4f.Identity;
        }

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

        public bool EnableAutoInstancing
        {
            get => _enableAutoInstancing;
            set => _enableAutoInstancing = value;
        }
    }
}