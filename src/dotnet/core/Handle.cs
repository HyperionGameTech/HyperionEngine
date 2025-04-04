using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class ManagedHandleNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "Handle_Get")]
        internal static extern void Handle_Get(TypeID type_id, IntPtr ptr, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "Handle_Set")]
        internal static extern void Handle_Set(TypeID type_id, IntPtr ptr, [In] ref HypDataBuffer hypDataBuffer);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct Handle<T> where T : HypObject
    {
        public static readonly Handle<T> Empty = new Handle<T>();

        private IntPtr ptr;

        public Handle(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Handle(T value)
        {
            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            using (HypData hypData = new HypData(value))
            {
                ManagedHandleNativeBindings.Handle_Set(((HypClass)hypClass).TypeID, ptr, ref hypData.Buffer);
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
            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            HypDataBuffer hypDataBuffer;
            ManagedHandleNativeBindings.Handle_Get(((HypClass)hypClass).TypeID, ptr, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Destruct();

            return value;
        }
    }
}