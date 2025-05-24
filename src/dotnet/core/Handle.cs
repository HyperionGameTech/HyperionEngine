using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class ManagedHandleNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "Handle_Get")]
        internal static extern void Handle_Get(IntPtr ptr, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "Handle_Set")]
        internal static extern void Handle_Set([In] ref HypDataBuffer hypDataBuffer, [Out] out IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "Handle_Destruct")]
        public static extern void Handle_Destruct(IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakHandle_Lock")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static extern bool WeakHandle_Lock(IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakHandle_Set")]
        internal static extern void WeakHandle_Set([In] ref HypDataBuffer hypDataBuffer, [Out] out IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "WeakHandle_Destruct")]
        internal static extern void WeakHandle_Destruct(IntPtr ptr);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct Handle<T> : IDisposable where T : HypObject
    {
        public static readonly Handle<T> Empty = new Handle<T>();

        internal IntPtr ptr;

        public Handle()
        {
            this.ptr = IntPtr.Zero;
        }

        public Handle(T? value)
        {
            if (value == null)
            {
                ptr = IntPtr.Zero;
                return;
            }

            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            HypDataBuffer hypDataBuffer = new HypDataBuffer();
            hypDataBuffer.SetValue((T)value);

            ManagedHandleNativeBindings.Handle_Set(ref hypDataBuffer, out ptr);

            hypDataBuffer.Dispose();
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.Handle_Destruct(ptr);
                ptr = IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public T? GetValue()
        {
            HypDataBuffer hypDataBuffer;
            ManagedHandleNativeBindings.Handle_Get(ptr, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Dispose();

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct WeakHandle<T> : IDisposable where T : HypObject
    {
        public static readonly WeakHandle<T> Empty = new WeakHandle<T>();

        internal IntPtr ptr;

        public WeakHandle()
        {
            this.ptr = IntPtr.Zero;
        }

        public WeakHandle(T? value)
        {
            if (value == null)
            {
                ptr = IntPtr.Zero;

                return;
            }

            using (HypData hypData = new HypData((T)value))
            {
                ManagedHandleNativeBindings.WeakHandle_Set(ref hypData.Buffer, out ptr);
            }
        }

        public void Dispose()
        {
            if (ptr != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.WeakHandle_Destruct(ptr);
                ptr = IntPtr.Zero;
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Handle<T> Lock()
        {
            if (ptr == IntPtr.Zero)
            {
                return Handle<T>.Empty;
            }

            if (ManagedHandleNativeBindings.WeakHandle_Lock(ptr))
            {
                Handle<T> handle = new Handle<T>();
                handle.ptr = ptr;
                return handle;
            }
            else
            {
                return Handle<T>.Empty;
            }
        }
    }
}