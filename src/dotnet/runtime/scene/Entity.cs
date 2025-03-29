using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Entity")]
    public class Entity : HypObject
    {
        public IDBase ID
        {
            get
            {
                return new IDBase(Entity_GetID(NativeAddress));
            }
        }

        [DllImport("hyperion", EntryPoint = "Entity_GetID")]
        private static extern uint Entity_GetID(IntPtr entityPtr);
    }
}