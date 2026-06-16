using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    [Flags]
    public enum ComponentAccess : byte
    {
        None = 0x0,
        Read = 0x1,
        Write = 0x2,
        ReadWrite = Read | Write
    }

    [ClassBinding(Name = "ComponentInfo")]
    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 4)]
    public struct ComponentInfo
    {
        public TypeId TypeId;
        public ComponentAccess Access;
        public bool ReceivesEvents;

        public ComponentInfo(TypeId typeId, ComponentAccess access, bool receivesEvents)
        {
            TypeId = typeId;
            Access = access;
            ReceivesEvents = receivesEvents;
        }
    }
}