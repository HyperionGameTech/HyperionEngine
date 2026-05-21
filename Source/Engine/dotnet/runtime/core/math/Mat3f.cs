using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Mat3f")]
    [StructLayout(LayoutKind.Explicit, Size = 48, Pack = 16)]
    public struct Mat3f
    {
        [FieldOffset(0)]
        private float m00;
        [FieldOffset(4)]
        private float m01;
        [FieldOffset(8)]
        private float m02;

        [FieldOffset(16)]
        private float m10;
        [FieldOffset(20)]
        private float m11;
        [FieldOffset(24)]
        private float m12;

        [FieldOffset(32)]
        private float m20;
        [FieldOffset(36)]
        private float m21;
        [FieldOffset(40)]
        private float m22;

        public Mat3f()
        {
            m00 = 1; m01 = 0; m02 = 0;
            m10 = 0; m11 = 1; m12 = 0;
            m20 = 0; m21 = 0; m22 = 1;
        }

        public Mat3f(float[] values)
        {
            if (values.Length != 9)
            {
                throw new ArgumentException("values must have a length of 9");
            }

            m00 = values[0]; m01 = values[1]; m02 = values[2];
            m10 = values[3]; m11 = values[4]; m12 = values[5];
            m20 = values[6]; m21 = values[7]; m22 = values[8];
        }

        public Mat3f(Mat3f other)
        {
            m00 = other.m00; m01 = other.m01; m02 = other.m02;
            m10 = other.m10; m11 = other.m11; m12 = other.m12;
            m20 = other.m20; m21 = other.m21; m22 = other.m22;
        }

        public float Determinant()
        {
            float a = m00 * (m11 * m22 - m12 * m21);
            float b = m01 * (m10 * m22 - m12 * m20);
            float c = m02 * (m10 * m21 - m11 * m20);

            return a - b + c;
        }

        public Mat3f Transpose
        {
            get
            {
                Mat3f result = new Mat3f();
                Matrix3_Transpose(ref this, out result);
                return result;
            }
        }

        public Mat3f Inverse
        {
            get
            {
                Mat3f result = new Mat3f();
                Matrix3_Inverse(ref this, out result);
                return result;
            }
        }

        public static Mat3f operator*(Mat3f a, Mat3f b)
        {
            Mat3f result = new Mat3f();
            Matrix3_Multiply(ref a, ref b, out result);
            return result;
        }

        public static bool operator==(Mat3f a, Mat3f b)
        {
            return a.m00 == b.m00 && a.m01 == b.m01 && a.m02 == b.m02
                && a.m10 == b.m10 && a.m11 == b.m11 && a.m12 == b.m12
                && a.m20 == b.m20 && a.m21 == b.m21 && a.m22 == b.m22;
        }

        public static bool operator!=(Mat3f a, Mat3f b)
        {
            return !(a == b);
        }

        public float this[int row, int column]
        {
            get
            {
                if (row == 0)
                {
                    if (column == 0) return m00;
                    if (column == 1) return m01;
                    if (column == 2) return m02;
                }
                else if (row == 1)
                {
                    if (column == 0) return m10;
                    if (column == 1) return m11;
                    if (column == 2) return m12;
                }
                else if (row == 2)
                {
                    if (column == 0) return m20;
                    if (column == 1) return m21;
                    if (column == 2) return m22;
                }

                throw new IndexOutOfRangeException();
            }
            set
            {
                if (row == 0)
                {
                    if (column == 0) { m00 = value; return; }
                    if (column == 1) { m01 = value; return; }
                    if (column == 2) { m02 = value; return; }
                }
                else if (row == 1)
                {
                    if (column == 0) { m10 = value; return; }
                    if (column == 1) { m11 = value; return; }
                    if (column == 2) { m12 = value; return; }
                }
                else if (row == 2)
                {
                    if (column == 0) { m20 = value; return; }
                    if (column == 1) { m21 = value; return; }
                    if (column == 2) { m22 = value; return; }
                }

                throw new IndexOutOfRangeException();
            }
        }

        public static Mat3f Identity
        {
            get
            {
                return new Mat3f();
            }
        }

        public static Mat3f Zeros()
        {
            return new Mat3f(new float[]
            {
                0, 0, 0,
                0, 0, 0,
                0, 0, 0
            });
        }

        public static Mat3f Ones()
        {
            return new Mat3f(new float[]
            {
                1, 1, 1,
                1, 1, 1,
                1, 1, 1
            });
        }

        public override string ToString()
        {
            return $"[{m00}, {m01}, {m02},\n" +
                   $"{m10}, {m11}, {m12},\n" +
                   $"{m20}, {m21}, {m22}]";
        }

        public override bool Equals(object? obj)
        {
            return obj is Mat3f other && this == other;
        }

        [DllImport("hyperion", EntryPoint = "Matrix3_Multiply")]
        private static extern void Matrix3_Multiply([In] ref Mat3f a, [In] ref Mat3f b, [Out] out Mat3f result);

        [DllImport("hyperion", EntryPoint = "Matrix3_Transpose")]
        private static extern void Matrix3_Transpose([In] ref Mat3f matrix, [Out] out Mat3f result);

        [DllImport("hyperion", EntryPoint = "Matrix3_Inverse")]
        private static extern void Matrix3_Inverse([In] ref Mat3f matrix, [Out] out Mat3f result);
    }
}
