using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class SharedPtrNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "SharedPtr_IncRef")]
        internal static extern void SharedPtr_IncRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "SharedPtr_DecRef")]
        internal static extern void SharedPtr_DecRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "SharedPtr_Get")]
        internal static extern void SharedPtr_Get(IntPtr address, [Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "WeakPtr_IncRef")]
        internal static extern void WeakPtr_IncRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "WeakPtr_DecRef")]
        internal static extern void WeakPtr_DecRef(IntPtr address);

        [DllImport("hyperion", EntryPoint = "WeakPtr_Lock")]
        internal static extern uint WeakPtr_Lock(IntPtr address);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct SharedPtr
    {
        public static readonly SharedPtr Null = new SharedPtr();

        private IntPtr ptr = IntPtr.Zero;

        public SharedPtr()
        {
        }

        public SharedPtr(IntPtr ptr)
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
                throw new Exception("SharedPtr is null");
            }

            SharedPtrNativeBindings.SharedPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("SharedPtr is null");
            }

            SharedPtrNativeBindings.SharedPtr_DecRef(ptr);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct SharedPtr<T>
    {
        public static readonly SharedPtr<T> Null = new SharedPtr<T>();

        private IntPtr ptr = IntPtr.Zero;

        public SharedPtr()
        {
        }

        public SharedPtr(IntPtr ptr)
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
                throw new Exception("SharedPtr is null");
            }
            
            SharedPtrNativeBindings.SharedPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("SharedPtr is null");
            }

            SharedPtrNativeBindings.SharedPtr_DecRef(ptr);
        }

        public T? GetValue()
        {
            Class? cls = Class.GetClass<T>();
            
            if (cls == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered Class");
            }

            BoxedValueInternal boxedInternal;
            SharedPtrNativeBindings.SharedPtr_Get(ptr, out boxedInternal);

            T? value = (T?)boxedInternal.GetValue();

            boxedInternal.Dispose();

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct WeakPtr
    {
        private IntPtr ptr = IntPtr.Zero;

        public WeakPtr()
        {
        }

        public WeakPtr(IntPtr ptr)
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
                throw new Exception("WeakPtr is null");
            }

            SharedPtrNativeBindings.WeakPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("WeakPtr is null");
            }

            SharedPtrNativeBindings.WeakPtr_DecRef(ptr);
        }

        public SharedPtr Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return SharedPtr.Null;
            }

            uint refCount = SharedPtrNativeBindings.WeakPtr_Lock(ptr);

            if (refCount == 0)
            {
                return SharedPtr.Null;
            }

            return new SharedPtr(ptr);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8, Pack = 8)]
    public struct WeakPtr<T>
    {
        private IntPtr ptr = IntPtr.Zero;

        public WeakPtr()
        {
        }

        public WeakPtr(IntPtr ptr)
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
                throw new Exception("WeakPtr is null");
            }

            SharedPtrNativeBindings.WeakPtr_IncRef(ptr);
        }

        public void DecRef()
        {
            if (IsNull)
            {
                throw new Exception("WeakPtr is null");
            }

            SharedPtrNativeBindings.WeakPtr_DecRef(ptr);
        }

        public SharedPtr<T> Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return SharedPtr<T>.Null;
            }

            uint refCount = SharedPtrNativeBindings.WeakPtr_Lock(ptr);

            if (refCount == 0)
            {
                return SharedPtr<T>.Null;
            }

            return new SharedPtr<T>(ptr);
        }
    }
}
