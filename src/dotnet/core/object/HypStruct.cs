using System;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;

namespace Hyperion
{
    public class HypStructHelpers
    {
        public static bool IsHypStruct(Type type, out HypClass? outHypClass)
        {
            outHypClass = null;

            HypClassBinding hypClassBindingAttribute = HypClassBinding.ForType(type);

            if (hypClassBindingAttribute != null)
            {
                HypClass hypClass = hypClassBindingAttribute.GetClass(type);

                if (hypClass.IsValid && hypClass.IsStructType)
                {
                    outHypClass = hypClass;

                    return true;
                }
            }

            return false;
        }
    }

    public delegate void DestructDynamicHypStructDelegate(IntPtr ptr);

    public class DynamicHypStruct : IDisposable
    {
        private static readonly Dictionary<Type, DynamicHypStruct> cache = new Dictionary<Type, DynamicHypStruct>();
        private static readonly object cacheLock = new object();
        private static readonly ConcurrentDictionary<TypeId, DynamicHypStruct> typeIdCache = new ConcurrentDictionary<TypeId, DynamicHypStruct>();
        private static readonly object typeIdCacheLock = new object();

        private HypClass hypClass;
        private Type type;
        private GCHandle? destructFunctionHandle;
        private bool ownsHypClass;

        // Must be a blittable type
        internal DynamicHypStruct(Type type)
        {
            this.type = type;

            TypeId typeId = TypeId.ForType(type);

            lock (typeIdCacheLock)
            {
                if (typeIdCache.ContainsKey(typeId))
                {
                    DynamicHypStruct existingDynamicHypStruct = typeIdCache[typeId];
                    Assert.Throw(existingDynamicHypStruct.type == type, "TypeId already exists for a different type: " + type.Name + " (hashcode: " + type.GetHashCode() + ") != " + existingDynamicHypStruct.type.Name + " (hashcode: " + existingDynamicHypStruct.type.GetHashCode() + ")");

                    hypClass = existingDynamicHypStruct.hypClass;
                    ownsHypClass = false;

                    return;
                }

                // Add this to cache
                typeIdCache[typeId] = this;
            }

            DestructDynamicHypStructDelegate destructFunction = GetDestructFunction(type);
            destructFunctionHandle = GCHandle.Alloc(destructFunction);

            Logger.Log(LogType.Debug, "Creating dynamic HypStruct for type: " + type.Name);

            IntPtr hypClassPtr = HypStruct_CreateDynamicHypStruct(ref typeId, type.Name, (uint)Marshal.SizeOf(type), Marshal.GetFunctionPointerForDelegate(destructFunction));

            if (hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to create dynamic HypStruct");
            }

            hypClass = new HypClass(hypClassPtr);
            ownsHypClass = true;

            lock (cacheLock)
            {
                cache[type] = this;
            }
        }

        ~DynamicHypStruct()
        {
            if (ownsHypClass)
                HypStruct_DestroyDynamicHypStruct(hypClass.Address);

            destructFunctionHandle?.Free();
        }

        public void Dispose()
        {
            if (ownsHypClass)
            {
                HypStruct_DestroyDynamicHypStruct(hypClass.Address);

                ownsHypClass = false;
            }

            destructFunctionHandle?.Free();
            destructFunctionHandle = null;

            GC.SuppressFinalize(this);
        }

        public HypClass HypClass
        {
            get
            {
                return hypClass;
            }
        }

        public Type Type
        {
            get
            {
                return type;
            }
        }

        public object? MarshalFromHypData(ref HypDataBuffer buffer)
        {
            TypeId typeId = buffer.TypeId;
            Assert.Throw(typeId == hypClass.TypeId, "TypeId mismatch: " + typeId + " != " + hypClass.TypeId);

            IntPtr hypDataPtr = buffer.Pointer;

            if (hypDataPtr == IntPtr.Zero)
            {
                return null;
            }

            return Marshal.PtrToStructure(hypDataPtr, type);
        }

        public static DynamicHypStruct GetOrCreate<T>()
        {
            return GetOrCreate(typeof(T));
        }

        public static DynamicHypStruct GetOrCreate(Type type)
        {
            lock (cacheLock)
            {
                DynamicHypStruct? dynamicHypStruct;

                if (!cache.TryGetValue(type, out dynamicHypStruct))
                {
                    dynamicHypStruct = new DynamicHypStruct(type);
                }

                return dynamicHypStruct;
            }
        }

        public static bool TryGet(TypeId typeId, out DynamicHypStruct? dynamicHypStruct)
        {
            dynamicHypStruct = null;

            if (typeIdCache.TryGetValue(typeId, out dynamicHypStruct))
            {
                return true;
            }

            return false;
        }

        private static unsafe DestructDynamicHypStructDelegate GetDestructFunction(Type type)
        {
            return (ptr) =>
            {
                // IntPtr to an instance of the type

                // @TODO

                throw new NotImplementedException();

                Marshal.FreeHGlobal(ptr);
            };
        }

        [DllImport("hyperion", EntryPoint = "HypStruct_CreateDynamicHypStruct")]
        private static extern IntPtr HypStruct_CreateDynamicHypStruct([In] ref TypeId typeId, [MarshalAs(UnmanagedType.LPStr)] string typeName, uint size, IntPtr destructFunction);

        [DllImport("hyperion", EntryPoint = "HypStruct_DestroyDynamicHypStruct")]
        private static extern void HypStruct_DestroyDynamicHypStruct([In] IntPtr hypClassPtr);
    }
}