using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    public delegate ManagedObject NewObjectDelegate();
    public delegate void FreeObjectDelegate(ManagedObject obj);

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

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedClass
    {
        private int typeHash;
        private IntPtr classObjectPtr;
        private Guid assemblyGuid;
        private Guid newObjectGuid;
        private Guid freeObjectGuid;

        public void AddMethod(string methodName, Guid guid, string[] attributeNames)
        {
            IntPtr methodNamePtr = Marshal.StringToHGlobalAnsi(methodName);
            IntPtr[] attributeNamesPtrs = new IntPtr[attributeNames.Length];

            for (int i = 0; i < attributeNames.Length; i++)
            {
                attributeNamesPtrs[i] = Marshal.StringToHGlobalAnsi(attributeNames[i]);
            }

            ManagedClass_AddMethod(this, methodNamePtr, guid, (uint)attributeNames.Length, attributeNamesPtrs);

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

                ManagedClass_SetNewObjectFunction(this, Marshal.GetFunctionPointerForDelegate(value));
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

                ManagedClass_SetFreeObjectFunction(this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        // Add a function pointer to the managed class
        [DllImport("hyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern void ManagedClass_AddMethod(ManagedClass managedClass, IntPtr methodNamePtr, Guid guid, uint numAttributes, IntPtr[] attributeNames);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern void ManagedClass_SetNewObjectFunction(ManagedClass managedClass, IntPtr newObjectFunctionPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetFreeObjectFunction")]
        private static extern void ManagedClass_SetFreeObjectFunction(ManagedClass managedClass, IntPtr freeObjectFunctionPtr);
    }
}