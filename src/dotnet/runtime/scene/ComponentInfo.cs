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

    [ClassBinding(Name = "ComponentInfo")]
    [StructLayout(LayoutKind.Sequential, Size = 12, Pack = 4)]
    public struct ComponentInfo
    {
        public TypeId TypeId;
        public ComponentRWFlags RwFlags;
        public bool ReceivesEvents;

        public ComponentInfo(TypeId typeId, ComponentRWFlags rwFlags, bool receivesEvents)
        {
            TypeId = typeId;
            RwFlags = rwFlags;
            ReceivesEvents = receivesEvents;
        }
    }
}