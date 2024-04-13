using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Wrapper for RefCountedPtr.hpp from the core library
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct RefCountedPtr
    {
        public static readonly RefCountedPtr Null = new RefCountedPtr(IntPtr.Zero);

        private IntPtr ctrlBlock;

        public RefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }

        public bool Valid
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
            RefCountedPtr_IncRef(this);
        }

        public void DecRef()
        {
            RefCountedPtr_DecRef(this);
        }

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_IncRef")]
        private static extern void RefCountedPtr_IncRef(RefCountedPtr ptr);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_DecRef")]
        private static extern void RefCountedPtr_DecRef(RefCountedPtr ptr);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct WeakRefCountedPtr
    {
        public static readonly WeakRefCountedPtr Null = new WeakRefCountedPtr(IntPtr.Zero);

        private IntPtr ctrlBlock;

        public WeakRefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }

        public bool Valid
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
            WeakRefCountedPtr_IncRef(this);
        }

        public void DecRef()
        {
            WeakRefCountedPtr_DecRef(this);
        }

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_IncRef")]
        private static extern void WeakRefCountedPtr_IncRef(WeakRefCountedPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_DecRef")]
        private static extern void WeakRefCountedPtr_DecRef(WeakRefCountedPtr ptr);
    }
}
