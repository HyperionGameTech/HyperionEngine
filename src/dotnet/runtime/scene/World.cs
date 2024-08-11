using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class World
    {
        private IntPtr ptr;

        public World(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Subsystem? GetSubsystem(TypeID typeID)
        {
            IntPtr subsystemPtr = World_GetSubsystem(ptr, typeID);

            if (subsystemPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Subsystem(subsystemPtr);
        }

        [DllImport("hyperion", EntryPoint = "World_GetSubsystem")]
        private static extern IntPtr World_GetSubsystem(IntPtr worldPtr, TypeID typeID);
    }
}