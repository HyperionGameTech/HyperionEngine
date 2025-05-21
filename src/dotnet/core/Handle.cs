using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class ManagedHandleNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "Handle_Get")]
        internal static extern void Handle_Get(TypeID type_id, IntPtr headerPtr, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "Handle_Set")]
        internal static extern void Handle_Set(TypeID type_id, [In] ref HypDataBuffer hypDataBuffer, [Out] out IntPtr outHeaderPtr);


        [DllImport("hyperion", EntryPoint = "Handle_Destruct")]
        internal static extern void Handle_Destruct(IntPtr headerPtr);
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct Handle<T> : IDisposable where T : HypObject
    {
        public static readonly Handle<T> Empty = new Handle<T>();

        private IntPtr header;

        public Handle()
        {
            this.header = IntPtr.Zero;
        }

        public Handle(T? value)
        {
            if (value == null)
            {
                header = IntPtr.Zero;

                return;
            }

            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            using (HypData hypData = new HypData((T)value))
            {
                ManagedHandleNativeBindings.Handle_Set(((HypClass)hypClass).TypeID, ref hypData.Buffer, out header);
            }
        }

        public void Dispose()
        {
            if (header != IntPtr.Zero)
            {
                ManagedHandleNativeBindings.Handle_Destruct(header);

                header = IntPtr.Zero;
            }
        }

        public bool IsValid
        {
            get
            {
                return header != IntPtr.Zero;
            }
        }

        public T? GetValue()
        {
            if (!IsValid)
            {
                return null;
            }

            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            HypDataBuffer hypDataBuffer;
            ManagedHandleNativeBindings.Handle_Get(((HypClass)hypClass).TypeID, header, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Destruct();

            return value;
        }
    }
}