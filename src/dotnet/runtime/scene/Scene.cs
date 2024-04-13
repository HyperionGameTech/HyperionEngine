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
            this.handle = Scene_Create();

            this.root = new Node(Scene_GetRoot(this.handle));
            this.entityManager = new EntityManager(Scene_GetEntityManager(this.handle));
        }

        public Scene(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Scene_GetTypeID());

            this.root = new Node(Scene_GetRoot(this.handle));
            this.entityManager = new EntityManager(Scene_GetEntityManager(this.handle));
        }

        public void Dispose()
        {
            handle.DecRef(Scene_GetTypeID());
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
        
        [DllImport("hyperion", EntryPoint = "Scene_GetTypeID")]
        private static extern TypeID Scene_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Scene_Create")]
        private static extern ManagedHandle Scene_Create();

        [DllImport("hyperion", EntryPoint = "Scene_GetRoot")]
        private static extern ManagedNode Scene_GetRoot(ManagedHandle scene);

        [DllImport("hyperion", EntryPoint = "Scene_GetEntityManager")]
        private static extern IntPtr Scene_GetEntityManager(ManagedHandle scene);
    }
}