using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [Flags]
    public enum ComponentFlags : uint
    {
        None = 0x0,
        Serialize = 0x2,
        Replicated = 0x4,
        ShowInEditor = 0x8
    }

    /// <summary>
    /// Registers the struct as an ECS component type when its assembly is loaded (and again, harmlessly, on hot reload).
    /// </summary>
    [AttributeUsage(AttributeTargets.Struct, Inherited = false)]
    public class Component : Attribute
    {
        public ComponentFlags Flags { get; set; } = ComponentFlags.Serialize | ComponentFlags.ShowInEditor;

        // Called by NativeInterop through the attribute instance, so registration runs against the same Hyperion.NET.Shared the script uses
        public Class Register(Type type)
        {
            return ComponentRegistry.RegisterComponent(type, Flags);
        }
    }

    public static class ComponentRegistry
    {
        public static Class RegisterComponent<T>(ComponentFlags flags = ComponentFlags.Serialize | ComponentFlags.ShowInEditor) where T : unmanaged
        {
            return RegisterComponent(typeof(T), flags);
        }

        public static Class RegisterComponent(Type type, ComponentFlags flags = ComponentFlags.Serialize | ComponentFlags.ShowInEditor)
        {
            if (!IsBlittable(type))
            {
                throw new ArgumentException("Component type " + type.FullName + " must be a struct with no references, bool or char fields");
            }

            DynamicStruct dynamicStruct = DynamicStruct.GetOrCreate(type);

            if (!ComponentInterfaceRegistry_RegisterRuntimeComponent(dynamicStruct.Class.Address, (uint)flags))
            {
                throw new Exception("Failed to register component type " + type.FullName);
            }

            return dynamicStruct.Class;
        }

        public static bool UnregisterComponent<T>() where T : unmanaged
        {
            if (!DynamicStruct.TryGet(typeof(T), out DynamicStruct? dynamicStruct))
            {
                return false;
            }

            return ComponentInterfaceRegistry_UnregisterRuntimeComponent(dynamicStruct.Class.TypeId);
        }

        private static bool IsBlittable(Type type)
        {
            if (!type.IsValueType || type.IsEnum || type.ContainsGenericParameters)
            {
                return false;
            }

            MethodInfo containsReferencesMethod = typeof(RuntimeHelpers).GetMethod(nameof(RuntimeHelpers.IsReferenceOrContainsReferences))!.MakeGenericMethod(type);

            if ((bool)containsReferencesMethod.Invoke(null, null)!)
            {
                return false;
            }

            // The native side sizes the struct with Marshal.SizeOf; bool/char marshal to a different size than they occupy
            MethodInfo sizeOfMethod = typeof(Unsafe).GetMethod(nameof(Unsafe.SizeOf))!.MakeGenericMethod(type);

            return Marshal.SizeOf(type) == (int)sizeOfMethod.Invoke(null, null)!;
        }

        [DllImport("hyperion", EntryPoint = "ComponentInterfaceRegistry_RegisterRuntimeComponent")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ComponentInterfaceRegistry_RegisterRuntimeComponent(IntPtr classPtr, uint flags);

        [DllImport("hyperion", EntryPoint = "ComponentInterfaceRegistry_UnregisterRuntimeComponent")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ComponentInterfaceRegistry_UnregisterRuntimeComponent(TypeId componentTypeId);
    }
}
