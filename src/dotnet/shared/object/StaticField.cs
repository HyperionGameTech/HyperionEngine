using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct StaticField
    {
        public static readonly StaticField Invalid = new StaticField(IntPtr.Zero);

        internal IntPtr ptr;

        internal StaticField(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                StaticField_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                StaticField_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        public object? ReadObject()
        {
            if (ptr == IntPtr.Zero)
            {
                throw new Exception("StaticField pointer is null");
            }

            object? result = null;

            HypDataBuffer outData = new HypDataBuffer();

            try
            {
                StaticField_Get(ptr, out outData);

                result = outData.GetValue();
            }
            finally
            {
                outData.Dispose();
            }

            return result;
        }

        [DllImport("hyperion", EntryPoint = "StaticField_GetName")]
        private static extern void StaticField_GetName([In] IntPtr constantPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "StaticField_GetTypeId")]
        private static extern void StaticField_GetTypeId([In] IntPtr constantPtr, [Out] out TypeId typeId);

        [DllImport("hyperion", EntryPoint = "StaticField_Get")]
        private static extern bool StaticField_Get([In] IntPtr staticFieldPtr, out HypDataBuffer outData);
    }
}