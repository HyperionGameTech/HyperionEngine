using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    using ComponentID = uint;

    [HypClassBinding(Name="EntityManager")]
    public class EntityManager : HypObject
    {
        public EntityManager()
        {
        }

        public bool HasComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));
            
            return EntityManager_HasComponent(NativeAddress, componentHypClass.TypeID, entity);
        }

        public ComponentID AddComponent<T>(Entity entity, T component) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            if (componentHypClass.Size != Marshal.SizeOf(component))
            {
                throw new Exception("Component size mismatch: " + componentHypClass.Size + " != " + Marshal.SizeOf(component));
            }

            IntPtr componentPtr = Marshal.AllocHGlobal(Marshal.SizeOf(component));
            Marshal.StructureToPtr(component, componentPtr, false);

            ComponentID result = EntityManager_AddComponent(NativeAddress, entity, componentHypClass.TypeID, componentPtr);

            Marshal.FreeHGlobal(componentPtr);

            return result;
        }

        public ref T GetComponent<T>(IDBase entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            IntPtr componentPtr = EntityManager_GetComponent(NativeAddress, componentHypClass.TypeID, entity);

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

        [DllImport("hyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr entityManagerPtr, TypeID componentTypeId, Entity entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr entityManagerPtr, TypeID componentTypeId, IDBase entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddComponent")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern ComponentID EntityManager_AddComponent(IntPtr entityManagerPtr, Entity entity, TypeID componentTypeId, IntPtr componentPtr);
    }
}
