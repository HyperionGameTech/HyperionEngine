using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    [Flags]
    public enum ComponentRWFlags : uint
    {
        None = 0x0,
        Read = 0x1,
        Write = 0x2,
        ReadWrite = Read | Write
    }

    [HypClassBinding(Name = "ComponentInfo")]
    [StructLayout(LayoutKind.Sequential, Size = 12, Pack = 4)]
    public struct ComponentInfo
    {
        private TypeID typeId;
        private ComponentRWFlags rwFlags;
        private bool receivesEvents;

        public ComponentInfo(TypeID typeId, ComponentRWFlags rwFlags, bool receivesEvents)
        {
            this.typeId = typeId;
            this.rwFlags = rwFlags;
            this.receivesEvents = receivesEvents;
        }
    }
}