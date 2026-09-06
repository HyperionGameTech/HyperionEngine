using System;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion.Editor
{
    /// <summary>
    /// Editor-facing API for per-layer property overrides on entities
    /// (the <c>$LayerOverrides</c> HMF schema section).
    /// Non-value operations route through <see cref="EditorSubsystem"/> reflection methods;
    /// BoxedValue-carrying operations use dedicated P/Invoke exports (BoxedValue is not
    /// supported by the code generator's parameter/return mapping).
    /// All methods must be called on the sim thread.
    /// </summary>
    public static class EntityLayerOverrides
    {
        private static EditorSubsystem? Subsystem => EngineManager.EditorGame?.EditorSubsystem;

        public static Name[] GetSetLayerNames(Entity entity)
        {
            // Note: the generated wrapper for Array<Name> returns a non-generic Array of boxed Names.
            Array sets = Subsystem?.GetEntityLayerOverrideSets(entity);

            if (sets == null)
            {
                return Array.Empty<Name>();
            }

            Name[] result = new Name[sets.Length];

            for (int i = 0; i < sets.Length; i++)
            {
                result[i] = sets.GetValue(i) is Name name ? name : Name.Invalid;
            }

            return result;
        }

        public static bool HasSet(Entity entity, Name layerName)
        {
            return Subsystem?.EntityHasLayerOverrideSet(entity, layerName) ?? false;
        }

        public static void AddSet(Entity entity, Name layerName)
        {
            Subsystem?.EntityAddLayerOverrideSet(entity, layerName);
        }

        public static bool RemoveSet(Entity entity, Name layerName)
        {
            return Subsystem?.EntityRemoveLayerOverrideSet(entity, layerName) ?? false;
        }

        public static bool IsPropertyOverridden(Entity entity, Name layerName, Name propertyName)
        {
            return Subsystem?.IsEntityPropertyOverridden(entity, layerName, propertyName) ?? false;
        }

        public static bool RemoveValue(Entity entity, Name layerName, Name propertyName)
        {
            return Subsystem?.EntityRemoveLayerOverrideValue(entity, layerName, propertyName) ?? false;
        }

        public static Name GetAppliedLayer(Entity entity)
        {
            return Subsystem?.GetEntityAppliedOverrideLayer(entity) ?? Name.Invalid;
        }

        public static void Apply(Entity entity, Name layerName)
        {
            Subsystem?.EntityApplyLayerOverrides(entity, layerName);
        }

        public static void Revert(Entity entity)
        {
            Subsystem?.EntityRevertLayerOverrides(entity);
        }

        /// <summary>
        /// Global editor toggle: when enabled, editor edits apply to the active layer's override
        /// set only; when disabled, edits apply to base (keeping existing active-layer overrides
        /// in sync).
        /// </summary>
        public static void SetOverrideMode(bool enabled)
        {
            Subsystem?.SetLayerOverrideMode(enabled);
        }

        public static bool SetValue(Entity entity, Name layerName, Name propertyName, BoxedValue value)
        {
            return EntityLayerOverrides_SetValue(entity.NativeAddress, layerName.HashCode, propertyName.HashCode, ref value.Buffer);
        }

        /// <summary>
        /// Writes a property's base value (through its setter), keeping the applied layer's
        /// base snapshot in sync so a revert returns to the edited base.
        /// </summary>
        public static bool SetBaseValue(Entity entity, Name propertyName, BoxedValue value)
        {
            return EntityLayerOverrides_SetBaseValue(entity.NativeAddress, propertyName.HashCode, ref value.Buffer);
        }

        public static bool GetBaseValue(Entity entity, Name layerName, Name propertyName, out BoxedValue value)
        {
            BoxedValueInternal internalValue = new BoxedValueInternal();

            try
            {
                if (!EntityLayerOverrides_GetBaseValue(entity.NativeAddress, layerName.HashCode, propertyName.HashCode, ref internalValue))
                {
                    value = new BoxedValue(null);
                    return false;
                }

                value = BoxedValue.FromBuffer(internalValue);
                return true;
            }
            catch
            {
                internalValue.Dispose();
                throw;
            }
        }

        public static bool GetValue(Entity entity, Name layerName, Name propertyName, out BoxedValue value)
        {
            BoxedValueInternal internalValue = new BoxedValueInternal();

            try
            {
                if (!EntityLayerOverrides_GetValue(entity.NativeAddress, layerName.HashCode, propertyName.HashCode, ref internalValue))
                {
                    value = new BoxedValue(null);
                    return false;
                }

                value = BoxedValue.FromBuffer(internalValue);
                return true;
            }
            catch
            {
                internalValue.Dispose();
                throw;
            }
        }

        //-- P/Invoke: BoxedValue-carrying operations --

        [DllImport("hyperion", EntryPoint = "EntityLayerOverrides_SetValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityLayerOverrides_SetValue(IntPtr pEntity, ulong layerHash, ulong propertyHash, [In] ref BoxedValueInternal pValue);

        [DllImport("hyperion", EntryPoint = "EntityLayerOverrides_SetBaseValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityLayerOverrides_SetBaseValue(IntPtr pEntity, ulong propertyHash, [In] ref BoxedValueInternal pValue);

        [DllImport("hyperion", EntryPoint = "EntityLayerOverrides_GetBaseValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityLayerOverrides_GetBaseValue(IntPtr pEntity, ulong layerHash, ulong propertyHash, [In, Out] ref BoxedValueInternal pOutValue);

        [DllImport("hyperion", EntryPoint = "EntityLayerOverrides_GetValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntityLayerOverrides_GetValue(IntPtr pEntity, ulong layerHash, ulong propertyHash, [In, Out] ref BoxedValueInternal pOutValue);
    }
}
