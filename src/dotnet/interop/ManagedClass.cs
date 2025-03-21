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

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetAttributes")]
        private static extern void ManagedClass_SetAttributes([In] ref ManagedClass managedClass, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern void ManagedClass_AddMethod([In] ref ManagedClass managedClass, IntPtr methodNamePtr, Guid guid, IntPtr functionPointer, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_AddProperty")]
        private static extern void ManagedClass_AddProperty([In] ref ManagedClass managedClass, IntPtr propertyNamePtr, Guid guid, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern void ManagedClass_SetNewObjectFunction([In] ref ManagedClass managedClass, IntPtr newObjectFunctionPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetFreeObjectFunction")]
        private static extern void ManagedClass_SetFreeObjectFunction([In] ref ManagedClass managedClass, IntPtr freeObjectFunctionPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetMarshalObjectFunction")]
        private static extern void ManagedClass_SetMarshalObjectFunction([In] ref ManagedClass managedClass, IntPtr marshalObjectFunctionPtr);
    }
}