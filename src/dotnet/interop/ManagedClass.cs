using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    public delegate ObjectReference NewObjectDelegate(bool keepAlive, IntPtr hypClassPtr, IntPtr nativeAddress, IntPtr contextPtr, IntPtr callbackPtr);
    public delegate void FreeObjectDelegate(ObjectReference obj);
    public delegate ObjectReference MarshalObjectDelegate(IntPtr ptr, uint size);

    internal class DelegateCache
    {
        private static readonly DelegateCache instance = new DelegateCache();

        public static DelegateCache Instance
        {
            get
            {
                return instance;
            }
        }

        private Dictionary<Guid, GCHandle> pinnedDelegates = new Dictionary<Guid, GCHandle>();

        public void AddToCache(Guid delegateGuid, GCHandle handle)
        {
            if (pinnedDelegates.ContainsKey(delegateGuid))
            {
                throw new Exception("Delegate already exists in cache");
            }

            pinnedDelegates.Add(delegateGuid, handle);
        }

        public void RemoveFromCache(Guid delegateGuid)
        {
            if (!pinnedDelegates.ContainsKey(delegateGuid))
            {
                throw new Exception("Delegate does not exist in cache");
            }
            
            pinnedDelegates[delegateGuid].Free();
            pinnedDelegates.Remove(delegateGuid);
        }
    }

    [Flags]
    public enum ManagedClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2,
        EnumType = 0x4
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

        public void AddMethod(string methodName, Guid guid, string[] attributeNames)
        {
            IntPtr methodNamePtr = Marshal.StringToHGlobalAnsi(methodName);
            IntPtr[] attributeNamesPtrs = new IntPtr[attributeNames.Length];

            for (int i = 0; i < attributeNames.Length; i++)
            {
                attributeNamesPtrs[i] = Marshal.StringToHGlobalAnsi(attributeNames[i]);
            }

            ManagedClass_AddMethod(ref this, methodNamePtr, guid, (uint)attributeNames.Length, attributeNamesPtrs);

            Marshal.FreeHGlobal(methodNamePtr);

            for (int i = 0; i < attributeNames.Length; i++)
            {
                Marshal.FreeHGlobal(attributeNamesPtrs[i]);
            }
        }

        public NewObjectDelegate NewObjectFunction
        {
            set
            {
                if (newObjectGuid != Guid.Empty)
                {
                    DelegateCache.Instance.RemoveFromCache(newObjectGuid);
                }

                newObjectGuid = Guid.NewGuid();

                GCHandle handle = GCHandle.Alloc(value);
                DelegateCache.Instance.AddToCache(newObjectGuid, handle);

                ManagedClass_SetNewObjectFunction(ref this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        public FreeObjectDelegate FreeObjectFunction
        {
            set
            {
                if (freeObjectGuid != Guid.Empty)
                {
                    DelegateCache.Instance.RemoveFromCache(freeObjectGuid);
                }

                freeObjectGuid = Guid.NewGuid();

                GCHandle handle = GCHandle.Alloc(value);
                DelegateCache.Instance.AddToCache(freeObjectGuid, handle);

                ManagedClass_SetFreeObjectFunction(ref this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        public MarshalObjectDelegate MarshalObjectFunction
        {
            set
            {
                if (marshalObjectGuid != Guid.Empty)
                {
                    DelegateCache.Instance.RemoveFromCache(marshalObjectGuid);
                }

                marshalObjectGuid = Guid.NewGuid();

                GCHandle handle = GCHandle.Alloc(value);
                DelegateCache.Instance.AddToCache(marshalObjectGuid, handle);

                ManagedClass_SetMarshalObjectFunction(ref this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        // Add a function pointer to the managed class
        [DllImport("hyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern void ManagedClass_AddMethod([In] ref ManagedClass managedClass, IntPtr methodNamePtr, Guid guid, uint numAttributes, IntPtr[] attributeNames);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern void ManagedClass_SetNewObjectFunction([In] ref ManagedClass managedClass, IntPtr newObjectFunctionPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetFreeObjectFunction")]
        private static extern void ManagedClass_SetFreeObjectFunction([In] ref ManagedClass managedClass, IntPtr freeObjectFunctionPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetMarshalObjectFunction")]
        private static extern void ManagedClass_SetMarshalObjectFunction([In] ref ManagedClass managedClass, IntPtr marshalObjectFunctionPtr);
    }
}