using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class PtrNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "Ptr_Get")]
        internal static extern void Ptr_Get(IntPtr pTypeInfo, IntPtr pObject, [Out] out HypDataBuffer outHypDataBuffer);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Ptr<T>
    {
        public static readonly Ptr<T> Null = new Ptr<T>(0);

        private IntPtr ptr;

        public Ptr(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsNull
        {
            get
            {
                return ptr == IntPtr.Zero;
            }
        }

        public T? GetValue()
        {
            Class? cls = Class.GetClass<T>();
            
            if (cls == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered Class");
            }

            if (ptr == IntPtr.Zero)
            {
                return default(T);
            }

            HypDataBuffer hypDataBuffer;
            PtrNativeBindings.Ptr_Get(((Class)cls).TypeInfo.Address, ptr, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Dispose();

            return value;
        }

        public static T? FromIntPtr(IntPtr ptr)
        {
            return new Ptr<T>(ptr).GetValue();
        }
    }
}