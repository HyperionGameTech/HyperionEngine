using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Sequential, Pack = 16)]
    public unsafe ref struct MeshComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(MeshComponent));

        private Handle<Mesh> _meshHandle;
        private Handle<MaterialInstance> _materialHandle;
        private Handle<Skeleton> _skeletonHandle;
        private uint _numInstances;
        private bool _enableAutoInstancing;
        private AssetReference _instanceData;

        private Mat4f _previousModelMatrix;
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

        public MaterialInstance? Material
        {
            get => _materialHandle.GetValue();
            set
            {
                _materialHandle.Dispose();

                if (value == null)
                {
                    _materialHandle = Handle<MaterialInstance>.Empty;
                    
                    return;
                }

                _materialHandle = new Handle<MaterialInstance>(value);
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

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (MeshComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}