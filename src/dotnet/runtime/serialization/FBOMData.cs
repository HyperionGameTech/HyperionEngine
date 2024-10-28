using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class FBOMData : IDisposable
    {
        internal IntPtr ptr;
        private FBOMType type = FBOMType.Unset;

        public FBOMData()
        {
            this.ptr = FBOMData_Create(IntPtr.Zero);
            this.type = new FBOMType(FBOMData_GetType(this.ptr));
        }

        internal FBOMData(IntPtr ptr)
        {
            this.ptr = ptr;
            this.type = new FBOMType(FBOMData_GetType(this.ptr));
        }

        public void Dispose()
        {
            if (this.ptr == IntPtr.Zero)
            {
                return;
            }

            FBOMData_Destroy(this.ptr);
            this.ptr = IntPtr.Zero;
        }

        ~FBOMData()
        {
            Dispose();
        }

        public FBOMType Type
        {
            get
            {
                return type;
            }
        }

        public ulong TotalSize
        {
            get
            {
                return FBOMData_TotalSize(this.ptr);
            }
        }

        public FBOMData? this[string key]
        {
            get
            {
                FBOMObject? asObject = GetObject();

                if (asObject == null)
                {
                    throw new Exception("Data is not subscriptable");
                }

                return asObject[key];
            }
            set
            {
                FBOMObject? asObject = GetObject();

                if (asObject == null)
                {
                    throw new Exception("Data is not subscriptable");
                }

                asObject[key] = value;
            }
        }

        public byte? GetUInt8()
        {
            byte value;

            if (FBOMData_GetByte(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromUInt8(byte value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetByte(data.ptr, ref value);
            return data;
        }

        public ushort? GetUInt16()
        {
            ushort value;

            if (FBOMData_GetUInt16(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromUInt16(ushort value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetUInt16(data.ptr, ref value);
            return data;
        }

        public uint? GetUInt32()
        {
            uint value;

            if (FBOMData_GetUInt32(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromUInt32(uint value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetUInt32(data.ptr, ref value);
            return data;
        }

        public ulong? GetUInt64()
        {
            ulong value;

            if (FBOMData_GetUInt64(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromUInt64(ulong value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetUInt64(data.ptr, ref value);
            return data;
        }

        public sbyte? GetInt8()
        {
            sbyte value;

            if (FBOMData_GetInt8(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromInt8(sbyte value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetInt8(data.ptr, ref value);
            return data;
        }

        public short? GetInt16()
        {
            short value;

            if (FBOMData_GetInt16(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromInt16(short value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetInt16(data.ptr, ref value);
            return data;
        }

        public int? GetInt32()
        {
            int value;

            if (FBOMData_GetInt32(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromInt32(int value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetInt32(data.ptr, ref value);
            return data;
        }

        public long? GetInt64()
        {
            long value;

            if (FBOMData_GetInt64(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromInt64(long value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetInt64(data.ptr, ref value);
            return data;
        }

        public char? GetCHar()
        {
            char value;

            if (FBOMData_GetChar(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromChar(char value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetChar(data.ptr, ref value);
            return data;
        }

        public float? GetFloat()
        {
            float value;

            if (FBOMData_GetFloat(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromFloat(float value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetFloat(data.ptr, ref value);
            return data;
        }

        public bool? GetBool()
        {
            bool value;

            if (FBOMData_GetBool(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromBool(bool value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetBool(data.ptr, ref value);
            return data;
        }

        public byte? GetByte()
        {
            byte value;

            if (FBOMData_GetByte(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromByte(byte value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetByte(data.ptr, ref value);
            return data;
        }

        public Name? GetName()
        {
            Name value;

            if (FBOMData_GetName(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromName(Name value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetName(data.ptr, ref value);
            return data;
        }

        // public Matrix3? GetMatrix3()

        public Matrix4? GetMatrix4()
        {
            Matrix4 value;

            if (FBOMData_GetMatrix4(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromMatrix4(Matrix4 value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetMatrix4(data.ptr, ref value);
            return data;
        }

        public Vec2f? GetVec2f()
        {
            Vec2f value;

            if (FBOMData_GetVec2f(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromVec2f(Vec2f value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetVec2f(data.ptr, ref value);
            return data;
        }

        public Vec3f? GetVec3f()
        {
            Vec3f value;

            if (FBOMData_GetVec3f(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromVec3f(Vec3f value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetVec3f(data.ptr, ref value);
            return data;
        }

        // public Vec4f? GetVec4f()
        // {
        //     Vec4f value;

        //     if (FBOMData_GetVec4f(this.ptr, out value))
        //     {
        //         return value;
        //     }

        //     return null;
        // }

        // public static FBOMData FromVec4f(Vec4f value)

        public Quaternion? GetQuaternion()
        {
            Quaternion value;

            if (FBOMData_GetQuaternion(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromQuaternion(Quaternion value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetQuaternion(data.ptr, ref value);
            return data;
        }

        public FBOMObject? GetObject()
        {
            FBOMObject value = new FBOMObject();

            if (FBOMData_GetObject(this.ptr, value.ptr))
            {
                return value;
            }

            return null;
        }

        public static FBOMData FromObject(FBOMObject value)
        {
            FBOMData data = new FBOMData();
            FBOMData_SetObject(data.ptr, value.ptr);
            return data;
        }

        [DllImport("hyperion", EntryPoint = "FBOMData_Create")]
        private static extern IntPtr FBOMData_Create(IntPtr typePtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_Destroy")]
        private static extern void FBOMData_Destroy(IntPtr dataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetType")]
        private static extern IntPtr FBOMData_GetType(IntPtr dataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_TotalSize")]
        private static extern ulong FBOMData_TotalSize(IntPtr dataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetUInt8")]
        private static extern bool FBOMData_GetUInt8(IntPtr dataPtr, [Out] out byte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetUInt8")]
        private static extern void FBOMData_SetUInt8(IntPtr dataPtr, [In] ref byte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetUInt16")]
        private static extern bool FBOMData_GetUInt16(IntPtr dataPtr, [Out] out ushort value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetUInt16")]
        private static extern void FBOMData_SetUInt16(IntPtr dataPtr, [In] ref ushort value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetUInt32")]
        private static extern bool FBOMData_GetUInt32(IntPtr dataPtr, [Out] out uint value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetUInt32")]
        private static extern void FBOMData_SetUInt32(IntPtr dataPtr, [In] ref uint value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetUInt64")]
        private static extern bool FBOMData_GetUInt64(IntPtr dataPtr, [Out] out ulong value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetUInt64")]
        private static extern void FBOMData_SetUInt64(IntPtr dataPtr, [In] ref ulong value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetInt8")]
        private static extern bool FBOMData_GetInt8(IntPtr dataPtr, [Out] out sbyte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetInt8")]
        private static extern void FBOMData_SetInt8(IntPtr dataPtr, [In] ref sbyte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetInt16")]
        private static extern bool FBOMData_GetInt16(IntPtr dataPtr, [Out] out short value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetInt16")]
        private static extern void FBOMData_SetInt16(IntPtr dataPtr, [In] ref short value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetInt32")]
        private static extern bool FBOMData_GetInt32(IntPtr dataPtr, [Out] out int value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetInt32")]
        private static extern void FBOMData_SetInt32(IntPtr dataPtr, [In] ref int value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetInt64")]
        private static extern bool FBOMData_GetInt64(IntPtr dataPtr, [Out] out long value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetInt64")]
        private static extern void FBOMData_SetInt64(IntPtr dataPtr, [In] ref long value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetChar")]
        private static extern bool FBOMData_GetChar(IntPtr dataPtr, [Out] out char value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetChar")]
        private static extern void FBOMData_SetChar(IntPtr dataPtr, [In] ref char value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetFloat")]
        private static extern bool FBOMData_GetFloat(IntPtr dataPtr, [Out] out float value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetFloat")]
        private static extern void FBOMData_SetFloat(IntPtr dataPtr, [In] ref float value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetBool")]
        private static extern bool FBOMData_GetBool(IntPtr dataPtr, [Out] out bool value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetBool")]
        private static extern void FBOMData_SetBool(IntPtr dataPtr, [In] ref bool value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetByte")]
        private static extern bool FBOMData_GetByte(IntPtr dataPtr, [Out] out byte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetByte")]
        private static extern void FBOMData_SetByte(IntPtr dataPtr, [In] ref byte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetName")]
        private static extern bool FBOMData_GetName(IntPtr dataPtr, [Out] out Name value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetName")]
        private static extern void FBOMData_SetName(IntPtr dataPtr, [In] ref Name value);

        // [DllImport("hyperion", EntryPoint = "FBOMData_GetMatrix3")]
        // private static extern bool FBOMData_GetMatrix3(IntPtr dataPtr, [Out] out Matrix3 value);

        // [DllImport("hyperion", EntryPoint = "FBOMData_SetMatrix3")]
        // private static extern void FBOMData_SetMatrix3(IntPtr dataPtr, [In] ref Matrix3 value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetMatrix4")]
        private static extern bool FBOMData_GetMatrix4(IntPtr dataPtr, [Out] out Matrix4 value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetMatrix4")]
        private static extern void FBOMData_SetMatrix4(IntPtr dataPtr, [In] ref Matrix4 value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetVec2f")]
        private static extern bool FBOMData_GetVec2f(IntPtr dataPtr, [Out] out Vec2f value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetVec2f")]
        private static extern void FBOMData_SetVec2f(IntPtr dataPtr, [In] ref Vec2f value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetVec3f")]
        private static extern bool FBOMData_GetVec3f(IntPtr dataPtr, [Out] out Vec3f value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetVec3f")]
        private static extern void FBOMData_SetVec3f(IntPtr dataPtr, [In] ref Vec3f value);

        // [DllImport("hyperion", EntryPoint = "FBOMData_GetVec4f")]
        // private static extern bool FBOMData_GetVec4f(IntPtr dataPtr, [Out] out Vec4f value);

        // [DllImport("hyperion", EntryPoint = "FBOMData_SetVec4f")]
        // private static extern void FBOMData_SetVec4f(IntPtr dataPtr, [In] ref Vec4f value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetQuaternion")]
        private static extern bool FBOMData_GetQuaternion(IntPtr dataPtr, [Out] out Quaternion value);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetQuaternion")]
        private static extern void FBOMData_SetQuaternion(IntPtr dataPtr, [In] ref Quaternion value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetObject")]
        private static extern bool FBOMData_GetObject(IntPtr dataPtr, [Out] IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "FBOMData_SetObject")]
        private static extern void FBOMData_SetObject(IntPtr dataPtr, [In] IntPtr ptr);
    }
}