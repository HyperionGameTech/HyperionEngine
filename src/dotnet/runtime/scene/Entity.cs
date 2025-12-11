using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Entity")]
    public class Entity : Node
    {
        public ObjIdBase Id
        {
            get
            {
                ulong idValue = Entity_GetID(NativeAddress);
                return new ObjIdBase(new TypeId((uint)(idValue >> 32)), (uint)(idValue & 0xFFFFFFFF));
            }
        }

        public EntityManager? EntityManager => this.GetEntityManager(); // extension method

        public World World => this.GetWorld(); // extension method

        public bool ReceivesUpdate
        {
            get => this.ReceivesUpdate(); // extension method
            set => this.SetReceivesUpdate(value); // extension method
        }

        public ref T GetComponent<T>() where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();
            
            if (entityManager == null)
            {
                throw new Exception("Entity does not have an EntityManager");
            }

            return ref entityManager.GetComponent<T>(this);
        }

        public bool HasComponent<T>() where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();
            
            if (entityManager == null)
            {
                return false;
            }

            return entityManager.HasComponent<T>(this);
        }

        public void AddComponent<T>(ref T component) where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();
            
            if (entityManager == null)
            {
                throw new Exception("Entity does not have an EntityManager");
            }

            entityManager.AddComponent<T>(this, ref component);
        }

        [DllImport("hyperion", EntryPoint = "Entity_GetID")]
        private static extern ulong Entity_GetID(IntPtr entityPtr);
    }
}