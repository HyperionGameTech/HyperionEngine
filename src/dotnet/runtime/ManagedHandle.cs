using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class ManagedHandleNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "ManagedHandle_Get")]
        internal static extern void ManagedHandle_Get(TypeID type_id, uint id, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "ManagedHandle_Set")]
        internal static extern void ManagedHandle_Set(TypeID type_id, uint id, [In] ref HypDataBuffer hypDataBuffer);
    }

    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct Handle<T> where T : HypObject
    {
        public static readonly Handle<T> Empty = new Handle<T>(0);

        [MarshalAs(UnmanagedType.U4)]
        public uint id;

        public Handle(uint id)
        {
            this.id = id;
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
                ManagedHandleNativeBindings.ManagedHandle_Set(((HypClass)hypClass).TypeID, id, ref hypData.Buffer);
            }
        }

        public bool IsValid
        {
            get
            {
                return id != 0;
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
            ManagedHandleNativeBindings.ManagedHandle_Get(((HypClass)hypClass).TypeID, id, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            HypDataBuffer.HypData_Destruct(ref hypDataBuffer);

            return value;
        }
    }
}