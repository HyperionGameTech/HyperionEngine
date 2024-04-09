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
            IntPtr namePtr = Marshal.StringToHGlobalAnsi(name);
            Name value = Name_FromString(namePtr);
            Marshal.FreeHGlobal(namePtr);
            return value;
        }

        [DllImport("libhyperion", EntryPoint = "Name_FromString")]
        private static extern Name Name_FromString(IntPtr namePtr);
    }
}