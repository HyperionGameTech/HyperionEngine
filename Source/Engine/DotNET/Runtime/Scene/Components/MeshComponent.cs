using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MeshComponent")]
    [StructLayout(LayoutKind.Explicit, Size = Layout.Size, Pack = 16)]
    public unsafe ref struct MeshComponent : IComponent
    {
        private static class Layout
        {
#if HYP_WEB
            public const int Material = 4, Skeleton = 8, EnableAutoInstancing = 12, InstanceData = 16, NumInstances = 32,
                PreviousModelMatrix = 48, UserData = 112, ForcedLod = 144, LodBias = 145, SkeletonInstance = 148, Size = 160;
#else
            public const int Material = 8, Skeleton = 16, EnableAutoInstancing = 24, InstanceData = 32, NumInstances = 56,
                PreviousModelMatrix = 64, UserData = 128, ForcedLod = 160, LodBias = 161, SkeletonInstance = 168, Size = 176;
#endif
        }

        [FieldOffset(0)]
        private Handle<Mesh> _meshHandle;

        [FieldOffset(Layout.Material)]
        private Handle<Material> _materialHandle;

        [FieldOffset(Layout.Skeleton)]
        private Handle<Skeleton> _skeletonHandle;

        [FieldOffset(Layout.EnableAutoInstancing)]
        private bool _enableAutoInstancing;

        [FieldOffset(Layout.InstanceData)]
        private AssetReference _instanceData;

        [FieldOffset(Layout.NumInstances)]
        private uint _numInstances;

        [FieldOffset(Layout.PreviousModelMatrix)]
        private Mat4f _previousModelMatrix;

        [FieldOffset(Layout.UserData)]
        private fixed byte _userData[32];

        [FieldOffset(Layout.ForcedLod)]
        private byte _forcedLod;

        [FieldOffset(Layout.LodBias)]
        private sbyte _lodBias;

        [FieldOffset(Layout.SkeletonInstance)]
        private Handle<Skeleton> _skeletonInstanceHandle;

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
            _skeletonInstanceHandle.Dispose();
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
