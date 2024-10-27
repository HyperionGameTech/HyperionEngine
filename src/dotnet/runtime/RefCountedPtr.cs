using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class RefCountedPtrNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "RefCountedPtr_IncRef")]
        internal static extern uint RefCountedPtr_IncRef(IntPtr ctrlBlock);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_DecRef")]
        internal static extern uint RefCountedPtr_DecRef(IntPtr ctrlBlock);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_Get")]
        internal static extern void RefCountedPtr_Get(IntPtr ctrlBlock, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_IncRef")]
        internal static extern uint WeakRefCountedPtr_IncRef(IntPtr ctrlBlock);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_DecRef")]
        internal static extern uint WeakRefCountedPtr_DecRef(IntPtr ctrlBlock);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_Lock")]
        internal static extern uint WeakRefCountedPtr_Lock(IntPtr ctrlBlock);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct RefCountedPtr
    {
        public static readonly RefCountedPtr Null = new RefCountedPtr(IntPtr.Zero);

        private IntPtr ctrlBlock;

        public RefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ctrlBlock;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_IncRef(ctrlBlock);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_DecRef(ctrlBlock);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct RefCountedPtr<T>
    {
        public static readonly RefCountedPtr<T> Null = new RefCountedPtr<T>(IntPtr.Zero);

        private IntPtr ctrlBlock;

        public RefCountedPtr()
        {
            this.ctrlBlock = IntPtr.Zero;
        }

        public RefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ctrlBlock;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_IncRef(ctrlBlock);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_DecRef(ctrlBlock);
        }

        public T? GetValue()
        {
            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            HypDataBuffer hypDataBuffer;
            RefCountedPtrNativeBindings.RefCountedPtr_Get(ctrlBlock, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            HypDataBuffer.HypData_Destruct(ref hypDataBuffer);

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct WeakRefCountedPtr
    {
        private IntPtr ctrlBlock;

        public WeakRefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ctrlBlock;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_IncRef(ctrlBlock);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_DecRef(ctrlBlock);
        }

        public RefCountedPtr Lock()
        {
            uint refCount = RefCountedPtrNativeBindings.WeakRefCountedPtr_Lock(ctrlBlock);

            if (refCount == 0)
            {
                return RefCountedPtr.Null;
            }

            return new RefCountedPtr(ctrlBlock);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct WeakRefCountedPtr<T>
    {
        private IntPtr ctrlBlock;

        public WeakRefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ctrlBlock;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_IncRef(ctrlBlock);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_DecRef(ctrlBlock);
        }

        public RefCountedPtr<T> Lock()
        {
            uint refCount = RefCountedPtrNativeBindings.WeakRefCountedPtr_Lock(ctrlBlock);

            if (refCount == 0)
            {
                return RefCountedPtr<T>.Null;
            }

            return new RefCountedPtr<T>(ctrlBlock);
        }
    }
}
