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

        public IDBase ID
        {
            get
            {
                return (IDBase)GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }

        public World? World
        {
            get
            {
                return (World?)GetProperty(PropertyNames.World)
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