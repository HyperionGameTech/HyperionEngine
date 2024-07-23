using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Represents a native (C++) TypeID (see core/utilities/TypeID.hpp)
    /// </summary>
    
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct TypeID
    {
        private static readonly TypeID Void = new TypeID(0);

        private uint value;

        public TypeID()
        {
            value = 0;
        }

        public TypeID(uint value)
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

        public static bool operator==(TypeID a, TypeID b)
        {
            return a.value == b.value;
        }

        public static bool operator!=(TypeID a, TypeID b)
        {
            return a.value != b.value;
        }

        public override bool Equals(object obj)
        {
            if (obj is TypeID)
            {
                return this == (TypeID)obj;
            }

            return false;
        }

        public override int GetHashCode()
        {
            return (int)value;
        }

        /// <summary>
        /// Returns the native TypeID for the given type using the type's name. If the type is intended to match a native type, the name of the C# type must match the native type's name.
        /// </summary>
        /// <typeparam name="T">The C# type to lookup the TypeID of</typeparam>
        /// <returns>TypeID</returns>
        public static TypeID ForType<T>()
        {
            string typeName = typeof(T).Name;

            TypeID value = new TypeID(0);
            TypeID_FromString(typeName, out value);

            return value;
        }

        public static TypeID FromString(string typeName)
        {
            TypeID value = new TypeID(0);
            TypeID_FromString(typeName, out value);

            return value;
        }

        [DllImport("hyperion", EntryPoint = "TypeID_FromString")]
        private static extern TypeID TypeID_FromString([MarshalAs(UnmanagedType.LPStr)] string typeName, [Out] out TypeID result);
    }
}