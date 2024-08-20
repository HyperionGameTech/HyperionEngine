using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Transform")]
    [StructLayout(LayoutKind.Sequential, Size = 112)]
    public struct Transform
    {
        private Vec3f translation;
        private Vec3f scale;
        private Quaternion rotation;
        private Matrix4 matrix;

        public Transform()
        {
            this.translation = new Vec3f();
            this.scale = new Vec3f(1);
            this.rotation = new Quaternion();
            this.matrix = new Matrix4();

            UpdateMatrix();
        }

        public Transform(Vec3f translation, Vec3f scale, Quaternion rotation)
        {
            this.translation = translation;
            this.scale = scale;
            this.rotation = rotation;
            this.matrix = new Matrix4();

            UpdateMatrix();
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

                UpdateMatrix();
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

                UpdateMatrix();
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

                UpdateMatrix();
            }
        }

        public Matrix4 Matrix
        {
            get
            {
                return matrix;
            }
        }

        public void UpdateMatrix()
        {
            Transform_UpdateMatrix(ref this);
        }

        public override string ToString()
        {
            return $"Translation: {translation}, Scale: {scale}, Rotation: {rotation}";
        }

        [DllImport("hyperion", EntryPoint = "Transform_UpdateMatrix")]
        private static extern void Transform_UpdateMatrix([In] ref Transform transform);
    }
}