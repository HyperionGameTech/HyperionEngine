using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Represents a native (C++) TypeId (see core/reflection/TypeId.hpp)
    /// </summary>
    
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct TypeId
    {
        public static readonly TypeId Void = new TypeId(0);

        private uint _value;

        public TypeId()
        {
            _value = 0;
        }

        public TypeId(uint value)
        {
            _value = value;
        }

        public uint Value => _value;

        public bool IsValid => _value != 0;

        public static bool operator==(TypeId a, TypeId b)
        {
            return a._value == b._value;
        }

        public static bool operator!=(TypeId a, TypeId b)
        {
            return a._value != b._value;
        }

        public override bool Equals(object obj)
        {
            if (obj is TypeId)
            {
                return this == (TypeId)obj;
            }

            return false;
        }

        public override int GetHashCode()
        {
            return (int)_value;
        }

        /// <summary>
        /// Returns the native TypeId for the given type using the type's name. The returned TypeId will have the DYNAMIC flag set on its first two bits.
        /// </summary>
        /// <typeparam name="T">The C# type to lookup the TypeId of</typeparam>
        /// <returns>TypeId</returns>
        public static TypeId ForType<T>()
        {
            string typeName = typeof(T).Name;

            TypeId value;
            TypeId_ForManagedType(typeName, out value);

            return value;
        }

        /// <summary>
        /// Returns the native TypeId for the given type using the type's name. The returned TypeId will have the DYNAMIC flag set on its first two bits.
        /// <param name="type">The C# type to lookup the TypeId of</param>
        /// <returns>TypeId</returns>
        /// </summary>
        public static TypeId ForType(Type type)
        {
            string typeName = type.Name;

            TypeId value;
            TypeId_ForManagedType(typeName, out value);

            return value;
        }

        [DllImport("hyperion", EntryPoint = "TypeId_ForManagedType")]
        private static extern TypeId TypeId_ForManagedType([MarshalAs(UnmanagedType.LPStr)] string typeName, [Out] out TypeId result);
    }
}