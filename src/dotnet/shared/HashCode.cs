using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Represents HashCode.hpp from the core library
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct HashCode
    {
        private uint _value;

        public HashCode(uint value)
        {
            _value = value;
        }

        public uint Value => _value;
    }
}
