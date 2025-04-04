using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class PtrNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "Ptr_Get")]
        internal static extern void Ptr_Get(TypeID type_id, IntPtr ptr, [Out] out HypDataBuffer outHypDataBuffer);
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
            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            if (ptr == IntPtr.Zero)
            {
                return default(T);
            }

            HypDataBuffer hypDataBuffer;
            PtrNativeBindings.Ptr_Get(((HypClass)hypClass).TypeID, ptr, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Destruct();

            return value;
        }

        public static T? FromIntPtr(IntPtr ptr)
        {
            return new Ptr<T>(ptr).GetValue();
        }
    }
}