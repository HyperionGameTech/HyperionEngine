using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Scene : IDisposable
    {
        private ManagedHandle handle;

        private Node root = null;
        private EntityManager entityManager = null;

        public Scene()
        {
            handle = Scene_Create();
            root = new Node(Scene_GetRoot(handle));
            entityManager = new EntityManager(Scene_GetEntityManager(handle));
        }

        public Scene(ManagedHandle handle)
        {
            this.handle = handle;
        }

        public void Dispose()
        {
            handle.Dispose();
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }

        public Node Root
        {
            get
            {
                return root;
            }
        }

        public EntityManager EntityManager
        {
            get
            {
                return entityManager;
            }
        }

        [DllImport("libhyperion", EntryPoint = "Scene_Create")]
        private static extern ManagedHandle Scene_Create();

        [DllImport("libhyperion", EntryPoint = "Scene_GetRoot")]
        private static extern ManagedNode Scene_GetRoot(ManagedHandle scene);

        [DllImport("libhyperion", EntryPoint = "Scene_GetEntityManager")]
        private static extern IntPtr Scene_GetEntityManager(ManagedHandle scene);
    }
}