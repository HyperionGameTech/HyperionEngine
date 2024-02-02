using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct Entity
    {
        private uint id;

        public Entity(uint id)
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