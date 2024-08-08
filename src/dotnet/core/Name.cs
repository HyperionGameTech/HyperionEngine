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

        public ulong HashCode
        {
            get
            {
                return hashCode;
            }
        }

        public override string ToString()
        {
            return Marshal.PtrToStringAnsi(Name_LookupString(ref this));
        }

        public static Name FromString(string nameString)
        {
            Name name = new Name(0);
            Name_FromString(nameString, out name);
            return name;
        }

        [DllImport("hyperion", EntryPoint = "Name_FromString")]
        private static extern void Name_FromString([MarshalAs(UnmanagedType.LPStr)] string str, [Out] out Name result);

        [DllImport("hyperion", EntryPoint = "Name_LookupString")]
        private static extern IntPtr Name_LookupString([In] ref Name name);
    }
}