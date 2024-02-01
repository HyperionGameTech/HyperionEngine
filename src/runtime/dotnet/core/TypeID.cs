using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Represents a native (C++) TypeID (see core/lib/TypeID.hpp)
    /// </summary>
    
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct TypeID
    {
        private uint id;

        public TypeID(uint id)
        {
            this.id = id;
        }

        public uint ID
        {
            get
            {
                return id;
            }
        }
    }
}