using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    using ComponentID = uint;

    public class ComponentDefinition
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

            RegisterComponent<MeshComponent>(new ComponentDefinition
            {
                nativeTypeId = MeshComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => MeshComponent_AddComponent(entityManagerPtr, entity, ptr)
            });

            RegisterComponent<BoundingBoxComponent>(new ComponentDefinition
            {
                nativeTypeId = BoundingBoxComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => BoundingBoxComponent_AddComponent(entityManagerPtr, entity, ptr)
            });

            RegisterComponent<VisibilityStateComponent>(new ComponentDefinition
            {
                nativeTypeId = VisibilityStateComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => VisibilityStateComponent_AddComponent(entityManagerPtr, entity, ptr)
            });

            RegisterComponent<LightComponent>(new ComponentDefinition
            {
                nativeTypeId = LightComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => LightComponent_AddComponent(entityManagerPtr, entity, ptr)
            });

            RegisterComponent<ShadowMapComponent>(new ComponentDefinition
            {
                nativeTypeId = ShadowMapComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => ShadowMapComponent_AddComponent(entityManagerPtr, entity, ptr)
            });

            RegisterComponent<UIComponent>(new ComponentDefinition
            {
                nativeTypeId = UIComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => UIComponent_AddComponent(entityManagerPtr, entity, ptr)
            });

            RegisterComponent<NodeLinkComponent>(new ComponentDefinition
            {
                nativeTypeId = NodeLinkComponent_GetNativeTypeID(),
                addComponent = (entityManagerPtr, entity, ptr) => NodeLinkComponent_AddComponent(entityManagerPtr, entity, ptr)
            });
        }

        public void RegisterComponent<T>(ComponentDefinition componentDefinition) where T : IComponent
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

        [DllImport("hyperion", EntryPoint = "EntityManager_AddEntity")]
        private static extern Entity EntityManager_AddEntity(IntPtr entityManagerPtr);

        [DllImport("hyperion", EntryPoint = "EntityManager_RemoveEntity")]
        private static extern void EntityManager_RemoveEntity(IntPtr entityManagerPtr, Entity entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_HasEntity")]
        private static extern bool EntityManager_HasEntity(IntPtr entityManagerPtr, Entity entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr entityManagerPtr, TypeID typeID, Entity entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr entityManagerPtr, TypeID typeID, Entity entity);

        // Components
        // TransformComponent
        [DllImport("hyperion", EntryPoint = "TransformComponent_GetNativeTypeID")]
        private static extern TypeID TransformComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "TransformComponent_AddComponent")]
        private static extern ComponentID TransformComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // MeshComponent
        [DllImport("hyperion", EntryPoint = "MeshComponent_GetNativeTypeID")]
        private static extern TypeID MeshComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "MeshComponent_AddComponent")]
        private static extern ComponentID MeshComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // BoundingBoxComponent
        [DllImport("hyperion", EntryPoint = "BoundingBoxComponent_GetNativeTypeID")]
        private static extern TypeID BoundingBoxComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "BoundingBoxComponent_AddComponent")]
        private static extern ComponentID BoundingBoxComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // VisibilityStateComponent
        [DllImport("hyperion", EntryPoint = "VisibilityStateComponent_GetNativeTypeID")]
        private static extern TypeID VisibilityStateComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "VisibilityStateComponent_AddComponent")]
        private static extern ComponentID VisibilityStateComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // LightComponent
        [DllImport("hyperion", EntryPoint = "LightComponent_GetNativeTypeID")]
        private static extern TypeID LightComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "LightComponent_AddComponent")]
        private static extern ComponentID LightComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // ShadowMapComponent
        [DllImport("hyperion", EntryPoint = "ShadowMapComponent_GetNativeTypeID")]
        private static extern TypeID ShadowMapComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "ShadowMapComponent_AddComponent")]
        private static extern ComponentID ShadowMapComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // UIComponent
        [DllImport("hyperion", EntryPoint = "UIComponent_GetNativeTypeID")]
        private static extern TypeID UIComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "UIComponent_AddComponent")]
        private static extern ComponentID UIComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // NodeLinkComponent
        [DllImport("hyperion", EntryPoint = "NodeLinkComponent_GetNativeTypeID")]
        private static extern TypeID NodeLinkComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "NodeLinkComponent_AddComponent")]
        private static extern ComponentID NodeLinkComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);
    }
}
