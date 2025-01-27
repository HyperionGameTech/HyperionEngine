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

    public class DynamicHypStruct
    {
        private static readonly Dictionary<Type, DynamicHypStruct> cache = new Dictionary<Type, DynamicHypStruct>();
        private static readonly ConcurrentDictionary<TypeID, DynamicHypStruct> typeIdCache = new ConcurrentDictionary<TypeID, DynamicHypStruct>();
        private static readonly object cacheLock = new object();

        private HypClass hypClass;
        private Type type;
        private GCHandle destructFunctionHandle;

        // Must be a blittable type
        internal DynamicHypStruct(Type type)
        {
            this.type = type;

            TypeID typeId = TypeID.ForType(type);
            typeIdCache[typeId] = this;

            DestructDynamicHypStructDelegate destructFunction = GetDestructFunction(type);
            destructFunctionHandle = GCHandle.Alloc(destructFunction);

            IntPtr hypClassPtr = HypStruct_CreateDynamicHypStruct(ref typeId, type.Name, (uint)Marshal.SizeOf(type), Marshal.GetFunctionPointerForDelegate(destructFunction));

            if (hypClassPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to create dynamic HypStruct");
            }

            hypClass = new HypClass(hypClassPtr);
        }

        ~DynamicHypStruct()
        {
            HypStruct_DestroyDynamicHypStruct(hypClass.Address);

            destructFunctionHandle.Free();
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
            TypeID typeId = buffer.TypeID;
            Assert.Throw(typeId == hypClass.TypeID, "TypeID mismatch: " + typeId + " != " + hypClass.TypeID);

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
                if (!cache.TryGetValue(type, out DynamicHypStruct dynamicHypStruct))
                {
                    dynamicHypStruct = new DynamicHypStruct(type);

                    cache[type] = dynamicHypStruct;
                }

                return dynamicHypStruct;
            }
        }

        public static bool TryGet(TypeID typeId, out DynamicHypStruct? dynamicHypStruct)
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
        private static extern IntPtr HypStruct_CreateDynamicHypStruct([In] ref TypeID typeId, [MarshalAs(UnmanagedType.LPStr)] string typeName, uint size, IntPtr destructFunction);

        [DllImport("hyperion", EntryPoint = "HypStruct_DestroyDynamicHypStruct")]
        private static extern void HypStruct_DestroyDynamicHypStruct([In] IntPtr hypClassPtr);
    }
}