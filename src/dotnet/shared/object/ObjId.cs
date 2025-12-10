using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct ObjIdBase
    {
        public static readonly ObjIdBase Invalid = new ObjIdBase();

        [FieldOffset(0), MarshalAs(UnmanagedType.U4)]
        private uint _typeIdValue;

        [FieldOffset(4), MarshalAs(UnmanagedType.U4)]
        private uint _value;

        public ObjIdBase()
        {
            _typeIdValue = TypeId.Void.Value;
            _value = 0;
        }

        public ObjIdBase(TypeId typeId, uint value)
        {
            _typeIdValue = typeId.Value;
            _value = value;
        }

        public TypeId TypeId => new TypeId(_typeIdValue);
        public uint Value => _value;
        public bool IsValid => _value != 0;

        public override bool Equals(object obj)
        {
            if (obj is ObjIdBase)
            {
                return this == (ObjIdBase)obj;
            }

            return false;
        }

        public override string ToString()
        {
            return string.Format("ObjIdBase(TypeId: {0}, Value: {0})", _typeIdValue, _value);
        }

        public static bool operator ==(ObjIdBase a, ObjIdBase b)
        {
            return a._typeIdValue == b._typeIdValue && a._value == b._value;
        }

        public static bool operator !=(ObjIdBase a, ObjIdBase b)
        {
            return a._typeIdValue != b._typeIdValue || a._value != b._value;
        }
    }

    // [StructLayout(LayoutKind.Sequential, Size = 4)]
    // public struct ObjId<T> where T : ObjectBase
    // {
    //     public static readonly ObjId<T> Invalid = new ObjId<T>(0);

    //     [MarshalAs(UnmanagedType.U4)]
    //     private uint value;

    //     public Id(uint value)
    //     {
    //         this.value = value;
    //     }

    //     public Id(ObjIdBase id)
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
    //         if (obj is ObjId<T>)
    //         {
    //             return this == (ObjId<T>)obj;
    //         }

    //         return false;
    //     }

    //     public override string ToString()
    //     {
    //         return string.Format("ObjId<{0}>({1})", typeof(T).Name, value);
    //     }

    //     public static bool operator==(ObjId<T> a, ObjId<T> b)
    //     {
    //         return a.value == b.value;
    //     }

    //     public static bool operator!=(ObjId<T> a, ObjId<T> b)
    //     {
    //         return a.value != b.value;
    //     }

    //     public static implicit operator ObjIdBase(ObjId<T> id)
    //     {
    //         return new ObjIdBase(id.value);
    //     }

    //     public static implicit operator ObjId<T>(ObjIdBase id)
    //     {
    //         return new ObjId<T>(id.value);
    //     }
    // }
}