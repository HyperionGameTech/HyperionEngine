using System;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    /// <summary>
    /// Represents a hashed name (see Core/name/Name.hpp for implementation)
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct Name
    {
        internal static ConcurrentDictionary<string, Name> nameCache = new ConcurrentDictionary<string, Name>();

        public static readonly Name Invalid = new Name(0);

        [FieldOffset(0)]
        internal ulong _value;

        public Name(ulong value)
        {
            _value = value;
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

        public ulong HashCode => _value;

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

        public static bool operator ==(Name a, Name b)
        {
            return a.Equals(b);
        }

        public static bool operator !=(Name a, Name b)
        {
            return !a.Equals(b);
        }

        public static bool operator ==(Name a, string b)
        {
            return a.Equals(new Name(b, weak: true));
        }

        public static bool operator !=(Name a, string b)
        {
            return !a.Equals(new Name(b, weak: true));
        }

        public bool Equals(Name other)
        {
            return _value == other._value;
        }

        public override bool Equals(object? obj)
        {
            if (obj is Name)
            {
                return Equals((Name)obj);
            }

            if (obj is string)
            {
                return Equals(new Name((string)obj, weak: true));
            }

            return false;
        }

        // allow implicit conversion from string to Name
        public static implicit operator Name(string nameString)
        {
            return new Name(nameString, weak: true); // weak by default to avoid polluting the cache
        }

        [DllImport("hyperion", EntryPoint = "Name_FromString")]
        private static extern void Name_FromString([MarshalAs(UnmanagedType.LPStr)] string str, [MarshalAs(UnmanagedType.I1)] bool weak, [Out] out Name result);

        [DllImport("hyperion", EntryPoint = "Name_LookupString")]
        private static extern IntPtr Name_LookupString([In] ref Name name);
    }
}