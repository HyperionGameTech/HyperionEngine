using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    using ComponentID = uint;

    internal class ComponentDefinition
    {
        public TypeID nativeTypeId;

        public Func<IntPtr, Entity, IntPtr, ComponentID> addComponent;
    }

    public class EntityManager
    {
        private IntPtr ptr = IntPtr.Zero;
        private Dictionary<Type, ComponentDefinition> componentNativeTypeIDs = new Dictionary<Type, ComponentDefinition>();

        public EntityManager(IntPtr ptr)
        {
            this.ptr = ptr;

            RegisterComponent<TransformComponent>(new ComponentDefinition
            {
                nativeTypeId = TransformComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => TransformComponent_AddComponent(entityManagerPtr, entity, ptr)
            });
        }

        private void RegisterComponent<T>(ComponentDefinition componentDefinition) where T : IComponent
        {
            componentNativeTypeIDs.Add(typeof(T), componentDefinition);
        }

        public Entity AddEntity()
        {
            return EntityManager_AddEntity(ptr);
        }

        public void RemoveEntity(Entity entity)
        {
            EntityManager_RemoveEntity(ptr, entity);
        }

        public bool HasEntity(Entity entity)
        {
            return EntityManager_HasEntity(ptr, entity);
        }

        public bool HasComponent<T>(Entity entity) where T : IComponent
        {
            TypeID typeId = componentNativeTypeIDs[typeof(T)].nativeTypeId;

            return EntityManager_HasComponent(ptr, typeId, entity);
        }

        public ComponentID AddComponent<T>(Entity entity, T component) where T : IComponent
        {
            ComponentDefinition typeId = componentNativeTypeIDs[typeof(T)];

            // Have to pass address of component to native code
            IntPtr componentPtr = Marshal.AllocHGlobal(Marshal.SizeOf(component));
            Marshal.StructureToPtr(component, componentPtr, false);

            // call the native method
            ComponentID result = typeId.addComponent(ptr, entity, componentPtr);

            // Free the memory
            Marshal.FreeHGlobal(componentPtr);

            return result;
        }

        public T GetComponent<T>(Entity entity) where T : IComponent
        {
            ComponentDefinition typeId = componentNativeTypeIDs[typeof(T)];

            IntPtr componentPtr = EntityManager_GetComponent(ptr, typeId.nativeTypeId, entity);

            if (componentPtr == IntPtr.Zero)
            {
                return default(T);
            }

            // convert the native pointer to component struct
            return Marshal.PtrToStructure<T>(componentPtr);
        }

        [DllImport("libhyperion", EntryPoint = "EntityManager_AddEntity")]
        private static extern Entity EntityManager_AddEntity(IntPtr entityManagerPtr);

        [DllImport("libhyperion", EntryPoint = "EntityManager_RemoveEntity")]
        private static extern void EntityManager_RemoveEntity(IntPtr entityManagerPtr, Entity entity);

        [DllImport("libhyperion", EntryPoint = "EntityManager_HasEntity")]
        private static extern bool EntityManager_HasEntity(IntPtr entityManagerPtr, Entity entity);

        [DllImport("libhyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr entityManagerPtr, TypeID typeID, Entity entity);

        [DllImport("libhyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr entityManagerPtr, TypeID typeID, Entity entity);

        // Components
        [DllImport("libhyperion", EntryPoint = "TransformComponent_GetNativeTypeID")]
        private static extern TypeID TransformComponent_GetNativeTypeID();

        [DllImport("libhyperion", EntryPoint = "TransformComponent_AddComponent")]
        private static extern ComponentID TransformComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);
    }
}