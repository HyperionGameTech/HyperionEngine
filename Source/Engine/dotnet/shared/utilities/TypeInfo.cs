using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [Flags]
    public enum TypeInfoFlags : uint
    {
        None = 0x0,

        PodType = 0x1,
        ClassType = 0x2,
        StructType = 0x4,
        ClassOrStructType = ClassType | StructType,

        EnumType = 0x8,
        EnumFlagsType = 0x10,

        FundamentalType = 0x20,
        IntegralType = 0x40,
        FloatType = 0x80,

        // Container types
        ArrayType = 0x2000,
        LinkedListType = 0x8000,
        StringType = 0x10000,
        MapType = 0x20000,
        SetType = 0x40000,
        VariantType = 0x100000,

        // Vector types
        Vec2Type = 0x200000,
        Vec3Type = 0x400000,
        Vec4Type = 0x800000,
        VectorType = Vec2Type | Vec3Type | Vec4Type,

        // Matrix types
        Mat3Type = 0x1000000,
        Mat4Type = 0x2000000,
        MatrixType = Mat3Type | Mat4Type,

        // Tuple/Pair types
        TupleType = 0x4000000,
        PairType = 0x8000000,

        // Handle<T> type (where T : ObjectBase)
        HandleType = 0x10000000
    }

    /// <summary>
    ///  Wraps a pointer to a native (C++) TypeInfo (see core/reflection/TypeInfoFwd.hpp)
    /// </summary>

    [StructLayout(LayoutKind.Sequential)]
    public struct TypeInfo
    {
        private IntPtr _ptr;

        public TypeInfo()
        {
            _ptr = IntPtr.Zero;
        }

        public TypeInfo(IntPtr ptr)
        {
            _ptr = ptr;
        }

        public IntPtr Address => _ptr;

        public bool IsNull => _ptr == IntPtr.Zero;

        public bool IsValid => TypeInfo_IsValid(_ptr);

        public Name Name
        {
            get
            {
                Name name;
                TypeInfo_GetName(_ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                TypeInfo_GetId(_ptr, out typeId);
                return typeId;
            }
        }

        public Class? Class
        {
            get
            {
                IntPtr pClass = TypeInfo_GetClass(_ptr);

                if (pClass == IntPtr.Zero)
                {
                    return null;
                }

                return new Class(pClass);
            }
        }

        public uint Size => TypeInfo_GetSize(_ptr);
        public uint Alignment => TypeInfo_GetAlignment(_ptr);

        public TypeInfoFlags Flags => (TypeInfoFlags)TypeInfo_GetFlags(_ptr);

        public bool IsString => (Flags & TypeInfoFlags.StringType) != 0;
        public bool IsArray => (Flags & TypeInfoFlags.ArrayType) != 0;
        public bool IsMap => (Flags & TypeInfoFlags.MapType) != 0;
        public bool IsSet => (Flags & TypeInfoFlags.SetType) != 0;
        public bool IsVariant => (Flags & TypeInfoFlags.VariantType) != 0;
        public bool IsVec2 => (Flags & TypeInfoFlags.Vec2Type) != 0;
        public bool IsVec3 => (Flags & TypeInfoFlags.Vec3Type) != 0;
        public bool IsVec4 => (Flags & TypeInfoFlags.Vec4Type) != 0;
        public bool IsVector => (Flags & TypeInfoFlags.VectorType) != 0;
        public bool IsMat3 => (Flags & TypeInfoFlags.Mat3Type) != 0;
        public bool IsMat4 => (Flags & TypeInfoFlags.Mat4Type) != 0;
        public bool IsMatrix => (Flags & TypeInfoFlags.MatrixType) != 0;
        public bool IsPod => (Flags & TypeInfoFlags.PodType) != 0;
        public bool IsClass => (Flags & TypeInfoFlags.ClassType) != 0;
        public bool IsStruct => (Flags & TypeInfoFlags.StructType) != 0;
        public bool IsEnum => (Flags & TypeInfoFlags.EnumType) != 0;
        public bool IsEnumFlags => (Flags & TypeInfoFlags.EnumFlagsType) != 0;
        public bool IsFundamental => (Flags & TypeInfoFlags.FundamentalType) != 0;
        public bool IsIntegral => (Flags & TypeInfoFlags.IntegralType) != 0;
        public bool IsFloat => (Flags & TypeInfoFlags.FloatType) != 0;
        public bool IsHandle => (Flags & TypeInfoFlags.HandleType) != 0;
        

        [DllImport("hyperion", EntryPoint = "TypeInfo_IsValid")]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool TypeInfo_IsValid(IntPtr typeInfo);

        [DllImport("hyperion", EntryPoint = "TypeInfo_GetName")]
        internal static extern void TypeInfo_GetName(IntPtr typeInfo, out Name name);

        [DllImport("hyperion", EntryPoint = "TypeInfo_GetId")]
        internal static extern void TypeInfo_GetId(IntPtr typeInfo, out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "TypeInfo_GetSize")]
        internal static extern uint TypeInfo_GetSize(IntPtr typeInfo);

        [DllImport("hyperion", EntryPoint = "TypeInfo_GetAlignment")]
        internal static extern uint TypeInfo_GetAlignment(IntPtr typeInfo);

        [DllImport("hyperion", EntryPoint = "TypeInfo_GetFlags")]
        internal static extern uint TypeInfo_GetFlags(IntPtr typeInfo);

        [DllImport("hyperion", EntryPoint = "TypeInfo_GetClass")]
        internal static extern IntPtr TypeInfo_GetClass(IntPtr typeInfo);
    }
}