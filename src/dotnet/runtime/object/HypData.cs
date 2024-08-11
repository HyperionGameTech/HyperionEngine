using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    internal unsafe struct InternalHypDataValue
    {
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I1)]
        public sbyte valueI8;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I2)]
        public short valueI16;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I4)]
        public int valueI32;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I8)]
        public long valueI64;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U1)]
        public byte valueU8;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U2)]
        public ushort valueU16;
        
        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U4)]
        public uint valueU32;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U8)]
        public ulong valueU64;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.R4)]
        public float valueFloat;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.R8)]
        public double valueDouble;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.I1)]
        public bool valueBool;

        [FieldOffset(0)]
        [MarshalAs(UnmanagedType.U4)]
        public uint valueId;
        
        [FieldOffset(0)]
        public IntPtr valueHypObjectPtr;
    }

    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public unsafe struct HypData
    {
        private fixed byte buffer[32];

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypData_GetTypeID(ref this, out typeId);
                return typeId;
            }
        }

        public unsafe object? GetValue()
        {
            InternalHypDataValue value;

            if (HypData_GetInt8(ref this, out value.valueI8))
            {
                return value.valueI8;
            }

            if (HypData_GetInt16(ref this, out value.valueI16))
            {
                return value.valueI16;
            }

            if (HypData_GetInt32(ref this, out value.valueI32))
            {
                return value.valueI32;
            }

            if (HypData_GetInt64(ref this, out value.valueI64))
            {
                return value.valueI64;
            }

            if (HypData_GetUInt8(ref this, out value.valueU8))
            {
                return value.valueU8;
            }

            if (HypData_GetUInt16(ref this, out value.valueU16))
            {
                return value.valueU16;
            }

            if (HypData_GetUInt32(ref this, out value.valueU32))
            {
                return value.valueU32;
            }

            if (HypData_GetUInt64(ref this, out value.valueU64))
            {
                return value.valueU64;
            }

            if (HypData_GetFloat(ref this, out value.valueFloat))
            {
                return value.valueFloat;
            }

            if (HypData_GetDouble(ref this, out value.valueDouble))
            {
                return value.valueDouble;
            }

            if (HypData_GetBool(ref this, out value.valueBool))
            {
                return value.valueBool;
            }

            if (HypData_GetID(ref this, out value.valueId))
            {
                return new IDBase(value.valueId);
            }

            if (HypData_GetHypObject(ref this, out value.valueHypObjectPtr))
            {
                if (value.valueHypObjectPtr == IntPtr.Zero)
                {
                    return null;
                }

                return *((object*)value.valueHypObjectPtr.ToPointer());
            }

            throw new NotImplementedException();

            // if (HypData_GetRefCountedPtr(ref this, out value.valueRefCountedPtr))
            // {
            //     return value.valueRefCountedPtr;
            // }
        }

        public bool IsInt8
        {
            get
            {
                return HypData_IsInt8(ref this);
            }
        }

        public sbyte? GetInt8()
        {
            sbyte value;

            if (HypData_GetInt8(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsInt16
        {
            get
            {
                return HypData_IsInt16(ref this);
            }
        }

        public short? GetInt16()
        {
            short value;

            if (HypData_GetInt16(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsInt32
        {
            get
            {
                return HypData_IsInt32(ref this);
            }
        }

        public int? GetInt32()
        {
            int value;

            if (HypData_GetInt32(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsInt64
        {
            get
            {
                return HypData_IsInt64(ref this);
            }
        }

        public long? GetInt64()
        {
            long value;

            if (HypData_GetInt64(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsUInt8
        {
            get
            {
                return HypData_IsUInt8(ref this);
            }
        }

        public byte? GetUInt8()
        {
            byte value;

            if (HypData_GetUInt8(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsUInt16
        {
            get
            {
                return HypData_IsUInt16(ref this);
            }
        }

        public ushort? GetUInt16()
        {
            ushort value;

            if (HypData_GetUInt16(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsUInt32
        {
            get
            {
                return HypData_IsUInt32(ref this);
            }
        }

        public uint? GetUInt32()
        {
            uint value;

            if (HypData_GetUInt32(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsUInt64
        {
            get
            {
                return HypData_IsUInt64(ref this);
            }
        }

        public ulong? GetUInt64()
        {
            ulong value;

            if (HypData_GetUInt64(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsFloat
        {
            get
            {
                return HypData_IsFloat(ref this);
            }
        }

        public float? GetFloat()
        {
            float value;

            if (HypData_GetFloat(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsDouble
        {
            get
            {
                return HypData_IsDouble(ref this);
            }
        }

        public double? GetDouble()
        {
            double value;

            if (HypData_GetDouble(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsBool
        {
            get
            {
                return HypData_IsBool(ref this);
            }
        }

        public bool? GetBool()
        {
            bool value;

            if (HypData_GetBool(ref this, out value))
            {
                return value;
            }

            return null;
        }

        public bool IsID
        {
            get
            {
                return HypData_IsID(ref this);
            }
        }

        public ID<T> GetID<T>() where T : HypObject
        {
            uint outId = 0;

            if (!HypData_GetID(ref this, out outId))
            {
                return ID<T>.Invalid;
            }

            return new ID<T>(outId);
        }

        public bool IsHypObject
        {
            get
            {
                return HypData_IsHypObject(ref this);
            }
        }

        public unsafe T? GetHypObject<T>() where T : HypObject
        {
            IntPtr objPtr;

            if (!HypData_GetHypObject(ref this, out objPtr))
            {
                return null;
            }

            if (objPtr == IntPtr.Zero) {
                return null;
            }

            return (T)(*((object*)objPtr.ToPointer()));
        }

        [DllImport("hyperion", EntryPoint = "HypData_GetTypeID")]
        private static extern void HypData_GetTypeID([In] ref HypData hypData, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetInt8([In] ref HypData hypData, [Out] out sbyte outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetInt16([In] ref HypData hypData, [Out] out short outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetInt32([In] ref HypData hypData, [Out] out int outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetInt64([In] ref HypData hypData, [Out] out long outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetUInt8([In] ref HypData hypData, [Out] out byte outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetUInt16([In] ref HypData hypData, [Out] out ushort outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetUInt32([In] ref HypData hypData, [Out] out uint outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetUInt64([In] ref HypData hypData, [Out] out ulong outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetFloat([In] ref HypData hypData, [Out] out float outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetDouble([In] ref HypData hypData, [Out] out double outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetBool([In] ref HypData hypData, [Out] out bool outValue);

        [DllImport("hyperion", EntryPoint = "HypData_GetHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetHypObject([In] ref HypData hypData, [Out] out IntPtr outObjectPtr);

        [DllImport("hyperion", EntryPoint = "HypData_GetID")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_GetID([In] ref HypData hypData, [Out] out uint outIdValue);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsInt8([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsInt16([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsInt32([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsInt64([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt8")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsUInt8([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt16")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsUInt16([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt32")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsUInt32([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsUInt64")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsUInt64([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsFloat")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsFloat([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsDouble")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsDouble([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsBool")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsBool([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsHypObject")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsHypObject([In] ref HypData hypData);

        [DllImport("hyperion", EntryPoint = "HypData_IsID")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool HypData_IsID([In] ref HypData hypData);
    }
}