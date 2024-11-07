using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class FBOMType : IDisposable
    {
        public static readonly FBOMType Unset;
        public static readonly FBOMType UInt8;
        public static readonly FBOMType UInt16;
        public static readonly FBOMType UInt32;
        public static readonly FBOMType UInt64;
        public static readonly FBOMType Int8;
        public static readonly FBOMType Int16;
        public static readonly FBOMType Int32;
        public static readonly FBOMType Int64;
        public static readonly FBOMType Float;
        public static readonly FBOMType Double;
        public static readonly FBOMType Bool;
        public static readonly FBOMType Name;
        public static readonly FBOMType Matrix3;
        public static readonly FBOMType Matrix4;
        public static readonly FBOMType Vec2f;
        public static readonly FBOMType Vec3f;
        public static readonly FBOMType Vec4f;
        public static readonly FBOMType Quaternion;

        static FBOMType()
        {
            Unset = new FBOMType(FBOMType_Unset());
            UInt8 = new FBOMType(FBOMType_UInt8());
            UInt16 = new FBOMType(FBOMType_UInt16());
            UInt32 = new FBOMType(FBOMType_UInt32());
            UInt64 = new FBOMType(FBOMType_UInt64());
            Int8 = new FBOMType(FBOMType_Int8());
            Int16 = new FBOMType(FBOMType_Int16());
            Int32 = new FBOMType(FBOMType_Int32());
            Int64 = new FBOMType(FBOMType_Int64());
            Float = new FBOMType(FBOMType_Float());
            Double = new FBOMType(FBOMType_Double());
            Bool = new FBOMType(FBOMType_Bool());
            Name = new FBOMType(FBOMType_Name());
            Matrix3 = new FBOMType(FBOMType_Matrix3());
            Matrix4 = new FBOMType(FBOMType_Matrix4());
            Vec2f = new FBOMType(FBOMType_Vec2f());
            Vec3f = new FBOMType(FBOMType_Vec3f());
            Vec4f = new FBOMType(FBOMType_Vec4f());
            Quaternion = new FBOMType(FBOMType_Quaternion());
        }

        internal IntPtr ptr;

        internal FBOMType(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        ~FBOMType()
        {
            if (this.ptr != IntPtr.Zero)
            {
                FBOMType_Destroy(this.ptr);
            }
        }

        public void Dispose()
        {
            if (this.ptr != IntPtr.Zero)
            {
                FBOMType_Destroy(this.ptr);
                this.ptr = IntPtr.Zero;
            }

            GC.SuppressFinalize(this);
        }

        public string TypeName
        {
            get
            {
                return Marshal.PtrToStringAnsi(FBOMType_GetName(ptr));
            }
        }

        public TypeID NativeTypeID
        {
            get
            {
                TypeID typeId;
                FBOMType_GetNativeTypeID(ptr, out typeId);
                return typeId;
            }
        }

        public HypClass? HypClass
        {
            get
            {
                IntPtr hypClassPtr = IntPtr.Zero;
                hypClassPtr = FBOMType_GetHypClass(ptr);

                if (hypClassPtr == IntPtr.Zero)
                {
                    return null;
                }

                return new HypClass(hypClassPtr);
            }
        }

        public bool IsOrExtends(FBOMType other, bool allowUnbounded = true)
        {
            return FBOMType_IsOrExtends(this.ptr, other.ptr, allowUnbounded);
        }

        public override bool Equals(object other)
        {
            if (other is FBOMType)
            {
                return FBOMType_Equals(this.ptr, ((FBOMType)other).ptr);
            }

            return false;
        }

        public override string ToString()
        {
            return TypeName;
        }

        [DllImport("hyperion", EntryPoint = "FBOMType_GetName")]
        public static extern IntPtr FBOMType_GetName([In] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "FBOMType_GetNativeTypeID")]
        public static extern void FBOMType_GetNativeTypeID([In] IntPtr ptr, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "FBOMType_GetHypClass")]
        public static extern IntPtr FBOMType_GetHypClass([In] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "FBOMType_Unset")]
        public static extern IntPtr FBOMType_Unset();

        [DllImport("hyperion", EntryPoint = "FBOMType_UInt8")]
        public static extern IntPtr FBOMType_UInt8();

        [DllImport("hyperion", EntryPoint = "FBOMType_UInt16")]
        public static extern IntPtr FBOMType_UInt16();

        [DllImport("hyperion", EntryPoint = "FBOMType_UInt32")]
        public static extern IntPtr FBOMType_UInt32();

        [DllImport("hyperion", EntryPoint = "FBOMType_UInt64")]
        public static extern IntPtr FBOMType_UInt64();

        [DllImport("hyperion", EntryPoint = "FBOMType_Int8")]
        public static extern IntPtr FBOMType_Int8();

        [DllImport("hyperion", EntryPoint = "FBOMType_Int16")]
        public static extern IntPtr FBOMType_Int16();

        [DllImport("hyperion", EntryPoint = "FBOMType_Int32")]
        public static extern IntPtr FBOMType_Int32();

        [DllImport("hyperion", EntryPoint = "FBOMType_Int64")]
        public static extern IntPtr FBOMType_Int64();

        [DllImport("hyperion", EntryPoint = "FBOMType_Float")]
        public static extern IntPtr FBOMType_Float();

        [DllImport("hyperion", EntryPoint = "FBOMType_Double")]
        public static extern IntPtr FBOMType_Double();

        [DllImport("hyperion", EntryPoint = "FBOMType_Bool")]
        public static extern IntPtr FBOMType_Bool();

        [DllImport("hyperion", EntryPoint = "FBOMType_Name")]
        public static extern IntPtr FBOMType_Name();

        [DllImport("hyperion", EntryPoint = "FBOMType_Matrix3")]
        public static extern IntPtr FBOMType_Matrix3();

        [DllImport("hyperion", EntryPoint = "FBOMType_Matrix4")]
        public static extern IntPtr FBOMType_Matrix4();

        [DllImport("hyperion", EntryPoint = "FBOMType_Vec2f")]
        public static extern IntPtr FBOMType_Vec2f();

        [DllImport("hyperion", EntryPoint = "FBOMType_Vec3f")]
        public static extern IntPtr FBOMType_Vec3f();

        [DllImport("hyperion", EntryPoint = "FBOMType_Vec4f")]
        public static extern IntPtr FBOMType_Vec4f();

        [DllImport("hyperion", EntryPoint = "FBOMType_Quaternion")]
        public static extern IntPtr FBOMType_Quaternion();

        [DllImport("hyperion", EntryPoint = "FBOMType_Destroy")]
        private static extern void FBOMType_Destroy([In] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "FBOMType_Equals")]
        private static extern bool FBOMType_Equals([In] IntPtr lhs, [In] IntPtr rhs);

        [DllImport("hyperion", EntryPoint = "FBOMType_IsOrExtends")]
        private static extern bool FBOMType_IsOrExtends([In] IntPtr lhs, [In] IntPtr rhs, bool allowUnbounded);
    }
}