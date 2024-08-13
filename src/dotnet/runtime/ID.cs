using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct IDBase
    {
        public static readonly IDBase Invalid = new IDBase(0);

        [MarshalAs(UnmanagedType.U4)]
        private uint value;

        public IDBase(uint value)
        {
            this.value = value;
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
                return value != 0;
            }
        }

        public override bool Equals(object obj)
        {
            if (obj is IDBase)
            {
                return this == (IDBase)obj;
            }

            return false;
        }

        public override string ToString()
        {
            return string.Format("IDBase({0})", value);
        }

        public static bool operator==(IDBase a, IDBase b)
        {
            return a.value == b.value;
        }

        public static bool operator!=(IDBase a, IDBase b)
        {
            return a.value != b.value;
        }
    }

    // [StructLayout(LayoutKind.Sequential, Size = 4)]
    // public struct ID<T> where T : HypObject
    // {
    //     public static readonly ID<T> Invalid = new ID<T>(0);

    //     [MarshalAs(UnmanagedType.U4)]
    //     private uint value;

    //     public ID(uint value)
    //     {
    //         this.value = value;
    //     }

    //     public ID(IDBase id)
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
    //         if (obj is ID<T>)
    //         {
    //             return this == (ID<T>)obj;
    //         }

    //         return false;
    //     }

    //     public override string ToString()
    //     {
    //         return string.Format("ID<{0}>({1})", typeof(T).Name, value);
    //     }

    //     public static bool operator==(ID<T> a, ID<T> b)
    //     {
    //         return a.value == b.value;
    //     }

    //     public static bool operator!=(ID<T> a, ID<T> b)
    //     {
    //         return a.value != b.value;
    //     }

    //     public static implicit operator IDBase(ID<T> id)
    //     {
    //         return new IDBase(id.value);
    //     }

    //     public static implicit operator ID<T>(IDBase id)
    //     {
    //         return new ID<T>(id.value);
    //     }
    // }
}