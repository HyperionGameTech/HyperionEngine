using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct IdBase
    {
        public static readonly IdBase Invalid = new IdBase();

        [FieldOffset(0), MarshalAs(UnmanagedType.U4)]
        private uint typeIdValue;

        [FieldOffset(4), MarshalAs(UnmanagedType.U4)]
        private uint value;

        public IdBase()
        {
            this.typeIdValue = TypeId.Void.Value;
            this.value = 0;
        }

        public IdBase(TypeId typeId, uint value)
        {
            this.typeIdValue = typeId.Value;
            this.value = value;
        }

        public TypeId TypeId
        {
            get
            {
                return new TypeId(typeIdValue);
            }
        }

        public uint Value
        {
            get
            {
                return value;
            }
        }

        public bool IsValid
        {
            get
            {
                return typeIdValue != 0 && value != 0;
            }
        }

        public override bool Equals(object obj)
        {
            if (obj is IdBase)
            {
                return this == (IdBase)obj;
            }

            return false;
        }

        public override string ToString()
        {
            return string.Format("IdBase(TypeId: {0}, Value: {0})", typeIdValue, value);
        }

        public static bool operator==(IdBase a, IdBase b)
        {
            return a.typeIdValue == b.typeIdValue && a.value == b.value;
        }

        public static bool operator!=(IdBase a, IdBase b)
        {
            return a.typeIdValue != b.typeIdValue || a.value != b.value;
        }
    }

    // [StructLayout(LayoutKind.Sequential, Size = 4)]
    // public struct Id<T> where T : HypObject
    // {
    //     public static readonly Id<T> Invalid = new Id<T>(0);

    //     [MarshalAs(UnmanagedType.U4)]
    //     private uint value;

    //     public Id(uint value)
    //     {
    //         this.value = value;
    //     }

    //     public Id(IdBase id)
    //     {
    //         this.value = id.Value;
    //     }

    //     public uint Value
    //     {
    //         get
    //         {
    //             return value;
    //         }
    //     }

    //     public bool IsValid
    //     {
    //         get
    //         {
    //             return value != 0;
    //         }
    //     }

    //     public override bool Equals(object obj)
    //     {
    //         if (obj is Id<T>)
    //         {
    //             return this == (Id<T>)obj;
    //         }

    //         return false;
    //     }

    //     public override string ToString()
    //     {
    //         return string.Format("Id<{0}>({1})", typeof(T).Name, value);
    //     }

    //     public static bool operator==(Id<T> a, Id<T> b)
    //     {
    //         return a.value == b.value;
    //     }

    //     public static bool operator!=(Id<T> a, Id<T> b)
    //     {
    //         return a.value != b.value;
    //     }

    //     public static implicit operator IdBase(Id<T> id)
    //     {
    //         return new IdBase(id.value);
    //     }

    //     public static implicit operator Id<T>(IdBase id)
    //     {
    //         return new Id<T>(id.value);
    //     }
    // }
}