using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    using ComponentId = uint;

    [ClassBinding(Name = "EntityTag")]
    public struct EntityTag
    {
        public static readonly EntityTag None = new EntityTag(0x0);

        public static readonly EntityTag MobStatic = new EntityTag(0x1);
        public static readonly EntityTag MobDynamic = new EntityTag(0x2);

        public static readonly EntityTag Light = new EntityTag(0x3);

        public static readonly EntityTag PrimaryCamera = new EntityTag(0x4);
        public static readonly EntityTag EditorCamera = new EntityTag(0x5);

        public static readonly EntityTag LightmapElement = new EntityTag(0x6);

        public static readonly EntityTag ReceivesUpdate = new EntityTag(0x7);

        public static readonly EntityTag UIVisible = new EntityTag(0x10);

        public static readonly EntityTag FocusedInEditor = new EntityTag(0x20);

        public static readonly EntityTag UpdateRenderProxy = new EntityTag(0x30);
        public static readonly EntityTag UpdateVisibility = new EntityTag(0x40);
        public static readonly EntityTag UpdateInstancedMeshData = new EntityTag(0x50);

        public static readonly EntityTag UpdatePhysicsShape = new EntityTag(0x100);
        public static readonly EntityTag UpdatePhysicsMaterial = new EntityTag(0x200);

        internal ulong Value;

        internal EntityTag(ulong value)
        {
            Value = value;
        }

        public override string ToString()
        {
            return $"EntityTag({Value})";
        }
    }

    [ClassBinding(Name = "EntityManager")]
    public class EntityManager : ObjectBase
    {
        public EntityManager()
        {
        }

        public Entity AddEntity()
        {
            return InvokeNativeMethod<Entity>("AddBasicEntity");
        }

        public T AddEntity<T>() where T : Entity
        {
            Class cls = Class.GetClass<T>();

            unsafe
            {
                BoxedValueInternal boxedInternal = new BoxedValueInternal();

                try
                {
                    if (!EntityManager_AddTypedEntity(NativeAddress, cls.Address, &boxedInternal))
                    {
                        throw new Exception("Failed to add entity of type " + typeof(T).Name);
                    }

                    T entity = (T)boxedInternal.GetValue();

                    return entity;
                }
                finally
                {
                    boxedInternal.Dispose();
                }
            }
        }

        public bool HasComponent<T>(Entity entity) where T : IComponent, allows ref struct
        {
            Class componentClass = Class.GetClass(typeof(T));

            return EntityManager_HasComponent(NativeAddress, componentClass.TypeId, entity.NativeAddress);
        }

        public void AddComponent<T>(Entity entity, ref T component) where T : IComponent, allows ref struct
        {
            Class componentClass = Class.GetClass(typeof(T));

            // if (componentClass.Size != Marshal.SizeOf<T>())
            // {
            //     throw new Exception("Component size mismatch: " + componentClass.Size + " != " + Marshal.SizeOf<T>());
            // }

            AddComponent<T>(entity, componentClass, ref component);
        }

        private unsafe void AddComponent<T>(Entity entity, Class componentClass, ref T component) where T : IComponent, allows ref struct
        {
            fixed (T* pComponent = &component)
            {
                EntityManager_AddComponent(NativeAddress, entity.NativeAddress, componentClass.TypeId, (IntPtr)pComponent);
            }
        }

        public unsafe void AddDefaultComponent(Entity entity, Class componentClass)
        {
            // Pass 0 (NULL) - will create new instance from managed code.
            EntityManager_AddComponent(NativeAddress, entity.NativeAddress, componentClass.TypeId, 0);
        }

        public bool RemoveComponent(Entity entity, TypeId componentTypeId)
        {
            return EntityManager_RemoveComponent(NativeAddress, componentTypeId, entity.NativeAddress);
        }

        public ref T GetComponent<T>(Entity entity) where T : IComponent, allows ref struct
        {
            Class componentClass = Class.GetClass(typeof(T));

            IntPtr componentPtr = EntityManager_GetComponent(NativeAddress, componentClass.TypeId, entity.NativeAddress);

            if (componentPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get component of type " + typeof(T).Name + " for entity " + entity.Id);
            }

            // marshal IntPtr to struct ref
            unsafe
            {
                return ref System.Runtime.CompilerServices.Unsafe.AsRef<T>(componentPtr.ToPointer());
            }
        }

        public IntPtr GetComponentPtr(Entity entity, TypeId componentTypeId)
        {
            return EntityManager_GetComponent(NativeAddress, componentTypeId, entity.NativeAddress);
        }

        public IEnumerable<TypeId> GetComponentTypeIds(Entity entity)
        {
            IntPtr pTypeIds = IntPtr.Zero;
            uint typeIdCount = EntityManager_GetComponentTypeIds(NativeAddress, entity.NativeAddress, pTypeIds);

            if (typeIdCount == 0)
            {
                yield break;
            }

            int typeIdSize = Marshal.SizeOf<TypeId>();
            pTypeIds = Marshal.AllocHGlobal(typeIdSize * (int)typeIdCount);

            try
            {
                EntityManager_GetComponentTypeIds(NativeAddress, entity.NativeAddress, pTypeIds);

                for (uint i = 0; i < typeIdCount; i++)
                {
                    IntPtr currentTypeIdPtr = IntPtr.Add(pTypeIds, (int)(i * typeIdSize));
                    TypeId typeId = Marshal.PtrToStructure<TypeId>(currentTypeIdPtr);
                    yield return typeId;
                }
            }
            finally
            {
                Marshal.FreeHGlobal(pTypeIds);
            }
        }

        // Iterating over components can be done by getting the component type IDs
        // and then using GetComponent<T>() for each type ID.
        // public IEnumerable<IComponent> GetComponents(Entity entity)
        // {
        //     foreach (TypeId typeId in GetComponentTypeIds(entity))
        //     {
        //         Class componentClass = Class.GetClass(typeId);

        //         if (componentClass == null)
        //         {
        //             continue;
        //         }

        //         // HAX! should refactor by making a non-generic GetComponent(Type componentType), but still needs to return a ref struct
        //         var getComponentMethod = typeof(EntityManager).GetMethod("GetComponent").MakeGenericMethod(componentClass.GetManagedType());
        //         var component = getComponentMethod.Invoke(this, new object[] { entity });

        //         yield return (IComponent)component;
        //     }
        // }

        [DllImport("hyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr pManager, TypeId componentTypeId, IntPtr pEntity);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr pManager, TypeId componentTypeId, IntPtr pEntity);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponentTypeIds")]
        private static extern uint EntityManager_GetComponentTypeIds(IntPtr pManager, IntPtr pEntity, [Out] IntPtr pOutTypeIds);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddComponent")]
        private static extern void EntityManager_AddComponent(IntPtr pManager, IntPtr pEntity, TypeId componentTypeId, IntPtr pComponent);

        [DllImport("hyperion", EntryPoint = "EntityManager_RemoveComponent")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityManager_RemoveComponent(IntPtr pManager, TypeId componentTypeId, IntPtr pEntity);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddTypedEntity")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static unsafe extern bool EntityManager_AddTypedEntity(IntPtr pManager, IntPtr pClass, [Out] BoxedValueInternal* pOutBoxed);

        public void AddTag(Entity entity, EntityTag tag)
        {
            EntityManager_AddTag(NativeAddress, entity.NativeAddress, tag.Value);
        }

        public bool RemoveTag(Entity entity, EntityTag tag)
        {
            return EntityManager_RemoveTag(NativeAddress, entity.NativeAddress, tag.Value);
        }

        public bool HasTag(Entity entity, EntityTag tag)
        {
            return EntityManager_HasTag(NativeAddress, entity.NativeAddress, tag.Value);
        }

        [DllImport("hyperion", EntryPoint = "EntityManager_AddTag")]
        private static extern void EntityManager_AddTag(IntPtr pManager, IntPtr pEntity, ulong tag);

        [DllImport("hyperion", EntryPoint = "EntityManager_RemoveTag")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityManager_RemoveTag(IntPtr pManager, IntPtr pEntity, ulong tag);

        [DllImport("hyperion", EntryPoint = "EntityManager_HasTag")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityManager_HasTag(IntPtr pManager, IntPtr pEntity, ulong tag);
    }
}
