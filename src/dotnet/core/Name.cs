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

        [DllImport("libhyperion", EntryPoint = "Name_FromString")]
        public static extern Name FromString(string name);
    }
}