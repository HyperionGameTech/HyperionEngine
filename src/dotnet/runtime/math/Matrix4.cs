using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 64, Pack = 16)]
    public struct Matrix4
    {
        [FieldOffset(0)]
        private float m00;
        [FieldOffset(4)]
        private float m01;
        [FieldOffset(8)]
        private float m02;
        [FieldOffset(12)]
        private float m03;
        [FieldOffset(16)]
        private float m10;
        [FieldOffset(20)]
        private float m11;
        [FieldOffset(24)]
        private float m12;
        [FieldOffset(28)]
        private float m13;
        [FieldOffset(32)]
        private float m20;
        [FieldOffset(36)]
        private float m21;
        [FieldOffset(40)]
        private float m22;
        [FieldOffset(44)]
        private float m23;
        [FieldOffset(48)]
        private float m30;
        [FieldOffset(52)]
        private float m31;
        [FieldOffset(56)]
        private float m32;
        [FieldOffset(60)]
        private float m33;

        public Matrix4()
        {
            m00 = 1; m01 = 0; m02 = 0; m03 = 0;
            m10 = 0; m11 = 1; m12 = 0; m13 = 0;
            m20 = 0; m21 = 0; m22 = 1; m23 = 0;
            m30 = 0; m31 = 0; m32 = 0; m33 = 1;
        }

        public Matrix4(float[] values)
        {
            if (values.Length != 16)
            {
                throw new ArgumentException("values must have a length of 16");
            }

            m00 = values[0]; m01 = values[1]; m02 = values[2]; m03 = values[3];
            m10 = values[4]; m11 = values[5]; m12 = values[6]; m13 = values[7];
            m20 = values[8]; m21 = values[9]; m22 = values[10]; m23 = values[11];
            m30 = values[12]; m31 = values[13]; m32 = values[14]; m33 = values[15];
        }

        public Matrix4(Matrix4 other)
        {
            m00 = other.m00; m01 = other.m01; m02 = other.m02; m03 = other.m03;
            m10 = other.m10; m11 = other.m11; m12 = other.m12; m13 = other.m13;
            m20 = other.m20; m21 = other.m21; m22 = other.m22; m23 = other.m23;
            m30 = other.m30; m31 = other.m31; m32 = other.m32; m33 = other.m33;
        }

        public Matrix4 Transpose
        {
            get
            {
                Matrix4 result = new Matrix4();
                Matrix4_Transposed(ref this, out result);
                return result;
            }
        }

        public Matrix4 Inverse
        {
            get
            {
                Matrix4 result = new Matrix4();
                Matrix4_Inverted(ref this, out result);
                return result;
            }
        }

        public static Matrix4 operator*(Matrix4 a, Matrix4 b)
        {
            Matrix4 result = new Matrix4();
            Matrix4_Multiply(ref a, ref b, out result);
            return result;
        }

        public static Matrix4 Identity
        {
            get
            {
                return new Matrix4();
            }
        }

        public override string ToString()
        {
            return $"[{m00}, {m01}, {m02}, {m03},\n" +
                   $"{m10}, {m11}, {m12}, {m13},\n" +
                   $"{m20}, {m21}, {m22}, {m23},\n" +
                   $"{m30}, {m31}, {m32}, {m33}]";
        }

        [DllImport("hyperion", EntryPoint = "Matrix4_Identity")]
        private static extern void Matrix4_Identity([Out] out Matrix4 matrix);

        [DllImport("hyperion", EntryPoint = "Matrix4_Multiply")]
        private static extern void Matrix4_Multiply([In] ref Matrix4 a, [In] ref Matrix4 b, [Out] out Matrix4 result);

        [DllImport("hyperion", EntryPoint = "Matrix4_Transposed")]
        private static extern void Matrix4_Transposed([In] ref Matrix4 matrix, [Out] out Matrix4 result);

        [DllImport("hyperion", EntryPoint = "Matrix4_Inverted")]
        private static extern void Matrix4_Inverted([In] ref Matrix4 matrix, [Out] out Matrix4 result);
    }
}