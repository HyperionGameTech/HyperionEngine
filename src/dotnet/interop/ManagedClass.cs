using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    public delegate ObjectReference NewObjectDelegate(bool keepAlive, IntPtr hypClassPtr, IntPtr nativeAddress, IntPtr contextPtr, IntPtr callbackPtr);
    public delegate void FreeObjectDelegate(ObjectReference obj);
    public delegate ObjectReference MarshalObjectDelegate(IntPtr ptr, uint size);

    internal struct StoredDelegate
    {
        public Guid AssemblyGuid { get; set; }
        public Guid Guid { get; set; }
        public GCHandle GCHandle { get; set; }
    }

    internal class DelegateCache : IBasicCache
    {
        private static readonly DelegateCache instance = new DelegateCache();

        public static DelegateCache Instance
        {
            get
            {
                return instance;
            }
        }

        private object lockObject = new object();
        private Dictionary<Guid, StoredDelegate> cache = new Dictionary<Guid, StoredDelegate>();

        public void Add(Guid assemblyGuid, Guid guid, GCHandle gcHandle)
        {
            lock (lockObject)
            {
                if (cache.ContainsKey(guid))
                    throw new Exception("Delegate already exists in cache");

                cache.Add(guid, new StoredDelegate
                {
                    AssemblyGuid = assemblyGuid,
                    Guid = guid,
                    GCHandle = gcHandle
                });
            }
        }

        public bool Remove(Guid guid)
        {
            lock (lockObject)
            {
                if (!cache.ContainsKey(guid))
                    return false;

                cache[guid].GCHandle.Free();
                cache.Remove(guid);

                return true;
            }

            return true;
        }

        public int RemoveForAssembly(Guid assemblyGuid)
        {
            lock (lockObject)
            {
                List<Guid> keysToRemove = new List<Guid>();

                foreach (KeyValuePair<Guid, StoredDelegate> entry in cache)
                {
                    if (entry.Value.AssemblyGuid == assemblyGuid)
                    {
                        entry.Value.GCHandle.Free();

                        keysToRemove.Add(entry.Key);
                    }
                }

                foreach (Guid key in keysToRemove)
                {
                    cache.Remove(key);
                }

                return keysToRemove.Count;
            }
        }
    }

    [Flags]
    public enum ManagedClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2,
        EnumType = 0x4,
        Abstract = 0x8
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedClass
    {
        internal int typeHash;
        internal IntPtr classObjectPtr;
        internal Guid assemblyGuid;
        internal Guid newObjectGuid;
        internal Guid freeObjectGuid;
        internal Guid marshalObjectGuid;
        internal ManagedClassFlags flags;

        public void SetAttributes(ref ManagedAttributeHolder managedAttributeHolder)
        {
            unsafe
            {
                fixed (ManagedAttributeHolder* managedAttributeHolderPtr = &managedAttributeHolder)
                {
                    ManagedClass_SetAttributes(ref this, (IntPtr)managedAttributeHolderPtr);
                }
            }
        }

        public void AddMethod(string methodName, Guid guid, IntPtr functionPointer, ref ManagedAttributeHolder managedAttributeHolder)
        {
            IntPtr methodNamePtr = Marshal.StringToHGlobalAnsi(methodName);
            
            unsafe
            {
                fixed (ManagedAttributeHolder* managedAttributeHolderPtr = &managedAttributeHolder)
                {
                    ManagedClass_AddMethod(ref this, methodNamePtr, guid, functionPointer, (IntPtr)managedAttributeHolderPtr);
                }
            }

            Marshal.FreeHGlobal(methodNamePtr);
        }

        public void AddProperty(string propertyName, Guid guid, ref ManagedAttributeHolder managedAttributeHolder)
        {
            IntPtr propertyNamePtr = Marshal.StringToHGlobalAnsi(propertyName);
            
            unsafe
            {
                fixed (ManagedAttributeHolder* managedAttributeHolderPtr = &managedAttributeHolder)
                {
                    ManagedClass_AddProperty(ref this, propertyNamePtr, guid, (IntPtr)managedAttributeHolderPtr);
                }
            }

            Marshal.FreeHGlobal(propertyNamePtr);
        }

        /// <summary>
        /// Pointer to the C++ dotnet::Class object
        /// </summary>
        public IntPtr ClassObjectPtr
        {
            get
            {
                return classObjectPtr;
            }
        }

        public void SetNewObjectFunction(Guid assemblyGuid, NewObjectDelegate value)
        {
            if (newObjectGuid != Guid.Empty)
                DelegateCache.Instance.Remove(newObjectGuid);

            newObjectGuid = Guid.NewGuid();

            GCHandle handle = GCHandle.Alloc(value);
            DelegateCache.Instance.Add(assemblyGuid, newObjectGuid, handle);

            ManagedClass_SetNewObjectFunction(ref this, Marshal.GetFunctionPointerForDelegate(value));
        }

        public void SetMarshalObjectFunction(Guid assemblyGuid, MarshalObjectDelegate value)
        {
            if (marshalObjectGuid != Guid.Empty)
                DelegateCache.Instance.Remove(marshalObjectGuid);

            marshalObjectGuid = Guid.NewGuid();

            GCHandle handle = GCHandle.Alloc(value);
            DelegateCache.Instance.Add(assemblyGuid, marshalObjectGuid, handle);

            ManagedClass_SetMarshalObjectFunction(ref this, Marshal.GetFunctionPointerForDelegate(value));
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetAttributes")]
        private static extern void ManagedClass_SetAttributes([In] ref ManagedClass managedClass, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern void ManagedClass_AddMethod([In] ref ManagedClass managedClass, IntPtr methodNamePtr, Guid guid, IntPtr functionPointer, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_AddProperty")]
        private static extern void ManagedClass_AddProperty([In] ref ManagedClass managedClass, IntPtr propertyNamePtr, Guid guid, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern void ManagedClass_SetNewObjectFunction([In] ref ManagedClass managedClass, IntPtr newObjectFunctionPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetMarshalObjectFunction")]
        private static extern void ManagedClass_SetMarshalObjectFunction([In] ref ManagedClass managedClass, IntPtr marshalObjectFunctionPtr);
    }
}