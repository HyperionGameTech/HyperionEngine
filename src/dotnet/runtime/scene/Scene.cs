using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Scene")]
    public class Scene : HypObject
    {
        private Node? root;
        private EntityManager? entityManager;

        public Scene()
        {
        }

        public ID<Scene> ID
        {
            get
            {
                return (IDBase)GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }

        public Node Root
        {
            get
            {
                return root!;
            }
        }

        public World? World
        {
            get
            {
                // IntPtr worldPtr = Scene_GetWorld(handle);

                // if (worldPtr == IntPtr.Zero)
                // {
                //     return null;
                // }

                // return new World(worldPtr);

                throw new NotImplementedException();
            }
        }

        public EntityManager EntityManager
        {
            get
            {
                return entityManager!;
            }
        }

        [DllImport("hyperion", EntryPoint = "Scene_GetWorld")]
        private static extern IntPtr Scene_GetWorld(ManagedHandle scene);
    }
}