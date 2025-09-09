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
        private uint value;

        public HashCode(uint value)
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
    }
}
