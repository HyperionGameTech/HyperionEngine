using System;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    public class StructHelpers
    {
        public static bool IsStruct(Type type, out Class? outClass)
        {
            outClass = null;

            ClassBinding classBindingAttribute = ClassBinding.ForType(type);

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

    public delegate IntPtr CopyDynamicStructDelegate(IntPtr ptr);
    public delegate void DestructDynamicStructDelegate(IntPtr ptr);

    public class DynamicStruct : IDisposable
    {
        private static readonly Dictionary<Type, DynamicStruct> cache = new Dictionary<Type, DynamicStruct>();
        private static readonly object cacheLock = new object();
        private static readonly ConcurrentDictionary<TypeId, DynamicStruct> typeIdCache = new ConcurrentDictionary<TypeId, DynamicStruct>();
        private static readonly object typeIdCacheLock = new object();

        private Class cls;
        private Type type;
        private GCHandle? copyFunctionHandle;
        private GCHandle? destructFunctionHandle;
        private bool ownsClass;

        // Must be a blittable type
        internal DynamicStruct(Type type)
        {
            this.type = type;

            TypeId typeId = TypeId.ForType(type);

            lock (typeIdCacheLock)
            {
                if (typeIdCache.ContainsKey(typeId))
                {
                    DynamicStruct existingDynamicStruct = typeIdCache[typeId];
                    Assert.Throw(existingDynamicStruct.type == type, "TypeId already exists for a different type: " + type.Name + " (hashcode: " + type.GetHashCode() + ") != " + existingDynamicStruct.type.Name + " (hashcode: " + existingDynamicStruct.type.GetHashCode() + ")");

                    cls = existingDynamicStruct.cls;
                    ownsClass = false;

                    return;
                }

                // Add this to cache
                typeIdCache[typeId] = this;
            }

            CopyDynamicStructDelegate copyFunction = GetCopyFunction(type);
            copyFunctionHandle = GCHandle.Alloc(copyFunction);

            DestructDynamicStructDelegate destructFunction = GetDestructFunction(type);
            destructFunctionHandle = GCHandle.Alloc(destructFunction);

            Logger.Log(LogLevel.Verbose, "Creating dynamic Struct for type: " + type.Name);

            IntPtr classPtr = Struct_CreateDynamicStruct(
                ref typeId,
                type.Name,
                (uint)Marshal.SizeOf(type),
                Marshal.GetFunctionPointerForDelegate(copyFunction),
                Marshal.GetFunctionPointerForDelegate(destructFunction));

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
                Struct_DestroyDynamicStruct(cls.Address);

            destructFunctionHandle?.Free();
        }

        public void Dispose()
        {
            if (ownsClass)
            {
                Struct_DestroyDynamicStruct(cls.Address);

                ownsClass = false;
            }

            destructFunctionHandle?.Free();
            destructFunctionHandle = null;

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

        public static bool TryGet(TypeId typeId, out DynamicStruct? dynamicStruct)
        {
            dynamicStruct = null;

            if (typeIdCache.TryGetValue(typeId, out dynamicStruct))
            {
                return true;
            }

            return false;
        }

        private static unsafe DestructDynamicStructDelegate GetDestructFunction(Type type)
        {
            return (ptr) =>
            {
                // IntPtr to an instance of the type

                // @TODO

                throw new NotImplementedException();

                Marshal.FreeHGlobal(ptr);
            };
        }

        public static unsafe CopyDynamicStructDelegate GetCopyFunction(Type type)
        {
            return (ptr) =>
            {
                IntPtr newPtr = Marshal.AllocHGlobal(Marshal.SizeOf(type));

                if (newPtr == IntPtr.Zero)
                {
                    throw new Exception("Failed to allocate memory for copy of dynamic Struct");
                }

                // Copy memory
                Buffer.MemoryCopy((void*)ptr, (void*)newPtr, Marshal.SizeOf(type), Marshal.SizeOf(type));

                return newPtr;
            };
        }

        [DllImport("hyperion", EntryPoint = "Struct_CreateDynamicStruct")]
        private static extern IntPtr Struct_CreateDynamicStruct(
            [In] ref TypeId typeId,
            [MarshalAs(UnmanagedType.LPStr)] string typeName,
            uint size,
            IntPtr copyFunction,
            IntPtr destructFunction);

        [DllImport("hyperion", EntryPoint = "Struct_DestroyDynamicStruct")]
        private static extern void Struct_DestroyDynamicStruct([In] IntPtr classPtr);
    }
}