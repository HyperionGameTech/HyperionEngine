using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Entity")]
    public class Entity : HypObject
    {
        public ObjIdBase Id
        {
            get
            {
                ulong idValue = Entity_GetID(NativeAddress);
                return new ObjIdBase(new TypeId((uint)(idValue >> 32)), (uint)(idValue & 0xFFFFFFFF));
            }
        }

        [DllImport("hyperion", EntryPoint = "Entity_GetID")]
        private static extern ulong Entity_GetID(IntPtr entityPtr);
    }
}