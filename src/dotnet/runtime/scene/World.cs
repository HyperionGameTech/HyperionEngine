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

        public void AddScene(Scene scene)
        {
            World_AddScene(ptr, scene.Handle);
        }

        public uint ID
        {
            get
            {
                return World_GetID(ptr);
            }
        }

        [DllImport("libhyperion", EntryPoint = "World_AddScene")]
        private static extern void World_AddScene(IntPtr worldPtr, ManagedHandle scene);

        [DllImport("libhyperion", EntryPoint = "World_GetID")]
        private static extern uint World_GetID(IntPtr worldPtr);
    }
}