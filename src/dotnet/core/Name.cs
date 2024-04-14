using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    /// Represents a hashed name (see core/Name.hpp for implementation)
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct Name
    {
        private ulong hashCode;

        public Name(ulong hashCode)
        {
            this.hashCode = hashCode;
        }

        public bool Equals(Name other)
        {
            return hashCode == other.hashCode;
        }

        public static Name FromString(string name)
        {
            return Name_FromString(name);
        }

        public ulong HashCode
        {
            get
            {
                return hashCode;
            }
        }

        [DllImport("hyperion", EntryPoint = "Name_FromString")]
        private static extern Name Name_FromString([MarshalAs(UnmanagedType.LPStr)] string namePtr);
    }
}