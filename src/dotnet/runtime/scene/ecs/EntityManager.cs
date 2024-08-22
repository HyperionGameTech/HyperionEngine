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

    [HypClassBinding(Name="EntityManager")]
    public class EntityManager : HypObject
    {
        private Dictionary<Type, ComponentDefinition> componentNativeTypeIDs = new Dictionary<Type, ComponentDefinition>();

        public EntityManager()
        {
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

        public void RegisterComponent<T>(ComponentDefinition componentDefinition) where T : struct, IComponent
        {
            componentNativeTypeIDs.Add(typeof(T), componentDefinition);
        }

        public Entity AddEntity()
        {
            return new Entity((IDBase)GetMethod(MethodNames.AddEntity)
                .Invoke(this)
                .GetValue());
        }

        public void RemoveEntity(Entity entity)
        {
            GetMethod(MethodNames.RemoveEntity)
                .Invoke(this, new HypData(entity.ID));
        }

        public bool HasEntity(Entity entity)
        {
            return (bool)GetMethod(MethodNames.HasEntity)
                .Invoke(this, new HypData(entity.ID))
                .GetValue();
        }

        // public Entity AddEntity()
        // {
        //     return EntityManager_AddEntity(ptr);
        // }

        // public void RemoveEntity(Entity entity)
        // {
        //     EntityManager_RemoveEntity(ptr, entity);
        // }

        // public bool HasEntity(Entity entity)
        // {
        //     return EntityManager_HasEntity(ptr, entity);
        // }

        public bool HasComponent<T>(Entity entity) where T : struct, IComponent
        {
            TypeID typeId = componentNativeTypeIDs[typeof(T)].nativeTypeId;

            return EntityManager_HasComponent(NativeAddress, typeId, entity);
        }

        public ComponentID AddComponent<T>(Entity entity, T component) where T : struct, IComponent
        {
            ComponentDefinition componentDefinition = componentNativeTypeIDs[typeof(T)];

            // Have to pass address of component to native code
            IntPtr componentPtr = Marshal.AllocHGlobal(Marshal.SizeOf(component));
            Marshal.StructureToPtr(component, componentPtr, false);

            // call the native method
            ComponentID result = componentDefinition.addComponent(NativeAddress, entity, componentPtr);

            // Free the memory
            Marshal.FreeHGlobal(componentPtr);

            return result;
        }

        public ref T GetComponent<T>(Entity entity) where T : struct, IComponent
        {
            ComponentDefinition typeId = componentNativeTypeIDs[typeof(T)];

            IntPtr componentPtr = EntityManager_GetComponent(NativeAddress, typeId.nativeTypeId, entity);

            if (componentPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get component of type " + typeof(T).Name + " for entity " + entity);
            }

            // marshal IntPtr to struct ref
            unsafe
            {
                return ref System.Runtime.CompilerServices.Unsafe.AsRef<T>(componentPtr.ToPointer());
            }
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

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponentInterface")]
        private static extern IntPtr EntityManager_GetComponentInterface(IntPtr entityManagerPtr, TypeID typeID);

        // Components
        // TransformComponent
        [DllImport("hyperion", EntryPoint = "TransformComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID TransformComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "TransformComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID TransformComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // MeshComponent
        [DllImport("hyperion", EntryPoint = "MeshComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID MeshComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "MeshComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID MeshComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // BoundingBoxComponent
        [DllImport("hyperion", EntryPoint = "BoundingBoxComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID BoundingBoxComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "BoundingBoxComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID BoundingBoxComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // VisibilityStateComponent
        [DllImport("hyperion", EntryPoint = "VisibilityStateComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID VisibilityStateComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "VisibilityStateComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID VisibilityStateComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // LightComponent
        [DllImport("hyperion", EntryPoint = "LightComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID LightComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "LightComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID LightComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // ShadowMapComponent
        [DllImport("hyperion", EntryPoint = "ShadowMapComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID ShadowMapComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "ShadowMapComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID ShadowMapComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // UIComponent
        [DllImport("hyperion", EntryPoint = "UIComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID UIComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "UIComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID UIComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);

        // NodeLinkComponent
        [DllImport("hyperion", EntryPoint = "NodeLinkComponent_GetNativeTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID NodeLinkComponent_GetNativeTypeID();

        [DllImport("hyperion", EntryPoint = "NodeLinkComponent_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID NodeLinkComponent_AddComponent(IntPtr entityManagerPtr, Entity entity, IntPtr ptr);
    }
}
