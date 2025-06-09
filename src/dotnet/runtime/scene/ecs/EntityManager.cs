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

        public bool HasComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            IDBase entityId = entity.ID;
            
            return EntityManager_HasComponent(NativeAddress, componentHypClass.TypeID, ref entityId);
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
            fixed (void* ptr = &component)
            {
                IDBase entityId = entity.ID;

                EntityManager_AddComponent(NativeAddress, ref entityId, componentHypClass.TypeID, new IntPtr(ptr));
            }
        }

        public ref T GetComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            IDBase entityId = entity.ID;

            IntPtr componentPtr = EntityManager_GetComponent(NativeAddress, componentHypClass.TypeID, ref entityId);

            if (componentPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get component of type " + typeof(T).Name + " for entity " + entity.ID);
            }

            // marshal IntPtr to struct ref
            unsafe
            {
                return ref System.Runtime.CompilerServices.Unsafe.AsRef<T>(componentPtr.ToPointer());
            }
        }

        [DllImport("hyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr entityManagerPtr, TypeID componentTypeId, ref IDBase entityId);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr entityManagerPtr, TypeID componentTypeId, ref IDBase entityId);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddComponent")]
        private static extern void EntityManager_AddComponent(IntPtr entityManagerPtr, ref IDBase entityId, TypeID componentTypeId, IntPtr componentPtr);
    }
}
