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

        public World? World => this.GetWorld(); // extension method

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

        public bool TryGetComponent<T>(out T component) where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();

            if (entityManager == null)
            {
                component = default;

                return false;
            }

            return entityManager.TryGetComponent<T>(this, out component);
        }

        public ref T GetOrAddComponent<T>() where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();

            if (entityManager == null)
            {
                throw new Exception("Entity does not have an EntityManager");
            }

            return ref entityManager.GetOrAddComponent<T>(this);
        }

        public ref T GetOrAddComponent<T>(T component) where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();

            if (entityManager == null)
            {
                throw new Exception("Entity does not have an EntityManager");
            }

            return ref entityManager.GetOrAddComponent<T>(this, component);
        }

        public bool RemoveComponent<T>() where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();

            return entityManager?.RemoveComponent<T>(this) ?? false;
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

        public bool AddComponent<T>(ref T component) where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();
            
            if (entityManager == null)
            {
                throw new Exception("Entity does not have an EntityManager");
            }

            return entityManager.AddComponent<T>(this, ref component);
        }

        public bool AddComponent<T>(T component) where T : IComponent, allows ref struct
        {
            return AddComponent<T>(ref component);
        }

        public bool AddComponent<T>() where T : IComponent, allows ref struct
        {
            EntityManager? entityManager = this.GetEntityManager();

            if (entityManager == null)
            {
                throw new Exception("Entity does not have an EntityManager");
            }

            return entityManager.AddComponent<T>(this);
        }

        public void AddTag(EntityTag tag)
        {
            EntityManager? entityManager = this.GetEntityManager();
            entityManager?.AddTag(this, tag);
        }

        public bool RemoveTag(EntityTag tag)
        {
            EntityManager? entityManager = this.GetEntityManager();
            return entityManager?.RemoveTag(this, tag) ?? false;
        }

        public bool HasTag(EntityTag tag)
        {
            EntityManager? entityManager = this.GetEntityManager();
            return entityManager?.HasTag(this, tag) ?? false;
        }

        [DllImport("hyperion", EntryPoint = "Entity_GetID")]
        private static extern ulong Entity_GetID(IntPtr entityPtr);
    }
}