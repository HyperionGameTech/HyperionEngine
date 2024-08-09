using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Scene")]
    public class Scene : HypObject
    {
        private ManagedHandle handle;
        private Node? root;
        private EntityManager? entityManager;

        public Scene()
        {
        }

        public Scene(ManagedHandle handle)
        {
        }

        // public Scene()
        // {
        //     // this.handle = new ManagedHandle();
        //     // Scene_Create(out this.handle);

        //     // ManagedNode rootNode = new ManagedNode();
        //     // Scene_GetRoot(this.handle, out rootNode);

        //     // this.root = new Node(rootNode);
        //     // this.entityManager = new EntityManager(Scene_GetEntityManager(this.handle));
        // }

        // public Scene(ManagedHandle handle)
        // {
        //     this.handle = handle;
        //     this.handle.IncRef(Scene_GetTypeID());

        //     ManagedNode rootNode = new ManagedNode();
        //     Scene_GetRoot(this.handle, out rootNode);

        //     this.root = new Node(rootNode);
        //     this.entityManager = new EntityManager(Scene_GetEntityManager(this.handle));
        // }

        ~Scene()
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
                return root!;
            }
        }

        public World? World
        {
            get
            {
                IntPtr worldPtr = Scene_GetWorld(handle);

                if (worldPtr == IntPtr.Zero)
                {
                    return null;
                }

                return new World(worldPtr);
            }
        }

        public EntityManager EntityManager
        {
            get
            {
                return entityManager!;
            }
        }
        
        [DllImport("hyperion", EntryPoint = "Scene_GetTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID Scene_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Scene_Create")]
        private static extern void Scene_Create([Out] out ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "Scene_GetWorld")]
        private static extern IntPtr Scene_GetWorld(ManagedHandle scene);

        [DllImport("hyperion", EntryPoint = "Scene_GetRoot")]
        private static extern void Scene_GetRoot(ManagedHandle scene, [Out] out ManagedNode root);

        [DllImport("hyperion", EntryPoint = "Scene_GetEntityManager")]
        private static extern IntPtr Scene_GetEntityManager(ManagedHandle scene);
    }
}