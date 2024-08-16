using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Corresponds to ID<Entity>
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct Entity
    {
        private IDBase id;

        public Entity(IDBase id)
        {
            this.id = id;
        }

        public IDBase ID
        {
            get
            {
                return id;
            }
        }
    }
}