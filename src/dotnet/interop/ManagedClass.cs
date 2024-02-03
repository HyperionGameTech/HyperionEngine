using System;
using System.Runtime.InteropServices;

namespace Hyperion {
    public delegate ManagedObject NewObjectDelegate();
    public delegate void FreeObjectDelegate(ManagedObject obj);

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedClass
    {
        private int typeHash;
        private IntPtr classObjectPtr;

        public ManagedMethod AddMethod(string methodName, Guid guid)
        {
            return ManagedClass_AddMethod(this, methodName, guid);
        }

        public NewObjectDelegate NewObjectFunction
        {
            set
            {
                ManagedClass_SetNewObjectFunction(this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        public FreeObjectDelegate FreeObjectFunction
        {
            set
            {
                ManagedClass_SetFreeObjectFunction(this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        // Add a function pointer to the managed class
        [DllImport("libhyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern ManagedMethod ManagedClass_AddMethod(ManagedClass managedClass, string methodName, Guid guid);

        [DllImport("libhyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern ManagedClass ManagedClass_SetNewObjectFunction(ManagedClass managedClass, IntPtr newObjectFunctionPtr);

        [DllImport("libhyperion", EntryPoint = "ManagedClass_SetFreeObjectFunction")]
        private static extern ManagedClass ManagedClass_SetFreeObjectFunction(ManagedClass managedClass, IntPtr freeObjectFunctionPtr);
    }
}