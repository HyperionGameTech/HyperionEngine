using System;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;

namespace Hyperion
{
    public class StructHelpers
    {
        public static bool IsStruct(Type type, [NotNullWhen(true)] out Class? outClass)
        {
            outClass = null;

            ClassBinding? classBindingAttribute = ClassBinding.ForType(type);

            if (classBindingAttribute != null)
            {
                Class cls = classBindingAttribute.GetClass(type);

                if (cls.IsValid && cls.IsStructType)
                {
                    outClass = cls;

                    return true;
                }
            }

            return false;
        }
    }

    internal static class ManagedLayout
    {
        public static bool IsUnmanagedStruct(Type type)
        {
            if (!type.IsValueType || type.IsEnum || type.ContainsGenericParameters)
            {
                return false;
            }

            return HypTypeRegistry.GetStructLayout(type) != null;
        }

        public static int SizeOf(Type type)
        {
            return GetLayout(type).Size;
        }

        /// <summary>
        /// Copies a boxed unmanaged struct to <paramref name="destination"/> - must have room for `SizeOf(value.GetType())` bytes
        /// </summary>
        public static void Write(object value, IntPtr destination)
        {
            GetLayout(value.GetType()).Write(value, destination);
        }

        /// <summary>
        /// Creates a function that reads an instance of <paramref name="type"/> from native memory into a box
        /// </summary>
        public static Func<IntPtr, object> CreateReader(Type type)
        {
            return GetLayout(type).Read;
        }

        public static HypStructLayout GetLayout(Type type)
        {
            return HypTypeRegistry.GetStructLayout(type) ?? throw MissingLayout(type);
        }

        internal static NotSupportedException MissingLayout(Type type)
        {
            return new NotSupportedException("No generated layout for " + type.FullName + "; build its assembly with Hyperion.NET.Generators");
        }
    }

    // Layout matches ManagedDynamicStructField in StructBindings.cpp
    [StructLayout(LayoutKind.Sequential)]
    internal struct DynamicStructField
    {
        public IntPtr Name;
        public uint Offset;
        public uint Size;
        public IntPtr TypeInfo;
    }

    public class DynamicStruct : IDisposable
    {
        private static readonly Dictionary<Type, DynamicStruct> cache = new();
        private static readonly Lock cacheLock = new();
        private static readonly ConcurrentDictionary<TypeId, DynamicStruct> typeIdCache = new();
        private static readonly Lock typeIdCacheLock = new();


        private Class cls;
        private Type type;
        private bool ownsClass;
        private Func<IntPtr, object> reader;

        // Must be an unmanaged struct with no reference-type fields
        internal DynamicStruct(Type type)
        {
            this.type = type;
            this.reader = ManagedLayout.CreateReader(type);

            TypeId typeId = TypeId.ForType(type);

            lock (typeIdCacheLock)
            {
                if (typeIdCache.TryGetValue(typeId, out DynamicStruct? existingDynamicStruct))
                {
                    // A reloaded script assembly brings a new Type with the same name; it can share the native Struct only if the layout is unchanged
                    if (existingDynamicStruct.type.FullName != type.FullName)
                    {
                        throw new Exception("TypeId for " + type.FullName + " collides with " + existingDynamicStruct.type.FullName);
                    }

                    if (GetLayoutSignature(existingDynamicStruct.type) == GetLayoutSignature(type))
                    {
                        cls = existingDynamicStruct.cls;
                        ownsClass = false;

                        lock (cacheLock)
                        {
                            cache[type] = this;
                        }

                        return;
                    }

                    // A new native Struct with the same TypeId replaces the old one.
                    // live components move over to it per-field, by name.
                    Logger.Log(LogLevel.Info, "Layout of {0} changed. Components will be migrated", type.FullName);
                }
            }

            Logger.Log(LogLevel.Verbose, "Creating dynamic Struct for type: " + type.Name);

            IntPtr defaultValuePtr = CreateDefaultValue(type);
            DynamicStructField[] fields = GetLayoutFields(type, ManagedLayout.GetLayout(type));
            IntPtr classPtr;

            try
            {
                classPtr = Struct_CreateDynamicStruct(
                    ref typeId,
                    type.Name,
                    (uint)ManagedLayout.SizeOf(type),
                    defaultValuePtr,
                    fields,
                    (uint)fields.Length);
            }
            finally
            {
                if (defaultValuePtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(defaultValuePtr);
                }

                foreach (DynamicStructField field in fields)
                {
                    Marshal.FreeHGlobal(field.Name);
                }
            }

            if (classPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to create dynamic Struct");
            }

            cls = new Class(classPtr);
            ownsClass = true;

            // only cached once the native Struct exists, so a failed creation isn't reused by later loads
            lock (typeIdCacheLock)
            {
                typeIdCache[typeId] = this;
            }

            lock (cacheLock)
            {
                cache[type] = this;
            }
        }

        ~DynamicStruct()
        {
            if (ownsClass)
            {
                Struct_DestroyDynamicStruct(cls.Address);
            }
        }

        public void Dispose()
        {
            if (ownsClass)
            {
                Struct_DestroyDynamicStruct(cls.Address);

                ownsClass = false;
            }

            GC.SuppressFinalize(this);
        }

        public Class Class
        {
            get
            {
                return cls;
            }
        }

        public Type Type
        {
            get
            {
                return type;
            }
        }

        public object? MarshalFromBoxed(ref BoxedValueInternal buffer)
        {
            TypeId typeId = buffer.TypeId;
            Assert.Throw(typeId == cls.TypeId, "TypeId mismatch: " + typeId + " != " + cls.TypeId);

            IntPtr boxedPtr = buffer.Pointer;

            if (boxedPtr == IntPtr.Zero)
            {
                return null;
            }

            return reader(boxedPtr);
        }

        public static DynamicStruct GetOrCreate<T>()
        {
            return GetOrCreate(typeof(T));
        }

        public static DynamicStruct GetOrCreate(Type type)
        {
            lock (cacheLock)
            {
                if (!cache.TryGetValue(type, out DynamicStruct? dynamicStruct))
                {
                    dynamicStruct = new DynamicStruct(type);
                }

                return dynamicStruct;
            }
        }

        private static IntPtr CreateDefaultValue(Type type)
        {
            object? defaultInstance;

            try
            {
                defaultInstance = ManagedLayout.GetLayout(type).CreateDefault();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, "Could not create a default {0}; instances will be zero-initialized: {1}", type.Name, ex.Message);

                return IntPtr.Zero;
            }

            if (defaultInstance == null)
            {
                return IntPtr.Zero;
            }

            IntPtr defaultValuePtr = Marshal.AllocHGlobal(ManagedLayout.SizeOf(type));

            try
            {
                ManagedLayout.Write(defaultInstance, defaultValuePtr);
            }
            catch (Exception ex)
            {
                Marshal.FreeHGlobal(defaultValuePtr);

                Logger.Log(LogLevel.Warning, "Could not marshal a default {0}; instances will be zero-initialized: {1}", type.Name, ex.Message);

                return IntPtr.Zero;
            }

            return defaultValuePtr;
        }

        private static DynamicStructField[] GetLayoutFields(Type type, HypStructLayout layout)
        {
            List<DynamicStructField> fields = new();

            foreach (HypFieldLayout field in layout.Fields)
            {
                if (!field.IsPublic || field.IsNotSerialized)
                {
                    continue;
                }

                TypeInfo nativeTypeInfo = GetNativeTypeInfoForValue(field.DefaultValue);

                if (nativeTypeInfo.IsNull)
                {
                    Logger.Log(LogLevel.Warning, "Field {0}.{1} of type {2} has no native equivalent; it won't be saved or shown in the editor", type.Name, field.Name, field.FieldType.Name);

                    continue;
                }

                fields.Add(new DynamicStructField
                {
                    Name = Marshal.StringToHGlobalAnsi(field.Name),
                    Offset = (uint)field.Offset,
                    Size = (uint)field.Size,
                    TypeInfo = nativeTypeInfo.Address
                });
            }

            return fields.ToArray();
        }

        private static TypeInfo GetNativeTypeInfoForValue(object? value)
        {
            try
            {
                using BoxedValue boxed = new(value);

                return boxed.TypeInfo;
            }
            catch (Exception)
            {
                return new TypeInfo();
            }
        }

        private static string GetLayoutSignature(Type type)
        {
            HypStructLayout layout = ManagedLayout.GetLayout(type);

            return layout.Size + ":" + string.Join(";", layout.Fields.Select(field => field.FieldType.FullName + " " + field.Name + "@" + field.Offset));
        }

        public static bool TryGet(Type type, [NotNullWhen(true)] out DynamicStruct? dynamicStruct)
        {
            lock (cacheLock)
            {
                return cache.TryGetValue(type, out dynamicStruct);
            }
        }

        public static bool TryGet(TypeId typeId, [NotNullWhen(true)] out DynamicStruct? dynamicStruct)
        {
            dynamicStruct = null;
            if (typeIdCache.TryGetValue(typeId, out dynamicStruct))
            {
                return true;
            }

            return false;
        }

        [DllImport("hyperion", EntryPoint = "Struct_CreateDynamicStruct")]
        private static extern IntPtr Struct_CreateDynamicStruct(
            [In] ref TypeId typeId,
            [MarshalAs(UnmanagedType.LPStr)] string typeName,
            uint size,
            IntPtr defaultValue,
            [In] DynamicStructField[] fields,
            uint numFields);

        [DllImport("hyperion", EntryPoint = "Struct_DestroyDynamicStruct")]
        private static extern void Struct_DestroyDynamicStruct([In] IntPtr classPtr);
    }
}
