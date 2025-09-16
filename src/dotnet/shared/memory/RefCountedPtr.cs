using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class RefCountedPtrNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "RefCountedPtr_IncRef")]
        internal static extern void RefCountedPtr_IncRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_DecRef")]
        internal static extern void RefCountedPtr_DecRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_Get")]
        internal static extern void RefCountedPtr_Get(IntPtr address, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_IncRef")]
        internal static extern void WeakRefCountedPtr_IncRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_DecRef")]
        internal static extern void WeakRefCountedPtr_DecRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_Lock")]
        internal static extern uint WeakRefCountedPtr_Lock(IntPtr address);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct RefCountedPtr
    {
        public static readonly RefCountedPtr Null = new RefCountedPtr();

        private IntPtr ptr = IntPtr.Zero;

        public RefCountedPtr()
        {
        }

        public RefCountedPtr(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public bool IsNull
        {
            get
            {
                return ptr == IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            if (IsNull)
            {
                throw new Exception("RefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.RefCountedPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("RefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.RefCountedPtr_DecRef(ptr);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct RefCountedPtr<T>
    {
        public static readonly RefCountedPtr<T> Null = new RefCountedPtr<T>();

        private IntPtr ptr = IntPtr.Zero;

        public RefCountedPtr()
        {
        }

        public RefCountedPtr(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public bool IsNull
        {
            get
            {
                return ptr == IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            if (IsNull)
            {
                throw new Exception("RefCountedPtr is null");
            }
            
            RefCountedPtrNativeBindings.RefCountedPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("RefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.RefCountedPtr_DecRef(ptr);
        }

        public T? GetValue()
        {
            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            HypDataBuffer hypDataBuffer;
            RefCountedPtrNativeBindings.RefCountedPtr_Get(ptr, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Dispose();

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct WeakRefCountedPtr
    {
        private IntPtr ptr = IntPtr.Zero;

        public WeakRefCountedPtr()
        {
        }

        public WeakRefCountedPtr(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public bool IsNull
        {
            get
            {
                return ptr == IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            if (IsNull)
            {
                throw new Exception("WeakRefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.WeakRefCountedPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("WeakRefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.WeakRefCountedPtr_DecRef(ptr);
        }

        public RefCountedPtr Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return RefCountedPtr.Null;
            }

            uint refCount = RefCountedPtrNativeBindings.WeakRefCountedPtr_Lock(ptr);

            if (refCount == 0)
            {
                return RefCountedPtr.Null;
            }

            return new RefCountedPtr(ptr);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct WeakRefCountedPtr<T>
    {
        private IntPtr ptr = IntPtr.Zero;

        public WeakRefCountedPtr()
        {
        }

        public WeakRefCountedPtr(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public bool IsNull
        {
            get
            {
                return ptr == IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            if (IsNull)
            {
                throw new Exception("WeakRefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.WeakRefCountedPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("WeakRefCountedPtr is null");
            }

            RefCountedPtrNativeBindings.WeakRefCountedPtr_DecRef(ptr);
        }

        public RefCountedPtr<T> Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return RefCountedPtr<T>.Null;
            }

            uint refCount = RefCountedPtrNativeBindings.WeakRefCountedPtr_Lock(ptr);

            if (refCount == 0)
            {
                return RefCountedPtr<T>.Null;
            }

            return new RefCountedPtr<T>(ptr);
        }
    }
}
