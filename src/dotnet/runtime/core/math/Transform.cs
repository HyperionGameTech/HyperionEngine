using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Transform")]
    [StructLayout(LayoutKind.Sequential, Size = 48, Pack = 16)]
    public struct Transform
    {
        public static readonly Transform Identity = new Transform();

        private Vec3f translation;
        private Vec3f scale;
        private Quaternion rotation;

        public Transform()
        {
            this.translation = new Vec3f();
            this.scale = new Vec3f(1);
            this.rotation = new Quaternion();
        }

        public Transform(Vec3f translation, Vec3f scale, Quaternion rotation)
        {
            this.translation = translation;
            this.scale = scale;
            this.rotation = rotation;
        }

        public Vec3f Translation
        {
            get
            {
                return translation;
            }
            set
            {
                translation = value;
            }
        }

        public Vec3f Scale
        {
            get
            {
                return scale;
            }
            set
            {
                scale = value;
            }
        }

        public Quaternion Rotation
        {
            get
            {
                return rotation;
            }
            set
            {
                rotation = value;
            }
        }

        public Mat4f Matrix
        {
            get
            {
                Mat4f matrix;
                Transform_GetMatrix(ref this, out matrix);

                return matrix;
            }
        }

        public override string ToString()
        {
            return $"Translation: {translation}, Scale: {scale}, Rotation: {rotation}";
        }

        [DllImport("hyperion", EntryPoint = "Transform_GetMatrix")]
        private static extern void Transform_GetMatrix([In] ref Transform transform, [Out] out Mat4f matrix);
    }
}