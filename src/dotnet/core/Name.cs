using System;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    /// <summary>
    /// Represents a hashed name (see core/Name.hpp for implementation)
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct Name
    {
        internal static ConcurrentDictionary<string, Name> nameCache = new ConcurrentDictionary<string, Name>();
        
        public static readonly Name Invalid = new Name(0);

        [FieldOffset(0)]
        internal ulong hashCode;

        public Name(ulong hashCode)
        {
            this.hashCode = hashCode;
        }

        public Name(string nameString, bool weak = false)
        {
            // try to find the name in the cache
            if (nameCache.TryGetValue(nameString, out Name cachedName))
            {
                this = cachedName;
                return;
            }

            // if the name is not in the cache, create a new one
            Name_FromString(nameString, weak, out this);

            if (!weak)
            {
                // add the name to the cache
                nameCache[nameString] = this;
            }
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

        public static Name FromString(string nameString, bool weak = false)
        {
            Name name;
            Name_FromString(nameString, weak, out name);
            return name;
        }

        public static bool operator == (Name a, Name b)
        {
            return a.Equals(b);
        }

        public static bool operator != (Name a, Name b)
        {
            return !a.Equals(b);
        }

        [DllImport("hyperion", EntryPoint = "Name_FromString")]
        private static extern void Name_FromString([MarshalAs(UnmanagedType.LPStr)] string str, [MarshalAs(UnmanagedType.I1)] bool weak, [Out] out Name result);

        [DllImport("hyperion", EntryPoint = "Name_LookupString")]
        private static extern IntPtr Name_LookupString([In] ref Name name);
    }
}