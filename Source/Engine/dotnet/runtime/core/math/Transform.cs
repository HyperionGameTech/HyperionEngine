using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Transform")]
    [StructLayout(LayoutKind.Sequential, Size = 48, Pack = 16)]
    public struct Transform
    {
        public static readonly Transform Identity = new Transform();

        private Vec3f _translation;
        private Vec3f _scale;
        private Quaternion _rotation;

        public Transform()
        {
            _translation = new Vec3f();
            _scale = new Vec3f(1);
            _rotation = new Quaternion();
        }

        public Transform(Vec3f translation, Vec3f scale, Quaternion rotation)
        {
            _translation = translation;
            _scale = scale;
            _rotation = rotation;
        }

        public Vec3f Translation
        {
            get => _translation;
            set => _translation = value;
        }

        public Vec3f Scale
        {
            get => _scale;
            set => _scale = value;
        }

        public Quaternion Rotation
        {
            get => _rotation;
            set => _rotation = value;
        }

        public Mat4f Matrix
        {
            get
            {
                Transform_GetMatrix(ref this, out Mat4f matrix);

                return matrix;
            }
        }

        public override string ToString()
        {
            return $"Translation: {_translation}, Scale: {_scale}, Rotation: {_rotation}";
        }

        [DllImport("hyperion", EntryPoint = "Transform_GetMatrix")]
        private static extern void Transform_GetMatrix([In] ref Transform transform, [Out] out Mat4f matrix);
    }
}