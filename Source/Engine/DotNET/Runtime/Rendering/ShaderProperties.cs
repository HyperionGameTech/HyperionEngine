using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum ShaderPropertyOptionKind : byte
    {
        /// <summary>A PERMUTE(NAME) - the property is either present or absent.</summary>
        Toggle = 0,

        /// <summary>One member of a PERMUTE(NAME, ...) value group - at most one of the group may be present.</summary>
        Value = 1
    }

    /// <summary>
    /// A shader property that one bit of a <see cref="ShaderPropertySet"/> stands for.
    /// </summary>
    public sealed class ShaderPropertyOption
    {
        public ShaderPropertyOption(Name name, string valueString, uint propertyId, ShaderPropertyOptionKind kind)
        {
            Name = name;
            ValueString = valueString;
            PropertyId = propertyId;
            Kind = kind;
        }

        public Name Name { get; }

        /// <summary>The value this option selects, or an empty string for a toggle.</summary>
        public string ValueString { get; }

        public uint PropertyId { get; }

        public ShaderPropertyOptionKind Kind { get; }

        /// <summary>
        /// False when the property was interned past the end of the bitset, in which case no
        /// ShaderPropertySet can reference it.
        /// </summary>
        public bool IsStorable => PropertyId < ShaderPropertySet.MaxProperties;

        public override string ToString()
        {
            return ValueString.Length != 0 ? $"{Name}={ValueString}" : Name.ToString();
        }
    }

    public static class ShaderProperties
    {
        /// <summary>
        /// The properties a variant of <paramref name="shaderName"/> can be built with, one entry
        /// per selectable value. Empty if the shader has no compiled bundle yet.
        /// Must be called on the sim thread - it may load the bundle asset.
        /// </summary>
        public static unsafe IReadOnlyList<ShaderPropertyOption> GetShaderOptions(Name shaderName)
        {
            uint count = ShaderProperties_GetShaderOptions(ref shaderName, IntPtr.Zero, 0);

            if (count == 0)
            {
                return Array.Empty<ShaderPropertyOption>();
            }

            Debug.Assert(sizeof(ShaderPropertyOptionData) == 80);
            IntPtr buffer = Marshal.AllocHGlobal(sizeof(ShaderPropertyOptionData) * (int)count);

            try
            {
                count = ShaderProperties_GetShaderOptions(ref shaderName, buffer, count);

                var options = new List<ShaderPropertyOption>((int)count);
                ShaderPropertyOptionData* items = (ShaderPropertyOptionData*)buffer;

                for (uint i = 0; i < count; i++)
                {
                    options.Add(ToOption(&items[i]));
                }

                return options;
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        /// <summary>
        /// Describes an interned property by id, for bits that aren't among a shader's own options.
        /// Returns null if nothing has been interned under that id.
        /// </summary>
        public static unsafe ShaderPropertyOption? GetPropertyById(uint propertyId)
        {
            ShaderPropertyOptionData data;

            if (!ShaderProperties_GetPropertyById(propertyId, &data))
            {
                return null;
            }

            return ToOption(&data);
        }

        private static unsafe ShaderPropertyOption ToOption(ShaderPropertyOptionData* data)
        {
            string valueString = Marshal.PtrToStringAnsi((IntPtr)data->ValueString) ?? string.Empty;

            return new ShaderPropertyOption(data->Name, valueString, data->PropertyId, (ShaderPropertyOptionKind)data->Kind);
        }

        // Mirrors ShaderPropertyOptionBinding in ShaderPropertyBindings.cpp.
        [StructLayout(LayoutKind.Sequential)]
        private unsafe struct ShaderPropertyOptionData
        {
            public Name Name;
            public fixed byte ValueString[64];
            public uint PropertyId;
            public byte Kind;
        }

        [DllImport("hyperion", EntryPoint = "ShaderProperties_GetShaderOptions")]
        private static extern uint ShaderProperties_GetShaderOptions([In] ref Name shaderName, IntPtr pOutOptions, uint maxCount);

        [DllImport("hyperion", EntryPoint = "ShaderProperties_GetPropertyById")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern unsafe bool ShaderProperties_GetPropertyById(uint propertyId, ShaderPropertyOptionData* pOutOption);
    }
}
