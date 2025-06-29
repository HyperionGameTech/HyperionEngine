using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    using ComponentID = uint;

    [HypClassBinding(Name = "EntityManager")]
    public class EntityManager : HypObject
    {
        public EntityManager()
        {
        }

        public Entity AddEntity()
        {
            return InvokeNativeMethod<Entity>(new Name("AddBasicEntity", weak: true));
        }

        public T AddEntity<T>() where T : Entity
        {
            HypClass hypClass = HypClass.GetClass<T>();

            unsafe
            {
                HypDataBuffer hypDataBuffer = new HypDataBuffer();

                try
                {
                    if (!EntityManager_AddTypedEntity(NativeAddress, hypClass.Address, &hypDataBuffer))
                    {
                        throw new Exception("Failed to add entity of type " + typeof(T).Name);
                    }

                    T entity = (T)hypDataBuffer.GetValue();

                    return entity;
                }
                finally
                {
                    hypDataBuffer.Dispose();
                }
            }
        }

        public bool HasComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));
            
            return EntityManager_HasComponent(NativeAddress, componentHypClass.TypeId, entity.NativeAddress);
        }

        public void AddComponent<T>(Entity entity, T component) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            if (componentHypClass.Size != Marshal.SizeOf(component))
            {
                throw new Exception("Component size mismatch: " + componentHypClass.Size + " != " + Marshal.SizeOf(component));
            }

            AddComponent<T>(entity, componentHypClass, ref component);
        }

        private unsafe void AddComponent<T>(Entity entity, HypClass componentHypClass, ref T component) where T : struct, IComponent
        {
            // fixed (void* componentPtr = &component)
            // {
            //     HypDataBuffer hypDataBuffer = new HypDataBuffer();
            //     hypDataBuffer.SetValue((IntPtr)componentPtr);

            //     EntityManager_AddComponent(NativeAddress, entity.NativeAddress, componentHypClass.TypeId, &hypDataBuffer);

            //     hypDataBuffer.Dispose();
            // }

            HypDataBuffer hypDataBuffer = new HypDataBuffer();
            hypDataBuffer.SetValue(component);

            EntityManager_AddComponent(NativeAddress, entity.NativeAddress, componentHypClass.TypeId, &hypDataBuffer);

            hypDataBuffer.Dispose();
        }

        public ref T GetComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            IntPtr componentPtr = EntityManager_GetComponent(NativeAddress, componentHypClass.TypeId, entity.NativeAddress);

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

        [DllImport("hyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr entityManagerPtr, TypeId componentTypeId, IntPtr entityAddress);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr entityManagerPtr, TypeId componentTypeId, IntPtr entityAddress);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddComponent")]
        private static unsafe extern void EntityManager_AddComponent(IntPtr entityManagerPtr, IntPtr entityAddress, TypeId componentTypeId, HypDataBuffer* componentHypDataPtr);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddTypedEntity")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static unsafe extern bool EntityManager_AddTypedEntity(IntPtr entityManagerPtr, IntPtr hypClassAddress, [Out] HypDataBuffer* outHypDataBufferPtr);
    }
}
