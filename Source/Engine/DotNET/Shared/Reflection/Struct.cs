using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;
using System.Diagnostics;

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

    public class DynamicStruct : IDisposable
    {
        private static readonly Dictionary<Type, DynamicStruct> cache = new Dictionary<Type, DynamicStruct>();
        private static readonly object cacheLock = new object();
        private static readonly ConcurrentDictionary<TypeId, DynamicStruct> typeIdCache = new ConcurrentDictionary<TypeId, DynamicStruct>();
        private static readonly object typeIdCacheLock = new object();

        private Class cls;
        private Type type;
        private bool ownsClass;

        // Must be a blittable type
        internal DynamicStruct(Type type)
        {
            this.type = type;

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

                    if (GetLayoutSignature(existingDynamicStruct.type) != GetLayoutSignature(type))
                    {
                        throw new Exception("Layout of " + type.FullName + " changed since it was first loaded; restart to apply the new layout");
                    }

                    cls = existingDynamicStruct.cls;
                    ownsClass = false;

                    lock (cacheLock)
                    {
                        cache[type] = this;
                    }

                    return;
                }

                // Add this to cache
                typeIdCache[typeId] = this;
            }

            Logger.Log(LogLevel.Verbose, "Creating dynamic Struct for type: " + type.Name);

            IntPtr defaultValuePtr = CreateDefaultValue(type);
            IntPtr classPtr;

            try
            {
                classPtr = Struct_CreateDynamicStruct(
                    ref typeId,
                    type.Name,
                    (uint)Marshal.SizeOf(type),
                    defaultValuePtr);
            }
            finally
            {
                if (defaultValuePtr != IntPtr.Zero)
                {
                    Marshal.DestroyStructure(defaultValuePtr, type);
                    Marshal.FreeHGlobal(defaultValuePtr);
                }
            }

            if (classPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to create dynamic Struct");
            }

            cls = new Class(classPtr);
            ownsClass = true;

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

            return Marshal.PtrToStructure(boxedPtr, type);
        }

        public static DynamicStruct GetOrCreate<T>()
        {
            return GetOrCreate(typeof(T));
        }

        public static DynamicStruct GetOrCreate(Type type)
        {
            lock (cacheLock)
            {
                DynamicStruct? dynamicStruct;

                if (!cache.TryGetValue(type, out dynamicStruct))
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
                defaultInstance = Activator.CreateInstance(type);
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

            IntPtr defaultValuePtr = Marshal.AllocHGlobal(Marshal.SizeOf(type));

            try
            {
                Marshal.StructureToPtr(defaultInstance, defaultValuePtr, false);
            }
            catch (Exception ex)
            {
                Marshal.FreeHGlobal(defaultValuePtr);

                Logger.Log(LogLevel.Warning, "Could not marshal a default {0}; instances will be zero-initialized: {1}", type.Name, ex.Message);

                return IntPtr.Zero;
            }

            return defaultValuePtr;
        }

        private static string GetLayoutSignature(Type type)
        {
            FieldInfo[] fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

            return Marshal.SizeOf(type) + ":" + string.Join(";", fields.Select(field => field.FieldType.FullName + " " + field.Name + "@" + Marshal.OffsetOf(type, field.Name)));
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
            IntPtr defaultValue);

        [DllImport("hyperion", EntryPoint = "Struct_DestroyDynamicStruct")]
        private static extern void Struct_DestroyDynamicStruct([In] IntPtr classPtr);
    }
}
