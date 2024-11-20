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
        /// Returns the native TypeID for the given type using the type's name. The returned TypeID will have the DYNAMIC flag set on its first two bits.
        /// </summary>
        /// <typeparam name="T">The C# type to lookup the TypeID of</typeparam>
        /// <returns>TypeID</returns>
        public static TypeID ForType<T>()
        {
            string typeName = typeof(T).Name;

            TypeID value = new TypeID(0);
            TypeID_ForManagedType(typeName, out value);

            return value;
        }

        /// <summary>
        /// Returns the native TypeID for the given type using the type's name. The returned TypeID will have the DYNAMIC flag set on its first two bits.
        /// <param name="type">The C# type to lookup the TypeID of</param>
        /// <returns>TypeID</returns>
        /// </summary>
        public static TypeID ForType(Type type)
        {
            string typeName = type.Name;

            TypeID value = new TypeID(0);
            TypeID_ForManagedType(typeName, out value);

            return value;
        }

        [DllImport("hyperion", EntryPoint = "TypeID_ForManagedType")]
        private static extern TypeID TypeID_ForManagedType([MarshalAs(UnmanagedType.LPStr)] string typeName, [Out] out TypeID result);
    }
}