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
            this.handle = new ManagedHandle();
            Scene_Create(out this.handle);

            ManagedNode rootNode = new ManagedNode();
            Scene_GetRoot(this.handle, out rootNode);
            this.root = new Node(rootNode);
            this.entityManager = new EntityManager(Scene_GetEntityManager(this.handle));
        }

        public Scene(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Scene_GetTypeID());

            ManagedNode rootNode = new ManagedNode();
            Scene_GetRoot(this.handle, out rootNode);
            this.root = new Node(rootNode);
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
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID Scene_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Scene_Create")]
        private static extern void Scene_Create([Out] out ManagedHandle scene);

        [DllImport("hyperion", EntryPoint = "Scene_GetRoot")]
        private static extern void Scene_GetRoot(ManagedHandle scene, [Out] out ManagedNode root);

        [DllImport("hyperion", EntryPoint = "Scene_GetEntityManager")]
        private static extern IntPtr Scene_GetEntityManager(ManagedHandle scene);
    }
}