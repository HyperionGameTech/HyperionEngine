using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Engine
    {
        private static Engine? instance = null;

        public static Engine Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new Engine(Engine_GetInstance());
                }

                return instance;
            }
        }

        private IntPtr ptr;

        private World? world;

        public Engine(IntPtr ptr)
        {
            this.ptr = ptr;

            // IntPtr worldPtr = Engine_GetWorld(ptr);

            // if (worldPtr != IntPtr.Zero)
            // {
            //     this.world = new World(worldPtr);
            // }
            // else
            // {
            //     this.world = null;
            // }
        }

        public World? World
        {
            get
            {
                return world;
            }
        }

        [DllImport("hyperion", EntryPoint = "Engine_GetInstance")]
        private static extern IntPtr Engine_GetInstance();

        [DllImport("hyperion", EntryPoint = "Engine_GetWorld")]
        private static extern IntPtr Engine_GetWorld(IntPtr enginePtr);
    }
}