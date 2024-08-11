using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="World")]
    public class World : HypObject
    {
        public World()
        {
        }

        // public Subsystem? GetSubsystem(TypeID typeID)
        // {
        //     IntPtr subsystemPtr = World_GetSubsystem(ptr, typeID);

        //     if (subsystemPtr == IntPtr.Zero)
        //     {
        //         return null;
        //     }

        //     return new Subsystem(subsystemPtr);
        // }

        // [DllImport("hyperion", EntryPoint = "World_GetSubsystem")]
        // private static extern IntPtr World_GetSubsystem(IntPtr worldPtr, TypeID typeID);
    }
}